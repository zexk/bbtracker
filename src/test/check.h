#pragma once

// Shared harness for the rule suites. CHECK stays live in every build type:
// these suites run under CMAKE_BUILD_TYPE=Release, where <cassert> asserts
// compile to nothing.

#include <cstdio>

namespace bb::test {

inline int g_fails = 0;

struct Case {
    const char* name;
    void (*fn)();
};

template <size_t N>
int run(const char* suite, const Case (&cases)[N])
{
    for (const Case& c : cases) {
        const int before = g_fails;
        c.fn();
        std::printf("[%s] %s\n", g_fails == before ? "ok" : "FAIL", c.name);
    }
    if (g_fails > 0) {
        std::printf("%s: %d check(s) failed\n", suite, g_fails);
        return 1;
    }
    std::printf("%s: all %zu tests passed\n", suite, N);
    return 0;
}

} // namespace bb::test

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++::bb::test::g_fails;                                      \
        }                                                               \
    } while (0)
