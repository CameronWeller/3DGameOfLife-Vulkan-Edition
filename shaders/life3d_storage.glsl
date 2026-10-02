// Chunk storage shared by the compute passes (life3d_step.comp, life3d_build.comp).
//
// The world is sparse 32^3-cell chunks. A chunk in pool slot s stores one bit per
// cell: 1024 rows of 32 cells along x, row (y, z) at word s * 1024 + z * 32 + y,
// cell x at bit x. Chunks that hold static blocks (include/CellTypes.h) also own
// a slot in the block pool: 1024 "blocked" rows (any block) then 1024 "emits"
// rows (Ember blocks, which count as live neighbors).

const uint CHUNK = 32u;
const uint ROWS = 1024u;
const uint NO_CHUNK = 0xFFFFFFFFu;

// 27 entries per chunk slot for offsets (dx, dy, dz) in [-1, 1]^3 at index
// (dz + 1) * 9 + (dy + 1) * 3 + (dx + 1); entry 13 is the chunk itself.
layout(std430, binding = 2) readonly buffer Neighbors { uint slots[]; } neighbors;
layout(std430, binding = 3) readonly buffer ActiveList { uint slots[]; } activeList;
layout(std430, binding = 4) readonly buffer Blocks { uint rows[]; } blocks;
layout(std430, binding = 5) readonly buffer BlockSlots { uint slots[]; } blockSlots; // per chunk slot

// The chunk's 27 neighbors and their block-pool slots, loaded once per workgroup.
shared uint nearChunk[27];
shared uint nearBlocks[27];

void loadNeighborhood(uint slot) {
    uint i = gl_LocalInvocationIndex;
    if (i < 27u) {
        uint n = neighbors.slots[slot * 27u + i];
        nearChunk[i] = n;
        nearBlocks[i] = n == NO_CHUNK ? NO_CHUNK : blockSlots.slots[n];
    }
    barrier();
}

// A row index and the neighbor entry (0..26) that holds it, for y and z that may
// lie one past either side of the chunk; dx picks the chunk along x.
uint rowAndNeighbor(int y, int z, int dx, out uint k) {
    int cy = y < 0 ? -1 : (y >= int(CHUNK) ? 1 : 0);
    int cz = z < 0 ? -1 : (z >= int(CHUNK) ? 1 : 0);
    k = uint((cz + 1) * 9 + (cy + 1) * 3 + (dx + 1));
    return uint(z - cz * int(CHUNK)) * CHUNK + uint(y - cy * int(CHUNK));
}

uint blockRow(uint k, uint row, uint plane) {
    uint b = nearBlocks[k];
    return b == NO_CHUNK ? 0u : blocks.rows[b * 2048u + plane * ROWS + row];
}
