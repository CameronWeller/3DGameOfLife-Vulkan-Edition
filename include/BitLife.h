#pragma once

// CPU twin of the GPU simulation: the world as sparse 32^3 chunks of bit rows,
// stepped with the same bit-sliced arithmetic as shaders/life3d_step.comp (both
// include shaders/life3d_bits.glsl). The tests use it to check that arithmetic
// against the plain reference in Life3DRules.h without a GPU.

#include <array>
#include <bit>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "CellTypes.h"
#include "Life3DRules.h"

namespace VulkanHIP::bitlife {

using uint = uint32_t;
#include "life3d_bits.glsl"

constexpr int CHUNK = 32;          // cells along each edge of a chunk
constexpr int ROWS = CHUNK * CHUNK; // one 32-bit row per (y, z)

inline int rowIndex(int y, int z) { return z * CHUNK + y; }

struct ChunkKey {
    int x, y, z;
    bool operator==(const ChunkKey&) const = default;
};

struct ChunkKeyHash {
    size_t operator()(const ChunkKey& k) const {
        return (size_t(uint32_t(k.x)) * 73856093u) ^ (size_t(uint32_t(k.y)) * 19349663u) ^
               (size_t(uint32_t(k.z)) * 83492791u);
    }
};

struct BitChunk {
    std::array<uint32_t, ROWS> life{};
    std::array<uint32_t, ROWS> blocked{}; // any static block
    std::array<uint32_t, ROWS> emits{};   // Ember blocks
    bool empty() const {
        for (int i = 0; i < ROWS; ++i)
            if (life[i] | blocked[i]) return false;
        return true;
    }
};

class BitWorld {
public:
    void setLife(int x, int y, int z, bool alive) {
        BitChunk& c = chunks[keyOf(x, y, z)];
        uint32_t& row = c.life[rowIndex(y & 31, z & 31)];
        uint32_t bit = 1u << (x & 31);
        if (alive && !(c.blocked[rowIndex(y & 31, z & 31)] & bit)) row |= bit; else row &= ~bit;
    }

    void setBlock(int x, int y, int z, CellKind kind) {
        BitChunk& c = chunks[keyOf(x, y, z)];
        int r = rowIndex(y & 31, z & 31);
        uint32_t bit = 1u << (x & 31);
        c.life[r] &= ~bit;
        c.blocked[r] = kind == CellKind::Life ? c.blocked[r] & ~bit : c.blocked[r] | bit;
        c.emits[r] = cellType(kind).countsAsNeighbor && kind != CellKind::Life ? c.emits[r] | bit : c.emits[r] & ~bit;
    }

    bool alive(int x, int y, int z) const {
        auto it = chunks.find(keyOf(x, y, z));
        return it != chunks.end() && (it->second.life[rowIndex(y & 31, z & 31)] >> (x & 31) & 1u);
    }

    uint64_t population() const {
        uint64_t n = 0;
        for (const auto& [key, c] : chunks)
            for (uint32_t row : c.life) n += static_cast<uint64_t>(std::popcount(row));
        return n;
    }

    size_t chunkCount() const { return chunks.size(); }

    void step(const LifeRule& rule) {
        // Life can reach any neighbor chunk of a chunk that holds life or embers.
        std::vector<ChunkKey> grow;
        for (const auto& [key, c] : chunks) {
            if (c.empty()) continue;
            for (int k = 0; k < 27; ++k) grow.push_back(offset(key, k));
        }
        for (const ChunkKey& key : grow) chunks.try_emplace(key);

        std::unordered_map<ChunkKey, BitChunk, ChunkKeyHash> next;
        for (const auto& [key, c] : chunks) {
            BitChunk out;
            out.blocked = c.blocked;
            out.emits = c.emits;
            for (int z = 0; z < CHUNK; ++z) {
                for (int y = 0; y < CHUNK; ++y) {
                    uint nb[27];
                    for (int dz = -1; dz <= 1; ++dz)
                        for (int dy = -1; dy <= 1; ++dy) {
                            uint west = 0, row = 0, east = 0;
                            rowTriple(key, y + dy, z + dz, west, row, east);
                            int base = (dz + 1) * 9 + (dy + 1) * 3;
                            nb[base + 0] = west;
                            nb[base + 1] = row;
                            nb[base + 2] = east;
                        }
                    int r = rowIndex(y, z);
                    out.life[r] = nextRow(nb, c.life[r], c.blocked[r], rule.surviveMask, rule.birthMask);
                }
            }
            if (!out.empty()) next.emplace(key, out);
        }
        chunks.swap(next);
    }

private:
    static ChunkKey keyOf(int x, int y, int z) { return {x >> 5, y >> 5, z >> 5}; }
    static ChunkKey offset(const ChunkKey& key, int k) {
        return {key.x + k % 3 - 1, key.y + (k / 3) % 3 - 1, key.z + k / 9 - 1};
    }

    // Live-neighbor row at local (y, z), which may lie one past the chunk, with
    // its x neighbors: west/east hold each cell's x-1 / x+1 neighbor.
    void rowTriple(const ChunkKey& key, int y, int z, uint& west, uint& row, uint& east) const {
        int cy = y < 0 ? -1 : y >= CHUNK ? 1 : 0;
        int cz = z < 0 ? -1 : z >= CHUNK ? 1 : 0;
        int r = rowIndex(y - cy * CHUNK, z - cz * CHUNK);
        auto live = [&](int cx) -> uint {
            auto it = chunks.find({key.x + cx, key.y + cy, key.z + cz});
            return it == chunks.end() ? 0u : it->second.life[r] | it->second.emits[r];
        };
        row = live(0);
        west = (row << 1) | (live(-1) >> 31);
        east = (row >> 1) | (live(1) << 31);
    }

    std::unordered_map<ChunkKey, BitChunk, ChunkKeyHash> chunks;
};

} // namespace VulkanHIP::bitlife
