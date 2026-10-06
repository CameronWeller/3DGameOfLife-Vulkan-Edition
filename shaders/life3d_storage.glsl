// Chunk storage shared by the compute passes (life3d_step.comp, life3d_build.comp).
// Must match src/world/ChunkLayout.h.
//
// The world is sparse 32^3-cell chunks. A chunk in pool slot s stores one bit per
// cell: 1024 rows of 32 cells along x, row (y, z) at word s * 1024 + z * 32 + y,
// cell x at bit x. Chunks that hold static blocks (src/life/CellTypes.h) also own
// a slot in the block pool: 1024 "blocked" rows (any block) then 1024 "emits"
// rows (Ember blocks, which count as live neighbors).

const uint CHUNK = 32u;            // cells along a chunk edge
const uint ROWS = 1024u;           // rows per chunk
const uint BLOCK_ROWS = 2u * ROWS; // rows per block-pool slot: blocked, then emits
const uint NO_CHUNK = 0xFFFFFFFFu;

// Neighbor tables have 27 entries per chunk slot, for chunk offsets (dx, dy, dz)
// in [-1, 1]^3 at index (dz + 1) * 9 + (dy + 1) * 3 + (dx + 1).
const uint SELF = 13u; // the chunk itself

layout(std430, binding = 2) readonly buffer Neighbors { uint slots[]; } neighbors;
layout(std430, binding = 3) readonly buffer ActiveList { uint slots[]; } activeList;
layout(std430, binding = 4) readonly buffer Blocks { uint rows[]; } blocks;
layout(std430, binding = 5) readonly buffer BlockSlots { uint slots[]; } blockSlots; // per chunk slot

// Both passes run one workgroup per 32 x 8 slab of rows: the invocation at
// local (i, j) handles row y = i, z = z0 + j of the chunk, where z0 = 8 * the
// workgroup's x index. A chunk is SLABS_PER_CHUNK workgroups.
const uint SLAB_DEPTH = 8u;
const uint SLABS_PER_CHUNK = CHUNK / SLAB_DEPTH;
const uint WORKGROUP_SIZE = CHUNK * SLAB_DEPTH; // 256 invocations

// Each pass first copies the rows its slab needs into shared memory: the slab
// plus a one-row border on every side, y in [-1, 32] and z in [z0 - 1, z0 + 8].
const uint TILE_Y = CHUNK + 2u;      // 34
const uint TILE_Z = SLAB_DEPTH + 2u; // 10
const uint TILE = TILE_Y * TILE_Z;   // rows in the tile

// The chunk's 27 neighbors and their block-pool slots, loaded once per workgroup.
shared uint nearChunk[27];
shared uint nearBlocks[27];

void loadNeighborhood(uint slot) {
    uint i = gl_LocalInvocationIndex;
    if (i < 27u) {
        uint neighbor = neighbors.slots[slot * 27u + i];
        nearChunk[i] = neighbor;
        nearBlocks[i] = neighbor == NO_CHUNK ? NO_CHUNK : blockSlots.slots[neighbor];
    }
    barrier();
}

// The chunk-local (y, z) of tile entry `entry`, for the slab starting at z0.
// y and z may lie one past either side of the chunk.
void tilePosition(uint entry, int z0, out int y, out int z) {
    y = int(entry % TILE_Y) - 1;
    z = z0 + int(entry / TILE_Y) - 1;
}

// A row index for (y, z), which may lie one past either side of the chunk, and
// the neighbor entry (0..26) of the chunk that holds it; dx picks the chunk
// along x.
uint rowAndNeighbor(int y, int z, int dx, out uint k) {
    int chunkDy = y < 0 ? -1 : (y >= int(CHUNK) ? 1 : 0);
    int chunkDz = z < 0 ? -1 : (z >= int(CHUNK) ? 1 : 0);
    k = uint((chunkDz + 1) * 9 + (chunkDy + 1) * 3 + (dx + 1));
    return uint(z - chunkDz * int(CHUNK)) * CHUNK + uint(y - chunkDy * int(CHUNK));
}

// Row `row` of block plane `plane` (0 = blocked, 1 = emits) in neighbor k, or 0
// when that chunk has no blocks.
uint blockRow(uint k, uint row, uint plane) {
    uint blockSlot = nearBlocks[k];
    return blockSlot == NO_CHUNK ? 0u : blocks.rows[blockSlot * BLOCK_ROWS + plane * ROWS + row];
}
