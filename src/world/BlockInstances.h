#pragma once

// The block list: one instance per visible block, written by
// shaders/life3d_build.comp and drawn by shaders/life3d_blocks.vert with an
// indirect draw, so the CPU never touches it.
//
// Each instance is a uvec2:
//   x: bits 0-26  which of the 27 cells around the block are occupied (face
//                 culling and ambient occlusion; bit 13, the block itself, unused)
//      bits 27-28 kind (CellKind)
//      bit  29    born in the last generation (grows in)
//      bit  30    died in the last generation (shrinks away)
//   y: bits 0-14  cell within its chunk: x | y << 5 | z << 10
//      bits 15-31 chunk pool slot

#include <cstdint>

namespace gol3d {

// Most blocks drawn per frame (8 bytes each, 32 MB). Larger worlds are still
// simulated in full; the farthest blocks are simply not drawn.
constexpr uint32_t MAX_BLOCK_INSTANCES = 1u << 22;

// Indices per block: three camera-facing quads of two triangles each.
constexpr uint32_t BLOCK_INDEX_COUNT = 18;

} // namespace gol3d
