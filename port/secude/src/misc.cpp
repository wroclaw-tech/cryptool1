// Files, randomness, licensing and version information.
#include "internal.hpp"

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <openssl/opensslv.h>
#include <openssl/rand.h>
#include <sys/stat.h>
#include <unistd.h>

namespace compat {

Bytes random_bytes(size_t n)
{
    Bytes b(n);
    if (n && RAND_bytes_ex(libctx(), b.data(), n, 0) != 1)
        fail(ERANDOM, "random number generator failed");
    return b;
}

std::string native_path(const std::string &path)
{
#ifdef _WIN32
    return path;
#else
    std::string p = path;
    if (p.size() >= 2 && p[1] == ':' && std::isalpha(static_cast<unsigned char>(p[0])))
        p.erase(0, 2);
    for (char &c : p)
        if (c == '\\')
            c = '/';
    return p;
#endif
}

Bytes read_file(const std::string &win_path, bool &exists)
{
    std::string path = native_path(win_path);
    exists = false;
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) {
        if (errno == ENOENT || errno == ENOTDIR)
            return {};
        fail(EREADFILE, "cannot open " + path + ": " + std::strerror(errno));
    }
    exists = true;
    Bytes data;
    uint8_t buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        data.insert(data.end(), buf, buf + n);
    bool error = std::ferror(f);
    std::fclose(f);
    if (error)
        fail(EREADFILE, "cannot read " + path);
    return data;
}

void write_file_atomic(const std::string &win_path, const Bytes &data, int mode)
{
    std::string path = native_path(win_path);
    std::string tmp = path + ".tmp" + std::to_string(::getpid());
    int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd < 0)
        fail(EWRITEFILE, "cannot create " + tmp + ": " + std::strerror(errno));
    size_t done = 0;
    while (done < data.size()) {
        ssize_t w = ::write(fd, data.data() + done, data.size() - done);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            ::close(fd);
            ::unlink(tmp.c_str());
            fail(EWRITEFILE, "cannot write " + tmp);
        }
        done += size_t(w);
    }
    if (::fsync(fd) != 0 || ::close(fd) != 0) {
        ::unlink(tmp.c_str());
        fail(EWRITEFILE, "cannot write " + tmp);
    }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        ::unlink(tmp.c_str());
        fail(EWRITEFILE, "cannot replace " + path + ": " + std::strerror(errno));
    }
}

std::string hex(const uint8_t *p, size_t n, bool upper)
{
    static const char *digits[2] = {"0123456789abcdef", "0123456789ABCDEF"};
    std::string s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) {
        s += digits[upper][p[i] >> 4];
        s += digits[upper][p[i] & 0xf];
    }
    return s;
}

} // namespace compat

using namespace compat;

namespace {

const char *version_string()
{
    static const std::string v = std::string("SECUDE compatibility layer 1.0 for CrypTool (") + OPENSSL_VERSION_TEXT + ")";
    return v.c_str();
}

} // namespace

extern "C" {

const char *secude_compat_version(void) { return version_string(); }

RC SECUDE_HasValidTicket(char **rcIdOrError)
{
    static char text[] = "no license ticket required";
    if (rcIdOrError)
        *rcIdOrError = text;
    return 0;
}

int light_version(void)
{
    return 0;
}

char *aux_sprint_version(char *string)
{
    return guarded<char *>("aux_sprint_version", nullptr, [&] { return append_string(string, version_string()); });
}

OctetString *aux_file2OctetString(char *fn)
{
    return guarded<OctetString *>("aux_file2OctetString", nullptr, [&] {
        if (!fn)
            fail(EINVALID, "missing file name");
        bool exists = false;
        Bytes data = read_file(fn, exists);
        if (!exists)
            fail(EFILENOTEXISTING, std::string("file ") + fn + " does not exist");
        return new_ostr(data);
    });
}

// flag 3 appends to the file, every other value creates or truncates it.
int aux_OctetString2file(OctetString *ostr, char *fn, int flag)
{
    return guarded<int>("aux_OctetString2file", -1, [&] {
        if (!ostr || !fn)
            fail(EINVALID, "missing parameter");
        FILE *f = std::fopen(native_path(fn).c_str(), flag == 3 ? "ab" : "wb");
        if (!f)
            fail(EWRITEFILE, std::string("cannot open ") + fn + ": " + std::strerror(errno));
        size_t n = ostr->noctets ? std::fwrite(ostr->octets, 1, ostr->noctets, f) : 0;
        bool ok = n == ostr->noctets;
        ok = std::fclose(f) == 0 && ok;
        if (!ok)
            fail(EWRITEFILE, std::string("cannot write ") + fn);
        return 0;
    });
}

OctetString *aux_latin1_to_unicode(char *latin_string, Boolean termination)
{
    return guarded<OctetString *>("aux_latin1_to_unicode", nullptr, [&] {
        if (!latin_string)
            fail(EINVALID, "missing string");
        Bytes b;
        for (const unsigned char *p = reinterpret_cast<const unsigned char *>(latin_string); *p; ++p) {
            b.push_back(0);
            b.push_back(*p);
        }
        if (termination) {
            b.push_back(0);
            b.push_back(0);
        }
        return new_ostr(b);
    });
}

} // extern "C"
