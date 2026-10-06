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

inline int rowIndex(int y, int z) {
    return z * CHUNK + y;
}

struct ChunkKey {
    int x, y, z;
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

// One chunk as three bit planes (see life/CellTypes.h).
struct BitChunk {
    std::array<uint32_t, ROWS> life{};
    std::array<uint32_t, ROWS> blocked{}; // any static block
    std::array<uint32_t, ROWS> emits{};   // Ember blocks

    bool empty() const {
        for (int i = 0; i < ROWS; ++i) {
            if (life[i] | blocked[i]) return false;
        }
        return true;
    }
};

class BitWorld {
public:
    void setLife(int x, int y, int z, bool alive) {
        BitChunk& chunk = chunks_[keyOf(x, y, z)];
        int row = rowIndex(y & LOCAL_MASK, z & LOCAL_MASK);
        uint32_t bit = 1u << (x & LOCAL_MASK);
        bool blocked = chunk.blocked[row] & bit;
        if (alive && !blocked) {
            chunk.life[row] |= bit;
        } else {
            chunk.life[row] &= ~bit;
        }
    }

    void setBlock(int x, int y, int z, CellKind kind) {
        BitChunk& chunk = chunks_[keyOf(x, y, z)];
        int row = rowIndex(y & LOCAL_MASK, z & LOCAL_MASK);
        uint32_t bit = 1u << (x & LOCAL_MASK);
        bool isBlock = kind != CellKind::Life;
        bool emits = isBlock && cellType(kind).countsAsNeighbor;
        chunk.life[row] &= ~bit;
        chunk.blocked[row] = isBlock ? (chunk.blocked[row] | bit) : (chunk.blocked[row] & ~bit);
        chunk.emits[row] = emits ? (chunk.emits[row] | bit) : (chunk.emits[row] & ~bit);
    }

    bool alive(int x, int y, int z) const {
        auto it = chunks_.find(keyOf(x, y, z));
        if (it == chunks_.end()) return false;
        return (it->second.life[rowIndex(y & LOCAL_MASK, z & LOCAL_MASK)] >> (x & LOCAL_MASK)) & 1u;
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

    void step(const LifeRule& rule) {
        // Life can reach any neighbor chunk of a chunk that holds life or embers,
        // so make sure those exist (empty) before stepping.
        std::vector<ChunkKey> reachable;
        for (const auto& [key, chunk] : chunks_) {
            if (chunk.empty()) continue;
            for (int k = 0; k < 27; ++k) {
                reachable.push_back(neighborKey(key, k));
            }
        }
        for (const ChunkKey& key : reachable) {
            chunks_.try_emplace(key);
        }

        std::unordered_map<ChunkKey, BitChunk, ChunkKeyHash> next;
        for (const auto& [key, chunk] : chunks_) {
            BitChunk out;
            out.blocked = chunk.blocked;
            out.emits = chunk.emits;
            for (int z = 0; z < CHUNK; ++z) {
                for (int y = 0; y < CHUNK; ++y) {
                    uint neighborRows[27];
                    gatherNeighborRows(key, y, z, neighborRows);
                    int row = rowIndex(y, z);
                    out.life[row] = nextRow(neighborRows, chunk.life[row], chunk.blocked[row],
                                            rule.surviveMask, rule.birthMask);
                }
            }
            if (!out.empty()) next.emplace(key, out);
        }
        chunks_.swap(next);
    }

private:
    static ChunkKey keyOf(int x, int y, int z) {
        return {x >> CHUNK_SHIFT, y >> CHUNK_SHIFT, z >> CHUNK_SHIFT};
    }

    // Neighbor k of a chunk, with k = (dz + 1) * 9 + (dy + 1) * 3 + (dx + 1).
    static ChunkKey neighborKey(const ChunkKey& key, int k) {
        return {key.x + k % 3 - 1, key.y + (k / 3) % 3 - 1, key.z + k / 9 - 1};
    }

    // The 27 rows nextRow() needs for row (y, z) of a chunk: for each (dy, dz) in
    // [-1, 1]^2, the row itself and the same row shifted so each bit holds its
    // x - 1 and x + 1 neighbor, in the order of life3d_bits.glsl.
    void gatherNeighborRows(const ChunkKey& key, int y, int z, uint rows[27]) const {
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dy = -1; dy <= 1; ++dy) {
                uint west = 0;
                uint center = 0;
                uint east = 0;
                shiftedRows(key, y + dy, z + dz, west, center, east);
                int base = (dz + 1) * 9 + (dy + 1) * 3;
                rows[base + 0] = west;
                rows[base + 1] = center;
                rows[base + 2] = east;
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
        west = (center << 1) | (liveRow(-1) >> 31); // the last cell of the chunk at -x
        east = (center >> 1) | (liveRow(1) << 31);  // the first cell of the chunk at +x
    }

    std::unordered_map<ChunkKey, BitChunk, ChunkKeyHash> chunks_;
};

} // namespace gol3d::bitlife
