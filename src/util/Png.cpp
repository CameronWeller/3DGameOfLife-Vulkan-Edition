#include "util/Png.h"

#include <algorithm>
#include <array>
#include <fstream>

namespace gol3d {
namespace {

void appendBigEndian32(std::vector<uint8_t>& out, uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.push_back(static_cast<uint8_t>(value >> shift));
    }
}

// Each row of PNG image data starts with a filter type; 0 means "None", the
// row's bytes as they are.
std::vector<uint8_t> unfilteredRows(uint32_t width, uint32_t height,
                                    const std::vector<uint8_t>& rgb) {
    const size_t rowBytes = static_cast<size_t>(width) * 3;
    std::vector<uint8_t> rows;
    rows.reserve((rowBytes + 1) * height);
    for (uint32_t y = 0; y < height; ++y) {
        rows.push_back(0); // filter type: None
        auto rowStart = rgb.begin() + static_cast<std::ptrdiff_t>(y * rowBytes);
        rows.insert(rows.end(), rowStart, rowStart + static_cast<std::ptrdiff_t>(rowBytes));
    }
    return rows;
}

// A zlib stream (RFC 1950) holding `data` in "stored" deflate blocks (RFC 1951,
// section 3.2.4), which copy bytes without compressing them.
std::vector<uint8_t> zlibStored(const std::vector<uint8_t>& data) {
    constexpr size_t MAX_STORED_BLOCK = 65535;
    // Header: deflate with a 32 KB window (0x78), no preset dictionary, fastest
    // level (0x01); together they are a multiple of 31 as RFC 1950 requires.
    std::vector<uint8_t> out = {0x78, 0x01};
    size_t pos = 0;
    do {
        const size_t length = std::min(MAX_STORED_BLOCK, data.size() - pos);
        const bool lastBlock = pos + length == data.size();
        // Block header byte: bit 0 = last block, bits 1-2 = type 00 (stored).
        out.push_back(lastBlock ? 1 : 0);
        // Length and its one's complement, both little-endian 16-bit.
        out.push_back(length & 0xFF);
        out.push_back((length >> 8) & 0xFF);
        out.push_back(~length & 0xFF);
        out.push_back((~length >> 8) & 0xFF);
        out.insert(out.end(), data.begin() + static_cast<std::ptrdiff_t>(pos),
                   data.begin() + static_cast<std::ptrdiff_t>(pos + length));
        pos += length;
    } while (pos < data.size());
    appendBigEndian32(out, adler32(data.data(), data.size()));
    return out;
}

// A PNG chunk: length, 4-letter type, data, then a CRC over type and data.
void writeChunk(std::ofstream& file, const char type[4], const std::vector<uint8_t>& data) {
    std::vector<uint8_t> chunk;
    appendBigEndian32(chunk, static_cast<uint32_t>(data.size()));
    chunk.insert(chunk.end(), type, type + 4);
    chunk.insert(chunk.end(), data.begin(), data.end());
    const uint8_t* typeAndData = chunk.data() + 4;
    appendBigEndian32(chunk, crc32(typeAndData, 4 + data.size()));
    file.write(reinterpret_cast<const char*>(chunk.data()),
               static_cast<std::streamsize>(chunk.size()));
}

} // namespace

uint32_t crc32(const uint8_t* data, size_t size) {
    // Table-driven CRC with the reversed polynomial 0xEDB88320: entry n is the
    // CRC of the single byte n.
    static const std::array<uint32_t, 256> TABLE = [] {
        std::array<uint32_t, 256> table{};
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int bit = 0; bit < 8; ++bit) {
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            }
            table[n] = c;
        }
        return table;
    }();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) {
        crc = TABLE[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

uint32_t adler32(const uint8_t* data, size_t size) {
    // Two running sums modulo the largest prime below 2^16.
    constexpr uint32_t MODULUS = 65521;
    uint32_t sum = 1;
    uint32_t sumOfSums = 0;
    for (size_t i = 0; i < size; ++i) {
        sum = (sum + data[i]) % MODULUS;
        sumOfSums = (sumOfSums + sum) % MODULUS;
    }
    return (sumOfSums << 16) | sum;
}

bool writePng(const std::string& path, uint32_t width, uint32_t height,
              const std::vector<uint8_t>& rgb) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;

    constexpr char SIGNATURE[8] = {'\x89', 'P', 'N', 'G', '\r', '\n', '\x1a', '\n'};
    file.write(SIGNATURE, sizeof(SIGNATURE));

    std::vector<uint8_t> header;
    appendBigEndian32(header, width);
    appendBigEndian32(header, height);
    header.push_back(8); // bits per channel
    header.push_back(2); // color type: RGB
    header.push_back(0); // compression method: deflate
    header.push_back(0); // filter method: adaptive (every row here uses "None")
    header.push_back(0); // interlace: none

    writeChunk(file, "IHDR", header);
    writeChunk(file, "IDAT", zlibStored(unfilteredRows(width, height, rgb)));
    writeChunk(file, "IEND", {});
    return static_cast<bool>(file);
}

} // namespace gol3d
