// Bit-sliced 3D life arithmetic: one 32-bit word is a row of 32 cells along x,
// and every operation below updates all 32 cells ("lanes") at once. Bit i of
// every word belongs to cell i; no operation mixes bits of different lanes.
//
// This file is written in the common subset of GLSL and C++ so the exact same
// arithmetic runs in shaders/life3d_step.comp and in the CPU tests
// (src/life/BitLife.h, tests/BitLifeTest.cpp). Keep it that way: no
// constructors, no vector types, no GLSL-only built-ins.
//
// docs/ARCHITECTURE.md explains the idea step by step.

// Numbers stored "sliced": each field holds one binary digit of 32 numbers.
// Bit i of `ones` is the 1s digit of lane i's number, bit i of `twos` its 2s
// digit, and so on.

// 0..3 in two digits.
struct Sum2 {
    uint ones;
    uint twos;
};

// 0..6 in three digits: b0 is the 1s digit, b1 the 2s, b2 the 4s.
struct Sum3 {
    uint b0;
    uint b1;
    uint b2;
};

// 0..12 in four digits.
struct Sum4 {
    uint b0;
    uint b1;
    uint b2;
    uint b3;
};

// 0..26 in five digits: a cell's live-neighbor count.
struct Sum5 {
    uint b0;
    uint b1;
    uint b2;
    uint b3;
    uint b4;
};

// A full adder per lane: a + b + c. The 1s digit is set when an odd number of
// inputs are; the 2s digit (the carry) when at least two are.
Sum2 addBits3(uint a, uint b, uint c) {
    Sum2 sum;
    uint aXorB = a ^ b;
    sum.ones = aXorB ^ c;
    sum.twos = (a & b) | (aXorB & c);
    return sum;
}

// A half adder per lane: a + b.
Sum2 addBits2(uint a, uint b) {
    Sum2 sum;
    sum.ones = a ^ b;
    sum.twos = a & b;
    return sum;
}

// The adders below add two sliced numbers digit by digit, like long addition
// on paper: each digit is a full adder of the two input digits and the carry
// from the digit below.

Sum3 addSum2(Sum2 x, Sum2 y) {
    Sum3 sum;
    uint carry = x.ones & y.ones;
    sum.b0 = x.ones ^ y.ones;
    uint partial = x.twos ^ y.twos;
    sum.b1 = partial ^ carry;
    sum.b2 = (x.twos & y.twos) | (partial & carry);
    return sum;
}

Sum4 addSum3(Sum3 x, Sum3 y) {
    Sum4 sum;
    uint carry = x.b0 & y.b0;
    sum.b0 = x.b0 ^ y.b0;
    uint partial = x.b1 ^ y.b1;
    sum.b1 = partial ^ carry;
    carry = (x.b1 & y.b1) | (partial & carry);
    partial = x.b2 ^ y.b2;
    sum.b2 = partial ^ carry;
    sum.b3 = (x.b2 & y.b2) | (partial & carry);
    return sum;
}

Sum5 addSum4(Sum4 x, Sum4 y) {
    Sum5 sum;
    uint carry = x.b0 & y.b0;
    sum.b0 = x.b0 ^ y.b0;
    uint partial = x.b1 ^ y.b1;
    sum.b1 = partial ^ carry;
    carry = (x.b1 & y.b1) | (partial & carry);
    partial = x.b2 ^ y.b2;
    sum.b2 = partial ^ carry;
    carry = (x.b2 & y.b2) | (partial & carry);
    partial = x.b3 ^ y.b3;
    sum.b3 = partial ^ carry;
    sum.b4 = (x.b3 & y.b3) | (partial & carry);
    return sum;
}

// x + y where the total stays below 32 (it is at most 26 here), so the carry
// out of the top digit can be dropped.
Sum5 addSum5Sum2(Sum5 x, Sum2 y) {
    Sum5 sum;
    uint carry = x.b0 & y.ones;
    sum.b0 = x.b0 ^ y.ones;
    uint partial = x.b1 ^ y.twos;
    sum.b1 = partial ^ carry;
    carry = (x.b1 & y.twos) | (partial & carry);
    sum.b2 = x.b2 ^ carry; // y has no 4s digit: from here only the carry ripples up
    carry = x.b2 & carry;
    sum.b3 = x.b3 ^ carry;
    carry = x.b3 & carry;
    sum.b4 = x.b4 ^ carry;
    return sum;
}

// Live-neighbor counts for a row. nb[k] is the row of cells at offset
// (dx, dy, dz) with k = (dz + 1) * 9 + (dy + 1) * 3 + (dx + 1): bit i of nb[k]
// is the cell at (i + dx, y + dy, z + dz).
Sum5 countNeighbors(uint nb[27]) {
    // Nine groups of three rows, except that nb[13] is the row itself and is
    // not counted, so its group has only two.
    Sum2 g0 = addBits3(nb[0], nb[1], nb[2]);
    Sum2 g1 = addBits3(nb[3], nb[4], nb[5]);
    Sum2 g2 = addBits3(nb[6], nb[7], nb[8]);
    Sum2 g3 = addBits3(nb[9], nb[10], nb[11]);
    Sum2 g4 = addBits2(nb[12], nb[14]);
    Sum2 g5 = addBits3(nb[15], nb[16], nb[17]);
    Sum2 g6 = addBits3(nb[18], nb[19], nb[20]);
    Sum2 g7 = addBits3(nb[21], nb[22], nb[23]);
    Sum2 g8 = addBits3(nb[24], nb[25], nb[26]);
    // Then add the group sums pairwise, like a tournament bracket.
    Sum4 low = addSum3(addSum2(g0, g1), addSum2(g2, g3));
    Sum4 high = addSum3(addSum2(g5, g6), addSum2(g7, g8));
    return addSum5Sum2(addSum4(low, high), g4);
}

// All ones in every lane when bit n of `mask` is set, else all zeros.
uint laneMaskFromBit(uint mask, uint n) {
    return 0u - ((mask >> n) & 1u); // 0 - 1 wraps around to 0xFFFFFFFF
}

// For each lane: bit `count` of surviveMask if the cell is alive, else bit
// `count` of birthMask. That is "look up the rule", done for 32 lanes at once.
//
// It is a binary multiplexer tree over the count's digits. The 32 leaves are
// the answers for counts 0..31; each level uses one digit of the count to pick
// one of each pair, halving the candidates, until one is left.
uint applyRule(Sum5 n, uint alive, uint surviveMask, uint birthMask) {
    uint dead = ~alive;
    uint level[16];
    // Level 1 (digit b0): pick between the answers for counts 2i and 2i + 1.
    for (uint i = 0u; i < 16u; i++) {
        uint even = 2u * i;
        uint odd = even + 1u;
        uint leafEven = (laneMaskFromBit(surviveMask, even) & alive) | (laneMaskFromBit(birthMask, even) & dead);
        uint leafOdd = (laneMaskFromBit(surviveMask, odd) & alive) | (laneMaskFromBit(birthMask, odd) & dead);
        level[i] = (n.b0 & leafOdd) | (~n.b0 & leafEven);
    }
    // Levels 2-4 (digits b1-b3). Entry i reads entries 2i and 2i + 1, which are
    // not yet overwritten, so the table can shrink in place.
    for (uint i = 0u; i < 8u; i++) {
        level[i] = (n.b1 & level[2u * i + 1u]) | (~n.b1 & level[2u * i]);
    }
    for (uint i = 0u; i < 4u; i++) {
        level[i] = (n.b2 & level[2u * i + 1u]) | (~n.b2 & level[2u * i]);
    }
    for (uint i = 0u; i < 2u; i++) {
        level[i] = (n.b3 & level[2u * i + 1u]) | (~n.b3 & level[2u * i]);
    }
    // Level 5 (digit b4).
    return (n.b4 & level[1]) | (~n.b4 & level[0]);
}

// The next generation of a row. nb holds the rows that count as live
// neighbors (live cells and Ember blocks), alive the row's own live cells and
// blocked the row's static blocks, which never hold life.
uint nextRow(uint nb[27], uint alive, uint blocked, uint surviveMask, uint birthMask) {
    return applyRule(countNeighbors(nb), alive, surviveMask, birthMask) & ~blocked;
}
