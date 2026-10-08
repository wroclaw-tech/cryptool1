#pragma once

// MFC core (non-GUI) subset: CObject, CString, exceptions, files, archives, time, collections.
#define __AFX_H__

#ifdef __cplusplus
#include <algorithm>
#include <cstdarg>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#endif

#include "mfcwx/win32.h"

#ifdef __cplusplus

#include "mfcwx/cstring.h"
#include "mfcwx/codepage.h"

#ifndef _DEBUG
#define ASSERT(f) ((void)0)
#define ASSERT_VALID(p) ((void)0)
#define ASSERT_KINDOF(class_name, object) ((void)0)
#define ASSERT_POINTER(p, type) ((void)0)
#define ASSERT_NULL_OR_POINTER(p, type) ((void)0)
#else
#define ASSERT(f) ((f) ? (void)0 : AfxAssertFailedLine(__FILE__, __LINE__, #f))
#define ASSERT_VALID(p) ((void)0)
#define ASSERT_KINDOF(class_name, object) ((void)0)
#define ASSERT_POINTER(p, type) ASSERT((p) != NULL)
#define ASSERT_NULL_OR_POINTER(p, type) ((void)0)
#endif
#define VERIFY(f) ((void)(f))
#define ENSURE(f) ((void)(f))
#define ENSURE_ARG(f) ((void)(f))
#define DEBUG_ONLY(f) ((void)0)
#define TRACE(...) ((void)0)
#define TRACE0(sz) ((void)0)
#define TRACE1(sz, p1) ((void)0)
#define TRACE2(sz, p1, p2) ((void)0)
#define TRACE3(sz, p1, p2, p3) ((void)0)
#define ATLTRACE(...) ((void)0)
#define ATLASSERT(f) ((void)0)
#define DEBUG_NEW new
#define THIS_FILE __FILE__
#define AFX_MANAGE_STATE(p)
#define AfxGetStaticModuleState() nullptr
#define afx_msg
#define AFX_MSG_CALL
#define AFX_INLINE inline

void AfxAssertFailedLine(const char* file, int line, const char* expr);

// MFC code calls members such as GetSafeHwnd() through null pointers; hide the origin of the
// pointer from the optimizer so the null test survives.
inline bool AfxIsNullThis(const void* p) {
    __asm__("" : "+r"(p));
    return p == nullptr;
}
void AfxDebugBreak();

class CArchive;
class CDumpContext;
class CObject;

struct CRuntimeClass {
    const char* m_lpszClassName;
    int m_nObjectSize;
    UINT m_wSchema;
    CObject* (*m_pfnCreateObject)();
    CRuntimeClass* (*m_pfnGetBaseClass)();
    CRuntimeClass* m_pNextClass;

    CObject* CreateObject();
    BOOL IsDerivedFrom(const CRuntimeClass* pBaseClass) const;
    CRuntimeClass* GetBaseClass() const { return m_pfnGetBaseClass ? m_pfnGetBaseClass() : nullptr; }
    static CRuntimeClass* FromName(const char* name);
    static CObject* CreateObject(const char* name);
};

struct AFX_CLASSINIT {
    explicit AFX_CLASSINIT(CRuntimeClass* pNewClass);
};

class CDumpContext {
public:
    CDumpContext& operator<<(const char*) { return *this; }
    CDumpContext& operator<<(int) { return *this; }
    CDumpContext& operator<<(unsigned) { return *this; }
    CDumpContext& operator<<(long) { return *this; }
    CDumpContext& operator<<(double) { return *this; }
    CDumpContext& operator<<(const void*) { return *this; }
    int GetDepth() const { return 0; }
};
extern CDumpContext afxDump;

class CObject {
public:
    virtual CRuntimeClass* GetRuntimeClass() const;
    virtual ~CObject() = default;
    BOOL IsKindOf(const CRuntimeClass* pClass) const;
    BOOL IsSerializable() const;
    virtual void Serialize(CArchive& ar);
    virtual void AssertValid() const {}
    virtual void Dump(CDumpContext&) const {}

    static CRuntimeClass classCObject;
    static CRuntimeClass* GetThisClass();

protected:
    CObject() = default;
    CObject(const CObject&) = default;
    CObject& operator=(const CObject&) = default;
};

#define _RUNTIME_CLASS(class_name) (class_name::GetThisClass())
#define RUNTIME_CLASS(class_name) _RUNTIME_CLASS(class_name)

#define DECLARE_DYNAMIC(class_name) \
public: \
    static CRuntimeClass class##class_name; \
    static CRuntimeClass* GetThisClass(); \
    CRuntimeClass* GetRuntimeClass() const override;

#define DECLARE_DYNCREATE(class_name) \
    DECLARE_DYNAMIC(class_name) \
    static CObject* CreateObject();

#define DECLARE_SERIAL(class_name) \
    DECLARE_DYNCREATE(class_name) \
    friend CArchive& operator>>(CArchive& ar, class_name*& pOb);

#define IMPLEMENT_RUNTIMECLASS(class_name, base_class_name, wSchema, pfnNew) \
    CRuntimeClass class_name::class##class_name = {#class_name, sizeof(class class_name), wSchema, pfnNew, \
                                                   &base_class_name::GetThisClass, nullptr}; \
    static AFX_CLASSINIT _init_##class_name(&class_name::class##class_name); \
    CRuntimeClass* class_name::GetThisClass() { return &class_name::class##class_name; } \
    CRuntimeClass* class_name::GetRuntimeClass() const { return &class_name::class##class_name; }

#define IMPLEMENT_DYNAMIC(class_name, base_class_name) \
    IMPLEMENT_RUNTIMECLASS(class_name, base_class_name, 0xFFFF, nullptr)

#define IMPLEMENT_DYNCREATE(class_name, base_class_name) \
    CObject* class_name::CreateObject() { return new class_name; } \
    IMPLEMENT_RUNTIMECLASS(class_name, base_class_name, 0xFFFF, class_name::CreateObject)

#define IMPLEMENT_SERIAL(class_name, base_class_name, wSchema) \
    CObject* class_name::CreateObject() { return new class_name; } \
    IMPLEMENT_RUNTIMECLASS(class_name, base_class_name, wSchema, class_name::CreateObject) \
    CArchive& operator>>(CArchive& ar, class_name*& pOb) { \
        pOb = static_cast<class_name*>(ar.ReadObject(RUNTIME_CLASS(class_name))); \
        return ar; \
    }

#define VERSIONABLE_SCHEMA (0x80000000)

// ---------------------------------------------------------------------------------------------
// Exceptions

class CException : public CObject {
    DECLARE_DYNAMIC(CException)
public:
    explicit CException(BOOL bAutoDelete = TRUE) : m_bAutoDelete(bAutoDelete) {}
    void Delete();
    virtual BOOL GetErrorMessage(char* lpszError, UINT nMaxError, PUINT pnHelpContext = nullptr) const;
    virtual int ReportError(UINT nType = MB_OK, UINT nMessageID = 0);
    BOOL m_bAutoDelete;
};

class CSimpleException : public CException {
    DECLARE_DYNAMIC(CSimpleException)
public:
    CSimpleException() = default;
    explicit CSimpleException(BOOL bAutoDelete) : CException(bAutoDelete) {}
};

class CMemoryException : public CSimpleException {
    DECLARE_DYNAMIC(CMemoryException)
public:
    CMemoryException() = default;
    BOOL GetErrorMessage(char* lpszError, UINT nMaxError, PUINT pnHelpContext = nullptr) const override;
};

class CNotSupportedException : public CSimpleException {
    DECLARE_DYNAMIC(CNotSupportedException)
};

class CInvalidArgException : public CSimpleException {
    DECLARE_DYNAMIC(CInvalidArgException)
};

class CUserException : public CSimpleException {
    DECLARE_DYNAMIC(CUserException)
};

class CResourceException : public CSimpleException {
    DECLARE_DYNAMIC(CResourceException)
};

class CArchiveException : public CException {
    DECLARE_DYNAMIC(CArchiveException)
public:
    enum { none, genericException, readOnly, endOfFile, writeOnly, badIndex, badClass, badSchema, bufferFull };
    explicit CArchiveException(int cause = none, const char* lpszArchiveName = nullptr)
        : m_cause(cause), m_strFileName(lpszArchiveName) {}
    BOOL GetErrorMessage(char* lpszError, UINT nMaxError, PUINT pnHelpContext = nullptr) const override;
    int m_cause;
    CString m_strFileName;
};

class CFileException : public CException {
    DECLARE_DYNAMIC(CFileException)
public:
    enum {
        none,
        genericException,
        fileNotFound,
        badPath,
        tooManyOpenFiles,
        accessDenied,
        invalidFile,
        removeCurrentDir,
        directoryFull,
        badSeek,
        hardIO,
        sharingViolation,
        lockViolation,
        diskFull,
        endOfFile
    };
    explicit CFileException(int cause = none, LONG lOsError = -1, const char* lpszArchiveName = nullptr)
        : m_cause(cause), m_lOsError(lOsError), m_strFileName(lpszArchiveName) {}
    BOOL GetErrorMessage(char* lpszError, UINT nMaxError, PUINT pnHelpContext = nullptr) const override;
    static int OsErrorToException(LONG lOsError);
    static int ErrnoToException(int nErrno);
    static void ThrowOsError(LONG lOsError, const char* lpszFileName = nullptr);
    static void ThrowErrno(int nErrno, const char* lpszFileName = nullptr);
    int m_cause;
    LONG m_lOsError;
    CString m_strFileName;
};

[[noreturn]] void AfxThrowMemoryException();
[[noreturn]] void AfxThrowNotSupportedException();
[[noreturn]] void AfxThrowInvalidArgException();
[[noreturn]] void AfxThrowUserException();
[[noreturn]] void AfxThrowResourceException();
[[noreturn]] void AfxThrowArchiveException(int cause, const char* lpszArchiveName = nullptr);
[[noreturn]] void AfxThrowFileException(int cause, LONG lOsError = -1, const char* lpszFileName = nullptr);
void AfxAbort();

#define TRY try
#define CATCH(class, e) catch (class * e)
#define AND_CATCH(class, e) catch (class * e)
#define END_CATCH
#define CATCH_ALL(e) catch (CException * e)
#define AND_CATCH_ALL(e) catch (CException * e)
#define END_CATCH_ALL
#define END_TRY
#define THROW(e) throw e
#define THROW_LAST() throw

// ---------------------------------------------------------------------------------------------
// Files

struct CFileStatus {
    time_t m_ctime;
    time_t m_mtime;
    time_t m_atime;
    ULONGLONG m_size;
    BYTE m_attribute;
    char m_szFullName[_MAX_PATH];
};

class CFile : public CObject {
    DECLARE_DYNAMIC(CFile)
public:
    enum OpenFlags {
        modeRead = 0x0000,
        modeWrite = 0x0001,
        modeReadWrite = 0x0002,
        shareCompat = 0x0000,
        shareExclusive = 0x0010,
        shareDenyWrite = 0x0020,
        shareDenyRead = 0x0030,
        shareDenyNone = 0x0040,
        modeNoInherit = 0x0080,
        modeCreate = 0x1000,
        modeNoTruncate = 0x2000,
        typeText = 0x4000,
        typeBinary = 0x8000,
        osNoBuffer = 0x10000,
        osWriteThrough = 0x20000,
        osRandomAccess = 0x40000,
        osSequentialScan = 0x80000
    };
    enum Attribute { normal = 0x00, readOnly = 0x01, hidden = 0x02, system = 0x04, volume = 0x08, directory = 0x10, archive = 0x20 };
    enum SeekPosition { begin = 0x0, current = 0x1, end = 0x2 };
    static const HANDLE hFileNull;

    CFile();
    explicit CFile(HANDLE hFile);
    CFile(const char* lpszFileName, UINT nOpenFlags);
    ~CFile() override;

    virtual BOOL Open(const char* lpszFileName, UINT nOpenFlags, CFileException* pError = nullptr);
    virtual void Close();
    virtual void Abort();
    virtual UINT Read(void* lpBuf, UINT nCount);
    virtual void Write(const void* lpBuf, UINT nCount);
    virtual ULONGLONG Seek(LONGLONG lOff, UINT nFrom);
    void SeekToBegin() { Seek(0, begin); }
    ULONGLONG SeekToEnd() { return Seek(0, end); }
    virtual ULONGLONG GetLength() const;
    virtual void SetLength(ULONGLONG dwNewLen);
    virtual ULONGLONG GetPosition() const;
    virtual void Flush();
    virtual CString GetFileName() const;
    virtual CString GetFileTitle() const;
    virtual CString GetFilePath() const { return m_strFileName; }
    virtual void SetFilePath(const char* lpszNewName) { m_strFileName = lpszNewName; }
    BOOL GetStatus(CFileStatus& rStatus) const;
    static BOOL GetStatus(const char* lpszFileName, CFileStatus& rStatus);
    static void SetStatus(const char* lpszFileName, const CFileStatus& status);
    static void Rename(const char* lpszOldName, const char* lpszNewName);
    static void Remove(const char* lpszFileName);
    virtual void LockRange(ULONGLONG, ULONGLONG) {}
    virtual void UnlockRange(ULONGLONG, ULONGLONG) {}
    virtual CFile* Duplicate() const;
    operator HANDLE() const { return m_hFile; }

    HANDLE m_hFile;
    BOOL m_bCloseOnDelete;
    CString m_strFileName;

protected:
    int m_fd;
};

class CStdioFile : public CFile {
    DECLARE_DYNAMIC(CStdioFile)
public:
    CStdioFile();
    explicit CStdioFile(FILE* pOpenStream);
    CStdioFile(const char* lpszFileName, UINT nOpenFlags);
    ~CStdioFile() override;

    BOOL Open(const char* lpszFileName, UINT nOpenFlags, CFileException* pError = nullptr) override;
    void Close() override;
    void Abort() override;
    UINT Read(void* lpBuf, UINT nCount) override;
    void Write(const void* lpBuf, UINT nCount) override;
    ULONGLONG Seek(LONGLONG lOff, UINT nFrom) override;
    ULONGLONG GetLength() const override;
    ULONGLONG GetPosition() const override;
    void Flush() override;
    virtual void WriteString(const char* lpsz);
    virtual char* ReadString(char* lpsz, UINT nMax);
    virtual BOOL ReadString(CString& rString);

    FILE* m_pStream;

private:
    bool m_text;
};

class CMemFile : public CFile {
    DECLARE_DYNAMIC(CMemFile)
public:
    explicit CMemFile(UINT nGrowBytes = 1024);
    CMemFile(BYTE* lpBuffer, UINT nBufferSize, UINT nGrowBytes = 0);
    ~CMemFile() override;
    void Attach(BYTE* lpBuffer, UINT nBufferSize, UINT nGrowBytes = 0);
    BYTE* Detach();
    BOOL Open(const char*, UINT, CFileException* = nullptr) override { return FALSE; }
    void Close() override;
    void Abort() override { Close(); }
    UINT Read(void* lpBuf, UINT nCount) override;
    void Write(const void* lpBuf, UINT nCount) override;
    ULONGLONG Seek(LONGLONG lOff, UINT nFrom) override;
    ULONGLONG GetLength() const override { return m_nFileSize; }
    void SetLength(ULONGLONG dwNewLen) override;
    ULONGLONG GetPosition() const override { return m_nPosition; }
    void Flush() override {}

private:
    void Grow(size_t size);
    BYTE* m_lpBuffer;
    size_t m_nBufferSize;
    size_t m_nFileSize;
    size_t m_nPosition;
    UINT m_nGrowBytes;
    bool m_bAutoDelete;
};

class CFileFind : public CObject {
    DECLARE_DYNAMIC(CFileFind)
public:
    CFileFind();
    ~CFileFind() override;
    virtual BOOL FindFile(const char* pstrName = nullptr, DWORD dwUnused = 0);
    virtual BOOL FindNextFile();
    void Close();
    ULONGLONG GetLength() const;
    CString GetFileName() const;
    CString GetFilePath() const;
    CString GetFileTitle() const;
    CString GetFileURL() const;
    CString GetRoot() const;
    BOOL IsDots() const;
    BOOL IsDirectory() const;
    BOOL IsReadOnly() const { return FALSE; }
    BOOL IsHidden() const;
    BOOL IsArchived() const { return FALSE; }
    BOOL IsNormal() const { return !IsDirectory(); }
    BOOL MatchesMask(DWORD dwMask) const;

private:
    struct Impl;
    Impl* m_impl;
};

// ---------------------------------------------------------------------------------------------
// Archives

class CDocument;

class CArchive {
public:
    enum Mode { store = 0, load = 1, bNoFlushOnDelete = 2, bNoByteSwap = 4 };
    CArchive(CFile* pFile, UINT nMode, int nBufSize = 4096, void* lpBuf = nullptr);
    ~CArchive();

    BOOL IsLoading() const { return (m_nMode & load) != 0; }
    BOOL IsStoring() const { return (m_nMode & load) == 0; }
    BOOL IsByteSwapping() const { return FALSE; }
    BOOL IsBufferEmpty() const;
    CFile* GetFile() const { return m_pFile; }
    UINT GetObjectSchema() { return m_nObjectSchema; }
    void SetObjectSchema(UINT nSchema) { m_nObjectSchema = nSchema; }
    void Close();
    void Abort() { m_pFile = nullptr; }
    void Flush() {}
    UINT Read(void* lpBuf, UINT nMax);
    void Write(const void* lpBuf, UINT nMax);
    BOOL ReadString(CString& rString);
    char* ReadString(char* lpsz, UINT nMax);
    void WriteString(const char* lpsz);
    UINT ReadCount();
    void WriteCount(UINT dwCount);
    CObject* ReadObject(const CRuntimeClass* pClass);
    void WriteObject(const CObject* pOb);
    CRuntimeClass* ReadClass(const CRuntimeClass* pClassRefRequested = nullptr, UINT* pSchema = nullptr,
                             DWORD* pObTag = nullptr);
    void WriteClass(const CRuntimeClass* pClassRef);
    void SerializeClass(const CRuntimeClass* pClassRef);

    CArchive& operator<<(BYTE by) { Write(&by, sizeof by); return *this; }
    CArchive& operator<<(WORD w) { Write(&w, sizeof w); return *this; }
    CArchive& operator<<(LONG l) { Write(&l, sizeof l); return *this; }
    CArchive& operator<<(DWORD dw) { Write(&dw, sizeof dw); return *this; }
    CArchive& operator<<(float f) { Write(&f, sizeof f); return *this; }
    CArchive& operator<<(double d) { Write(&d, sizeof d); return *this; }
    CArchive& operator<<(LONGLONG d) { Write(&d, sizeof d); return *this; }
    CArchive& operator<<(ULONGLONG d) { Write(&d, sizeof d); return *this; }
    CArchive& operator<<(short w) { return *this << static_cast<WORD>(w); }
    CArchive& operator<<(char ch) { return *this << static_cast<BYTE>(ch); }
    CArchive& operator<<(bool b) { return *this << static_cast<BYTE>(b ? 1 : 0); }
    CArchive& operator<<(long l) { return *this << static_cast<LONG>(l); }
    CArchive& operator<<(unsigned long l) { return *this << static_cast<DWORD>(l); }
    CArchive& operator<<(const CString& s);
    CArchive& operator<<(const CObject* pOb) { WriteObject(pOb); return *this; }

    CArchive& operator>>(BYTE& by) { Read(&by, sizeof by); return *this; }
    CArchive& operator>>(WORD& w) { Read(&w, sizeof w); return *this; }
    CArchive& operator>>(DWORD& dw) { Read(&dw, sizeof dw); return *this; }
    CArchive& operator>>(LONG& l) { Read(&l, sizeof l); return *this; }
    CArchive& operator>>(float& f) { Read(&f, sizeof f); return *this; }
    CArchive& operator>>(double& d) { Read(&d, sizeof d); return *this; }
    CArchive& operator>>(LONGLONG& d) { Read(&d, sizeof d); return *this; }
    CArchive& operator>>(ULONGLONG& d) { Read(&d, sizeof d); return *this; }
    CArchive& operator>>(short& w) { WORD v; *this >> v; w = static_cast<short>(v); return *this; }
    CArchive& operator>>(char& ch) { BYTE v; *this >> v; ch = static_cast<char>(v); return *this; }
    CArchive& operator>>(bool& b) { BYTE v; *this >> v; b = v != 0; return *this; }
    CArchive& operator>>(long& l) { LONG v; *this >> v; l = v; return *this; }
    CArchive& operator>>(unsigned long& l) { DWORD v; *this >> v; l = v; return *this; }
    CArchive& operator>>(CString& s);
    CArchive& operator>>(CObject*& pOb) { pOb = ReadObject(nullptr); return *this; }

    CDocument* m_pDocument;
    CString m_strFileName;

private:
    CFile* m_pFile;
    UINT m_nMode;
    UINT m_nObjectSchema;
    std::vector<const CRuntimeClass*> m_loadClasses;
    std::vector<CObject*> m_loadObjects;
    std::map<const CRuntimeClass*, DWORD> m_storeClasses;
    std::map<const CObject*, DWORD> m_storeObjects;
};

// ---------------------------------------------------------------------------------------------
// Time

class CTimeSpan {
public:
    CTimeSpan() : m_span(0) {}
    CTimeSpan(time_t span) : m_span(span) {}
    CTimeSpan(LONG lDays, int nHours, int nMins, int nSecs)
        : m_span(((static_cast<time_t>(lDays) * 24 + nHours) * 60 + nMins) * 60 + nSecs) {}
    LONGLONG GetDays() const { return m_span / (24 * 3600); }
    LONGLONG GetTotalHours() const { return m_span / 3600; }
    LONG GetHours() const { return static_cast<LONG>(GetTotalHours() - GetDays() * 24); }
    LONGLONG GetTotalMinutes() const { return m_span / 60; }
    LONG GetMinutes() const { return static_cast<LONG>(GetTotalMinutes() - GetTotalHours() * 60); }
    LONGLONG GetTotalSeconds() const { return m_span; }
    LONG GetSeconds() const { return static_cast<LONG>(GetTotalSeconds() - GetTotalMinutes() * 60); }
    time_t GetTimeSpan() const { return m_span; }
    CString Format(const char* pFormat) const;
    CTimeSpan operator+(CTimeSpan s) const { return CTimeSpan(m_span + s.m_span); }
    CTimeSpan operator-(CTimeSpan s) const { return CTimeSpan(m_span - s.m_span); }
    CTimeSpan& operator+=(CTimeSpan s) { m_span += s.m_span; return *this; }
    CTimeSpan& operator-=(CTimeSpan s) { m_span -= s.m_span; return *this; }
    bool operator==(CTimeSpan s) const { return m_span == s.m_span; }
    bool operator!=(CTimeSpan s) const { return m_span != s.m_span; }
    bool operator<(CTimeSpan s) const { return m_span < s.m_span; }
    bool operator>(CTimeSpan s) const { return m_span > s.m_span; }
    bool operator<=(CTimeSpan s) const { return m_span <= s.m_span; }
    bool operator>=(CTimeSpan s) const { return m_span >= s.m_span; }

private:
    time_t m_span;
};

class CTime {
public:
    CTime() : m_time(0) {}
    CTime(time_t t) : m_time(t) {}
    CTime(int nYear, int nMonth, int nDay, int nHour, int nMin, int nSec, int nDST = -1);
    CTime(const SYSTEMTIME& st, int nDST = -1);
    CTime(const FILETIME& ft, int nDST = -1);
    static CTime GetCurrentTime();
    time_t GetTime() const { return m_time; }
    struct tm* GetLocalTm(struct tm* ptm) const;
    struct tm* GetGmtTm(struct tm* ptm) const;
    BOOL GetAsSystemTime(SYSTEMTIME& st) const;
    int GetYear() const;
    int GetMonth() const;
    int GetDay() const;
    int GetHour() const;
    int GetMinute() const;
    int GetSecond() const;
    int GetDayOfWeek() const;
    CString Format(const char* pFormat) const;
    CString Format(UINT nFormatID) const;
    CString FormatGmt(const char* pFormat) const;
    CTimeSpan operator-(CTime t) const { return CTimeSpan(m_time - t.m_time); }
    CTime operator-(CTimeSpan s) const { return CTime(m_time - s.GetTimeSpan()); }
    CTime operator+(CTimeSpan s) const { return CTime(m_time + s.GetTimeSpan()); }
    CTime& operator+=(CTimeSpan s) { m_time += s.GetTimeSpan(); return *this; }
    CTime& operator-=(CTimeSpan s) { m_time -= s.GetTimeSpan(); return *this; }
    bool operator==(CTime t) const { return m_time == t.m_time; }
    bool operator!=(CTime t) const { return m_time != t.m_time; }
    bool operator<(CTime t) const { return m_time < t.m_time; }
    bool operator>(CTime t) const { return m_time > t.m_time; }
    bool operator<=(CTime t) const { return m_time <= t.m_time; }
    bool operator>=(CTime t) const { return m_time >= t.m_time; }

private:
    time_t m_time;
};

// File APIs accept Windows-style paths; see mfcwx::NativePath.
extern "C" FILE* mfcwx_fopen(const char* name, const char* mode);
extern "C" int mfcwx_rename(const char* from, const char* to);
extern "C" int mfcwx_unlink(const char* name);
inline int mfcwx_remove(const char* name) { return mfcwx_unlink(name); }
namespace std {
using ::mfcwx_fopen;
using ::mfcwx_remove;
using ::mfcwx_rename;
template <class It, class T>
It mfcwx_remove(It first, It last, const T& value) { return (std::remove)(first, last, value); }
}
#define fopen mfcwx_fopen
#define rename mfcwx_rename
#define remove(...) mfcwx_remove(__VA_ARGS__)

#include "mfcwx/geometry.h"
#include "mfcwx/collections.h"
#include "mfcwx/fstreamcompat.h"

#endif // __cplusplus
