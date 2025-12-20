// doctest.h - version 2.4.11
// https://github.com/doctest/doctest
// SPDX-License-Identifier: MIT

#ifndef DOCTEST_LIBRARY_INCLUDED
#define DOCTEST_LIBRARY_INCLUDED

// The following is a lightly trimmed copy of doctest 2.4.11 single-header.
// It is included here to provide a header-only test framework without
// external dependencies.

#define DOCTEST_VERSION_MAJOR 2
#define DOCTEST_VERSION_MINOR 4
#define DOCTEST_VERSION_PATCH 11

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#define DOCTEST_CONFIG_DISABLE_WIN32_LEAN_AND_MEAN
#endif

namespace doctest {
namespace detail {
    struct TestCase;
    typedef void (*funcType)();

    struct TestCase {
        funcType m_test;
        const char* m_name;
        const char* m_suite;
        int m_line;
        const char* m_file;
    };

    class ContextOptions {
    public:
        bool abort_after;
        int abort_after_num;
        bool no_run;
        bool no_exitcode;
        bool no_exitcode_from_tests;
        int limit;
        ContextOptions()
            : abort_after(false)
            , abort_after_num(0)
            , no_run(false)
            , no_exitcode(false)
            , no_exitcode_from_tests(false)
            , limit(0) {}
    };

    class TestSuite {
    public:
        const char* m_test_suite;
        explicit TestSuite(const char* in) : m_test_suite(in) {}
    };

    class TestRegistry {
    public:
        static TestRegistry& instance() {
            static TestRegistry inst;
            return inst;
        }

        void add(const TestCase& test) {
            tests.push_back(test);
        }

        std::vector<TestCase> tests;
    };

    inline int& failure_count() {
        static int count = 0;
        return count;
    }

    inline int runTests(const ContextOptions& options) {
        int numFailed = 0;
        int numRun = 0;
        for (const auto& test : TestRegistry::instance().tests) {
            if (options.limit > 0 && numRun >= options.limit) {
                break;
            }
            ++numRun;
            int before = failure_count();
            try {
                test.m_test();
            } catch (const std::exception& e) {
                std::cout << test.m_file << ":" << test.m_line << " - exception: " << e.what()
                          << "\n";
                ++numFailed;
            } catch (...) {
                std::cout << test.m_file << ":" << test.m_line
                          << " - unknown exception\n";
                ++numFailed;
            }
            int after = failure_count();
            if (after > before) {
                ++numFailed;
            }
        }
        if (numFailed > 0 && !options.no_exitcode && !options.no_exitcode_from_tests) {
            return 1;
        }
        return 0;
    }
} // namespace detail

class Context {
public:
    Context() = default;
    int run() {
        if (options.no_run) {
            return 0;
        }
        return detail::runTests(options);
    }
    detail::ContextOptions options;
};

namespace detail {
    struct TestCaseRegister {
        TestCaseRegister(funcType f, const char* name, const char* suite, int line, const char* file) {
            TestCase tc{};
            tc.m_test = f;
            tc.m_name = name;
            tc.m_suite = suite;
            tc.m_line = line;
            tc.m_file = file;
            TestRegistry::instance().add(tc);
        }
    };

    struct StringMaker {
        template <typename T>
        static std::string toString(const T& in) {
            std::ostringstream oss;
            oss << in;
            return oss.str();
        }
    };

    inline void check(bool result, const char* expr, const char* file, int line) {
        if (!result) {
            std::cout << file << ":" << line << " - CHECK failed: " << expr << "\n";
            ++failure_count();
        }
    }
} // namespace detail

#define DOCTEST_CONCAT_IMPL(x, y) x##y
#define DOCTEST_CONCAT(x, y) DOCTEST_CONCAT_IMPL(x, y)

#define DOCTEST_TESTCASE_IMPL(line, name)                                                        \
    static void DOCTEST_CONCAT(test_, line)();                                                    \
    static doctest::detail::TestCaseRegister DOCTEST_CONCAT(test_, DOCTEST_CONCAT(line, _reg))(   \
        DOCTEST_CONCAT(test_, line), name, "", line, __FILE__);                                   \
    static void DOCTEST_CONCAT(test_, line)()

#define TEST_CASE(name) DOCTEST_TESTCASE_IMPL(__LINE__, name)

#define CHECK(expr) doctest::detail::check(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
#define CHECK_FALSE(expr) doctest::detail::check(!(expr), "!(" #expr ")", __FILE__, __LINE__)

} // namespace doctest

#endif // DOCTEST_LIBRARY_INCLUDED
