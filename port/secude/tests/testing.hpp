#pragma once

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace testing {

struct Case {
    const char *name;
    std::function<void()> fn;
};

std::vector<Case> &registry();
void report_failure(const char *file, int line, const std::string &what);
std::string scratch_dir();

struct Register {
    Register(const char *name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

std::string hex(const void *p, size_t n);

} // namespace testing

#define TEST_CONCAT2(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT2(a, b)
#define TEST(name)                                                                       \
    static void name();                                                                  \
    static testing::Register TEST_CONCAT(register_, name)(#name, name);                  \
    static void name()

#define CHECK(cond)                                                                      \
    do {                                                                                 \
        if (!(cond))                                                                     \
            testing::report_failure(__FILE__, __LINE__, "CHECK(" #cond ")");             \
    } while (0)

#define CHECK_EQ(a, b)                                                                   \
    do {                                                                                 \
        auto va_ = (a);                                                                  \
        auto vb_ = (b);                                                                  \
        if (!(va_ == vb_))                                                               \
            testing::report_failure(__FILE__, __LINE__, "CHECK_EQ(" #a ", " #b ")");     \
    } while (0)

#define CHECK_HEX(ptr, len, expected)                                                    \
    do {                                                                                 \
        std::string got_ = testing::hex((ptr), (len));                                   \
        if (got_ != (expected))                                                          \
            testing::report_failure(__FILE__, __LINE__,                                  \
                                    std::string(#ptr " = ") + got_ + ", expected " + (expected)); \
    } while (0)
