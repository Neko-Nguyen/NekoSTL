// A ~150 line test framework. Deliberately tiny and dependency-free: it uses
// std::vector / std::string internally (it is testing your STL, not using it).
//
//   NEKO_TEST(name) { ... }      declare + auto-register a test case
//   CHECK(expr)                   record a failure, keep going
//   REQUIRE(expr)                 record a failure, abandon this test case
//   CHECK_EQ(a, b)                CHECK with both values printed on failure
//   CHECK_THROWS_AS(expr, E)      expects expr to throw E
//   STATIC_CHECK(expr)            compile-time assertion
//
// Run one binary directly to filter:  ./test_vector push_back
#pragma once

#include <cstdio>
#include <exception>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "neko/config.hpp"

namespace neko_test {

struct Abort {};  // thrown by REQUIRE to unwind out of the current test

struct TestCase {
    const char* name;
    const char* file;
    int line;
    void (*fn)();
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

struct Registrar {
    explicit Registrar(TestCase tc) { registry().push_back(tc); }
};

inline int& current_failures() {
    static int n = 0;
    return n;
}
inline int& total_checks() {
    static int n = 0;
    return n;
}

// Best-effort stringification so CHECK_EQ can show what it actually got.
template <typename T>
std::string show(const T& v) {
    if constexpr (std::is_same_v<T, bool>) {
        return v ? "true" : "false";
    } else if constexpr (std::is_convertible_v<T, std::string_view>) {
        return '"' + std::string(std::string_view(v)) + '"';
    } else if constexpr (std::is_integral_v<T>) {
        return std::to_string(v);
    } else if constexpr (std::is_floating_point_v<T>) {
        return std::to_string(v);
    } else if constexpr (std::is_pointer_v<T>) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%p", static_cast<const void*>(v));
        return buf;
    } else {
        return "<value>";
    }
}

inline void fail(const char* file, int line, const std::string& what) {
    ++current_failures();
    std::printf("      \033[31mFAILED\033[0m %s:%d\n        %s\n", file, line,
                what.c_str());
}

inline bool do_check(bool ok, const char* expr, const char* file, int line) {
    ++total_checks();
    if (!ok) fail(file, line, std::string("CHECK(") + expr + ")");
    return ok;
}

template <typename A, typename B>
bool do_check_eq(const A& a, const B& b, const char* expr, const char* file,
                 int line) {
    ++total_checks();
    if (a == b) return true;
    fail(file, line,
         std::string(expr) + "\n          lhs = " + show(a) +
             "\n          rhs = " + show(b));
    return false;
}

int run_all(int argc, char** argv);  // defined in test_main.cpp

}  // namespace neko_test

#define NEKO_TEST_CAT2(a, b) a##b
#define NEKO_TEST_CAT(a, b) NEKO_TEST_CAT2(a, b)

#define NEKO_TEST(NAME)                                                        \
    static void NAME();                                                        \
    static ::neko_test::Registrar NEKO_TEST_CAT(neko_reg_, NAME){              \
        ::neko_test::TestCase{#NAME, __FILE__, __LINE__, &NAME}};              \
    static void NAME()

#define CHECK(expr)                                                            \
    ::neko_test::do_check(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

#define REQUIRE(expr)                                                          \
    do {                                                                       \
        if (!::neko_test::do_check(static_cast<bool>(expr), #expr, __FILE__,   \
                                   __LINE__))                                  \
            throw ::neko_test::Abort{};                                        \
    } while (false)

#define CHECK_EQ(a, b)                                                         \
    ::neko_test::do_check_eq((a), (b), #a " == " #b, __FILE__, __LINE__)

#define REQUIRE_EQ(a, b)                                                       \
    do {                                                                       \
        if (!::neko_test::do_check_eq((a), (b), #a " == " #b, __FILE__,        \
                                      __LINE__))                               \
            throw ::neko_test::Abort{};                                        \
    } while (false)

#define CHECK_THROWS_AS(expr, EXC)                                             \
    do {                                                                       \
        ++::neko_test::total_checks();                                         \
        bool caught_ = false;                                                  \
        try {                                                                  \
            (void)(expr);                                                      \
        } catch (const EXC&) {                                                 \
            caught_ = true;                                                    \
        } catch (...) {}                                                       \
        if (!caught_)                                                          \
            ::neko_test::fail(__FILE__, __LINE__,                              \
                              "expected " #expr " to throw " #EXC);            \
    } while (false)

#define STATIC_CHECK(...) static_assert((__VA_ARGS__), #__VA_ARGS__)
