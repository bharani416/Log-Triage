#pragma once

// Tiny self-contained test framework (no external dependencies, works offline).

#include <iostream>
#include <vector>

struct TestCase {
    const char* name;
    void (*fn)();
};

inline std::vector<TestCase>& test_registry() {
    static std::vector<TestCase> registry;
    return registry;
}

inline int& test_failures() {
    static int failures = 0;
    return failures;
}

struct TestRegistrar {
    TestRegistrar(const char* name, void (*fn)()) { test_registry().push_back({name, fn}); }
};

#define TEST(name)                                  \
    static void name();                             \
    static TestRegistrar registrar_##name(#name, name); \
    static void name()

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond \
                      << "\n";                                                   \
            ++test_failures();                                                   \
        }                                                                        \
    } while (0)
