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

} // namespace

// ------------------------------------------------------------------ lifetime

void ChunkWorld::init(const GpuContext& gpu, BufferAllocator& allocator,
                      ImmediateCommands& commands, uint32_t chunkLimit) {
    gpu_ = &gpu;
    allocator_ = &allocator;
    commands_ = &commands;

    // One storage buffer may not exceed maxStorageBufferRange (128 MB on some GPUs).
    const uint64_t range = gpu.properties.limits.maxStorageBufferRange;
    const uint32_t chunksThatFit =
        static_cast<uint32_t>(std::min<uint64_t>(range / CHUNK_BYTES, MAX_CHUNK_SLOTS));
    chunkLimit_ = chunkLimit;
    if (chunkLimit_ > chunksThatFit) {
        std::cout << "This GPU's buffers hold at most " << chunksThatFit
                  << " chunks; using that as the chunk limit." << std::endl;
        chunkLimit_ = chunksThatFit;
    }
    maxBlockSlots_ =
        static_cast<uint32_t>(std::min<uint64_t>(range / BLOCK_SLOT_BYTES, MAX_CHUNK_SLOTS));

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
    // Free lists are stacks; push high slots first so low slots are used first.
    for (uint32_t slot = pool_.capacity; slot-- > 0;) {
        freeSlots_.push_back(slot);
    }
    activeSlots_.clear();
    freeBlockSlots_.clear();
    for (uint32_t blockSlot = blockCapacity_; blockSlot-- > 0;) {
        freeBlockSlots_.push_back(blockSlot);
    }
    std::fill(blockSlotOf_.begin(), blockSlotOf_.end(), NO_CHUNK);
    std::fill(blockCount_.begin(), blockCount_.end(), 0u);
    limitReached_ = false;
}

// --------------------------------------------------------------- the pools

// Cells, tables and stats for `capacity` chunks. Cell storage is device-local
// and CPU-visible (resizable BAR) when available: the GPU does the heavy reads,
// the CPU only touches a few rows to edit and to collide with blocks.
bool ChunkWorld::allocatePool(ChunkPool& pool, uint32_t capacity) {
    const VkDeviceSize slots = capacity;
    const auto preferred = {memory::DEVICE_HOST, memory::HOST};
    pool.capacity = capacity;
    bool ok = true;
    for (GpuBuffer& cells : pool.cells) {
        ok =
            ok && allocator_->tryCreate(cells, slots * CHUNK_BYTES, usage::STORAGE_COPY, preferred);
    }
    ok = ok && allocator_->tryCreate(pool.neighbors, slots * NEIGHBORHOOD_SIZE * sizeof(uint32_t),
                                     usage::STORAGE_COPY, preferred);
    ok = ok && allocator_->tryCreate(pool.activeList, slots * sizeof(uint32_t), usage::STORAGE_COPY,
                                     preferred);
    ok = ok && allocator_->tryCreate(pool.origins, slots * sizeof(glm::ivec4), usage::STORAGE_COPY,
                                     preferred);
    ok = ok && allocator_->tryCreate(pool.blockSlots, slots * sizeof(uint32_t), usage::STORAGE_COPY,
                                     preferred);
    ok = ok && allocator_->tryCreate(pool.stats, slots * sizeof(ChunkStats), usage::STORAGE_COPY,
                                     {memory::DEVICE, memory::HOST});
    ok = ok &&
         allocator_->tryCreate(pool.statsReadback, slots * sizeof(ChunkStats),
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
    for (int i = 0; i < 2; ++i) {
        recordCopy(cmd, pool_.cells[i], bigger.cells[i]);
    }
    recordCopy(cmd, pool_.neighbors, bigger.neighbors);
    recordCopy(cmd, pool_.activeList, bigger.activeList);
    recordCopy(cmd, pool_.origins, bigger.origins);
    recordCopy(cmd, pool_.blockSlots, bigger.blockSlots);
    // maintain() may grow the pool while it is still reading the stats.
    recordCopy(cmd, pool_.statsReadback, bigger.statsReadback);
    copiesVisibleToHost(cmd);
    commands_->submitAndWait("growing the world");

    releasePool(pool_);
    pool_ = bigger;
    resizeTables(capacity);
    for (uint32_t slot = capacity; slot-- > oldCapacity;) {
        freeSlots_.push_back(slot);
    }
    if (onBuffersReplaced) onBuffersReplaced();
    return true;
}

bool ChunkWorld::growBlockPool() {
    const uint32_t capacity = std::min(blockCapacity_ * 2, maxBlockSlots_);
    if (capacity <= blockCapacity_) return false;
    vkDeviceWaitIdle(gpu_->device);
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
    for (uint32_t blockSlot = capacity; blockSlot-- > blockCapacity_;) {
        freeBlockSlots_.push_back(blockSlot);
    }
    blockCapacity_ = capacity;
    if (onBuffersReplaced) onBuffersReplaced();
    return true;
}

// ------------------------------------------------------------------ chunks

void ChunkWorld::setNeighbor(uint32_t slot, int k, uint32_t neighbor) {
    size_t i = static_cast<size_t>(slot) * NEIGHBORHOOD_SIZE + k;
    neighborSlots_[i] = neighbor;
    pool_.neighbors.as<uint32_t>()[i] = neighbor;
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
    for (uint32_t buffer = 0; buffer < 2; ++buffer) {
        std::memset(cellRows(buffer) + static_cast<size_t>(slot) * CHUNK_ROWS, 0, CHUNK_BYTES);
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

    activeIndex_[slot] = static_cast<uint32_t>(activeSlots_.size());
    pool_.activeList.as<uint32_t>()[activeSlots_.size()] = slot;
    activeSlots_.push_back(slot);
    return slot;
}

void ChunkWorld::freeChunk(uint32_t slot) {
    // Unlink it from its neighbors.
    for (int k = 0; k < NEIGHBORHOOD_SIZE; ++k) {
        uint32_t neighbor = neighborSlots_[static_cast<size_t>(slot) * NEIGHBORHOOD_SIZE + k];
        if (k != SELF_NEIGHBOR && neighbor != NO_CHUNK) {
            setNeighbor(neighbor, oppositeNeighbor(k), NO_CHUNK);
        }
    }
    const glm::ivec3& chunk = slotChunk_[slot];
    chunkMap_.erase(chunk.x, chunk.y, chunk.z);
    if (blockSlotOf_[slot] != NO_CHUNK) releaseBlockSlot(slot);

    // Remove it from the active list by moving the last entry into its place.
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
    std::memset(blockRows() + static_cast<size_t>(blockSlot) * BLOCK_ROWS, 0, BLOCK_SLOT_BYTES);
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
    // Slots freed by the previous maintenance are safe to reuse now.
    freeSlots_.insert(freeSlots_.end(), quarantine_.begin(), quarantine_.end());
    quarantine_.clear();
    ++maintenanceCount_;
    uint64_t population = 0;

    // Mark every chunk that life can reach, creating the missing ones. Iterate a
    // copy: ensureChunk() appends to the active list.
    const std::vector<uint32_t> processed = activeSlots_;
    for (uint32_t slot : processed) {
        ChunkStats stats = readbackStats(slot);
        population += stats.population;
        // Visit each set bit k, lowest first (`mask &= mask - 1` clears it).
        for (uint32_t reachMask = stats.reachMask; reachMask != 0; reachMask &= reachMask - 1) {
            int k = std::countr_zero(reachMask);
            uint32_t neighbor = neighborSlots_[static_cast<size_t>(slot) * NEIGHBORHOOD_SIZE + k];
            if (neighbor == NO_CHUNK) neighbor = ensureChunk(slotChunk_[slot] + neighborOffset(k));
            if (neighbor != NO_CHUNK) lastWanted_[neighbor] = maintenanceCount_;
        }
    }

    // Free chunks with no life, no blocks, and no life nearby.
    for (uint32_t slot : processed) {
        bool empty = readbackStats(slot).population == 0 && blockCount_[slot] == 0;
        if (empty && lastWanted_[slot] != maintenanceCount_) freeChunk(slot);
    }
    return population;
}

void ChunkWorld::sortByDistance(const glm::vec3& eye) {
    const glm::vec3 center =
        eye - static_cast<float>(CHUNK_SIZE) * 0.5f; // compare against chunk corners
    std::vector<std::pair<float, uint32_t>> order;
    order.reserve(activeSlots_.size());
    for (uint32_t slot : activeSlots_) {
        glm::vec3 offset = glm::vec3(slotChunk_[slot] * CHUNK_SIZE) - center;
        order.emplace_back(glm::dot(offset, offset), slot);
    }
    std::sort(order.begin(), order.end());
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
    const uint32_t lifeRow = cellRows(currentBuffer_)[static_cast<size_t>(slot) * CHUNK_ROWS + row];
    if (lifeRow & bit) return CellKind::Life;

    uint32_t blockSlot = blockSlotOf_[slot];
    if (blockSlot == NO_CHUNK) return std::nullopt;
    const uint32_t* blocks = blockRows() + static_cast<size_t>(blockSlot) * BLOCK_ROWS;
    if (!(blocks[row] & bit)) return std::nullopt;
    bool emits = blocks[CHUNK_ROWS + row] & bit;
    return emits ? CellKind::Ember : CellKind::Stone;
}

void ChunkWorld::setLife(const glm::ivec3& cell, bool alive) {
    const glm::ivec3 chunk = chunkOf(cell);
    uint32_t slot = alive ? ensureChunk(chunk) : findChunk(chunk);
    if (slot == NO_CHUNK) return;
    const uint32_t row = rowOf(cell);
    const uint32_t bit = bitOf(cell);
    uint32_t blockSlot = blockSlotOf_[slot];
    bool blocked = blockSlot != NO_CHUNK &&
                   (blockRows()[static_cast<size_t>(blockSlot) * BLOCK_ROWS + row] & bit);
    if (alive && blocked) return;
    uint32_t& word = cellRows(currentBuffer_)[static_cast<size_t>(slot) * CHUNK_ROWS + row];
    word = alive ? (word | bit) : (word & ~bit);
}

bool ChunkWorld::placeBlock(const glm::ivec3& cell, CellKind kind) {
    uint32_t slot = ensureChunk(chunkOf(cell));
    if (slot == NO_CHUNK) return false;
    uint32_t blockSlot = ensureBlockSlot(slot);
    if (blockSlot == NO_CHUNK) return false;
    uint32_t* blocks = blockRows() + static_cast<size_t>(blockSlot) * BLOCK_ROWS;
    const uint32_t row = rowOf(cell);
    const uint32_t bit = bitOf(cell);
    blocks[row] |= bit;
    if (cellType(kind).countsAsNeighbor) blocks[CHUNK_ROWS + row] |= bit;
    ++blockCount_[slot];
    return true;
}

void ChunkWorld::removeBlock(const glm::ivec3& cell) {
    uint32_t slot = findChunk(chunkOf(cell));
    uint32_t* blocks = blockRows() + static_cast<size_t>(blockSlotOf_[slot]) * BLOCK_ROWS;
    const uint32_t row = rowOf(cell);
    const uint32_t bit = bitOf(cell);
    blocks[row] &= ~bit;
    blocks[CHUNK_ROWS + row] &= ~bit;
    if (--blockCount_[slot] == 0) releaseBlockSlot(slot);
}

void ChunkWorld::copyCurrentToPrevious() {
    if (activeSlots_.empty()) return; // new chunks start empty in both buffers
    std::vector<VkBufferCopy> regions;
    regions.reserve(activeSlots_.size());
    for (uint32_t slot : activeSlots_) {
        VkDeviceSize offset = static_cast<VkDeviceSize>(slot) * CHUNK_BYTES;
        regions.push_back({offset, offset, CHUNK_BYTES});
    }
    VkCommandBuffer cmd = commands_->begin();
    vkCmdCopyBuffer(cmd, pool_.cells[currentBuffer_].buffer, pool_.cells[1 - currentBuffer_].buffer,
                    static_cast<uint32_t>(regions.size()), regions.data());
    copiesVisibleToHost(cmd);
    commands_->submitAndWait("saving the world before an edit");
}

} // namespace gol3d
