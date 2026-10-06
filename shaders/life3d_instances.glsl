// The block list: one instance per visible block, written by life3d_build.comp
// and read by life3d_blocks.vert. Must match src/world/BlockInstances.h.
//
//   x: bits 0-26  which of the 27 cells around the block are occupied
//                 (bit (dz + 1) * 9 + (dy + 1) * 3 + (dx + 1); bit 13, the block
//                 itself, is unused)
//      bits 27-28 kind (a CellKind value)
//      bit  29    born in the last generation (grows in)
//      bit  30    died in the last generation (shrinks away)
//   y: bits 0-14  cell within its chunk: x | y << 5 | z << 10
//      bits 15-31 chunk pool slot

const uint INSTANCE_KIND_SHIFT = 27u;
const uint INSTANCE_BORN_BIT = 1u << 29;
const uint INSTANCE_DYING_BIT = 1u << 30;
const uint INSTANCE_SLOT_SHIFT = 15u;

// Kinds, as in CellKind (src/life/CellTypes.h).
const uint KIND_LIFE = 0u;
const uint KIND_STONE = 1u;
const uint KIND_EMBER = 2u;

uvec2 packInstance(uint neighborBits, uint kind, bool born, bool dying, uint slot, uint x, uint y,
                   uint z) {
    uint flags = (kind << INSTANCE_KIND_SHIFT) | (born ? INSTANCE_BORN_BIT : 0u) |
                 (dying ? INSTANCE_DYING_BIT : 0u);
    uint location = x | (y << 5) | (z << 10) | (slot << INSTANCE_SLOT_SHIFT);
    return uvec2(neighborBits | flags, location);
}

uint instanceNeighbors(uvec2 instance) { return instance.x & ((1u << 27) - 1u); }
uint instanceKind(uvec2 instance) { return (instance.x >> INSTANCE_KIND_SHIFT) & 3u; }
bool instanceBorn(uvec2 instance) { return (instance.x & INSTANCE_BORN_BIT) != 0u; }
bool instanceDying(uvec2 instance) { return (instance.x & INSTANCE_DYING_BIT) != 0u; }
uint instanceSlot(uvec2 instance) { return instance.y >> INSTANCE_SLOT_SHIFT; }

// The cell's position within its chunk, 0..31 on each axis.
ivec3 instanceLocalCell(uvec2 instance) {
    uint local = instance.y & ((1u << INSTANCE_SLOT_SHIFT) - 1u);
    return ivec3(local & 31u, (local >> 5) & 31u, local >> 10);
}
