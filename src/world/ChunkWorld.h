#pragma once

// The unbounded world, stored as a pool of 32^3-cell chunks that exist only
// where life is, or could be within a few generations.
//
// GPU side (see shaders/life3d_storage.glsl): every chunk has a *slot* in a pool
// of fixed-size arrays. The cell bits of slot s live at rows s * 1024 ...
// s * 1024 + 1023 of two buffers that ping-pong between generations. Next to
// them sit per-slot tables the compute passes read: the slots of the 27
// neighboring chunks, the world position of the chunk, and which block-pool
// slot (if any) holds its static blocks.
//
// CPU side: a hash map from chunk coordinate to slot, a CPU copy of the neighbor
// table, and free lists. The cell and table buffers are mapped, so the CPU edits
// cells and tables by writing straight into GPU-visible memory.
//
// Life of a chunk:
//   1. ensureChunk() takes a free slot, zeroes its cells, and links it to its
//      neighbors in both directions.
//   2. Each build pass reports, per chunk, its population and which neighbor
//      chunks its live cells could reach within MAX_BATCH generations
//      (maintain() reads this back).
//   3. maintain() creates the reachable neighbors and frees chunks that hold no
//      life or blocks and that nothing can reach.
//   4. A freed slot sits in quarantine until the next maintenance. The block
//      list drawn until then may still show its dying cells shrinking, and the
//      vertex shader places each block by looking up its slot's origin, so a
//      reused slot would make those blocks jump to the new chunk.
//
// When the pool is full it doubles (up to the chunk limit), copying every chunk
// into the same slot of the bigger buffers.

#include <array>
#include <bit>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "gpu/GpuBuffer.h"
#include "life/CellTypes.h"
#include "world/ChunkLayout.h"
#include "world/ChunkMap.h"

namespace gol3d {

class GpuContext;
class ImmediateCommands;

// What the build pass reports per chunk. Must match `stats` (a uvec2: x
// population, y reach mask) in shaders/life3d_build.comp.
struct ChunkStats {
    uint32_t population; // live cells
    uint32_t reachMask;  // bit k: live cells could reach neighbor k within MAX_BATCH generations
};
static_assert(sizeof(ChunkStats) == 8, "one uvec2 per chunk on the GPU");

// The GPU buffers sized by the chunk pool, regrown together. All are indexed
// by chunk slot; SimulationPasses binds them to the compute shaders.
struct ChunkPool {
    uint32_t capacity = 0;          // slots
    std::array<GpuBuffer, 2> cells; // CHUNK_ROWS words per slot; two generations, ping-ponging
    GpuBuffer neighbors;            // NEIGHBORHOOD_SIZE slots per slot (NO_CHUNK where missing)
    GpuBuffer activeList;           // slots of the chunks in use, in draw order
    GpuBuffer origins;              // ivec4 per slot: world cell of the chunk's local (0, 0, 0)
    GpuBuffer blockSlots;           // block-pool slot per slot, or NO_CHUNK
    GpuBuffer stats;                // ChunkStats per slot (written by the GPU)
    GpuBuffer statsReadback;        // stats, copied back for the CPU after each build
};

// Owns the chunk pool and its CPU bookkeeping. See the top of this file for how
// chunks are created and freed; every slot index below is a chunk-pool slot
// unless it is named blockSlot.
class ChunkWorld {
public:
    // Called after growth replaced pool buffers, so descriptor sets that point at
    // them can be rewritten.
    std::function<void()> onBuffersReplaced;
    // Called with a message for the player, such as running out of GPU memory.
    std::function<void(const std::string&)> onWarning;

    // `chunkLimit` may be lowered to what the GPU's buffers can hold.
    void init(const GpuContext& gpu, BufferAllocator& allocator, ImmediateCommands& commands,
              uint32_t chunkLimit);
    void destroy();

    // Removes every chunk. Waits for the GPU, since frames in flight read the
    // chunk origins this rewrites.
    void reset();

    // ---- Reading cells

    // What occupies a cell, or nothing for an empty cell.
    std::optional<CellKind> cellKind(const glm::ivec3& cell) const;
    bool isOccupied(const glm::ivec3& cell) const { return cellKind(cell).has_value(); }
    bool isAlive(const glm::ivec3& cell) const { return cellKind(cell) == CellKind::Life; }

    // Visits every occupied cell as fn(cell, kind).
    template <typename Fn>
    void forEachCell(Fn&& fn) const;

    // Visits every chunk in use as fn(chunk coordinate).
    template <typename Fn>
    void forEachChunk(Fn&& fn) const {
        for (uint32_t slot : activeSlots_) {
            fn(slotChunk_[slot]);
        }
    }

    // ---- Editing cells. Edits land in the current generation and reach the
    // GPU's bookkeeping with the next build pass.

    // Sets or clears a live cell. A cell holding a block never comes alive.
    void setLife(const glm::ivec3& cell, bool alive);
    // Puts a Stone or Ember block into the cell. False when the world is full.
    bool placeBlock(const glm::ivec3& cell, CellKind kind);
    // Removes the block in a cell that holds one.
    void removeBlock(const glm::ivec3& cell);
    // Copies the current generation over the previous one, so the next block
    // list sees edits as births and deaths and can animate them.
    void copyCurrentToPrevious();

    // ---- Bookkeeping between passes

    // After a build pass: adds the chunks life can reach, frees the empty ones
    // nothing can reach, and returns the population.
    uint64_t maintain();
    // Orders the active list nearest to `eye` first. The build pass appends
    // blocks roughly in that order, so the depth test rejects most hidden
    // fragments early and, past the draw cap, the blocks left out are the far ones.
    void sortByDistance(const glm::vec3& eye);

    // Which cell buffer holds the current generation (0 or 1).
    uint32_t currentBuffer() const { return currentBuffer_; }
    void setCurrentBuffer(uint32_t index) {
        if (index != currentBuffer_) ++version_;
        currentBuffer_ = index;
    }

    // Changes whenever the cells, the set of chunks or the order of the active
    // list change. A generation stepped in slices over several frames is only
    // valid while this stays the same.
    uint64_t version() const { return version_; }

    // ---- Sizes and buffers

    uint32_t activeChunkCount() const { return static_cast<uint32_t>(activeSlots_.size()); }
    uint32_t capacity() const { return pool_.capacity; }
    uint32_t chunkLimit() const { return chunkLimit_; }
    // True once a chunk could not be created since the last reset: growth stops
    // at the edge of what exists.
    bool limitReached() const { return limitReached_; }
    const ChunkPool& pool() const { return pool_; }
    const GpuBuffer& blockPool() const { return blockPool_; }

private:
    // Slot of the chunk, or NO_CHUNK.
    uint32_t findChunk(const glm::ivec3& chunk) const {
        return chunkMap_.find(chunk.x, chunk.y, chunk.z);
    }
    // Slot of the chunk, creating it if needed; NO_CHUNK at the chunk limit.
    uint32_t ensureChunk(const glm::ivec3& chunk);
    void freeChunk(uint32_t slot);
    // Block-pool slot of a chunk, creating it if needed; NO_CHUNK when full.
    uint32_t ensureBlockSlot(uint32_t slot);
    void releaseBlockSlot(uint32_t slot);

    bool allocatePool(ChunkPool& pool, uint32_t capacity);
    void releasePool(ChunkPool& pool);
    void resizeTables(uint32_t capacity);
    bool growPool();
    void prefetchNextPool();
    ChunkPool takePrefetchedPool();
    bool growBlockPool();

    // Neighbor k of a chunk, from the CPU copy of the neighbor table.
    uint32_t neighborSlot(uint32_t slot, int k) const {
        return neighborSlots_[static_cast<size_t>(slot) * NEIGHBORHOOD_SIZE + k];
    }
    // Writes a neighbor-table entry to the CPU copy and through to the GPU.
    void setNeighbor(uint32_t slot, int k, uint32_t neighbor);
    // Whether an existing neighbor's live cells (or Embers) can reach this chunk
    // within MAX_BATCH generations, by the stats of the last build pass.
    bool reachedByNeighbor(uint32_t slot) const;

    // The CHUNK_ROWS rows of one chunk in cell buffer `buffer` (0 or 1), and the
    // two block planes of a block-pool slot, all in mapped GPU memory. The
    // pointers are re-derived on every call because growing a pool replaces its
    // buffers.
    uint32_t* chunkRows(uint32_t buffer, uint32_t slot) const {
        return pool_.cells[buffer].as<uint32_t>() + static_cast<size_t>(slot) * CHUNK_ROWS;
    }
    uint32_t* blockedRows(uint32_t blockSlot) const {
        return blockPool_.as<uint32_t>() + static_cast<size_t>(blockSlot) * BLOCK_ROWS;
    }
    uint32_t* emitsRows(uint32_t blockSlot) const {
        return blockedRows(blockSlot) + EMITS_PLANE_OFFSET;
    }
    // Re-read on every call: growing the pool replaces the buffer.
    ChunkStats readbackStats(uint32_t slot) const {
        return pool_.statsReadback.as<ChunkStats>()[slot];
    }

    const GpuContext* gpu_ = nullptr;
    BufferAllocator* allocator_ = nullptr;
    ImmediateCommands* commands_ = nullptr;

    ChunkPool pool_;
    std::future<ChunkPool> nextPool_; // being allocated ahead of growPool(); see prefetchNextPool()
    uint32_t chunkLimit_ = DEFAULT_CHUNK_LIMIT;
    uint32_t currentBuffer_ = 0;
    bool limitReached_ = false;
    uint64_t version_ = 0;

    // Static blocks: BLOCK_ROWS words per block slot, only for chunks with blocks.
    GpuBuffer blockPool_;
    uint32_t blockCapacity_ = 0;
    uint32_t maxBlockSlots_ = 0; // the largest block pool one storage buffer can hold
    // Free lists are stacks: the slot at back() is taken next.
    std::vector<uint32_t> freeBlockSlots_;

    // CPU tables, indexed by slot.
    ChunkMap chunkMap_;
    std::vector<glm::ivec3> slotChunk_; // chunk coordinate of each slot
    // CPU copy of pool_.neighbors, NEIGHBORHOOD_SIZE entries per slot. Reading
    // device memory through the mapping is slow, so the CPU reads this instead.
    std::vector<uint32_t> neighborSlots_;
    std::vector<uint32_t> activeIndex_; // position of each slot in activeSlots_
    std::vector<uint32_t> blockSlotOf_; // block-pool slot of each slot, or NO_CHUNK
    std::vector<uint32_t> blockCount_;  // static blocks in each chunk
    // Bit k set: the chunk has no neighbor k yet. Only chunks at the edge of the
    // world have any, so maintain() skips the rest with one AND.
    std::vector<uint32_t> missingNeighbors_;
    // The value of maintenanceCount_ when the chunk was created. Chunks created
    // during a maintenance have no stats from the build before it.
    std::vector<uint32_t> createdAt_;
    uint32_t maintenanceCount_ = 0;
    std::vector<uint32_t> freeSlots_;
    std::vector<uint32_t> quarantine_;  // freed slots the last block list may still draw
    std::vector<uint32_t> activeSlots_; // CPU copy of pool_.activeList
};

// Order: chunks in active-list order, rows in index order, and within a row its
// live cells then its blocks, lowest x first. Saves are written in this order.
template <typename Fn>
void ChunkWorld::forEachCell(Fn&& fn) const {
    for (uint32_t slot : activeSlots_) {
        const glm::ivec3 origin = slotChunk_[slot] * CHUNK_SIZE;
        const uint32_t* lifeRows = chunkRows(currentBuffer_, slot);
        const uint32_t blockSlot = blockSlotOf_[slot];
        for (uint32_t row = 0; row < CHUNK_ROWS; ++row) {
            const int y = static_cast<int>(row % CHUNK_SIZE);
            const int z = static_cast<int>(row / CHUNK_SIZE);
            const glm::ivec3 rowStart = origin + glm::ivec3(0, y, z);
            // Visit set bits lowest first: countr_zero finds one, `bits &= bits - 1` clears it.
            for (uint32_t bits = lifeRows[row]; bits != 0; bits &= bits - 1) {
                fn(rowStart + glm::ivec3(std::countr_zero(bits), 0, 0), CellKind::Life);
            }
            if (blockSlot == NO_CHUNK) continue;
            const uint32_t emits = emitsRows(blockSlot)[row];
            for (uint32_t bits = blockedRows(blockSlot)[row]; bits != 0; bits &= bits - 1) {
                int x = std::countr_zero(bits);
                CellKind kind = ((emits >> x) & 1u) ? CellKind::Ember : CellKind::Stone;
                fn(rowStart + glm::ivec3(x, 0, 0), kind);
            }
        }
    }
}

} // namespace gol3d
