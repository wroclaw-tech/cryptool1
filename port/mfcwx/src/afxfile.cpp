#include "afx.h"
#include "runtime.h"

#include <algorithm>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#undef fopen
#undef rename
#undef remove

using namespace mfcwx;

namespace {

#ifdef __APPLE__
inline time_t BirthSeconds(const struct stat& st) { return st.st_birthtimespec.tv_sec; }
#else
inline time_t BirthSeconds(const struct stat& st) { return st.st_ctim.tv_sec; }
#endif

CString FileNamePart(const CString& path) {
    const char* p = path.GetString();
    const char* last = p;
    for (const char* q = p; *q; ++q)
        if (*q == '\\' || *q == '/' || *q == ':')
            last = q + 1;
    return CString(last);
}

CString StripExtension(const CString& name) {
    int dot = name.ReverseFind('.');
    return dot > 0 ? name.Left(dot) : name;
}

void FillStatus(const struct stat& st, const std::string& native, CFileStatus& rStatus) {
    rStatus.m_ctime = BirthSeconds(st);
    rStatus.m_atime = st.st_atime;
    rStatus.m_mtime = st.st_mtime;
    rStatus.m_size = static_cast<ULONGLONG>(st.st_size);
    BYTE attr = 0;
    if (S_ISDIR(st.st_mode))
        attr |= CFile::directory;
    else
        attr |= CFile::archive;
    if (access(native.c_str(), W_OK) != 0)
        attr |= CFile::readOnly;
    size_t slash = native.rfind('/');
    const char* base = native.c_str() + (slash == std::string::npos ? 0 : slash + 1);
    if (base[0] == '.' && strcmp(base, ".") != 0 && strcmp(base, "..") != 0)
        attr |= CFile::hidden;
    rStatus.m_attribute = attr;
}

void ThrowArchive(int cause, CFile* file) {
    AfxThrowArchiveException(cause, file ? file->GetFilePath().GetString() : nullptr);
}

constexpr WORD kNullTag = 0;
constexpr WORD kNewClassTag = 0xFFFF;
constexpr WORD kClassTag = 0x8000;
constexpr DWORD kBigClassTag = 0x80000000u;
constexpr WORD kBigObjectTag = 0x7FFF;

} // namespace

// ---------------------------------------------------------------------------------------------
// CFile

IMPLEMENT_DYNAMIC(CFile, CObject)
IMPLEMENT_DYNAMIC(CStdioFile, CFile)
IMPLEMENT_DYNAMIC(CMemFile, CFile)
IMPLEMENT_DYNAMIC(CFileFind, CObject)

const HANDLE CFile::hFileNull = INVALID_HANDLE_VALUE;

CFile::CFile() : m_hFile(hFileNull), m_bCloseOnDelete(FALSE), m_fd(-1) {}

CFile::CFile(HANDLE hFile) : m_hFile(hFile), m_bCloseOnDelete(FALSE), m_fd(FdFromHandle(hFile)) {}

CFile::CFile(const char* lpszFileName, UINT nOpenFlags) : CFile() {
    CFileException e;
    if (!Open(lpszFileName, nOpenFlags, &e))
        AfxThrowFileException(e.m_cause, e.m_lOsError, e.m_strFileName);
}

CFile::~CFile() {
    if (m_hFile != hFileNull && m_bCloseOnDelete)
        Abort();
}

BOOL CFile::Open(const char* lpszFileName, UINT nOpenFlags, CFileException* pError) {
    if (m_hFile != hFileNull)
        Abort();
    auto fail = [&](int err, int cause) {
        if (pError) {
            pError->m_cause = cause;
            pError->m_lOsError = static_cast<LONG>(ErrnoToWin32(err));
            pError->m_strFileName = lpszFileName;
        }
        return FALSE;
    };
    if (!lpszFileName || !*lpszFileName)
        return fail(ENOENT, CFileException::badPath);
    int oflags = O_CLOEXEC;
    switch (nOpenFlags & 3) {
    case modeWrite:
        oflags |= O_WRONLY;
        break;
    case modeReadWrite:
        oflags |= O_RDWR;
        break;
    default:
        oflags |= O_RDONLY;
        break;
    }
    if (nOpenFlags & modeCreate) {
        oflags |= O_CREAT;
        if (!(nOpenFlags & modeNoTruncate))
            oflags |= O_TRUNC;
    }
    std::string path = FsPath(lpszFileName);
    struct stat st;
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
        return fail(EACCES, CFileException::accessDenied);
    int fd = open(path.c_str(), oflags, 0666);
    if (fd < 0) {
        int err = errno;
        return fail(err, CFileException::ErrnoToException(err));
    }
    m_strFileName = FullAppPath(lpszFileName).c_str();
    m_fd = fd;
    m_hFile = HandleFromFd(fd, path, true);
    m_bCloseOnDelete = TRUE;
    return TRUE;
}

void CFile::Close() {
    if (m_hFile != hFileNull)
        CloseHandle(m_hFile);
    m_hFile = hFileNull;
    m_fd = -1;
    m_bCloseOnDelete = FALSE;
    m_strFileName.Empty();
}

void CFile::Abort() {
    if (m_hFile != hFileNull)
        CloseHandle(m_hFile);
    m_hFile = hFileNull;
    m_fd = -1;
    m_bCloseOnDelete = FALSE;
    m_strFileName.Empty();
}

UINT CFile::Read(void* lpBuf, UINT nCount) {
    if (nCount == 0)
        return 0;
    if (m_fd < 0)
        AfxThrowFileException(CFileException::invalidFile, ERROR_INVALID_HANDLE, m_strFileName);
    char* p = static_cast<char*>(lpBuf);
    UINT total = 0;
    while (total < nCount) {
        ssize_t n = read(m_fd, p + total, nCount - total);
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0)
            CFileException::ThrowErrno(errno, m_strFileName);
        if (n == 0)
            break;
        total += static_cast<UINT>(n);
    }
    return total;
}

void CFile::Write(const void* lpBuf, UINT nCount) {
    if (nCount == 0)
        return;
    if (m_fd < 0)
        AfxThrowFileException(CFileException::invalidFile, ERROR_INVALID_HANDLE, m_strFileName);
    const char* p = static_cast<const char*>(lpBuf);
    UINT total = 0;
    while (total < nCount) {
        ssize_t n = write(m_fd, p + total, nCount - total);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            CFileException::ThrowErrno(n < 0 ? errno : ENOSPC, m_strFileName);
        total += static_cast<UINT>(n);
    }
}

ULONGLONG CFile::Seek(LONGLONG lOff, UINT nFrom) {
    int whence = nFrom == current ? SEEK_CUR : nFrom == end ? SEEK_END : SEEK_SET;
    off_t pos = m_fd >= 0 ? lseek(m_fd, static_cast<off_t>(lOff), whence) : -1;
    if (pos < 0)
        AfxThrowFileException(CFileException::badSeek, 131, m_strFileName);
    return static_cast<ULONGLONG>(pos);
}

ULONGLONG CFile::GetLength() const {
    struct stat st;
    if (m_fd < 0 || fstat(m_fd, &st) != 0)
        AfxThrowFileException(CFileException::invalidFile, ERROR_INVALID_HANDLE, m_strFileName);
    return static_cast<ULONGLONG>(st.st_size);
}

void CFile::SetLength(ULONGLONG dwNewLen) {
    if (m_fd < 0 || ftruncate(m_fd, static_cast<off_t>(dwNewLen)) != 0)
        CFileException::ThrowErrno(m_fd < 0 ? EBADF : errno, m_strFileName);
}

ULONGLONG CFile::GetPosition() const {
    off_t pos = m_fd >= 0 ? lseek(m_fd, 0, SEEK_CUR) : -1;
    if (pos < 0)
        AfxThrowFileException(CFileException::badSeek, 131, m_strFileName);
    return static_cast<ULONGLONG>(pos);
}

void CFile::Flush() {}

CString CFile::GetFileName() const { return FileNamePart(m_strFileName); }

CString CFile::GetFileTitle() const { return StripExtension(GetFileName()); }

BOOL CFile::GetStatus(CFileStatus& rStatus) const {
    memset(&rStatus, 0, sizeof rStatus);
    struct stat st;
    if (m_fd < 0 || fstat(m_fd, &st) != 0)
        return FALSE;
    FillStatus(st, NativePath(m_strFileName), rStatus);
    strncpy(rStatus.m_szFullName, m_strFileName.GetString(), sizeof rStatus.m_szFullName - 1);
    return TRUE;
}

BOOL CFile::GetStatus(const char* lpszFileName, CFileStatus& rStatus) {
    memset(&rStatus, 0, sizeof rStatus);
    if (!lpszFileName || !*lpszFileName)
        return FALSE;
    std::string path = FsPath(lpszFileName);
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return FALSE;
    FillStatus(st, path, rStatus);
    strncpy(rStatus.m_szFullName, FullAppPath(lpszFileName).c_str(), sizeof rStatus.m_szFullName - 1);
    return TRUE;
}

void CFile::SetStatus(const char* lpszFileName, const CFileStatus& status) {
    std::string path = FsPath(lpszFileName);
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        CFileException::ThrowErrno(errno, lpszFileName);
    if (status.m_mtime != 0) {
        struct timeval tv[2];
        tv[0].tv_sec = status.m_atime ? status.m_atime : status.m_mtime;
        tv[0].tv_usec = 0;
        tv[1].tv_sec = status.m_mtime;
        tv[1].tv_usec = 0;
        if (utimes(path.c_str(), tv) != 0)
            CFileException::ThrowErrno(errno, lpszFileName);
    }
    mode_t mode = st.st_mode & 07777;
    mode_t wanted = (status.m_attribute & readOnly) ? mode & ~static_cast<mode_t>(S_IWUSR | S_IWGRP | S_IWOTH)
                                                    : mode | S_IWUSR;
    if (wanted != mode && chmod(path.c_str(), wanted) != 0)
        CFileException::ThrowErrno(errno, lpszFileName);
}

void CFile::Rename(const char* lpszOldName, const char* lpszNewName) {
    if (rename(FsPath(lpszOldName).c_str(), FsPath(lpszNewName).c_str()) != 0)
        CFileException::ThrowErrno(errno, lpszOldName);
}

void CFile::Remove(const char* lpszFileName) {
    std::string path = FsPath(lpszFileName);
    struct stat st;
    if (lstat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
        CFileException::ThrowErrno(EACCES, lpszFileName);
    if (unlink(path.c_str()) != 0)
        CFileException::ThrowErrno(errno, lpszFileName);
}

CFile* CFile::Duplicate() const {
    if (m_fd < 0)
        return nullptr;
    int fd = fcntl(m_fd, F_DUPFD_CLOEXEC, 0);
    if (fd < 0)
        CFileException::ThrowErrno(errno, m_strFileName);
    auto* file = new CFile;
    file->m_fd = fd;
    file->m_hFile = HandleFromFd(fd, NativePath(m_strFileName), true);
    file->m_bCloseOnDelete = m_bCloseOnDelete;
    file->m_strFileName = m_strFileName;
    return file;
}

// ---------------------------------------------------------------------------------------------
// CStdioFile

CStdioFile::CStdioFile() : m_pStream(nullptr), m_text(true) {}

CStdioFile::CStdioFile(FILE* pOpenStream) : m_pStream(pOpenStream), m_text(true) {
    if (pOpenStream) {
        m_fd = fileno(pOpenStream);
        m_hFile = HandleFromFd(m_fd, std::string(), false);
    }
}

CStdioFile::CStdioFile(const char* lpszFileName, UINT nOpenFlags) : CStdioFile() {
    CFileException e;
    if (!Open(lpszFileName, nOpenFlags, &e))
        AfxThrowFileException(e.m_cause, e.m_lOsError, e.m_strFileName);
}

CStdioFile::~CStdioFile() {
    if (m_pStream && m_bCloseOnDelete) {
        Abort();
    } else if (m_hFile != hFileNull) {
        CloseHandle(m_hFile);
        m_hFile = hFileNull;
    }
}

BOOL CStdioFile::Open(const char* lpszFileName, UINT nOpenFlags, CFileException* pError) {
    if (m_pStream)
        Abort();
    m_text = !(nOpenFlags & typeBinary);
    if (!CFile::Open(lpszFileName, nOpenFlags & ~(typeText | typeBinary), pError))
        return FALSE;
    char mode[4];
    int n = 0;
    if (nOpenFlags & modeCreate)
        mode[n++] = (nOpenFlags & modeNoTruncate) ? 'a' : 'w';
    else if (nOpenFlags & modeWrite)
        mode[n++] = 'a';
    else
        mode[n++] = 'r';
    if ((mode[0] == 'r' && (nOpenFlags & modeReadWrite)) || (mode[0] != 'r' && !(nOpenFlags & modeWrite)))
        mode[n++] = '+';
    mode[n] = 0;
    // The stream takes its own descriptor; the kernel file object keeps a non-owning view of it.
    int fd = fcntl(m_fd, F_DUPFD_CLOEXEC, 0);
    FILE* stream = fd >= 0 ? fdopen(fd, mode) : nullptr;
    if (!stream) {
        int err = errno;
        if (fd >= 0)
            close(fd);
        CString name = m_strFileName;
        CFile::Abort();
        if (pError) {
            pError->m_cause = CFileException::ErrnoToException(err);
            pError->m_lOsError = static_cast<LONG>(ErrnoToWin32(err));
            pError->m_strFileName = lpszFileName;
        }
        return FALSE;
    }
    std::string native = FsPath(lpszFileName);
    CloseHandle(m_hFile);
    m_pStream = stream;
    m_fd = fd;
    m_hFile = HandleFromFd(fd, native, false);
    return TRUE;
}

void CStdioFile::Close() {
    int rc = 0;
    if (m_pStream)
        rc = fclose(m_pStream);
    m_pStream = nullptr;
    CString name = m_strFileName;
    CFile::Abort();
    if (rc != 0)
        AfxThrowFileException(CFileException::diskFull, static_cast<LONG>(ErrnoToWin32(errno)), name);
}

void CStdioFile::Abort() {
    if (m_pStream)
        fclose(m_pStream);
    m_pStream = nullptr;
    CFile::Abort();
}

UINT CStdioFile::Read(void* lpBuf, UINT nCount) {
    if (nCount == 0)
        return 0;
    if (!m_pStream)
        AfxThrowFileException(CFileException::invalidFile, ERROR_INVALID_HANDLE, m_strFileName);
    size_t n = fread(lpBuf, 1, nCount, m_pStream);
    if (n == 0 && ferror(m_pStream)) {
        clearerr(m_pStream);
        CFileException::ThrowErrno(errno, m_strFileName);
    }
    return static_cast<UINT>(n);
}

void CStdioFile::Write(const void* lpBuf, UINT nCount) {
    if (!m_pStream)
        AfxThrowFileException(CFileException::invalidFile, ERROR_INVALID_HANDLE, m_strFileName);
    if (nCount && fwrite(lpBuf, 1, nCount, m_pStream) != nCount)
        CFileException::ThrowErrno(errno ? errno : ENOSPC, m_strFileName);
}

ULONGLONG CStdioFile::Seek(LONGLONG lOff, UINT nFrom) {
    int whence = nFrom == current ? SEEK_CUR : nFrom == end ? SEEK_END : SEEK_SET;
    if (!m_pStream || fseeko(m_pStream, static_cast<off_t>(lOff), whence) != 0)
        AfxThrowFileException(CFileException::badSeek, 131, m_strFileName);
    return static_cast<ULONGLONG>(ftello(m_pStream));
}

ULONGLONG CStdioFile::GetLength() const {
    if (!m_pStream)
        AfxThrowFileException(CFileException::invalidFile, ERROR_INVALID_HANDLE, m_strFileName);
    off_t pos = ftello(m_pStream);
    fseeko(m_pStream, 0, SEEK_END);
    off_t len = ftello(m_pStream);
    fseeko(m_pStream, pos, SEEK_SET);
    return static_cast<ULONGLONG>(len < 0 ? 0 : len);
}

ULONGLONG CStdioFile::GetPosition() const {
    off_t pos = m_pStream ? ftello(m_pStream) : -1;
    if (pos < 0)
        AfxThrowFileException(CFileException::badSeek, 131, m_strFileName);
    return static_cast<ULONGLONG>(pos);
}

void CStdioFile::Flush() {
    if (m_pStream && fflush(m_pStream) != 0)
        AfxThrowFileException(CFileException::diskFull, static_cast<LONG>(ErrnoToWin32(errno)), m_strFileName);
}

void CStdioFile::WriteString(const char* lpsz) {
    if (!m_pStream)
        AfxThrowFileException(CFileException::invalidFile, ERROR_INVALID_HANDLE, m_strFileName);
    if (lpsz && fputs(lpsz, m_pStream) == EOF)
        AfxThrowFileException(CFileException::diskFull, static_cast<LONG>(ErrnoToWin32(errno)), m_strFileName);
}

char* CStdioFile::ReadString(char* lpsz, UINT nMax) {
    if (!m_pStream || !lpsz || nMax == 0)
        return nullptr;
    char* r = fgets(lpsz, static_cast<int>(nMax), m_pStream);
    if (!r) {
        if (ferror(m_pStream)) {
            clearerr(m_pStream);
            CFileException::ThrowErrno(errno, m_strFileName);
        }
        return nullptr;
    }
    size_t len = strlen(lpsz);
    if (m_text && len >= 2 && lpsz[len - 2] == '\r' && lpsz[len - 1] == '\n') {
        lpsz[len - 2] = '\n';
        lpsz[len - 1] = 0;
    }
    return lpsz;
}

BOOL CStdioFile::ReadString(CString& rString) {
    rString.Empty();
    if (!m_pStream)
        return FALSE;
    std::string line;
    bool any = false;
    int c;
    while ((c = getc(m_pStream)) != EOF) {
        any = true;
        if (c == '\n')
            break;
        line += static_cast<char>(c);
    }
    if (!any) {
        if (ferror(m_pStream)) {
            clearerr(m_pStream);
            CFileException::ThrowErrno(errno, m_strFileName);
        }
        return FALSE;
    }
    if (m_text && !line.empty() && line.back() == '\r')
        line.pop_back();
    rString = CString(line.data(), static_cast<int>(line.size()));
    return TRUE;
}

// ---------------------------------------------------------------------------------------------
// CMemFile

CMemFile::CMemFile(UINT nGrowBytes)
    : m_lpBuffer(nullptr), m_nBufferSize(0), m_nFileSize(0), m_nPosition(0), m_nGrowBytes(nGrowBytes),
      m_bAutoDelete(true) {}

CMemFile::CMemFile(BYTE* lpBuffer, UINT nBufferSize, UINT nGrowBytes) : CMemFile(nGrowBytes) {
    Attach(lpBuffer, nBufferSize, nGrowBytes);
}

CMemFile::~CMemFile() { Close(); }

void CMemFile::Attach(BYTE* lpBuffer, UINT nBufferSize, UINT nGrowBytes) {
    Close();
    m_lpBuffer = lpBuffer;
    m_nBufferSize = nBufferSize;
    m_nFileSize = nGrowBytes == 0 ? nBufferSize : 0;
    m_nGrowBytes = nGrowBytes;
    m_nPosition = 0;
    m_bAutoDelete = false;
}

BYTE* CMemFile::Detach() {
    BYTE* buffer = m_lpBuffer;
    m_lpBuffer = nullptr;
    m_nBufferSize = 0;
    m_nFileSize = 0;
    m_nPosition = 0;
    return buffer;
}

void CMemFile::Grow(size_t size) {
    if (size <= m_nBufferSize)
        return;
    if (m_nGrowBytes == 0)
        AfxThrowMemoryException();
    size_t n = m_nBufferSize;
    while (n < size)
        n += m_nGrowBytes;
    auto* p = static_cast<BYTE*>(realloc(m_lpBuffer, n));
    if (!p)
        AfxThrowMemoryException();
    m_lpBuffer = p;
    m_nBufferSize = n;
}

void CMemFile::Close() {
    if (m_lpBuffer && m_bAutoDelete)
        free(m_lpBuffer);
    m_lpBuffer = nullptr;
    m_nBufferSize = 0;
    m_nFileSize = 0;
    m_nPosition = 0;
}

UINT CMemFile::Read(void* lpBuf, UINT nCount) {
    if (m_nPosition >= m_nFileSize || nCount == 0)
        return 0;
    size_t n = std::min(static_cast<size_t>(nCount), m_nFileSize - m_nPosition);
    memcpy(lpBuf, m_lpBuffer + m_nPosition, n);
    m_nPosition += n;
    return static_cast<UINT>(n);
}

void CMemFile::Write(const void* lpBuf, UINT nCount) {
    if (nCount == 0)
        return;
    size_t endPos = m_nPosition + nCount;
    if (endPos > m_nBufferSize)
        Grow(endPos);
    if (m_nPosition > m_nFileSize)
        memset(m_lpBuffer + m_nFileSize, 0, m_nPosition - m_nFileSize);
    memcpy(m_lpBuffer + m_nPosition, lpBuf, nCount);
    m_nPosition = endPos;
    if (m_nPosition > m_nFileSize)
        m_nFileSize = m_nPosition;
}

ULONGLONG CMemFile::Seek(LONGLONG lOff, UINT nFrom) {
    LONGLONG base = nFrom == current ? static_cast<LONGLONG>(m_nPosition)
                    : nFrom == end   ? static_cast<LONGLONG>(m_nFileSize)
                                     : 0;
    LONGLONG pos = base + lOff;
    if (pos < 0)
        AfxThrowFileException(CFileException::badSeek, 131, m_strFileName);
    m_nPosition = static_cast<size_t>(pos);
    return static_cast<ULONGLONG>(pos);
}

void CMemFile::SetLength(ULONGLONG dwNewLen) {
    size_t len = static_cast<size_t>(dwNewLen);
    if (len > m_nBufferSize)
        Grow(len);
    if (len > m_nFileSize)
        memset(m_lpBuffer + m_nFileSize, 0, len - m_nFileSize);
    if (len < m_nPosition)
        m_nPosition = len;
    m_nFileSize = len;
}

// ---------------------------------------------------------------------------------------------
// CFileFind

struct CFileFind::Impl {
    CString root;
    std::vector<WIN32_FIND_DATA> entries;
    size_t next = 0;
    long current = -1;
    const WIN32_FIND_DATA* Current() const { return current >= 0 ? &entries[static_cast<size_t>(current)] : nullptr; }
};

CFileFind::CFileFind() : m_impl(nullptr) {}

CFileFind::~CFileFind() { Close(); }

BOOL CFileFind::FindFile(const char* pstrName, DWORD) {
    Close();
    const char* pattern = pstrName && *pstrName ? pstrName : "*.*";
    WIN32_FIND_DATA fd;
    HANDLE h = ::FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return FALSE;
    m_impl = new Impl;
    do
        m_impl->entries.push_back(fd);
    while (::FindNextFileA(h, &fd));
    ::FindClose(h);
    std::string full = FullAppPath(pattern);
    size_t sep = full.rfind('\\');
    m_impl->root = sep == std::string::npos ? CString("\\") : CString(full.substr(0, sep + 1).c_str());
    SetLastError(0);
    return TRUE;
}

BOOL CFileFind::FindNextFile() {
    if (!m_impl || m_impl->next >= m_impl->entries.size())
        return FALSE;
    m_impl->current = static_cast<long>(m_impl->next++);
    return m_impl->next < m_impl->entries.size();
}

void CFileFind::Close() {
    delete m_impl;
    m_impl = nullptr;
}

ULONGLONG CFileFind::GetLength() const {
    const WIN32_FIND_DATA* fd = m_impl ? m_impl->Current() : nullptr;
    return fd ? (static_cast<ULONGLONG>(fd->nFileSizeHigh) << 32) | fd->nFileSizeLow : 0;
}

CString CFileFind::GetFileName() const {
    const WIN32_FIND_DATA* fd = m_impl ? m_impl->Current() : nullptr;
    return fd ? CString(fd->cFileName) : CString();
}

CString CFileFind::GetFilePath() const {
    if (!m_impl || !m_impl->Current())
        return CString();
    CString path = m_impl->root;
    if (!path.IsEmpty() && path[path.GetLength() - 1] != '\\' && path[path.GetLength() - 1] != '/')
        path += '\\';
    return path + GetFileName();
}

CString CFileFind::GetFileTitle() const { return StripExtension(GetFileName()); }

CString CFileFind::GetFileURL() const {
    CString path = GetFilePath();
    if (path.IsEmpty())
        return path;
    path.Replace('\\', '/');
    return CString("file://") + path;
}

CString CFileFind::GetRoot() const { return m_impl ? m_impl->root : CString(); }

BOOL CFileFind::IsDots() const {
    const WIN32_FIND_DATA* fd = m_impl ? m_impl->Current() : nullptr;
    return fd && (strcmp(fd->cFileName, ".") == 0 || strcmp(fd->cFileName, "..") == 0);
}

BOOL CFileFind::IsDirectory() const { return MatchesMask(FILE_ATTRIBUTE_DIRECTORY); }

BOOL CFileFind::IsHidden() const { return MatchesMask(FILE_ATTRIBUTE_HIDDEN); }

BOOL CFileFind::MatchesMask(DWORD dwMask) const {
    const WIN32_FIND_DATA* fd = m_impl ? m_impl->Current() : nullptr;
    return fd && (fd->dwFileAttributes & dwMask) != 0;
}

// ---------------------------------------------------------------------------------------------
// CArchive

CArchive::CArchive(CFile* pFile, UINT nMode, int, void*)
    : m_pDocument(nullptr), m_pFile(pFile), m_nMode(nMode), m_nObjectSchema(static_cast<UINT>(-1)) {}

CArchive::~CArchive() {
    if (m_pFile && !(m_nMode & bNoFlushOnDelete))
        Close();
}

BOOL CArchive::IsBufferEmpty() const {
    if (!IsLoading() || !m_pFile)
        return TRUE;
    return m_pFile->GetPosition() >= m_pFile->GetLength();
}

void CArchive::Close() {
    if (m_pFile && IsStoring())
        m_pFile->Flush();
    m_pFile = nullptr;
}

UINT CArchive::Read(void* lpBuf, UINT nMax) {
    if (!m_pFile || nMax == 0)
        return 0;
    if (IsStoring())
        ThrowArchive(CArchiveException::writeOnly, m_pFile);
    return m_pFile->Read(lpBuf, nMax);
}

void CArchive::Write(const void* lpBuf, UINT nMax) {
    if (!m_pFile || nMax == 0)
        return;
    if (IsLoading())
        ThrowArchive(CArchiveException::readOnly, m_pFile);
    m_pFile->Write(lpBuf, nMax);
}

namespace {

template <class T>
T ReadValue(CArchive& ar, CFile* file) {
    T v{};
    if (ar.Read(&v, sizeof v) != sizeof v)
        ThrowArchive(CArchiveException::endOfFile, file);
    return v;
}

} // namespace

CArchive& CArchive::operator<<(const CString& s) {
    DWORD len = static_cast<DWORD>(s.GetLength());
    if (len < 255) {
        *this << static_cast<BYTE>(len);
    } else if (len < 0xFFFE) {
        *this << static_cast<BYTE>(0xFF);
        *this << static_cast<WORD>(len);
    } else {
        *this << static_cast<BYTE>(0xFF);
        *this << static_cast<WORD>(0xFFFF);
        *this << len;
    }
    Write(s.GetString(), len);
    return *this;
}

CArchive& CArchive::operator>>(CString& s) {
    size_t charSize = 1;
    // MFC length prefix: BYTE, 0xFF + WORD, 0xFF 0xFFFF + DWORD; 0xFF 0xFFFE marks a UTF-16 string.
    auto readLength = [&]() -> ULONGLONG {
        BYTE b = ReadValue<BYTE>(*this, m_pFile);
        if (b < 0xFF)
            return b;
        WORD w = ReadValue<WORD>(*this, m_pFile);
        if (w == 0xFFFE) {
            charSize = 2;
            b = ReadValue<BYTE>(*this, m_pFile);
            if (b < 0xFF)
                return b;
            w = ReadValue<WORD>(*this, m_pFile);
        }
        if (w < 0xFFFF)
            return w;
        DWORD d = ReadValue<DWORD>(*this, m_pFile);
        return d < 0xFFFFFFFFu ? d : ReadValue<ULONGLONG>(*this, m_pFile);
    };
    ULONGLONG len = readLength();
    if (len > 0x7FFFFFFFULL / charSize)
        ThrowArchive(CArchiveException::badIndex, m_pFile);
    std::string bytes(static_cast<size_t>(len * charSize), '\0');
    if (!bytes.empty() && Read(&bytes[0], static_cast<UINT>(bytes.size())) != bytes.size())
        ThrowArchive(CArchiveException::endOfFile, m_pFile);
    if (charSize == 1) {
        s = CString(bytes.data(), static_cast<int>(bytes.size()));
        return *this;
    }
    std::string ansi;
    for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
        uint32_t cp = static_cast<unsigned char>(bytes[i]) | (static_cast<unsigned char>(bytes[i + 1]) << 8);
        if (cp >= 0xD800 && cp < 0xDC00 && i + 3 < bytes.size()) {
            uint32_t lo = static_cast<unsigned char>(bytes[i + 2]) | (static_cast<unsigned char>(bytes[i + 3]) << 8);
            if (lo >= 0xDC00 && lo < 0xE000) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                i += 2;
            }
        }
        char ch;
        UnicodeToAnsi(static_cast<wchar_t>(cp), ch);
        ansi.push_back(ch);
    }
    s = CString(ansi.data(), static_cast<int>(ansi.size()));
    return *this;
}

BOOL CArchive::ReadString(CString& rString) {
    rString.Empty();
    std::string line;
    bool any = false;
    char ch;
    while (Read(&ch, 1) == 1) {
        any = true;
        if (ch == '\n')
            break;
        line += ch;
    }
    if (!any)
        return FALSE;
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    rString = CString(line.data(), static_cast<int>(line.size()));
    return TRUE;
}

char* CArchive::ReadString(char* lpsz, UINT nMax) {
    if (!lpsz)
        return nullptr;
    UINT nRead = 0;
    char ch;
    while (nRead < nMax) {
        if (Read(&ch, 1) != 1) {
            if (nRead == 0)
                return nullptr;
            break;
        }
        if (ch == '\n' || ch == '\r') {
            if (ch == '\r')
                Read(&ch, 1);
            break;
        }
        lpsz[nRead++] = ch;
    }
    lpsz[nRead] = 0;
    return lpsz;
}

void CArchive::WriteString(const char* lpsz) {
    if (lpsz)
        Write(lpsz, static_cast<UINT>(strlen(lpsz)));
}

UINT CArchive::ReadCount() {
    WORD w = ReadValue<WORD>(*this, m_pFile);
    if (w != 0xFFFF)
        return w;
    DWORD d = ReadValue<DWORD>(*this, m_pFile);
    if (d != 0xFFFFFFFFu)
        return d;
    return static_cast<UINT>(ReadValue<ULONGLONG>(*this, m_pFile));
}

void CArchive::WriteCount(UINT dwCount) {
    if (dwCount < 0xFFFF) {
        *this << static_cast<WORD>(dwCount);
    } else {
        *this << static_cast<WORD>(0xFFFF);
        *this << static_cast<DWORD>(dwCount);
    }
}

void CArchive::WriteClass(const CRuntimeClass* pClassRef) {
    auto it = m_storeClasses.find(pClassRef);
    if (it != m_storeClasses.end()) {
        if (it->second < kBigObjectTag) {
            *this << static_cast<WORD>(kClassTag | it->second);
        } else {
            *this << kBigObjectTag;
            *this << static_cast<DWORD>(kBigClassTag | it->second);
        }
        return;
    }
    *this << kNewClassTag;
    WORD len = static_cast<WORD>(strlen(pClassRef->m_lpszClassName));
    *this << static_cast<WORD>(pClassRef->m_wSchema);
    *this << len;
    Write(pClassRef->m_lpszClassName, len);
    DWORD index = static_cast<DWORD>(1 + m_storeClasses.size() + m_storeObjects.size());
    m_storeClasses[pClassRef] = index;
}

void CArchive::WriteObject(const CObject* pOb) {
    if (!pOb) {
        *this << kNullTag;
        return;
    }
    auto it = m_storeObjects.find(pOb);
    if (it != m_storeObjects.end()) {
        if (it->second < kBigObjectTag) {
            *this << static_cast<WORD>(it->second);
        } else {
            *this << kBigObjectTag;
            *this << static_cast<DWORD>(it->second);
        }
        return;
    }
    WriteClass(pOb->GetRuntimeClass());
    DWORD index = static_cast<DWORD>(1 + m_storeClasses.size() + m_storeObjects.size());
    m_storeObjects[pOb] = index;
    const_cast<CObject*>(pOb)->Serialize(*this);
}

CRuntimeClass* CArchive::ReadClass(const CRuntimeClass* pClassRefRequested, UINT* pSchema, DWORD* pObTag) {
    WORD wTag = ReadValue<WORD>(*this, m_pFile);
    DWORD obTag;
    if (wTag == kBigObjectTag)
        obTag = ReadValue<DWORD>(*this, m_pFile);
    else
        obTag = (static_cast<DWORD>(wTag & kClassTag) << 16) | (wTag & ~kClassTag);
    if (!(obTag & kBigClassTag)) {
        if (!pObTag)
            ThrowArchive(CArchiveException::badIndex, m_pFile);
        *pObTag = obTag;
        return nullptr;
    }
    if (m_loadClasses.empty()) {
        m_loadClasses.push_back(nullptr);
        m_loadObjects.push_back(nullptr);
    }
    CRuntimeClass* pClass;
    UINT schema;
    if (wTag == kNewClassTag) {
        WORD w = ReadValue<WORD>(*this, m_pFile);
        WORD len = ReadValue<WORD>(*this, m_pFile);
        char name[64];
        if (len >= sizeof name || Read(name, len) != len)
            ThrowArchive(CArchiveException::badClass, m_pFile);
        name[len] = 0;
        pClass = CRuntimeClass::FromName(name);
        if (!pClass)
            ThrowArchive(CArchiveException::badClass, m_pFile);
        schema = w;
        if ((pClass->m_wSchema & ~VERSIONABLE_SCHEMA) != schema && !(pClass->m_wSchema & VERSIONABLE_SCHEMA))
            ThrowArchive(CArchiveException::badSchema, m_pFile);
        m_loadClasses.push_back(pClass);
        m_loadObjects.push_back(nullptr);
    } else {
        DWORD index = obTag & ~kBigClassTag;
        if (index == 0 || index >= m_loadClasses.size() || !m_loadClasses[index])
            ThrowArchive(CArchiveException::badIndex, m_pFile);
        pClass = const_cast<CRuntimeClass*>(m_loadClasses[index]);
        schema = pClass->m_wSchema & ~VERSIONABLE_SCHEMA;
    }
    if (pClassRefRequested && !pClass->IsDerivedFrom(pClassRefRequested))
        ThrowArchive(CArchiveException::badClass, m_pFile);
    if (pSchema)
        *pSchema = schema;
    else
        m_nObjectSchema = schema;
    if (pObTag)
        *pObTag = obTag;
    return pClass;
}

CObject* CArchive::ReadObject(const CRuntimeClass* pClassRefRequested) {
    UINT schema = 0;
    DWORD obTag = 0;
    CRuntimeClass* pClass = ReadClass(pClassRefRequested, &schema, &obTag);
    if (m_loadObjects.empty()) {
        m_loadClasses.push_back(nullptr);
        m_loadObjects.push_back(nullptr);
    }
    if (!pClass) {
        if (obTag == 0)
            return nullptr;
        if (obTag >= m_loadObjects.size() || !m_loadObjects[obTag])
            ThrowArchive(CArchiveException::badIndex, m_pFile);
        CObject* pOb = m_loadObjects[obTag];
        if (pClassRefRequested && !pOb->IsKindOf(pClassRefRequested))
            ThrowArchive(CArchiveException::badClass, m_pFile);
        return pOb;
    }
    CObject* pOb = pClass->CreateObject();
    if (!pOb)
        AfxThrowMemoryException();
    m_loadClasses.push_back(nullptr);
    m_loadObjects.push_back(pOb);
    UINT saved = m_nObjectSchema;
    m_nObjectSchema = schema;
    pOb->Serialize(*this);
    m_nObjectSchema = saved;
    return pOb;
}

void CArchive::SerializeClass(const CRuntimeClass* pClassRef) {
    if (IsStoring())
        WriteClass(pClassRef);
    else
        ReadClass(pClassRef, &m_nObjectSchema);
}
