// ChunkWorld: the chunk pool's allocation, growth, linking and per-cell edits.
// The design is described at the top of ChunkWorld.h.

#include "world/ChunkWorld.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include "gpu/Barriers.h"
#include "gpu/GpuContext.h"
#include "gpu/ImmediateCommands.h"

namespace gol3d {

// The CPU's hash map and the GPU tables use the same "no chunk" value.
static_assert(ChunkMap::NONE == NO_CHUNK);

namespace {

// The pool starts small and doubles as the world grows.
constexpr uint32_t INITIAL_CHUNK_SLOTS = 512;
constexpr uint32_t INITIAL_BLOCK_SLOTS = 64;

constexpr VkDeviceSize CHUNK_BYTES = CHUNK_ROWS * sizeof(uint32_t);
constexpr VkDeviceSize BLOCK_SLOT_BYTES = BLOCK_ROWS * sizeof(uint32_t);

// Copies the whole of `from` into the start of `to`.
void recordCopy(VkCommandBuffer cmd, const GpuBuffer& from, const GpuBuffer& to) {
    VkBufferCopy region{0, 0, from.size};
    vkCmdCopyBuffer(cmd, from.buffer, to.buffer, 1, &region);
}

// Adds slots first .. end - 1 to a free list. Free lists are stacks, so the
// highest slot goes in first and the lowest is handed out first.
void addFreeSlots(std::vector<uint32_t>& freeList, uint32_t first, uint32_t end) {
    for (uint32_t slot = end; slot-- > first;) {
        freeList.push_back(slot);
    }
}

} // namespace

// ------------------------------------------------------------------ lifetime

void ChunkWorld::init(const GpuContext& gpu, BufferAllocator& allocator,
                      ImmediateCommands& commands, uint32_t chunkLimit) {
    gpu_ = &gpu;
    allocator_ = &allocator;
    commands_ = &commands;

    // One storage buffer may not exceed maxStorageBufferRange (128 MB on some GPUs).
    const uint64_t maxBufferBytes = gpu.properties.limits.maxStorageBufferRange;
    const uint32_t chunksThatFit =
        static_cast<uint32_t>(std::min<uint64_t>(maxBufferBytes / CHUNK_BYTES, MAX_CHUNK_SLOTS));
    chunkLimit_ = chunkLimit;
    if (chunkLimit_ > chunksThatFit) {
        std::cout << "This GPU's buffers hold at most " << chunksThatFit
                  << " chunks; using that as the chunk limit." << std::endl;
        chunkLimit_ = chunksThatFit;
    }
    maxBlockSlots_ = static_cast<uint32_t>(
        std::min<uint64_t>(maxBufferBytes / BLOCK_SLOT_BYTES, MAX_CHUNK_SLOTS));

    if (!allocatePool(pool_, std::min(INITIAL_CHUNK_SLOTS, chunkLimit_))) {
        throw std::runtime_error("Out of GPU memory for the world");
    }
    resizeTables(pool_.capacity);
    allocator.create(blockPool_, static_cast<VkDeviceSize>(INITIAL_BLOCK_SLOTS) * BLOCK_SLOT_BYTES,
                     usage::STORAGE_COPY, {memory::DEVICE_HOST, memory::HOST});
    blockCapacity_ = INITIAL_BLOCK_SLOTS;
}

void ChunkWorld::destroy() {
    if (!allocator_) return;
    releasePool(pool_);
    allocator_->destroy(blockPool_);
    allocator_ = nullptr;
}

void ChunkWorld::reset() {
    vkDeviceWaitIdle(gpu_->device);
    chunkMap_.clear();
    freeSlots_.clear();
    quarantine_.clear();
    addFreeSlots(freeSlots_, 0, pool_.capacity);
    activeSlots_.clear();
    freeBlockSlots_.clear();
    addFreeSlots(freeBlockSlots_, 0, blockCapacity_);
    std::fill(blockSlotOf_.begin(), blockSlotOf_.end(), NO_CHUNK);
    std::fill(blockCount_.begin(), blockCount_.end(), 0u);
    limitReached_ = false;
}

// --------------------------------------------------------------- the pools

// Cells, tables and stats for `capacity` chunks. All but the stats are
// device-local and CPU-visible (resizable BAR) when available, falling back to
// host memory: the GPU does the heavy reads, while the CPU writes the tables
// directly and only touches a few cell rows to edit and to collide with blocks.
// The stats are written only by the GPU and reach the CPU through
// statsReadback, which is host-cached because maintain() reads every active
// chunk's entry (and a transfer source so growPool() can carry it over).
bool ChunkWorld::allocatePool(ChunkPool& pool, uint32_t capacity) {
    const VkDeviceSize slots = capacity;
    const auto mapped = {memory::DEVICE_HOST, memory::HOST};
    pool.capacity = capacity;
    // Stops at the first failure; the caller then gets `false` and nothing leaks.
    bool ok = true;
    auto create = [&](GpuBuffer& buffer, VkDeviceSize bytes, VkBufferUsageFlags usageFlags,
                      std::initializer_list<VkMemoryPropertyFlags> memoryTypes) {
        ok = ok && allocator_->tryCreate(buffer, bytes, usageFlags, memoryTypes);
    };
    for (GpuBuffer& cells : pool.cells) {
        create(cells, slots * CHUNK_BYTES, usage::STORAGE_COPY, mapped);
    }
    create(pool.neighbors, slots * NEIGHBORHOOD_SIZE * sizeof(uint32_t), usage::STORAGE_COPY,
           mapped);
    create(pool.activeList, slots * sizeof(uint32_t), usage::STORAGE_COPY, mapped);
    create(pool.origins, slots * sizeof(glm::ivec4), usage::STORAGE_COPY, mapped);
    create(pool.blockSlots, slots * sizeof(uint32_t), usage::STORAGE_COPY, mapped);
    create(pool.stats, slots * sizeof(ChunkStats), usage::STORAGE_COPY,
           {memory::DEVICE, memory::HOST});
    create(pool.statsReadback, slots * sizeof(ChunkStats),
           VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
           {memory::HOST_CACHED, memory::HOST});
    if (!ok) releasePool(pool);
    return ok;
}

void ChunkWorld::releasePool(ChunkPool& pool) {
    for (GpuBuffer& cells : pool.cells) {
        allocator_->destroy(cells);
    }
    for (GpuBuffer* buffer : {&pool.neighbors, &pool.activeList, &pool.origins, &pool.blockSlots,
                              &pool.stats, &pool.statsReadback}) {
        allocator_->destroy(*buffer);
    }
    pool.capacity = 0;
}

void ChunkWorld::resizeTables(uint32_t capacity) {
    slotChunk_.resize(capacity, glm::ivec3(0));
    neighborSlots_.resize(static_cast<size_t>(capacity) * NEIGHBORHOOD_SIZE, NO_CHUNK);
    activeIndex_.resize(capacity, 0);
    blockSlotOf_.resize(capacity, NO_CHUNK);
    blockCount_.resize(capacity, 0);
    lastWanted_.resize(capacity, 0);
}

// Doubles the chunk pool (up to the limit), keeping every chunk in its slot.
// Rare and brief: it waits for the GPU and copies the old pool on the GPU, since
// reading device memory back through the CPU mapping is slow.
bool ChunkWorld::growPool() {
    if (pool_.capacity >= chunkLimit_) return false;
    const uint32_t oldCapacity = pool_.capacity;
    const uint32_t capacity = std::min(oldCapacity * 2, chunkLimit_);
    // Frames in flight still read the old buffers, which are destroyed below.
    vkDeviceWaitIdle(gpu_->device);
    ChunkPool bigger;
    if (!allocatePool(bigger, capacity)) {
        if (onWarning) {
            onWarning("Out of GPU memory: the world can't grow past " +
                      std::to_string(pool_.capacity) + " chunks");
        }
        chunkLimit_ = pool_.capacity;
        return false;
    }

    VkCommandBuffer cmd = commands_->begin();
    for (size_t generation = 0; generation < pool_.cells.size(); ++generation) {
        recordCopy(cmd, pool_.cells[generation], bigger.cells[generation]);
    }
    recordCopy(cmd, pool_.neighbors, bigger.neighbors);
    recordCopy(cmd, pool_.activeList, bigger.activeList);
    recordCopy(cmd, pool_.origins, bigger.origins);
    recordCopy(cmd, pool_.blockSlots, bigger.blockSlots);
    // maintain() may grow the pool while it is still reading the stats. (The
    // GPU-side stats need no copy: the next build pass clears and refills them.)
    recordCopy(cmd, pool_.statsReadback, bigger.statsReadback);
    // The CPU reads and writes the new buffers through their mappings next.
    copiesVisibleToHost(cmd);
    commands_->submitAndWait("growing the world");

    releasePool(pool_);
    pool_ = bigger;
    resizeTables(capacity);
    addFreeSlots(freeSlots_, oldCapacity, capacity);
    if (onBuffersReplaced) onBuffersReplaced();
    return true;
}

// Doubles the block pool, like growPool(). On failure the caller's block is not
// placed, but the world keeps running.
bool ChunkWorld::growBlockPool() {
    const uint32_t capacity = std::min(blockCapacity_ * 2, maxBlockSlots_);
    if (capacity <= blockCapacity_) return false;
    vkDeviceWaitIdle(gpu_->device); // frames in flight still read the old buffer
    GpuBuffer bigger;
    if (!allocator_->tryCreate(bigger, static_cast<VkDeviceSize>(capacity) * BLOCK_SLOT_BYTES,
                               usage::STORAGE_COPY, {memory::DEVICE_HOST, memory::HOST})) {
        return false;
    }
    VkCommandBuffer cmd = commands_->begin();
    recordCopy(cmd, blockPool_, bigger);
    copiesVisibleToHost(cmd);
    commands_->submitAndWait("growing the block pool");

    allocator_->destroy(blockPool_);
    blockPool_ = bigger;
    addFreeSlots(freeBlockSlots_, blockCapacity_, capacity);
    blockCapacity_ = capacity;
    if (onBuffersReplaced) onBuffersReplaced();
    return true;
}

// ------------------------------------------------------------------ chunks

void ChunkWorld::setNeighbor(uint32_t slot, int k, uint32_t neighbor) {
    size_t entry = static_cast<size_t>(slot) * NEIGHBORHOOD_SIZE + k;
    neighborSlots_[entry] = neighbor;
    pool_.neighbors.as<uint32_t>()[entry] = neighbor;
}

uint32_t ChunkWorld::ensureChunk(const glm::ivec3& chunk) {
    uint32_t slot = findChunk(chunk);
    if (slot != NO_CHUNK) return slot;
    bool inRange = ChunkMap::inRange(chunk.x, chunk.y, chunk.z);
    if (!inRange || (freeSlots_.empty() && !growPool())) {
        limitReached_ = true;
        return NO_CHUNK;
    }

    slot = freeSlots_.back();
    freeSlots_.pop_back();
    // Both generations: the build pass compares them to find births and deaths.
    for (uint32_t buffer = 0; buffer < 2; ++buffer) {
        std::memset(chunkRows(buffer, slot), 0, CHUNK_BYTES);
    }
    pool_.origins.as<glm::ivec4>()[slot] = glm::ivec4(chunk * CHUNK_SIZE, 0);
    pool_.blockSlots.as<uint32_t>()[slot] = NO_CHUNK;
    slotChunk_[slot] = chunk;
    blockSlotOf_[slot] = NO_CHUNK;
    blockCount_[slot] = 0;
    chunkMap_.insert(chunk.x, chunk.y, chunk.z, slot);

    // Link it with the neighbors that exist, in both directions.
    for (int k = 0; k < NEIGHBORHOOD_SIZE; ++k) {
        if (k == SELF_NEIGHBOR) {
            setNeighbor(slot, k, slot);
            continue;
        }
        uint32_t neighbor = findChunk(chunk + neighborOffset(k));
        setNeighbor(slot, k, neighbor);
        if (neighbor != NO_CHUNK) setNeighbor(neighbor, oppositeNeighbor(k), slot);
    }

    // Append it to the active list (CPU copy and GPU list).
    activeIndex_[slot] = static_cast<uint32_t>(activeSlots_.size());
    pool_.activeList.as<uint32_t>()[activeSlots_.size()] = slot;
    activeSlots_.push_back(slot);
    return slot;
}

// Unlinks and deactivates a chunk. Its own neighbor entries, origin and cells
// are left as they are: ensureChunk() rewrites them when the slot is reused.
void ChunkWorld::freeChunk(uint32_t slot) {
    // Unlink it from its neighbors.
    for (int k = 0; k < NEIGHBORHOOD_SIZE; ++k) {
        uint32_t neighbor = neighborSlot(slot, k);
        if (k != SELF_NEIGHBOR && neighbor != NO_CHUNK) {
            setNeighbor(neighbor, oppositeNeighbor(k), NO_CHUNK);
        }
    }
    const glm::ivec3& chunk = slotChunk_[slot];
    chunkMap_.erase(chunk.x, chunk.y, chunk.z);
    if (blockSlotOf_[slot] != NO_CHUNK) releaseBlockSlot(slot);

    // Remove it from the active list by moving the last entry into its place.
    // That breaks the nearest-first order slightly until the next sortByDistance().
    uint32_t index = activeIndex_[slot];
    uint32_t last = activeSlots_.back();
    activeSlots_[index] = last;
    pool_.activeList.as<uint32_t>()[index] = last;
    activeIndex_[last] = index;
    activeSlots_.pop_back();

    quarantine_.push_back(slot);
}

uint32_t ChunkWorld::ensureBlockSlot(uint32_t slot) {
    if (blockSlotOf_[slot] != NO_CHUNK) return blockSlotOf_[slot];
    if (freeBlockSlots_.empty() && !growBlockPool()) return NO_CHUNK;
    uint32_t blockSlot = freeBlockSlots_.back();
    freeBlockSlots_.pop_back();
    std::memset(blockedRows(blockSlot), 0, BLOCK_SLOT_BYTES); // both planes
    blockSlotOf_[slot] = blockSlot;
    pool_.blockSlots.as<uint32_t>()[slot] = blockSlot;
    return blockSlot;
}

void ChunkWorld::releaseBlockSlot(uint32_t slot) {
    freeBlockSlots_.push_back(blockSlotOf_[slot]);
    blockSlotOf_[slot] = NO_CHUNK;
    pool_.blockSlots.as<uint32_t>()[slot] = NO_CHUNK;
    blockCount_[slot] = 0;
}

uint64_t ChunkWorld::maintain() {
    // Slots freed by the previous maintenance are safe to reuse now (see the
    // quarantine note at the top of ChunkWorld.h).
    freeSlots_.insert(freeSlots_.end(), quarantine_.begin(), quarantine_.end());
    quarantine_.clear();
    ++maintenanceCount_;
    uint64_t population = 0;

    // Mark every chunk that life can reach, creating the missing ones. Iterate a
    // copy: ensureChunk() appends to the active list, and the chunks it creates
    // have no stats from the last build to read.
    const std::vector<uint32_t> processed = activeSlots_;
    for (uint32_t slot : processed) {
        ChunkStats stats = readbackStats(slot);
        population += stats.population;
        // Visit each set bit k, lowest first (`mask &= mask - 1` clears it).
        for (uint32_t reachMask = stats.reachMask; reachMask != 0; reachMask &= reachMask - 1) {
            int k = std::countr_zero(reachMask);
            uint32_t neighbor = neighborSlot(slot, k);
            if (neighbor == NO_CHUNK) neighbor = ensureChunk(slotChunk_[slot] + neighborOffset(k));
            if (neighbor != NO_CHUNK) lastWanted_[neighbor] = maintenanceCount_;
        }
    }

    // Free chunks with no life, no blocks, and no life nearby. (Reach masks never
    // include the chunk itself, so its own life counts only through population.)
    for (uint32_t slot : processed) {
        bool empty = readbackStats(slot).population == 0 && blockCount_[slot] == 0;
        if (empty && lastWanted_[slot] != maintenanceCount_) freeChunk(slot);
    }
    return population;
}

void ChunkWorld::sortByDistance(const glm::vec3& eye) {
    // Distance from the eye to a chunk's center: corner + 16 - eye is the same as
    // corner - (eye - 16), so shifting the eye once saves a vector add per chunk.
    const glm::vec3 eyeMinusHalfChunk = eye - static_cast<float>(CHUNK_SIZE) * 0.5f;
    std::vector<std::pair<float, uint32_t>> order; // (squared distance, slot)
    order.reserve(activeSlots_.size());
    for (uint32_t slot : activeSlots_) {
        glm::vec3 offset = glm::vec3(slotChunk_[slot] * CHUNK_SIZE) - eyeMinusHalfChunk;
        order.emplace_back(glm::dot(offset, offset), slot);
    }
    std::sort(order.begin(), order.end()); // ties go to the lower slot, so the order is repeatable
    uint32_t* gpuList = pool_.activeList.as<uint32_t>();
    for (size_t i = 0; i < order.size(); ++i) {
        uint32_t slot = order[i].second;
        activeSlots_[i] = slot;
        activeIndex_[slot] = static_cast<uint32_t>(i);
        gpuList[i] = slot;
    }
}

// ------------------------------------------------------------------- cells

std::optional<CellKind> ChunkWorld::cellKind(const glm::ivec3& cell) const {
    uint32_t slot = findChunk(chunkOf(cell));
    if (slot == NO_CHUNK) return std::nullopt;
    const uint32_t row = rowOf(cell);
    const uint32_t bit = bitOf(cell);
    if (chunkRows(currentBuffer_, slot)[row] & bit) return CellKind::Life;

    uint32_t blockSlot = blockSlotOf_[slot];
    if (blockSlot == NO_CHUNK) return std::nullopt;
    if (!(blockedRows(blockSlot)[row] & bit)) return std::nullopt;
    bool emits = emitsRows(blockSlot)[row] & bit;
    return emits ? CellKind::Ember : CellKind::Stone;
}

void ChunkWorld::setLife(const glm::ivec3& cell, bool alive) {
    const glm::ivec3 chunk = chunkOf(cell);
    // Clearing a cell never needs a new chunk.
    uint32_t slot = alive ? ensureChunk(chunk) : findChunk(chunk);
    if (slot == NO_CHUNK) return;
    const uint32_t row = rowOf(cell);
    const uint32_t bit = bitOf(cell);
    uint32_t blockSlot = blockSlotOf_[slot];
    bool blocked = blockSlot != NO_CHUNK && (blockedRows(blockSlot)[row] & bit);
    if (alive && blocked) return;
    uint32_t& word = chunkRows(currentBuffer_, slot)[row];
    word = alive ? (word | bit) : (word & ~bit);
}

bool ChunkWorld::placeBlock(const glm::ivec3& cell, CellKind kind) {
    uint32_t slot = ensureChunk(chunkOf(cell));
    if (slot == NO_CHUNK) return false;
    uint32_t blockSlot = ensureBlockSlot(slot);
    if (blockSlot == NO_CHUNK) return false;
    const uint32_t row = rowOf(cell);
    const uint32_t bit = bitOf(cell);
    blockedRows(blockSlot)[row] |= bit;
    if (cellType(kind).countsAsNeighbor) emitsRows(blockSlot)[row] |= bit;
    ++blockCount_[slot];
    return true;
}

void ChunkWorld::removeBlock(const glm::ivec3& cell) {
    uint32_t slot = findChunk(chunkOf(cell));
    uint32_t blockSlot = blockSlotOf_[slot];
    const uint32_t row = rowOf(cell);
    const uint32_t bit = bitOf(cell);
    blockedRows(blockSlot)[row] &= ~bit;
    emitsRows(blockSlot)[row] &= ~bit;
    // The last block gone: hand the block slot back.
    if (--blockCount_[slot] == 0) releaseBlockSlot(slot);
}

void ChunkWorld::copyCurrentToPrevious() {
    if (activeSlots_.empty()) return; // new chunks start empty in both buffers
    // Only the active chunks' rows: one copy region per chunk.
    std::vector<VkBufferCopy> regions;
    regions.reserve(activeSlots_.size());
    for (uint32_t slot : activeSlots_) {
        VkDeviceSize offset = static_cast<VkDeviceSize>(slot) * CHUNK_BYTES;
        regions.push_back({offset, offset, CHUNK_BYTES});
    }
    const uint32_t previousBuffer = 1 - currentBuffer_;
    VkCommandBuffer cmd = commands_->begin();
    vkCmdCopyBuffer(cmd, pool_.cells[currentBuffer_].buffer, pool_.cells[previousBuffer].buffer,
                    static_cast<uint32_t>(regions.size()), regions.data());
    copiesVisibleToHost(cmd);
    commands_->submitAndWait("saving the world before an edit");
}

} // namespace gol3d
