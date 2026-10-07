#pragma once

// How the unbounded world is cut into chunks, and how a chunk's cells are packed
// into memory. Must match shaders/life3d_storage.glsl (CHUNK, ROWS, BLOCK_ROWS,
// NO_CHUNK, SELF and the neighbor numbering).
//
//   - A chunk is a 32 x 32 x 32 cube of cells. Chunk (cx, cy, cz) covers cells
//     32 * cx .. 32 * cx + 31 on each axis.
//   - A chunk stores one bit per cell: 1024 rows of 32 cells along x, one row per
//     (y, z) at index z * 32 + y. Cell x of a row is bit x.
//   - A chunk's 27 neighbors (itself included) are numbered
//     k = (dz + 1) * 9 + (dy + 1) * 3 + (dx + 1) for offsets dx, dy, dz in -1..1.
//     That is, k is written in base 3 with digits dz + 1, dy + 1, dx + 1.
//     k = 13 is the chunk itself, and 26 - k is the opposite direction of k:
//     26 is "222" in base 3, so 26 - k turns every digit d + 1 into 1 - d.

#include <cstdint>

#include <glm/glm.hpp>

namespace gol3d {

constexpr int CHUNK_SIZE = 32;        // cells along a chunk edge
constexpr int CHUNK_SHIFT = 5;        // log2(CHUNK_SIZE)
constexpr uint32_t CHUNK_ROWS = 1024; // 32-bit rows per chunk: one per (y, z)
// A block-pool slot: CHUNK_ROWS "blocked" rows, then CHUNK_ROWS "emits" rows
// (see life/CellTypes.h for what the two planes mean).
constexpr uint32_t BLOCK_ROWS = 2 * CHUNK_ROWS;
constexpr uint32_t EMITS_PLANE_OFFSET = CHUNK_ROWS; // where the "emits" rows start in a slot
constexpr uint32_t NO_CHUNK = 0xFFFFFFFFu;          // "no slot" in every chunk table

// The chunk pool: GPU memory for at most MAX_CHUNK_SLOTS chunks (the block
// instance format has 17 bits for a slot). By default a world may use
// DEFAULT_CHUNK_LIMIT chunks: 1.07 billion cells, 256 MB of cell storage (4 KB
// per chunk, times two generations).
constexpr uint32_t MAX_CHUNK_SLOTS = 1u << 17;
constexpr uint32_t MIN_CHUNK_LIMIT = 64; // the smallest --chunks accepted
constexpr uint32_t DEFAULT_CHUNK_LIMIT = 32768;

constexpr int NEIGHBORHOOD_SIZE = 27; // a chunk and its 26 neighbors
constexpr int SELF_NEIGHBOR = 13;     // neighbor index of the chunk itself

// The chunk holding a cell. An arithmetic shift is floor division, so cell -1
// lands in chunk -1 rather than chunk 0.
inline glm::ivec3 chunkOf(const glm::ivec3& cell) {
    return cell >> CHUNK_SHIFT;
}

// The row index of a cell within its chunk. `& (CHUNK_SIZE - 1)` keeps the low
// five bits, which is the coordinate modulo 32 even for negative cells.
inline uint32_t rowOf(const glm::ivec3& cell) {
    glm::ivec3 local = cell & (CHUNK_SIZE - 1);
    return static_cast<uint32_t>(local.z * CHUNK_SIZE + local.y);
}

// The bit of a cell within its row.
inline uint32_t bitOf(const glm::ivec3& cell) {
    return 1u << (cell.x & (CHUNK_SIZE - 1));
}

// The chunk offset (dx, dy, dz) of neighbor index k: its base-3 digits, minus 1.
inline glm::ivec3 neighborOffset(int k) {
    return glm::ivec3(k % 3 - 1, (k / 3) % 3 - 1, k / 9 - 1);
}

// The neighbor index that points back: if B is neighbor k of A, A is neighbor
// oppositeNeighbor(k) of B.
constexpr int oppositeNeighbor(int k) {
    return NEIGHBORHOOD_SIZE - 1 - k;
}

} // namespace gol3d
