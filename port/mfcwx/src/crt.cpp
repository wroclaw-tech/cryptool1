#include "afx.h"
#include "runtime.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <string>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

#undef fopen
#undef rename
#undef remove

using namespace mfcwx;

namespace {

char* FormatUnsigned(unsigned long long v, char* buf, int radix, bool negative) {
    if (!buf)
        return buf;
    if (radix < 2 || radix > 36) {
        buf[0] = 0;
        errno = EINVAL;
        return buf;
    }
    char tmp[72];
    int n = 0;
    do {
        int d = static_cast<int>(v % static_cast<unsigned>(radix));
        tmp[n++] = static_cast<char>(d < 10 ? '0' + d : 'a' + d - 10);
        v /= static_cast<unsigned>(radix);
    } while (v);
    char* p = buf;
    if (negative)
        *p++ = '-';
    while (n)
        *p++ = tmp[--n];
    *p = 0;
    return buf;
}

char* FormatSigned(long long value, char* buf, int radix, unsigned long long unsignedValue) {
    if (radix == 10 && value < 0)
        return FormatUnsigned(0ULL - static_cast<unsigned long long>(value), buf, 10, true);
    return FormatUnsigned(unsignedValue, buf, radix, false);
}

// MSVC's long is 32 bits: negative values print as 32-bit two's complement in non-decimal radixes.
unsigned long long LongBits(long value) {
    if (value < 0 && value >= INT32_MIN)
        return static_cast<uint32_t>(value);
    return static_cast<unsigned long long>(value);
}

int CopyChecked(const std::string& s, char* buf, size_t size) {
    if (!buf || size == 0)
        return EINVAL;
    if (s.size() + 1 > size) {
        buf[0] = 0;
        return ERANGE;
    }
    memcpy(buf, s.c_str(), s.size() + 1);
    return 0;
}

thread_local char t_cvtBuffer[400];

int FilterFopenMode(const char* mode, char* out, size_t size) {
    size_t o = 0;
    for (const char* p = mode ? mode : "r"; *p && *p != ',' && o + 1 < size; ++p)
        if (strchr("rwa+bx", *p))
            out[o++] = *p;
    out[o] = 0;
    return static_cast<int>(o);
}

void FillFindData(const WIN32_FIND_DATA& fd, struct _finddata_t* data) {
    memset(data, 0, sizeof *data);
    data->attrib = fd.dwFileAttributes & (_A_RDONLY | _A_HIDDEN | _A_SYSTEM | _A_SUBDIR | _A_ARCH);
    data->time_create = FileTimeToUnix(fd.ftCreationTime);
    data->time_access = FileTimeToUnix(fd.ftLastAccessTime);
    data->time_write = FileTimeToUnix(fd.ftLastWriteTime);
    data->size = (static_cast<unsigned long>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
    strncpy(data->name, fd.cFileName, sizeof data->name - 1);
}

} // namespace

int mfcwx_open(const char* name, int flags, ...) {
    mode_t mode = 0666;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        int requested = va_arg(ap, int);
        va_end(ap);
        if (requested & ~(S_IRUSR | S_IWUSR))
            mode = static_cast<mode_t>(requested);
        else if (!(requested & S_IWUSR))
            mode = 0444;
    }
    return open(FsPath(name).c_str(), flags, mode);
}

int mfcwx_unlink(const char* name) { return unlink(FsPath(name).c_str()); }

int mfcwx_rmdir(const char* name) { return rmdir(FsPath(name).c_str()); }

int mfcwx_chdir(const char* name) { return chdir(FsPath(name).c_str()); }

int mfcwx_access(const char* name, int mode) { return access(FsPath(name).c_str(), mode & 6); }

int mfcwx_chmod(const char* name, int mode) {
    std::string path = FsPath(name);
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return -1;
    mode_t m = st.st_mode & 07777;
    if (mode & S_IWUSR)
        m |= S_IWUSR;
    else
        m &= ~static_cast<mode_t>(S_IWUSR | S_IWGRP | S_IWOTH);
    return chmod(path.c_str(), m);
}

int mfcwx_rename(const char* from, const char* to) { return rename(FsPath(from).c_str(), FsPath(to).c_str()); }

FILE* mfcwx_fopen(const char* name, const char* mode) {
    if (!name) {
        errno = EINVAL;
        return nullptr;
    }
    char m[16];
    FilterFopenMode(mode, m, sizeof m);
    return fopen(FsPath(name).c_str(), m);
}

size_t mfcwx_msize(void* p) {
#ifdef __APPLE__
    return p ? malloc_size(p) : 0;
#else
    return p ? malloc_usable_size(p) : 0;
#endif
}

unsigned int mfcwx_rotl(unsigned int v, int s) {
    s &= 31;
    return s ? (v << s) | (v >> (32 - s)) : v;
}

unsigned int mfcwx_rotr(unsigned int v, int s) {
    s &= 31;
    return s ? (v >> s) | (v << (32 - s)) : v;
}

void mfcwx_normalize_path(char* path) {
    if (!path)
        return;
    for (char* p = path; *p; ++p)
        if (*p == '\\')
            *p = '/';
    if (path[0] && path[1] == ':' && isalpha(static_cast<unsigned char>(path[0])))
        memmove(path, path + 2, strlen(path + 2) + 1);
}

char* mfcwx_getcwd(char* buf, int size) {
    char native[PATH_MAX];
    if (!getcwd(native, sizeof native))
        return nullptr;
    std::string app = AppPath(native);
    if (!buf) {
        size_t bytes = std::max(static_cast<size_t>(size > 0 ? size : 0), app.size() + 1);
        buf = static_cast<char*>(malloc(bytes));
        if (!buf) {
            errno = ENOMEM;
            return nullptr;
        }
    } else if (size <= 0 || app.size() + 1 > static_cast<size_t>(size)) {
        errno = ERANGE;
        return nullptr;
    }
    memcpy(buf, app.c_str(), app.size() + 1);
    return buf;
}

int _mkdir(const char* path) { return mkdir(FsPath(path).c_str(), 0777); }

char* _strupr(char* s) {
    for (char* p = s; p && *p; ++p)
        if (*p >= 'a' && *p <= 'z')
            *p = static_cast<char>(*p - 'a' + 'A');
    return s;
}

char* _strlwr(char* s) {
    for (char* p = s; p && *p; ++p)
        if (*p >= 'A' && *p <= 'Z')
            *p = static_cast<char>(*p - 'A' + 'a');
    return s;
}

char* _strrev(char* s) {
    if (s)
        std::reverse(s, s + strlen(s));
    return s;
}

char* strupr(char* s) { return _strupr(s); }

char* strlwr(char* s) { return _strlwr(s); }

char* strrev(char* s) { return _strrev(s); }

char* _strset(char* s, int c) {
    for (char* p = s; p && *p; ++p)
        *p = static_cast<char>(c);
    return s;
}

char* _strnset(char* s, int c, size_t n) {
    for (char* p = s; p && *p && n; ++p, --n)
        *p = static_cast<char>(c);
    return s;
}

char* _itoa(int value, char* buf, int radix) { return FormatSigned(value, buf, radix, static_cast<unsigned>(value)); }

char* _ltoa(long value, char* buf, int radix) { return FormatSigned(value, buf, radix, LongBits(value)); }

char* _ultoa(unsigned long value, char* buf, int radix) { return FormatUnsigned(value, buf, radix, false); }

char* _i64toa(long long value, char* buf, int radix) {
    return FormatSigned(value, buf, radix, static_cast<unsigned long long>(value));
}

char* _ui64toa(unsigned long long value, char* buf, int radix) { return FormatUnsigned(value, buf, radix, false); }

char* itoa(int value, char* buf, int radix) { return _itoa(value, buf, radix); }

char* ltoa(long value, char* buf, int radix) { return _ltoa(value, buf, radix); }

char* ultoa(unsigned long value, char* buf, int radix) { return _ultoa(value, buf, radix); }

char* _gcvt(double value, int digits, char* buf) {
    if (!buf)
        return buf;
    if (digits < 1)
        digits = 1;
    char tmp[400];
    snprintf(tmp, sizeof tmp, "%.*g", digits, value);
    std::string s = tmp;
    if (std::isfinite(value) && s.find('.') == std::string::npos) {
        size_t e = s.find('e');
        if (e == std::string::npos)
            s += '.';
        else
            s.insert(e, ".");
    }
    strcpy(buf, s.c_str());
    return buf;
}

char* gcvt(double value, int digits, char* buf) { return _gcvt(value, digits, buf); }

char* _ecvt(double value, int digits, int* dec, int* sign) {
    char* out = t_cvtBuffer;
    int dummyDec, dummySign;
    if (!dec)
        dec = &dummyDec;
    if (!sign)
        sign = &dummySign;
    *sign = std::signbit(value) ? 1 : 0;
    value = std::fabs(value);
    if (digits > 340)
        digits = 340;
    if (!std::isfinite(value)) {
        strcpy(out, std::isnan(value) ? "1#QNAN" : "1#INF");
        *dec = 1;
        return out;
    }
    if (digits <= 0) {
        out[0] = 0;
        *dec = value == 0 ? 0 : static_cast<int>(std::floor(std::log10(value))) + 1;
        return out;
    }
    if (value == 0) {
        memset(out, '0', static_cast<size_t>(digits));
        out[digits] = 0;
        *dec = 0;
        return out;
    }
    char tmp[400];
    snprintf(tmp, sizeof tmp, "%.*e", digits - 1, value);
    int o = 0;
    const char* p = tmp;
    for (; *p && *p != 'e'; ++p)
        if (*p != '.')
            out[o++] = *p;
    out[o] = 0;
    *dec = (*p == 'e' ? atoi(p + 1) : 0) + 1;
    return out;
}

char* _fcvt(double value, int digits, int* dec, int* sign) {
    char* out = t_cvtBuffer;
    int dummyDec, dummySign;
    if (!dec)
        dec = &dummyDec;
    if (!sign)
        sign = &dummySign;
    *sign = std::signbit(value) ? 1 : 0;
    value = std::fabs(value);
    if (digits < 0)
        digits = 0;
    if (!std::isfinite(value)) {
        strcpy(out, std::isnan(value) ? "1#QNAN" : "1#INF");
        *dec = 1;
        return out;
    }
    char tmp[400];
    if (value >= 1e300) {
        snprintf(tmp, sizeof tmp, "%.0f", value);
        digits = 0;
    } else {
        snprintf(tmp, sizeof tmp, "%.*f", std::min(digits, 60), value);
    }
    std::string s = tmp;
    size_t point = s.find('.');
    std::string intPart = s.substr(0, point);
    std::string frac = point == std::string::npos ? std::string() : s.substr(point + 1);
    std::string all = intPart + frac;
    int d = static_cast<int>(intPart.size());
    size_t lead = 0;
    while (lead < all.size() && all[lead] == '0')
        ++lead;
    if (lead == all.size()) {
        std::string zeros(static_cast<size_t>(std::min(digits, 60)), '0');
        strcpy(out, zeros.c_str());
        *dec = 0;
        return out;
    }
    all.erase(0, lead);
    d -= static_cast<int>(lead);
    strncpy(out, all.c_str(), sizeof t_cvtBuffer - 1);
    out[sizeof t_cvtBuffer - 1] = 0;
    *dec = d;
    return out;
}

long _filelength(int fd) {
    struct stat st;
    return fstat(fd, &st) == 0 ? static_cast<long>(st.st_size) : -1L;
}

long filelength(int fd) { return _filelength(fd); }

long long _filelengthi64(int fd) {
    struct stat st;
    return fstat(fd, &st) == 0 ? static_cast<long long>(st.st_size) : -1LL;
}

int _chsize(int fd, long size) { return ftruncate(fd, size); }

int _eof(int fd) {
    struct stat st;
    off_t pos = lseek(fd, 0, SEEK_CUR);
    if (pos < 0 || fstat(fd, &st) != 0)
        return -1;
    return pos >= st.st_size ? 1 : 0;
}

int _setmode(int fd, int) { return fcntl(fd, F_GETFL) == -1 ? -1 : 0x8000; }

void _splitpath(const char* path, char* drive, char* dir, char* fname, char* ext) {
    std::string p = path ? path : "";
    std::string d, rest;
    if (p.size() >= 2 && p[1] == ':') {
        d = p.substr(0, 2);
        p.erase(0, 2);
    }
    size_t sep = p.find_last_of("\\/");
    std::string directory = sep == std::string::npos ? std::string() : p.substr(0, sep + 1);
    rest = sep == std::string::npos ? p : p.substr(sep + 1);
    size_t dot = rest.rfind('.');
    std::string name = dot == std::string::npos ? rest : rest.substr(0, dot);
    std::string extension = dot == std::string::npos ? std::string() : rest.substr(dot);
    auto put = [](char* out, const std::string& s, size_t max) {
        if (!out)
            return;
        size_t n = std::min(s.size(), max - 1);
        memcpy(out, s.data(), n);
        out[n] = 0;
    };
    put(drive, d, _MAX_DRIVE);
    put(dir, directory, _MAX_DIR);
    put(fname, name, _MAX_FNAME);
    put(ext, extension, _MAX_EXT);
}

void _makepath(char* path, const char* drive, const char* dir, const char* fname, const char* ext) {
    if (!path)
        return;
    std::string out;
    if (drive && *drive) {
        out += drive[0];
        out += ':';
    }
    if (dir && *dir) {
        out += dir;
        if (out.back() != '\\' && out.back() != '/')
            out += '\\';
    }
    if (fname)
        out += fname;
    if (ext && *ext) {
        if (ext[0] != '.')
            out += '.';
        out += ext;
    }
    size_t n = std::min(out.size(), static_cast<size_t>(_MAX_PATH - 1));
    memcpy(path, out.data(), n);
    path[n] = 0;
}

char* _fullpath(char* abs, const char* rel, size_t size) {
    std::string full = FullAppPath(rel && *rel ? rel : ".");
    if (rel && *rel) {
        char last = rel[strlen(rel) - 1];
        if ((last == '\\' || last == '/') && full.back() != '\\')
            full += '\\';
    }
    if (!abs) {
        abs = static_cast<char*>(malloc(std::max(static_cast<size_t>(_MAX_PATH), full.size() + 1)));
        if (!abs) {
            errno = ENOMEM;
            return nullptr;
        }
    } else if (full.size() + 1 > size) {
        errno = ERANGE;
        return nullptr;
    }
    memcpy(abs, full.c_str(), full.size() + 1);
    return abs;
}

int _getdrive(void) { return 3; }

int _chdrive(int drive) { return drive >= 1 && drive <= 26 ? 0 : -1; }

char* _mktemp(char* templ) {
    if (!templ)
        return nullptr;
    size_t len = strlen(templ);
    if (len < 6 || strcmp(templ + len - 6, "XXXXXX") != 0) {
        errno = EINVAL;
        return nullptr;
    }
    char* x = templ + len - 6;
    char digits[8];
    snprintf(digits, sizeof digits, "%05u", static_cast<unsigned>(getpid() % 100000));
    for (char c = 'a'; c <= 'z'; ++c) {
        x[0] = c;
        memcpy(x + 1, digits, 5);
        if (access(FsPath(templ).c_str(), F_OK) != 0)
            return templ;
    }
    memcpy(x, "XXXXXX", 6);
    errno = EEXIST;
    return nullptr;
}

char* _tempnam(const char* dir, const char* prefix) {
    std::string base;
    struct stat st;
    if (dir && *dir && stat(FsPath(dir).c_str(), &st) == 0 && S_ISDIR(st.st_mode))
        base = dir;
    else
        base = AppPath(TempDirectory().c_str());
    while (base.size() > 1 && (base.back() == '\\' || base.back() == '/'))
        base.pop_back();
    static std::atomic<unsigned> counter{1};
    for (int attempt = 0; attempt < 100000; ++attempt) {
        unsigned n = counter.fetch_add(1);
        std::string name = base + "\\" + (prefix ? prefix : "") + std::to_string(n);
        if (access(FsPath(name.c_str()).c_str(), F_OK) != 0)
            return strdup(name.c_str());
    }
    errno = EEXIST;
    return nullptr;
}

int _vsnprintf_s(char* buf, size_t size, size_t count, const char* fmt, va_list args) {
    if (!buf || size == 0 || !fmt) {
        errno = EINVAL;
        return -1;
    }
    char translated[4096];
    fmt = AfxTranslateFormat(fmt, translated, sizeof translated);
    va_list copy;
    va_copy(copy, args);
    int n = vsnprintf(nullptr, 0, fmt, copy);
    va_end(copy);
    if (n < 0) {
        buf[0] = 0;
        return -1;
    }
    size_t needed = static_cast<size_t>(n);
    if (count == static_cast<size_t>(-1)) {
        vsnprintf(buf, size, fmt, args);
        return needed >= size ? -1 : n;
    }
    size_t limit = std::min(count, size - 1);
    if (needed <= limit) {
        vsnprintf(buf, size, fmt, args);
        return n;
    }
    if (count < size) {
        std::string tmp(needed + 1, '\0');
        vsnprintf(&tmp[0], tmp.size(), fmt, args);
        memcpy(buf, tmp.data(), count);
        buf[count] = 0;
        return -1;
    }
    buf[0] = 0;
    errno = ERANGE;
    return -1;
}

int vsprintf_s(char* buf, size_t size, const char* fmt, va_list args) {
    if (!buf || size == 0 || !fmt) {
        errno = EINVAL;
        return -1;
    }
    char translated[4096];
    fmt = AfxTranslateFormat(fmt, translated, sizeof translated);
    int n = vsnprintf(buf, size, fmt, args);
    if (n < 0 || static_cast<size_t>(n) >= size) {
        buf[0] = 0;
        errno = ERANGE;
        return -1;
    }
    return n;
}

int sprintf_s(char* buf, size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = vsprintf_s(buf, size, fmt, args);
    va_end(args);
    return n;
}

int _snprintf_s(char* buf, size_t size, size_t count, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = _vsnprintf_s(buf, size, count, fmt, args);
    va_end(args);
    return n;
}

int strcpy_s(char* dst, size_t size, const char* src) {
    if (!dst || size == 0)
        return EINVAL;
    if (!src) {
        dst[0] = 0;
        return EINVAL;
    }
    return CopyChecked(src, dst, size);
}

int strncpy_s(char* dst, size_t size, const char* src, size_t count) {
    if (!dst || size == 0)
        return EINVAL;
    if (!src) {
        dst[0] = 0;
        return count ? EINVAL : 0;
    }
    size_t len = strlen(src);
    if (count == static_cast<size_t>(-1)) {
        size_t n = std::min(len, size - 1);
        memcpy(dst, src, n);
        dst[n] = 0;
        return n < len ? 80 : 0;
    }
    size_t n = std::min(len, count);
    if (n >= size) {
        dst[0] = 0;
        return ERANGE;
    }
    memcpy(dst, src, n);
    dst[n] = 0;
    return 0;
}

int strcat_s(char* dst, size_t size, const char* src) {
    if (!dst || size == 0)
        return EINVAL;
    size_t len = strnlen(dst, size);
    if (len == size || !src) {
        dst[0] = 0;
        return EINVAL;
    }
    if (len + strlen(src) >= size) {
        dst[0] = 0;
        return ERANGE;
    }
    strcpy(dst + len, src);
    return 0;
}

int strncat_s(char* dst, size_t size, const char* src, size_t count) {
    if (!dst || size == 0)
        return EINVAL;
    size_t len = strnlen(dst, size);
    if (len == size || (!src && count)) {
        dst[0] = 0;
        return EINVAL;
    }
    size_t add = src ? strlen(src) : 0;
    if (count != static_cast<size_t>(-1))
        add = std::min(add, count);
    bool truncated = false;
    if (len + add >= size) {
        if (count != static_cast<size_t>(-1)) {
            dst[0] = 0;
            return ERANGE;
        }
        add = size - 1 - len;
        truncated = true;
    }
    memcpy(dst + len, src, add);
    dst[len + add] = 0;
    return truncated ? 80 : 0;
}

char* strtok_s(char* s, const char* delim, char** ctx) { return strtok_r(s, delim, ctx); }

int _strupr_s(char* s, size_t size) {
    if (!s || strnlen(s, size) == size)
        return EINVAL;
    _strupr(s);
    return 0;
}

int _strlwr_s(char* s, size_t size) {
    if (!s || strnlen(s, size) == size)
        return EINVAL;
    _strlwr(s);
    return 0;
}

int _itoa_s(int value, char* buf, size_t size, int radix) {
    char tmp[72];
    _itoa(value, tmp, radix);
    return CopyChecked(tmp, buf, size);
}

int _ltoa_s(long value, char* buf, size_t size, int radix) {
    char tmp[72];
    _ltoa(value, tmp, radix);
    return CopyChecked(tmp, buf, size);
}

int _i64toa_s(long long value, char* buf, size_t size, int radix) {
    char tmp[72];
    _i64toa(value, tmp, radix);
    return CopyChecked(tmp, buf, size);
}

int fopen_s(FILE** f, const char* name, const char* mode) {
    if (!f)
        return EINVAL;
    *f = mfcwx_fopen(name, mode);
    return *f ? 0 : errno;
}

int memcpy_s(void* dst, size_t dstSize, const void* src, size_t count) {
    if (count == 0)
        return 0;
    if (!dst)
        return EINVAL;
    if (!src || count > dstSize) {
        memset(dst, 0, dstSize);
        return src ? ERANGE : EINVAL;
    }
    memcpy(dst, src, count);
    return 0;
}

int memmove_s(void* dst, size_t dstSize, const void* src, size_t count) {
    if (count == 0)
        return 0;
    if (!dst || !src)
        return EINVAL;
    if (count > dstSize)
        return ERANGE;
    memmove(dst, src, count);
    return 0;
}

int localtime_s(struct tm* out, const time_t* t) {
    if (!out || !t)
        return EINVAL;
    return localtime_r(t, out) ? 0 : EINVAL;
}

int gmtime_s(struct tm* out, const time_t* t) {
    if (!out || !t)
        return EINVAL;
    return gmtime_r(t, out) ? 0 : EINVAL;
}

int asctime_s(char* buf, size_t size, const struct tm* t) {
    if (!buf || !t)
        return EINVAL;
    char tmp[64];
    if (!asctime_r(t, tmp))
        return EINVAL;
    return CopyChecked(tmp, buf, size);
}

int ctime_s(char* buf, size_t size, const time_t* t) {
    if (!buf || !t)
        return EINVAL;
    struct tm tm;
    if (!localtime_r(t, &tm))
        return EINVAL;
    return asctime_s(buf, size, &tm);
}

int _dupenv_s(char** buf, size_t* len, const char* name) {
    if (!buf || !name)
        return EINVAL;
    const char* v = getenv(name);
    if (!v) {
        *buf = nullptr;
        if (len)
            *len = 0;
        return 0;
    }
    *buf = strdup(v);
    if (len)
        *len = strlen(v) + 1;
    return *buf ? 0 : ENOMEM;
}

char* _strdate(char* buf) {
    time_t now = time(nullptr);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(buf, 9, "%m/%d/%y", &tm);
    return buf;
}

char* _strtime(char* buf) {
    time_t now = time(nullptr);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(buf, 9, "%H:%M:%S", &tm);
    return buf;
}

void _tzset(void) { tzset(); }

intptr_t _findfirst(const char* pattern, struct _finddata_t* data) {
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        errno = ENOENT;
        return -1;
    }
    if (data)
        FillFindData(fd, data);
    return reinterpret_cast<intptr_t>(h);
}

int _findnext(intptr_t handle, struct _finddata_t* data) {
    WIN32_FIND_DATA fd;
    if (!FindNextFileA(reinterpret_cast<HANDLE>(handle), &fd)) {
        errno = ENOENT;
        return -1;
    }
    if (data)
        FillFindData(fd, data);
    return 0;
}

int _findclose(intptr_t handle) {
    if (!FindClose(reinterpret_cast<HANDLE>(handle))) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}
