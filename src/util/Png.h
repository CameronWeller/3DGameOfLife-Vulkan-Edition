#pragma once

// A minimal PNG writer for screenshots, so the game needs no image library.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gol3d {

// Writes 8-bit RGB pixels (rows top to bottom, 3 bytes per pixel) as a PNG.
// The image data is stored uncompressed, which makes files large but keeps the
// writer short. Returns false when the file cannot be written.
bool writePng(const std::string& path, uint32_t width, uint32_t height,
              const std::vector<uint8_t>& rgb);

// The checksums PNG needs, exposed for the tests.
uint32_t crc32(const uint8_t* data, size_t size);   // per chunk (ISO 3309 / ITU-T V.42)
uint32_t adler32(const uint8_t* data, size_t size); // per zlib stream (RFC 1950)

} // namespace gol3d
