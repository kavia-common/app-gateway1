#include <cstdlib>
#include <iostream>
#include <vector>
#include <functional>
#include <string>
#include <stdexcept>

// Minimal test registration and assertion framework when GTest is absent.
namespace mini {
    using TestFn = std::function<void()>;
    struct TestCase { std::string name; TestFn fn; };
    inline std::vector<TestCase>& registry() { static std::vector<TestCase> r; return r; }

    struct Registrar {
        Registrar(const std::string& name, TestFn fn) { registry().push_back({name, std::move(fn)}); }
    };

    inline void assert_true(bool expr, const char* exprStr, const char* file, int line) {
        if (!expr) {
            std::cerr << "Assertion failed: " << exprStr << " at " << file << ":" << line << std::endl;
            throw std::runtime_error("assertion_failed");
        }
    }
}
#define TEST(SUITE, NAME) \
    static void SUITE##_##NAME(); \
    static mini::Registrar registrar_##SUITE##_##NAME(#SUITE "." #NAME, SUITE##_##NAME); \
    static void SUITE##_##NAME()

#define EXPECT_TRUE(x) mini::assert_true((x), #x, __FILE__, __LINE__)
#define EXPECT_FALSE(x) mini::assert_true(!(x), #x, __FILE__, __LINE__)
#define EXPECT_EQ(a,b) mini::assert_true(((a)==(b)), #a " == " #b, __FILE__, __LINE__)
#define EXPECT_NE(a,b) mini::assert_true(((a)!=(b)), #a " != " #b, __FILE__, __LINE__)

int main() {
    int failures = 0;
    for (auto& t : mini::registry()) {
        try {
            t.fn();
            std::cout << "[  PASSED  ] " << t.name << std::endl;
        } catch (...) {
            std::cout << "[  FAILED  ] " << t.name << std::endl;
            failures++;
        }
    }
    std::cout << (failures == 0 ? "[  ALL PASSED  ]" : "[  SOME FAILED  ]") << std::endl;
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
