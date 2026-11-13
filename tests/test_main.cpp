#include "test_util.hpp"

int main() {
    int failed_tests = 0;
    for (const TestCase& test : test_registry()) {
        const int before = test_failures();
        test.fn();
        const bool ok = test_failures() == before;
        std::cout << (ok ? "[ PASS ] " : "[ FAIL ] ") << test.name << "\n";
        if (!ok) ++failed_tests;
    }
    std::cout << "\n" << test_registry().size() << " tests, " << failed_tests << " failed\n";
    return failed_tests == 0 ? 0 : 1;
}
