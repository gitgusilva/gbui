// A test harness in one header.
//
// gtest would be a build dependency for a library that otherwise has none, and
// what these tests need is a name, an assertion and a non-zero exit code.
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace gbui::test {

struct Case {
    std::string name;
    std::function<void()> body;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

inline int& failures() {
    static int count = 0;
    return count;
}

inline std::string& currentCase() {
    static std::string name;
    return name;
}

struct Registrar {
    Registrar(std::string name, std::function<void()> body) {
        registry().push_back({std::move(name), std::move(body)});
    }
};

inline void reportFailure(const char* file, int line, std::string_view expression,
                          const std::string& detail) {
    ++failures();
    std::fprintf(stderr, "  FAIL %s\n    %s:%d: %.*s\n", currentCase().c_str(), file, line,
                 static_cast<int>(expression.size()), expression.data());
    if (!detail.empty()) std::fprintf(stderr, "    %s\n", detail.c_str());
}

inline bool nearlyEqual(double a, double b, double tolerance = 0.01) {
    return std::fabs(a - b) <= tolerance;
}

inline int run() {
    int passed = 0;
    for (const auto& testCase : registry()) {
        currentCase() = testCase.name;
        const int before = failures();
        testCase.body();
        if (failures() == before) ++passed;
    }
    std::printf("%d/%zu passed\n", passed, registry().size());
    return failures() == 0 ? 0 : 1;
}

}  // namespace gbui::test

#define GBUI_CONCAT_INNER(a, b) a##b
#define GBUI_CONCAT(a, b) GBUI_CONCAT_INNER(a, b)

/** Declares a test case. The body runs at startup order; order is irrelevant
 *  because cases share no state. */
#define TEST(name)                                                                       \
    static void GBUI_CONCAT(test_body_, __LINE__)();                                     \
    static const ::gbui::test::Registrar GBUI_CONCAT(test_reg_, __LINE__){               \
        name, &GBUI_CONCAT(test_body_, __LINE__)};                                       \
    static void GBUI_CONCAT(test_body_, __LINE__)()

#define CHECK(expr)                                                                      \
    do {                                                                                 \
        if (!(expr)) ::gbui::test::reportFailure(__FILE__, __LINE__, #expr, {});          \
    } while (false)

#define CHECK_EQ(actual, expected)                                                       \
    do {                                                                                 \
        const auto gbui_actual = (actual);                                               \
        const auto gbui_expected = (expected);                                           \
        if (!(gbui_actual == gbui_expected)) {                                           \
            ::gbui::test::reportFailure(__FILE__, __LINE__, #actual " == " #expected,    \
                                        "got a value that differs from the expectation"); \
        }                                                                                \
    } while (false)

#define CHECK_NEAR(actual, expected)                                                     \
    do {                                                                                 \
        const double gbui_a = static_cast<double>(actual);                               \
        const double gbui_b = static_cast<double>(expected);                             \
        if (!::gbui::test::nearlyEqual(gbui_a, gbui_b)) {                                \
            char detail[128];                                                            \
            std::snprintf(detail, sizeof(detail), "expected %.3f, got %.3f", gbui_b,     \
                          gbui_a);                                                       \
            ::gbui::test::reportFailure(__FILE__, __LINE__, #actual " ~= " #expected,    \
                                        detail);                                         \
        }                                                                                \
    } while (false)
