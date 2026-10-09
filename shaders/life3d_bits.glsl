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
// on paper: each digit is a full adder (the expressions of addBits3) of the two
// input digits and the carry from the digit below. `partial` is the two digits'
// sum without the carry; the new carry is set when both digits are, or when
// one is and the carry in is.

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

// Live-neighbor counts for a row (y, z). neighborRows[k] is the row of cells at
// offset (dx, dy, dz) with k = (dz + 1) * 9 + (dy + 1) * 3 + (dx + 1): bit i of
// neighborRows[k] is the cell at (i + dx, y + dy, z + dz). So for every lane,
// the 27 entries hold that cell's 3 x 3 x 3 neighborhood.
//
// The rows of the layers in front of and behind the cell (dz = -1 and dz = 1,
// entries 0-8 and 18-26) are ANDed with two masks, so a rule can leave them
// out: `frontBackMiddle` for the three with dx = 0 (entries 1, 4, 7, 19, 22,
// 25, the cell's own y-z layer) and `frontBackSides` for the rest. All ones in
// both counts the whole 3 x 3 x 3 cube (26 neighbors); zero in both only the
// cell's own x-y layer (8); sides zero and middle all ones the x-y and y-z
// layers together (14). See Neighborhood in src/life/LifeRules.h.
Sum5 countNeighbors(uint neighborRows[27], uint frontBackSides, uint frontBackMiddle) {
    // Nine groups of three rows (0..3 each), except that neighborRows[13] is
    // the row itself and is not counted, so its group has only two.
    Sum2 g0 = addBits3(neighborRows[0] & frontBackSides, neighborRows[1] & frontBackMiddle,
                       neighborRows[2] & frontBackSides);
    Sum2 g1 = addBits3(neighborRows[3] & frontBackSides, neighborRows[4] & frontBackMiddle,
                       neighborRows[5] & frontBackSides);
    Sum2 g2 = addBits3(neighborRows[6] & frontBackSides, neighborRows[7] & frontBackMiddle,
                       neighborRows[8] & frontBackSides);
    Sum2 g3 = addBits3(neighborRows[9], neighborRows[10], neighborRows[11]);
    Sum2 g4 = addBits2(neighborRows[12], neighborRows[14]);
    Sum2 g5 = addBits3(neighborRows[15], neighborRows[16], neighborRows[17]);
    Sum2 g6 = addBits3(neighborRows[18] & frontBackSides, neighborRows[19] & frontBackMiddle,
                       neighborRows[20] & frontBackSides);
    Sum2 g7 = addBits3(neighborRows[21] & frontBackSides, neighborRows[22] & frontBackMiddle,
                       neighborRows[23] & frontBackSides);
    Sum2 g8 = addBits3(neighborRows[24] & frontBackSides, neighborRows[25] & frontBackMiddle,
                       neighborRows[26] & frontBackSides);
    // Then add the group sums pairwise, like a tournament bracket, each round
    // one digit wider: pairs are 0..6, fours 0..12, the eight groups 0..24,
    // and the short group g4 (0..2) brings the total to at most 26.
    Sum4 low = addSum3(addSum2(g0, g1), addSum2(g2, g3));
    Sum4 high = addSum3(addSum2(g5, g6), addSum2(g7, g8));
    return addSum5Sum2(addSum4(low, high), g4);
}

// All ones in every lane when bit n of `mask` is set, else all zeros.
uint laneMaskFromBit(uint mask, uint n) {
    return 0u - ((mask >> n) & 1u); // 0 - 1 wraps around to 0xFFFFFFFF
}

// For each lane: the bit of `ifSet` where `selector` is set, else the bit of
// `ifClear`. A 32-lane version of `selector ? ifSet : ifClear`.
uint pickLanes(uint selector, uint ifSet, uint ifClear) {
    return (selector & ifSet) | (~selector & ifClear);
}

// For each lane: bit `count` of surviveMask if the cell is alive, else bit
// `count` of birthMask. That is "look up the rule", done for 32 lanes at once.
//
// It is a binary multiplexer tree over the count's digits. The 32 leaves are
// the answers for counts 0..31 (counts above 26 never occur); each level uses
// one digit of the count to pick one of each pair, halving the candidates,
// until one is left. Every lane can pick a different leaf, because the digits
// are themselves per-lane masks.
uint applyRule(Sum5 count, uint alive, uint surviveMask, uint birthMask) {
    uint dead = ~alive;
    uint level[16];
    // Level 1 (digit b0): pick between the answers for counts 2i and 2i + 1.
    // A leaf is the survive bit for live lanes and the birth bit for dead ones.
    for (uint i = 0u; i < 16u; i++) {
        uint even = 2u * i;
        uint odd = even + 1u;
        uint leafEven = (laneMaskFromBit(surviveMask, even) & alive) | (laneMaskFromBit(birthMask, even) & dead);
        uint leafOdd = (laneMaskFromBit(surviveMask, odd) & alive) | (laneMaskFromBit(birthMask, odd) & dead);
        level[i] = pickLanes(count.b0, leafOdd, leafEven);
    }
    // Levels 2-4 (digits b1-b3). Entry i reads entries 2i and 2i + 1, which are
    // not yet overwritten, so the table can shrink in place.
    for (uint i = 0u; i < 8u; i++) {
        level[i] = pickLanes(count.b1, level[2u * i + 1u], level[2u * i]);
    }
    for (uint i = 0u; i < 4u; i++) {
        level[i] = pickLanes(count.b2, level[2u * i + 1u], level[2u * i]);
    }
    for (uint i = 0u; i < 2u; i++) {
        level[i] = pickLanes(count.b3, level[2u * i + 1u], level[2u * i]);
    }
    // Level 5 (digit b4).
    return pickLanes(count.b4, level[1], level[0]);
}

// The next generation of a row. neighborRows holds the rows that count as live
// neighbors (live cells and Ember blocks; see countNeighbors for the order and
// the two masks), alive the row's own live cells and blocked the row's static
// blocks, which never hold life.
uint nextRow(uint neighborRows[27], uint alive, uint blocked, uint surviveMask, uint birthMask,
             uint frontBackSides, uint frontBackMiddle) {
    Sum5 count = countNeighbors(neighborRows, frontBackSides, frontBackMiddle);
    return applyRule(count, alive, surviveMask, birthMask) & ~blocked;
}
