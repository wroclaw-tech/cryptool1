#include "testing.hpp"

#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "secude_compat.h"

extern "C" int secude_compat_test_libec(void);

namespace testing {

namespace {
int failures = 0;
std::string g_scratch;
} // namespace

std::vector<Case> &registry()
{
    static std::vector<Case> r;
    return r;
}

void report_failure(const char *file, int line, const std::string &what)
{
    ++failures;
    std::fprintf(stderr, "  FAILED %s:%d: %s\n", file, line, what.c_str());
}

std::string scratch_dir() { return g_scratch; }

std::string hex(const void *p, size_t n)
{
    static const char d[] = "0123456789abcdef";
    std::string s;
    const unsigned char *b = static_cast<const unsigned char *>(p);
    for (size_t i = 0; i < n; ++i) {
        s += d[b[i] >> 4];
        s += d[b[i] & 15];
    }
    return s;
}

} // namespace testing

TEST(libec_binding)
{
    CHECK_EQ(secude_compat_test_libec(), 0);
}

int main(int argc, char **argv)
{
    char tmpl[] = "/tmp/secude_compat_tests.XXXXXX";
    const char *dir = mkdtemp(tmpl);
    if (!dir) {
        std::perror("mkdtemp");
        return 2;
    }
    testing::g_scratch = dir;
    std::printf("SECUDE compat tests (%s, legacy provider %s)\n", secude_compat_version(),
                secude_compat_legacy_provider_active() ? "active" : "not used");
    const char *only = argc > 1 ? argv[1] : nullptr;
    for (const auto &c : testing::registry()) {
        if (only && !std::strstr(c.name, only))
            continue;
        int before = testing::failures;
        c.fn();
        std::printf("%s %s\n", testing::failures == before ? "ok  " : "FAIL", c.name);
    }
    std::string cleanup = "rm -rf '" + testing::g_scratch + "'";
    if (std::system(cleanup.c_str()) != 0)
        std::fprintf(stderr, "could not remove %s\n", testing::g_scratch.c_str());
    std::printf("%d failure(s)\n", testing::failures);
    return testing::failures ? 1 : 0;
}
