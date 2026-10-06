#pragma once

// Small unit conversions used for timing and memory reports.

#include <chrono>
#include <cstdint>

namespace gol3d {

// Milliseconds of wall time since `start`.
inline double millisecondsSince(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
        .count();
}

// Whole mebibytes (1 MB = 2^20 bytes here, as GPU tools report memory).
constexpr uint64_t toMegabytes(uint64_t bytes) {
    return bytes >> 20;
}

} // namespace gol3d
