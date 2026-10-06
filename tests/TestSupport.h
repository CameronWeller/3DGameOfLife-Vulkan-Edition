#pragma once

// A deliberately tiny test harness. Each test file is a plain program: checks
// call expect(), and main() returns finish(), which prints a summary and gives
// ctest the exit code. A failed check prints what was expected and carries on,
// so one run shows every failure.

#include <iostream>
#include <string>

namespace gol3d::testing {

inline int& failureCount() {
    static int failures = 0;
    return failures;
}

inline void expect(bool condition, const std::string& what) {
    if (condition) return;
    std::cerr << "FAIL: " << what << "\n";
    ++failureCount();
}

inline int finish(const std::string& suite) {
    if (failureCount() > 0) {
        std::cerr << suite << ": " << failureCount() << " check(s) failed\n";
        return 1;
    }
    std::cout << suite << ": all checks passed\n";
    return 0;
}

} // namespace gol3d::testing
