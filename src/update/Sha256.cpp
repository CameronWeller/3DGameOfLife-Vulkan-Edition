// SHA-256 as specified in FIPS 180-4; the comments give the section for each
// step. tests/UpdaterTest.cpp checks it against the standard's test vectors.

#include "update/Sha256.h"

#include <fstream>

namespace gol3d {
namespace {

constexpr int ROUNDS = 64;
constexpr int BLOCK_WORDS = 16; // 32-bit words in a 64-byte block

// The round constants: the first 32 bits of the fractional parts of the cube
// roots of the first 64 primes (section 4.2.2).
constexpr std::array<uint32_t, ROUNDS> ROUND_CONSTANTS = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

// Every caller passes 0 < bits < 32, where both shifts are defined.
uint32_t rotateRight(uint32_t x, int bits) {
    return (x >> bits) | (x << (32 - bits));
}

// The six logical functions of FIPS 180-4, section 4.1.2, named as there.

// "Choose": each bit of x picks the bit of y (x = 1) or z (x = 0).
uint32_t choose(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

// "Majority": each result bit is the value most of x, y and z have.
uint32_t majority(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

// Big sigma 0 and 1 mix the working variables in each round.
uint32_t bigSigma0(uint32_t x) {
    return rotateRight(x, 2) ^ rotateRight(x, 13) ^ rotateRight(x, 22);
}

uint32_t bigSigma1(uint32_t x) {
    return rotateRight(x, 6) ^ rotateRight(x, 11) ^ rotateRight(x, 25);
}

// Small sigma 0 and 1 expand the 16 message words into 64.
uint32_t smallSigma0(uint32_t x) {
    return rotateRight(x, 7) ^ rotateRight(x, 18) ^ (x >> 3);
}

uint32_t smallSigma1(uint32_t x) {
    return rotateRight(x, 17) ^ rotateRight(x, 19) ^ (x >> 10);
}

uint32_t readBigEndian32(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) << 24 | static_cast<uint32_t>(bytes[1]) << 16 |
           static_cast<uint32_t>(bytes[2]) << 8 | static_cast<uint32_t>(bytes[3]);
}

} // namespace

// Mixes one 64-byte block into the state (FIPS 180-4, section 6.2.2).
void Sha256::compressBlock() {
    // 1. The message schedule: the block's 16 big-endian words, extended to one
    //    word per round. `t` is the round number, as in the standard.
    std::array<uint32_t, ROUNDS> schedule{};
    for (int t = 0; t < BLOCK_WORDS; ++t) {
        schedule[t] = readBigEndian32(&block_[t * 4]);
    }
    for (int t = BLOCK_WORDS; t < ROUNDS; ++t) {
        schedule[t] = smallSigma1(schedule[t - 2]) + schedule[t - 7] +
                      smallSigma0(schedule[t - 15]) + schedule[t - 16];
    }

    // 2. Eight working variables start as the current state.
    uint32_t a = state_[0];
    uint32_t b = state_[1];
    uint32_t c = state_[2];
    uint32_t d = state_[3];
    uint32_t e = state_[4];
    uint32_t f = state_[5];
    uint32_t g = state_[6];
    uint32_t h = state_[7];

    // 3. 64 rounds. Each computes two temporaries from the variables, then shifts
    //    the variables down one place (h is dropped, a takes the new value) with
    //    the first temporary also added into e.
    for (int t = 0; t < ROUNDS; ++t) {
        uint32_t temp1 = h + bigSigma1(e) + choose(e, f, g) + ROUND_CONSTANTS[t] + schedule[t];
        uint32_t temp2 = bigSigma0(a) + majority(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    // 4. The block's result is added into the state.
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Sha256::update(const uint8_t* data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        block_[blockUsed_++] = data[i];
        if (blockUsed_ == block_.size()) {
            compressBlock();
            blockUsed_ = 0;
        }
    }
    messageBits_ += static_cast<uint64_t>(size) * 8;
}

std::string Sha256::finishHex() {
    // Padding (FIPS 180-4, section 5.1.1): a single 1 bit, zeros until 8 bytes
    // short of a block boundary, then the message length in bits as a 64-bit
    // big-endian number.
    // The length is captured first: update() below counts the padding too.
    constexpr size_t LENGTH_FIELD_BYTES = 8;
    const uint64_t lengthBits = messageBits_;
    const uint8_t oneBit = 0x80;
    const uint8_t zero = 0;
    update(&oneBit, 1);
    while (blockUsed_ != BLOCK_BYTES - LENGTH_FIELD_BYTES) {
        update(&zero, 1);
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
        uint8_t byte = static_cast<uint8_t>(lengthBits >> shift);
        update(&byte, 1);
    }

    // The digest is the state words in order, each as 8 hex digits, most
    // significant first.
    constexpr const char* HEX_DIGITS = "0123456789abcdef";
    std::string hex;
    for (uint32_t word : state_) {
        for (int shift = 28; shift >= 0; shift -= 4) {
            hex += HEX_DIGITS[(word >> shift) & 0xF];
        }
    }
    return hex;
}

std::string sha256Hex(const std::string& bytes) {
    Sha256 sha;
    sha.update(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
    return sha.finishHex();
}

std::optional<std::string> sha256OfFile(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return std::nullopt;
    Sha256 sha;
    constexpr size_t READ_CHUNK_BYTES = 64 * 1024; // the file is read a piece at a time
    std::array<char, READ_CHUNK_BYTES> buffer{};
    // The last read() comes up short and ends the loop, but gcount() still
    // says how many bytes it got.
    while (in) {
        in.read(buffer.data(), buffer.size());
        sha.update(reinterpret_cast<const uint8_t*>(buffer.data()),
                   static_cast<size_t>(in.gcount()));
    }
    return sha.finishHex();
}

} // namespace gol3d
