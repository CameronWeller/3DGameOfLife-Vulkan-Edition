#pragma once

// CPU twin of the GPU simulation: the world as sparse 32^3 chunks of bit rows,
// stepped with the same bit-sliced arithmetic as shaders/life3d_step.comp. Both
// include shaders/life3d_bits.glsl, which is written in the common subset of
// GLSL and C++. The tests use this twin to check that arithmetic against the
// plain reference in LifeRules.h without a GPU.
//
// It favors clarity over speed: chunks live in a hash map and every step visits
// every chunk. The game itself never runs it.

#include <array>
#include <bit>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "life/CellTypes.h"
#include "life/LifeRules.h"
#include "world/ChunkLayout.h"

namespace gol3d::bitlife {

// life3d_bits.glsl uses GLSL's `uint`; give it the same meaning in C++.
using uint = uint32_t;
#include "life3d_bits.glsl"

// The same chunk layout as the GPU (world/ChunkLayout.h).
constexpr int CHUNK = CHUNK_SIZE;                  // cells along each edge of a chunk
constexpr int ROWS = static_cast<int>(CHUNK_ROWS); // one 32-bit row (32 cells along x) per (y, z)
constexpr int LOCAL_MASK = CHUNK - 1;              // a coordinate within its chunk: c & LOCAL_MASK
constexpr int LAST_BIT = CHUNK - 1;                // the bit of the last cell of a row

// The row of chunk-local (y, z), as in ChunkLayout.h's rowOf.
inline int rowIndex(int y, int z) {
    return z * CHUNK + y;
}

// A chunk coordinate.
struct ChunkKey {
    int x;
    int y;
    int z;
    bool operator==(const ChunkKey&) const = default;
};

struct ChunkKeyHash {
    size_t operator()(const ChunkKey& key) const {
        // The classic spatial hash of Teschner et al. (three large primes).
        return (static_cast<size_t>(static_cast<uint32_t>(key.x)) * 73856093u) ^
               (static_cast<size_t>(static_cast<uint32_t>(key.y)) * 19349663u) ^
               (static_cast<size_t>(static_cast<uint32_t>(key.z)) * 83492791u);
    }
};

// One chunk as three bit planes (see life/CellTypes.h). The GPU keeps the
// same planes, with the two block planes in a separate block pool.
struct BitChunk {
    std::array<uint32_t, ROWS> life{};
    std::array<uint32_t, ROWS> blocked{}; // any static block
    std::array<uint32_t, ROWS> emits{};   // Ember blocks

    // No life and no blocks (emits implies blocked, so it need not be checked).
    bool empty() const {
        for (int i = 0; i < ROWS; ++i) {
            if (life[i] | blocked[i]) return false;
        }
        return true;
    }
};

// A sparse world of BitChunks, stepped one generation at a time.
class BitWorld {
public:
    // Sets or clears a live cell; a cell holding a block stays empty.
    void setLife(int x, int y, int z, bool alive) {
        BitChunk& chunk = chunks_[keyOf(x, y, z)];
        int row = localRow(y, z);
        uint32_t bit = localBit(x);
        bool blocked = chunk.blocked[row] & bit;
        if (alive && !blocked) {
            chunk.life[row] |= bit;
        } else {
            chunk.life[row] &= ~bit;
        }
    }

    // Puts a block of `kind` into the cell, replacing whatever was there.
    // CellKind::Life removes the block and leaves the cell empty.
    void setBlock(int x, int y, int z, CellKind kind) {
        BitChunk& chunk = chunks_[keyOf(x, y, z)];
        int row = localRow(y, z);
        uint32_t bit = localBit(x);
        bool isBlock = kind != CellKind::Life;
        bool emits = isBlock && cellType(kind).countsAsNeighbor;
        chunk.life[row] &= ~bit;
        chunk.blocked[row] = isBlock ? (chunk.blocked[row] | bit) : (chunk.blocked[row] & ~bit);
        chunk.emits[row] = emits ? (chunk.emits[row] | bit) : (chunk.emits[row] & ~bit);
    }

    bool alive(int x, int y, int z) const {
        auto it = chunks_.find(keyOf(x, y, z));
        if (it == chunks_.end()) return false;
        return (it->second.life[localRow(y, z)] & localBit(x)) != 0;
    }

    uint64_t population() const {
        uint64_t count = 0;
        for (const auto& [key, chunk] : chunks_) {
            for (uint32_t row : chunk.life) {
                count += static_cast<uint64_t>(std::popcount(row));
            }
        }
        return count;
    }

    size_t chunkCount() const { return chunks_.size(); }

    // Advances every chunk one generation. Chunks left empty are dropped.
    void step(const LifeRule& rule) {
        addNeighborChunks();
        std::unordered_map<ChunkKey, BitChunk, ChunkKeyHash> next;
        for (const auto& [key, chunk] : chunks_) {
            BitChunk out = stepChunk(key, chunk, rule);
            if (!out.empty()) next.emplace(key, out);
        }
        chunks_.swap(next);
    }

private:
    static ChunkKey keyOf(int x, int y, int z) {
        return {x >> CHUNK_SHIFT, y >> CHUNK_SHIFT, z >> CHUNK_SHIFT};
    }
    static int localRow(int y, int z) { return rowIndex(y & LOCAL_MASK, z & LOCAL_MASK); }
    static uint32_t localBit(int x) { return 1u << (x & LOCAL_MASK); }

    // Neighbor k of a chunk, numbered as in ChunkLayout.h.
    static ChunkKey neighborKey(const ChunkKey& key, int k) {
        const glm::ivec3 offset = neighborOffset(k);
        return {key.x + offset.x, key.y + offset.y, key.z + offset.z};
    }

    // In one generation life spreads at most one cell, so at most into the
    // neighbors of chunks that hold something. Create those (empty) first, so
    // the step computes them too. (The game's ChunkWorld instead creates only
    // the neighbors live cells are close enough to reach.)
    void addNeighborChunks() {
        std::vector<ChunkKey> reachable;
        for (const auto& [key, chunk] : chunks_) {
            if (chunk.empty()) continue;
            for (int k = 0; k < NEIGHBORHOOD_SIZE; ++k) {
                reachable.push_back(neighborKey(key, k));
            }
        }
        for (const ChunkKey& key : reachable) {
            chunks_.try_emplace(key);
        }
    }

    // The next generation of one chunk; its blocks carry over unchanged.
    BitChunk stepChunk(const ChunkKey& key, const BitChunk& chunk, const LifeRule& rule) const {
        BitChunk out;
        out.blocked = chunk.blocked;
        out.emits = chunk.emits;
        const FrontBackMasks masks = frontBackMasks(rule);
        for (int z = 0; z < CHUNK; ++z) {
            for (int y = 0; y < CHUNK; ++y) {
                uint neighborRows[NEIGHBORHOOD_SIZE];
                gatherNeighborRows(key, y, z, neighborRows);
                int row = rowIndex(y, z);
                out.life[row] =
                    nextRow(neighborRows, chunk.life[row], chunk.blocked[row], rule.surviveMask,
                            rule.birthMask, masks.sides, masks.middle);
            }
        }
        return out;
    }

    // The 27 rows nextRow() needs for row (y, z) of a chunk: for each (dy, dz) in
    // [-1, 1]^2, the row itself and the same row shifted so each bit holds its
    // x - 1 and x + 1 neighbor. Row (dx, dy, dz) goes to index
    // (dz + 1) * 9 + (dy + 1) * 3 + (dx + 1), the order life3d_bits.glsl expects.
    void gatherNeighborRows(const ChunkKey& key, int y, int z, uint rows[NEIGHBORHOOD_SIZE]) const {
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dy = -1; dy <= 1; ++dy) {
                uint west = 0;
                uint center = 0;
                uint east = 0;
                shiftedRows(key, y + dy, z + dz, west, center, east);
                int westIndex = (dz + 1) * 9 + (dy + 1) * 3; // the dx = -1 entry
                rows[westIndex] = west;
                rows[westIndex + 1] = center;
                rows[westIndex + 2] = east;
            }
        }
    }

    // The live-neighbor row (life and embers) at local (y, z), which may lie one
    // past either side of the chunk, plus its two shifts: bit i of `west` holds
    // cell i - 1 and bit i of `east` holds cell i + 1, taking the edge bit from
    // the chunk next door along x.
    void shiftedRows(const ChunkKey& key, int y, int z, uint& west, uint& center,
                     uint& east) const {
        int chunkDy = y < 0 ? -1 : (y >= CHUNK ? 1 : 0);
        int chunkDz = z < 0 ? -1 : (z >= CHUNK ? 1 : 0);
        int row = rowIndex(y - chunkDy * CHUNK, z - chunkDz * CHUNK);
        auto liveRow = [&](int chunkDx) -> uint {
            auto it = chunks_.find({key.x + chunkDx, key.y + chunkDy, key.z + chunkDz});
            if (it == chunks_.end()) return 0u;
            return it->second.life[row] | it->second.emits[row];
        };
        center = liveRow(0);
        west = (center << 1) | (liveRow(-1) >> LAST_BIT); // the last cell of the chunk at -x
        east = (center >> 1) | (liveRow(1) << LAST_BIT);  // the first cell of the chunk at +x
    }

    std::unordered_map<ChunkKey, BitChunk, ChunkKeyHash> chunks_;
};

} // namespace gol3d::bitlife
