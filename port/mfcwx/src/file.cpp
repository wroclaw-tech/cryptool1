#include "afx.h"
#include "runtime.h"

#include <algorithm>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#undef fopen
#undef rename
#undef remove

namespace mfcwx {

std::string GetDataDirectory();

namespace {

#ifdef __APPLE__
inline const struct timespec& MTime(const struct stat& st) { return st.st_mtimespec; }
inline const struct timespec& ATime(const struct stat& st) { return st.st_atimespec; }
inline const struct timespec& BirthTime(const struct stat& st) { return st.st_birthtimespec; }
#else
inline const struct timespec& MTime(const struct stat& st) { return st.st_mtim; }
inline const struct timespec& ATime(const struct stat& st) { return st.st_atim; }
inline const struct timespec& BirthTime(const struct stat& st) { return st.st_ctim; }
#endif

std::string Join(const std::string& a, const std::string& b) {
    if (a.empty())
        return b;
    if (a.back() == '/')
        return a + b;
    return a + "/" + b;
}

bool Exists(const std::string& p) {
    struct stat st;
    return lstat(p.c_str(), &st) == 0;
}

std::vector<std::string> SplitComponents(const std::string& p) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= p.size()) {
        size_t slash = p.find('/', start);
        std::string part = p.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (!part.empty())
            parts.push_back(part);
        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }
    return parts;
}

std::string ResolveCase(const std::string& native) {
    bool absolute = native[0] == '/';
    std::vector<std::string> parts = SplitComponents(native);
    std::string out = absolute ? "/" : "";
    for (size_t i = 0; i < parts.size(); ++i) {
        const std::string& part = parts[i];
        std::string candidate = Join(out, part);
        if (part == "." || part == ".." || Exists(candidate)) {
            out = candidate;
            continue;
        }
        bool found = false;
        if (DIR* d = opendir(out.empty() ? "." : out.c_str())) {
            while (dirent* e = readdir(d)) {
                if (strcasecmp(e->d_name, part.c_str()) == 0) {
                    out = Join(out, e->d_name);
                    found = true;
                    break;
                }
            }
            closedir(d);
        }
        if (!found) {
            for (size_t j = i; j < parts.size(); ++j)
                out = Join(out, parts[j]);
            break;
        }
    }
    if (native.size() > 1 && native.back() == '/' && (out.empty() || out.back() != '/'))
        out += '/';
    return out;
}

std::string ResolveNative(const std::string& native) {
    if (native.empty() || Exists(native))
        return native;
    return ResolveCase(native);
}

std::string CurrentDirectory() {
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof buf))
        return buf;
    return "/";
}

std::string NormalizeNative(const std::string& native) {
    std::string full = !native.empty() && native[0] == '/' ? native : Join(CurrentDirectory(), native);
    std::vector<std::string> out;
    for (const std::string& part : SplitComponents(full)) {
        if (part == ".")
            continue;
        if (part == "..") {
            if (!out.empty())
                out.pop_back();
            continue;
        }
        out.push_back(part);
    }
    std::string result;
    for (const std::string& part : out)
        result += "/" + part;
    return result.empty() ? "/" : result;
}

DWORD CopyResult(const std::string& s, LPSTR buffer, DWORD size) {
    if (!buffer || size <= s.size())
        return static_cast<DWORD>(s.size() + 1);
    memcpy(buffer, s.c_str(), s.size() + 1);
    return static_cast<DWORD>(s.size());
}

DWORD AttributesFromStat(const struct stat& st, const std::string& path, const char* name) {
    DWORD attrs = 0;
    if (S_ISDIR(st.st_mode))
        attrs |= FILE_ATTRIBUTE_DIRECTORY;
    else
        attrs |= FILE_ATTRIBUTE_ARCHIVE;
    if (!(st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) || access(path.c_str(), W_OK) != 0)
        attrs |= FILE_ATTRIBUTE_READONLY;
    if (name && name[0] == '.' && strcmp(name, ".") != 0 && strcmp(name, "..") != 0)
        attrs |= FILE_ATTRIBUTE_HIDDEN;
    return attrs ? attrs : FILE_ATTRIBUTE_NORMAL;
}

bool MatchGlob(const char* name, const char* mask) {
    while (*mask) {
        if (*mask == '*') {
            while (*mask == '*')
                ++mask;
            if (!*mask)
                return true;
            for (const char* p = name; *p; ++p)
                if (MatchGlob(p, mask))
                    return true;
            return MatchGlob(name + strlen(name), mask);
        }
        if (!*name)
            return false;
        if (*mask != '?' && tolower(static_cast<unsigned char>(*mask)) != tolower(static_cast<unsigned char>(*name)))
            return false;
        ++mask;
        ++name;
    }
    return *name == 0;
}

bool MatchMask(const char* name, const std::string& mask) {
    if (mask == "*" || mask == "*.*")
        return true;
    if (MatchGlob(name, mask.c_str()))
        return true;
    // DOS semantics: "name.*" also matches "name"; "name." matches names without extension.
    if (mask.size() > 2 && mask.compare(mask.size() - 2, 2, ".*") == 0 && !strchr(name, '.'))
        return MatchGlob(name, mask.substr(0, mask.size() - 2).c_str());
    if (mask.size() > 1 && mask.back() == '.' && mask != "." && mask != ".." && !strchr(name, '.'))
        return MatchGlob(name, mask.substr(0, mask.size() - 1).c_str());
    return false;
}

struct FindObj {
    std::string dir;
    std::vector<std::string> names;
    size_t index = 0;
};

std::mutex& FindLock() {
    static std::mutex* m = new std::mutex;
    return *m;
}

std::set<FindObj*>& Finds() {
    static auto* s = new std::set<FindObj*>;
    return *s;
}

void FillFindData(const FindObj& f, const std::string& name, LPWIN32_FIND_DATA data) {
    memset(data, 0, sizeof *data);
    std::string path = Join(f.dir, name);
    struct stat st;
    if (stat(path.c_str(), &st) == 0 || lstat(path.c_str(), &st) == 0) {
        data->dwFileAttributes = AttributesFromStat(st, path, name.c_str());
        data->ftCreationTime = MakeFileTime(BirthTime(st).tv_sec, BirthTime(st).tv_nsec);
        data->ftLastAccessTime = MakeFileTime(ATime(st).tv_sec, ATime(st).tv_nsec);
        data->ftLastWriteTime = MakeFileTime(MTime(st).tv_sec, MTime(st).tv_nsec);
        if (!S_ISDIR(st.st_mode)) {
            data->nFileSizeHigh = static_cast<DWORD>(static_cast<ULONGLONG>(st.st_size) >> 32);
            data->nFileSizeLow = static_cast<DWORD>(st.st_size);
        }
    } else {
        data->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    }
    std::string ansi = Utf8ToAnsi(name.c_str());
    strncpy(data->cFileName, ansi.c_str(), sizeof data->cFileName - 1);
}

bool CopyFileContents(const std::string& src, const std::string& dst) {
    int in = open(src.c_str(), O_RDONLY | O_CLOEXEC);
    if (in < 0)
        return false;
    struct stat st;
    if (fstat(in, &st) != 0 || S_ISDIR(st.st_mode)) {
        int err = S_ISDIR(st.st_mode) ? EACCES : errno;
        close(in);
        errno = err;
        return false;
    }
    int out = open(dst.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, st.st_mode & 0777);
    if (out < 0) {
        int err = errno;
        close(in);
        errno = err;
        return false;
    }
    char buf[65536];
    bool ok = true;
    for (;;) {
        ssize_t n = read(in, buf, sizeof buf);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            ok = n == 0;
            break;
        }
        for (ssize_t done = 0; done < n;) {
            ssize_t w = write(out, buf + done, static_cast<size_t>(n - done));
            if (w < 0 && errno == EINTR)
                continue;
            if (w <= 0) {
                ok = false;
                break;
            }
            done += w;
        }
        if (!ok)
            break;
    }
    int err = errno;
    if (ok) {
        struct timespec times[2] = {ATime(st), MTime(st)};
        futimens(out, times);
        fchmod(out, st.st_mode & 07777);
    }
    close(in);
    if (close(out) != 0 && ok) {
        ok = false;
        err = errno;
    }
    errno = err;
    return ok;
}

} // namespace

std::string FsPath(const char* appPath) { return ResolveNative(NativePath(appPath)); }

std::string AbsoluteNativePath(const char* appPath) { return NormalizeNative(NativePath(appPath)); }

std::string FullAppPath(const char* appPath) { return AppPath(AbsoluteNativePath(appPath).c_str()); }

std::string ExecutablePath() {
    static const std::string path = []() {
#ifdef __APPLE__
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        std::string buf(size + 1, '\0');
        if (_NSGetExecutablePath(&buf[0], &size) != 0)
            return std::string();
        buf.resize(strlen(buf.c_str()));
        char real[PATH_MAX];
        return realpath(buf.c_str(), real) ? std::string(real) : buf;
#else
        char buf[PATH_MAX];
        ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
        if (n <= 0)
            return std::string();
        buf[n] = 0;
        return std::string(buf);
#endif
    }();
    return path;
}

std::string TempDirectory() {
    static std::mutex* m = new std::mutex;
    std::lock_guard<std::mutex> lk(*m);
    const char* env = getenv("TMPDIR");
    std::string base = env && *env ? env : "/tmp";
    while (base.size() > 1 && base.back() == '/')
        base.pop_back();
    std::string dir = base + "/cryptool1-" + std::to_string(static_cast<unsigned long>(getuid()));
    struct stat st;
    if (lstat(dir.c_str(), &st) != 0) {
        mkdir(dir.c_str(), 0700);
        if (lstat(dir.c_str(), &st) != 0)
            return base;
    }
    if (!S_ISDIR(st.st_mode) || st.st_uid != getuid())
        return base;
    return dir;
}

std::string UserConfigDirectory() {
    const char* home = getenv("HOME");
    std::string h;
    if (home && *home) {
        h = home;
    } else if (struct passwd* pw = getpwuid(getuid())) {
        h = pw->pw_dir;
    }
#ifdef __APPLE__
    return h + "/Library/Preferences";
#else
    const char* xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg == '/')
        return xdg;
    return h + "/.config";
#endif
}

DWORD ErrnoToWin32(int err) {
    switch (err) {
    case 0:
        return ERROR_SUCCESS;
    case ENOENT:
        return ERROR_FILE_NOT_FOUND;
    case ENOTDIR:
        return ERROR_PATH_NOT_FOUND;
    case EACCES:
    case EPERM:
    case EISDIR:
        return ERROR_ACCESS_DENIED;
    case EBADF:
        return ERROR_INVALID_HANDLE;
    case ENOMEM:
        return ERROR_NOT_ENOUGH_MEMORY;
    case EEXIST:
        return ERROR_ALREADY_EXISTS;
    case EMFILE:
    case ENFILE:
        return 4;
    case ENOSPC:
        return 112;
    case EINVAL:
        return 87;
    case ENOTEMPTY:
        return 145;
    case EXDEV:
        return 17;
    case EROFS:
        return 19;
    case EBUSY:
    case ETXTBSY:
        return 32;
    case ENAMETOOLONG:
        return 206;
    case ENOEXEC:
        return 193;
    case EIO:
        return 1117;
    case ESPIPE:
        return 131;
    default:
        return 31;
    }
}

void SetLastErrorFromErrno(int err) { ::SetLastError(ErrnoToWin32(err)); }

} // namespace mfcwx

using namespace mfcwx;

HANDLE CreateFileA(LPCSTR name, DWORD access, DWORD, LPSECURITY_ATTRIBUTES, DWORD disposition, DWORD flags, HANDLE) {
    if (!name || !*name) {
        SetLastError(ERROR_PATH_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }
    const DWORD readBits = 0x80000000u | 0x10000000u | FILE_READ_DATA;
    const DWORD writeBits = 0x40000000u | 0x10000000u | FILE_WRITE_DATA | FILE_APPEND_DATA;
    bool rd = (access & readBits) != 0;
    bool wr = (access & writeBits) != 0;
    int oflags = O_CLOEXEC | (rd && wr ? O_RDWR : wr ? O_WRONLY : O_RDONLY);
    if ((access & FILE_APPEND_DATA) && !(access & (0x40000000u | 0x10000000u | FILE_WRITE_DATA)))
        oflags |= O_APPEND;
    switch (disposition) {
    case CREATE_NEW:
        oflags |= O_CREAT | O_EXCL;
        break;
    case CREATE_ALWAYS:
        oflags |= O_CREAT | O_TRUNC;
        break;
    case OPEN_EXISTING:
        break;
    case OPEN_ALWAYS:
        oflags |= O_CREAT;
        break;
    case TRUNCATE_EXISTING:
        oflags |= O_TRUNC;
        break;
    default:
        SetLastError(87);
        return INVALID_HANDLE_VALUE;
    }
    std::string path = FsPath(name);
    struct stat st;
    bool existed = stat(path.c_str(), &st) == 0;
    if (existed && S_ISDIR(st.st_mode)) {
        SetLastError(ERROR_ACCESS_DENIED);
        return INVALID_HANDLE_VALUE;
    }
    int fd = open(path.c_str(), oflags, (flags & FILE_ATTRIBUTE_READONLY) ? 0444 : 0666);
    if (fd < 0) {
        SetLastError(errno == EEXIST ? 80 : ErrnoToWin32(errno));
        return INVALID_HANDLE_VALUE;
    }
    if (flags & 0x04000000u)
        unlink(path.c_str());
    HANDLE h = HandleFromFd(fd, path);
    SetLastError(existed && (disposition == CREATE_ALWAYS || disposition == OPEN_ALWAYS) ? ERROR_ALREADY_EXISTS : 0);
    return h;
}

BOOL ReadFile(HANDLE f, LPVOID buffer, DWORD toRead, LPDWORD readCount, LPOVERLAPPED ov) {
    if (readCount)
        *readCount = 0;
    int fd = FdFromHandle(f);
    if (fd < 0) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    char* p = static_cast<char*>(buffer);
    off_t offset = ov ? static_cast<off_t>((static_cast<ULONGLONG>(ov->OffsetHigh) << 32) | ov->Offset) : 0;
    DWORD total = 0;
    while (total < toRead) {
        ssize_t n = ov ? pread(fd, p + total, toRead - total, offset + total) : read(fd, p + total, toRead - total);
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0) {
            SetLastErrorFromErrno(errno);
            if (readCount)
                *readCount = total;
            return FALSE;
        }
        if (n == 0)
            break;
        total += static_cast<DWORD>(n);
        struct stat st;
        if (fstat(fd, &st) == 0 && !S_ISREG(st.st_mode))
            break;
    }
    if (readCount)
        *readCount = total;
    return TRUE;
}

BOOL WriteFile(HANDLE f, LPCVOID buffer, DWORD toWrite, LPDWORD written, LPOVERLAPPED ov) {
    if (written)
        *written = 0;
    int fd = FdFromHandle(f);
    if (fd < 0) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    const char* p = static_cast<const char*>(buffer);
    off_t offset = ov ? static_cast<off_t>((static_cast<ULONGLONG>(ov->OffsetHigh) << 32) | ov->Offset) : 0;
    DWORD total = 0;
    while (total < toWrite) {
        ssize_t n = ov ? pwrite(fd, p + total, toWrite - total, offset + total) : write(fd, p + total, toWrite - total);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            SetLastErrorFromErrno(n < 0 ? errno : ENOSPC);
            if (written)
                *written = total;
            return FALSE;
        }
        total += static_cast<DWORD>(n);
    }
    if (written)
        *written = total;
    return TRUE;
}

DWORD SetFilePointer(HANDLE f, LONG distance, PLONG distanceHigh, DWORD method) {
    int fd = FdFromHandle(f);
    if (fd < 0) {
        SetLastError(ERROR_INVALID_HANDLE);
        return INVALID_SET_FILE_POINTER;
    }
    LONGLONG off = distanceHigh
                       ? static_cast<LONGLONG>((static_cast<ULONGLONG>(static_cast<DWORD>(*distanceHigh)) << 32) |
                                               static_cast<DWORD>(distance))
                       : distance;
    int whence = method == FILE_CURRENT ? SEEK_CUR : method == FILE_END ? SEEK_END : SEEK_SET;
    off_t pos = lseek(fd, static_cast<off_t>(off), whence);
    if (pos < 0) {
        SetLastError(errno == EINVAL ? 131 : ErrnoToWin32(errno));
        return INVALID_SET_FILE_POINTER;
    }
    if (distanceHigh)
        *distanceHigh = static_cast<LONG>(static_cast<ULONGLONG>(pos) >> 32);
    SetLastError(0);
    return static_cast<DWORD>(pos);
}

BOOL SetEndOfFile(HANDLE f) {
    int fd = FdFromHandle(f);
    off_t pos = fd >= 0 ? lseek(fd, 0, SEEK_CUR) : -1;
    if (pos < 0 || ftruncate(fd, pos) != 0) {
        SetLastError(fd < 0 ? ERROR_INVALID_HANDLE : ErrnoToWin32(errno));
        return FALSE;
    }
    return TRUE;
}

DWORD GetFileSize(HANDLE f, LPDWORD high) {
    int fd = FdFromHandle(f);
    struct stat st;
    if (fd < 0 || fstat(fd, &st) != 0) {
        SetLastError(fd < 0 ? ERROR_INVALID_HANDLE : ErrnoToWin32(errno));
        return INVALID_FILE_SIZE;
    }
    if (high)
        *high = static_cast<DWORD>(static_cast<ULONGLONG>(st.st_size) >> 32);
    SetLastError(0);
    return static_cast<DWORD>(st.st_size);
}

BOOL GetFileSizeEx(HANDLE f, PLARGE_INTEGER size) {
    int fd = FdFromHandle(f);
    struct stat st;
    if (fd < 0 || fstat(fd, &st) != 0) {
        SetLastError(fd < 0 ? ERROR_INVALID_HANDLE : ErrnoToWin32(errno));
        return FALSE;
    }
    size->QuadPart = st.st_size;
    return TRUE;
}

BOOL FlushFileBuffers(HANDLE f) {
    int fd = FdFromHandle(f);
    if (fd < 0) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    fsync(fd);
    return TRUE;
}

BOOL GetFileTime(HANDLE f, LPFILETIME creation, LPFILETIME accessTime, LPFILETIME writeTime) {
    int fd = FdFromHandle(f);
    struct stat st;
    if (fd < 0 || fstat(fd, &st) != 0) {
        SetLastError(fd < 0 ? ERROR_INVALID_HANDLE : ErrnoToWin32(errno));
        return FALSE;
    }
    if (creation)
        *creation = MakeFileTime(BirthTime(st).tv_sec, BirthTime(st).tv_nsec);
    if (accessTime)
        *accessTime = MakeFileTime(ATime(st).tv_sec, ATime(st).tv_nsec);
    if (writeTime)
        *writeTime = MakeFileTime(MTime(st).tv_sec, MTime(st).tv_nsec);
    return TRUE;
}

BOOL DeleteFileA(LPCSTR name) {
    std::string path = FsPath(name);
    struct stat st;
    if (lstat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    if (unlink(path.c_str()) != 0) {
        SetLastErrorFromErrno(errno);
        return FALSE;
    }
    return TRUE;
}

BOOL CopyFileA(LPCSTR from, LPCSTR to, BOOL failIfExists) {
    std::string src = FsPath(from);
    std::string dst = FsPath(to);
    if (failIfExists && Exists(dst)) {
        SetLastError(80);
        return FALSE;
    }
    if (!CopyFileContents(src, dst)) {
        SetLastErrorFromErrno(errno);
        return FALSE;
    }
    return TRUE;
}

BOOL MoveFileA(LPCSTR from, LPCSTR to) { return MoveFileExA(from, to, 0); }

BOOL MoveFileExA(LPCSTR from, LPCSTR to, DWORD flags) {
    std::string src = FsPath(from);
    if (!to)
        return DeleteFileA(from);
    std::string dst = FsPath(to);
    struct stat a, b;
    if (lstat(src.c_str(), &a) != 0) {
        SetLastErrorFromErrno(errno);
        return FALSE;
    }
    if (!(flags & MOVEFILE_REPLACE_EXISTING) && lstat(dst.c_str(), &b) == 0 &&
        !(a.st_dev == b.st_dev && a.st_ino == b.st_ino)) {
        SetLastError(ERROR_ALREADY_EXISTS);
        return FALSE;
    }
    if (rename(src.c_str(), dst.c_str()) == 0)
        return TRUE;
    if (errno == EXDEV && (flags & MOVEFILE_COPY_ALLOWED) && !S_ISDIR(a.st_mode)) {
        if (CopyFileContents(src, dst) && unlink(src.c_str()) == 0)
            return TRUE;
    }
    SetLastErrorFromErrno(errno);
    return FALSE;
}

BOOL CreateDirectoryA(LPCSTR path, LPSECURITY_ATTRIBUTES) {
    if (mkdir(FsPath(path).c_str(), 0777) != 0) {
        SetLastError(errno == ENOENT ? ERROR_PATH_NOT_FOUND : ErrnoToWin32(errno));
        return FALSE;
    }
    return TRUE;
}

BOOL RemoveDirectoryA(LPCSTR path) {
    if (rmdir(FsPath(path).c_str()) != 0) {
        SetLastErrorFromErrno(errno);
        return FALSE;
    }
    return TRUE;
}

BOOL SetCurrentDirectoryA(LPCSTR path) {
    if (!path || chdir(FsPath(path).c_str()) != 0) {
        SetLastError(path ? ErrnoToWin32(errno) : 87);
        return FALSE;
    }
    return TRUE;
}

DWORD GetCurrentDirectoryA(DWORD size, LPSTR buffer) {
    return CopyResult(AppPath(CurrentDirectory().c_str()), buffer, size);
}

DWORD GetTempPathA(DWORD size, LPSTR buffer) {
    return CopyResult(AppPath(TempDirectory().c_str()) + "\\", buffer, size);
}

UINT GetTempFileNameA(LPCSTR path, LPCSTR prefix, UINT unique, LPSTR buffer) {
    std::string dir = path && *path ? path : ".";
    while (dir.size() > 1 && (dir.back() == '\\' || dir.back() == '/'))
        dir.pop_back();
    std::string pre = prefix ? std::string(prefix).substr(0, 3) : std::string();
    auto makeName = [&](UINT u) {
        char hex[16];
        snprintf(hex, sizeof hex, "%X", u & 0xFFFF);
        return dir + "\\" + pre + hex + ".tmp";
    };
    if (unique) {
        std::string name = makeName(unique);
        if (buffer)
            strncpy(buffer, name.c_str(), MAX_PATH - 1), buffer[MAX_PATH - 1] = 0;
        return unique & 0xFFFF;
    }
    UINT start = (static_cast<UINT>(GetTickCount()) ^ static_cast<UINT>(getpid()) * 2654435761u) & 0xFFFF;
    for (UINT i = 0; i < 0x10000; ++i) {
        UINT u = (start + i) & 0xFFFF;
        if (!u)
            continue;
        std::string name = makeName(u);
        int fd = open(FsPath(name.c_str()).c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
        if (fd >= 0) {
            close(fd);
            if (buffer)
                strncpy(buffer, name.c_str(), MAX_PATH - 1), buffer[MAX_PATH - 1] = 0;
            return u;
        }
        if (errno != EEXIST) {
            SetLastError(errno == ENOENT ? ERROR_PATH_NOT_FOUND : ErrnoToWin32(errno));
            return 0;
        }
    }
    SetLastError(80);
    return 0;
}

DWORD GetFileAttributesA(LPCSTR name) {
    std::string path = FsPath(name);
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        SetLastErrorFromErrno(errno);
        return INVALID_FILE_ATTRIBUTES;
    }
    size_t slash = path.rfind('/');
    return AttributesFromStat(st, path, path.c_str() + (slash == std::string::npos ? 0 : slash + 1));
}

BOOL SetFileAttributesA(LPCSTR name, DWORD attrs) {
    std::string path = FsPath(name);
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        SetLastErrorFromErrno(errno);
        return FALSE;
    }
    mode_t mode = st.st_mode & 07777;
    if (attrs & FILE_ATTRIBUTE_READONLY)
        mode &= ~static_cast<mode_t>(S_IWUSR | S_IWGRP | S_IWOTH);
    else
        mode |= S_IWUSR;
    if (mode != (st.st_mode & 07777) && chmod(path.c_str(), mode) != 0) {
        SetLastErrorFromErrno(errno);
        return FALSE;
    }
    return TRUE;
}

DWORD GetFullPathNameA(LPCSTR name, DWORD size, LPSTR buffer, LPSTR* filePart) {
    if (!name || !*name) {
        SetLastError(87);
        return 0;
    }
    std::string full = FullAppPath(name);
    char last = name[strlen(name) - 1];
    if ((last == '\\' || last == '/') && full.back() != '\\')
        full += '\\';
    DWORD r = CopyResult(full, buffer, size);
    if (filePart) {
        *filePart = nullptr;
        if (buffer && r < size) {
            char* slash = strrchr(buffer, '\\');
            if (slash && slash[1])
                *filePart = slash + 1;
        }
    }
    return r;
}

DWORD GetShortPathNameA(LPCSTR longPath, LPSTR shortPath, DWORD size) {
    if (!longPath || !Exists(FsPath(longPath))) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return 0;
    }
    std::string s = longPath;
    return CopyResult(s, shortPath, size);
}

DWORD GetLongPathNameA(LPCSTR shortPath, LPSTR longPath, DWORD size) { return GetShortPathNameA(shortPath, longPath, size); }

HANDLE FindFirstFileA(LPCSTR pattern, LPWIN32_FIND_DATA data) {
    std::string native = NativePath(pattern);
    size_t slash = native.rfind('/');
    std::string dir = slash == std::string::npos ? "." : (slash == 0 ? "/" : native.substr(0, slash));
    std::string mask = slash == std::string::npos ? native : native.substr(slash + 1);
    if (mask.empty()) {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }
    dir = ResolveNative(dir);
    DIR* d = opendir(dir.c_str());
    if (!d) {
        SetLastError(errno == ENOENT || errno == ENOTDIR ? ERROR_PATH_NOT_FOUND : ErrnoToWin32(errno));
        return INVALID_HANDLE_VALUE;
    }
    auto* f = new FindObj;
    f->dir = dir;
    while (dirent* e = readdir(d))
        if (MatchMask(e->d_name, mask))
            f->names.push_back(e->d_name);
    closedir(d);
    if (f->names.empty()) {
        delete f;
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }
    std::sort(f->names.begin(), f->names.end(),
              [](const std::string& a, const std::string& b) { return strcasecmp(a.c_str(), b.c_str()) < 0; });
    FillFindData(*f, f->names[0], data);
    f->index = 1;
    {
        std::lock_guard<std::mutex> lk(FindLock());
        Finds().insert(f);
    }
    SetLastError(0);
    return f;
}

BOOL FindNextFileA(HANDLE find, LPWIN32_FIND_DATA data) {
    std::lock_guard<std::mutex> lk(FindLock());
    auto* f = static_cast<FindObj*>(find);
    if (!Finds().count(f)) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (f->index >= f->names.size()) {
        SetLastError(ERROR_NO_MORE_FILES);
        return FALSE;
    }
    FillFindData(*f, f->names[f->index++], data);
    return TRUE;
}

BOOL FindClose(HANDLE find) {
    auto* f = static_cast<FindObj*>(find);
    {
        std::lock_guard<std::mutex> lk(FindLock());
        if (!Finds().erase(f)) {
            SetLastError(ERROR_INVALID_HANDLE);
            return FALSE;
        }
    }
    delete f;
    return TRUE;
}

UINT GetWindowsDirectoryA(LPSTR buffer, UINT size) {
    return CopyResult(AppPath(GetDataDirectory().c_str()), buffer, size);
}

UINT GetSystemDirectoryA(LPSTR buffer, UINT size) { return GetWindowsDirectoryA(buffer, size); }

UINT GetDriveTypeA(LPCSTR root) {
    if (root && *root && !Exists(FsPath(root)))
        return 1;
    return 3;
}

BOOL GetDiskFreeSpaceExA(LPCSTR dir, PULARGE_INTEGER freeToCaller, PULARGE_INTEGER total, PULARGE_INTEGER totalFree) {
    struct statvfs vfs;
    std::string path = dir && *dir ? FsPath(dir) : std::string(".");
    if (statvfs(path.c_str(), &vfs) != 0) {
        SetLastErrorFromErrno(errno);
        return FALSE;
    }
    ULONGLONG unit = vfs.f_frsize ? vfs.f_frsize : vfs.f_bsize;
    if (freeToCaller)
        freeToCaller->QuadPart = unit * vfs.f_bavail;
    if (total)
        total->QuadPart = unit * vfs.f_blocks;
    if (totalFree)
        totalFree->QuadPart = unit * vfs.f_bfree;
    return TRUE;
}
