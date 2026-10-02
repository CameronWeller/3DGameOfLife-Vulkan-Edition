// Bit-sliced 3D life arithmetic: one 32-bit word is a row of 32 cells along x,
// and every operation below updates all 32 cells at once.
//
// This file is written in the common subset of GLSL and C++ so the exact same
// arithmetic runs in shaders/life3d_step.comp and in the CPU tests
// (include/BitLife.h, prototype/life3d_bits_test.cpp). Keep it that way: no
// constructors, no vector types, no GLSL-only built-ins.

// Sum of up to three 1-bit lanes: s has weight 1, c weight 2.
struct Sum2 {
    uint s;
    uint c;
};

// 0..6 in three bit planes.
struct Sum3 {
    uint b0;
    uint b1;
    uint b2;
};

// 0..12 in four bit planes.
struct Sum4 {
    uint b0;
    uint b1;
    uint b2;
    uint b3;
};

// 0..26 in five bit planes.
struct Sum5 {
    uint b0;
    uint b1;
    uint b2;
    uint b3;
    uint b4;
};

Sum2 addBits3(uint a, uint b, uint d) {
    Sum2 r;
    uint t = a ^ b;
    r.s = t ^ d;
    r.c = (a & b) | (t & d);
    return r;
}

Sum2 addBits2(uint a, uint b) {
    Sum2 r;
    r.s = a ^ b;
    r.c = a & b;
    return r;
}

Sum3 addSum2(Sum2 x, Sum2 y) {
    Sum3 r;
    uint k = x.s & y.s;
    r.b0 = x.s ^ y.s;
    uint t = x.c ^ y.c;
    r.b1 = t ^ k;
    r.b2 = (x.c & y.c) | (t & k);
    return r;
}

Sum4 addSum3(Sum3 x, Sum3 y) {
    Sum4 r;
    uint k = x.b0 & y.b0;
    r.b0 = x.b0 ^ y.b0;
    uint t = x.b1 ^ y.b1;
    r.b1 = t ^ k;
    k = (x.b1 & y.b1) | (t & k);
    t = x.b2 ^ y.b2;
    r.b2 = t ^ k;
    r.b3 = (x.b2 & y.b2) | (t & k);
    return r;
}

Sum5 addSum4(Sum4 x, Sum4 y) {
    Sum5 r;
    uint k = x.b0 & y.b0;
    r.b0 = x.b0 ^ y.b0;
    uint t = x.b1 ^ y.b1;
    r.b1 = t ^ k;
    k = (x.b1 & y.b1) | (t & k);
    t = x.b2 ^ y.b2;
    r.b2 = t ^ k;
    k = (x.b2 & y.b2) | (t & k);
    t = x.b3 ^ y.b3;
    r.b3 = t ^ k;
    r.b4 = (x.b3 & y.b3) | (t & k);
    return r;
}

// x + y where the total stays below 32 (it is at most 26 here).
Sum5 addSum5Sum2(Sum5 x, Sum2 y) {
    Sum5 r;
    uint k = x.b0 & y.s;
    r.b0 = x.b0 ^ y.s;
    uint t = x.b1 ^ y.c;
    r.b1 = t ^ k;
    k = (x.b1 & y.c) | (t & k);
    r.b2 = x.b2 ^ k;
    k = x.b2 & k;
    r.b3 = x.b3 ^ k;
    k = x.b3 & k;
    r.b4 = x.b4 ^ k;
    return r;
}

// Live-neighbor counts for a row. nb[k] is the row of cells at offset
// (dx, dy, dz) with k = (dz + 1) * 9 + (dy + 1) * 3 + (dx + 1): bit i of nb[k]
// is the cell at (i + dx, y + dy, z + dz). nb[13] (the row itself) is not counted.
Sum5 countNeighbors(uint nb[27]) {
    Sum2 g0 = addBits3(nb[0], nb[1], nb[2]);
    Sum2 g1 = addBits3(nb[3], nb[4], nb[5]);
    Sum2 g2 = addBits3(nb[6], nb[7], nb[8]);
    Sum2 g3 = addBits3(nb[9], nb[10], nb[11]);
    Sum2 g4 = addBits2(nb[12], nb[14]);
    Sum2 g5 = addBits3(nb[15], nb[16], nb[17]);
    Sum2 g6 = addBits3(nb[18], nb[19], nb[20]);
    Sum2 g7 = addBits3(nb[21], nb[22], nb[23]);
    Sum2 g8 = addBits3(nb[24], nb[25], nb[26]);
    Sum4 low = addSum3(addSum2(g0, g1), addSum2(g2, g3));
    Sum4 high = addSum3(addSum2(g5, g6), addSum2(g7, g8));
    return addSum5Sum2(addSum4(low, high), g4);
}

// Lanes whose count n has bit n set in surviveMask (live cells) or birthMask
// (empty cells). A 32-way multiplexer over the count's bit planes.
uint applyRule(Sum5 n, uint alive, uint surviveMask, uint birthMask) {
    uint dead = ~alive;
    uint level[16];
    for (uint i = 0u; i < 16u; i++) {
        uint lo = 2u * i;
        uint hi = lo + 1u;
        uint leafLo = ((0u - ((surviveMask >> lo) & 1u)) & alive) | ((0u - ((birthMask >> lo) & 1u)) & dead);
        uint leafHi = ((0u - ((surviveMask >> hi) & 1u)) & alive) | ((0u - ((birthMask >> hi) & 1u)) & dead);
        level[i] = (n.b0 & leafHi) | (~n.b0 & leafLo);
    }
    // Each level halves the table; entry i only reads entries 2i and 2i + 1, which
    // are not yet overwritten, so the table can shrink in place.
    for (uint i = 0u; i < 8u; i++) level[i] = (n.b1 & level[2u * i + 1u]) | (~n.b1 & level[2u * i]);
    for (uint i = 0u; i < 4u; i++) level[i] = (n.b2 & level[2u * i + 1u]) | (~n.b2 & level[2u * i]);
    for (uint i = 0u; i < 2u; i++) level[i] = (n.b3 & level[2u * i + 1u]) | (~n.b3 & level[2u * i]);
    return (n.b4 & level[1]) | (~n.b4 & level[0]);
}

// The next generation of a row. nb holds the rows that count as live
// neighbors (live cells and Ember blocks), alive the row's own live cells and
// blocked the row's static blocks, which never hold life.
uint nextRow(uint nb[27], uint alive, uint blocked, uint surviveMask, uint birthMask) {
    return applyRule(countNeighbors(nb), alive, surviveMask, birthMask) & ~blocked;
}
