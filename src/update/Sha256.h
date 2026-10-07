#pragma once

// SHA-256 (FIPS 180-4), used to check downloaded updates against the digest
// GitHub publishes for each release asset. A small self-contained
// implementation so the game needs no crypto library; it only ever verifies
// public files, it does not protect secrets.

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace gol3d {

// An incremental SHA-256: feed the message in pieces of any size, then finish.
// The message is processed in 64-byte blocks; bytes that do not yet fill a
// block wait in block_.
class Sha256 {
public:
    // Feeds more message bytes; may be called any number of times.
    void update(const uint8_t* data, size_t size);
    // Pads the message, and returns the digest as 64 lowercase hex digits.
    // The object must not be used afterwards.
    std::string finishHex();

private:
    static constexpr size_t BLOCK_BYTES = 64;

    void compressBlock();

    // The hash state, which becomes the digest. It starts as the first 32 bits
    // of the fractional parts of the square roots of the first 8 primes
    // (FIPS 180-4, section 5.3.3).
    std::array<uint32_t, 8> state_ = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                      0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::array<uint8_t, BLOCK_BYTES> block_{}; // message bytes not yet compressed
    size_t blockUsed_ = 0;                     // how many bytes of block_ are filled
    uint64_t messageBits_ = 0;                 // message length so far, in bits
};

// Hex digest of a byte string.
std::string sha256Hex(const std::string& bytes);
// Hex digest of a file's contents, or nothing when it cannot be read.
std::optional<std::string> sha256OfFile(const std::filesystem::path& file);

} // namespace gol3d
