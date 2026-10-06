#include "afxwin.h"
#include "afxole.h"
#include "runtime.h"

#include <cmath>
#include <string>

using namespace mfcwx;

// ---------------------------------------------------------------------------------------------
// Runtime classes

namespace {

CRuntimeClass* g_firstClass = nullptr;

long long DaysFromCivil(long long y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long long>(doe) - 719468;
}

void CivilFromDays(long long z, int& y, int& m, int& d) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long long yy = static_cast<long long>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    y = static_cast<int>(yy + (m <= 2));
}

const long long kOleEpochDays = DaysFromCivil(1899, 12, 30);

bool IsLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int DaysInMonth(int y, int m) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return m == 2 && IsLeap(y) ? 29 : days[m - 1];
}

std::string LocalizedPiece(const char* buf, size_t n) {
    std::string piece(buf, n);
    for (unsigned char c : piece)
        if (c >= 0x80) {
            std::string ansi = Utf8ToAnsi(piece.c_str());
            return ansi.empty() ? piece : ansi;
        }
    return piece;
}

std::string Strftime(const char* fmt, const struct tm& tm) {
    char buf[512];
    size_t n = strftime(buf, sizeof buf, fmt, &tm);
    return LocalizedPiece(buf, n);
}

// strftime with the Microsoft '#' flag (no leading zeros, long %c/%x forms).
CString FormatTm(const char* fmt, const struct tm& tm) {
    std::string out;
    for (const char* p = fmt ? fmt : ""; *p; ++p) {
        if (*p != '%') {
            out += *p;
            continue;
        }
        if (!*++p)
            break;
        bool alt = false;
        if (*p == '#') {
            alt = true;
            if (!*++p)
                break;
        }
        char spec = *p;
        if (spec == '%') {
            out += '%';
            continue;
        }
        if (alt && (spec == 'x' || spec == 'c')) {
            out += Strftime("%A, %B ", tm) + std::to_string(tm.tm_mday) + Strftime(", %Y", tm);
            if (spec == 'c')
                out += Strftime(" %H:%M:%S", tm);
            continue;
        }
        char one[3] = {'%', spec, 0};
        std::string piece = Strftime(one, tm);
        if (alt && strchr("dHIjmMSUwWy", spec)) {
            size_t z = 0;
            while (z + 1 < piece.size() && piece[z] == '0')
                ++z;
            piece.erase(0, z);
        }
        out += piece;
    }
    return CString(out.c_str(), static_cast<int>(out.size()));
}

bool OleFromParts(int y, int mo, int d, int h, int mi, int s, double& out) {
    if (y < 100 || y > 9999 || mo < 1 || mo > 12 || d < 1 || d > DaysInMonth(y, mo) || h < 0 || h > 23 || mi < 0 ||
        mi > 59 || s < 0 || s > 59)
        return false;
    long long days = DaysFromCivil(y, static_cast<unsigned>(mo), static_cast<unsigned>(d)) - kOleEpochDays;
    double frac = (h * 3600 + mi * 60 + s) / 86400.0;
    out = days >= 0 ? static_cast<double>(days) + frac : static_cast<double>(days) - frac;
    return true;
}

void TmFromOle(double dt, struct tm& tm) {
    long long dayPart = static_cast<long long>(std::trunc(dt));
    double frac = std::fabs(dt - static_cast<double>(dayPart));
    long long secs = std::llround(frac * 86400.0);
    if (secs >= 86400) {
        secs -= 86400;
        ++dayPart;
    }
    long long days = dayPart + kOleEpochDays;
    int y, m, d;
    CivilFromDays(days, y, m, d);
    memset(&tm, 0, sizeof tm);
    tm.tm_year = y - 1900;
    tm.tm_mon = m - 1;
    tm.tm_mday = d;
    tm.tm_hour = static_cast<int>(secs / 3600);
    tm.tm_min = static_cast<int>(secs / 60 % 60);
    tm.tm_sec = static_cast<int>(secs % 60);
    tm.tm_wday = static_cast<int>(((days + 4) % 7 + 7) % 7);
    tm.tm_yday = static_cast<int>(days - DaysFromCivil(y, 1, 1));
    tm.tm_isdst = -1;
}

std::string FileNameOrUnnamed(const CString& name) { return name.IsEmpty() ? "an unnamed file" : name.GetString(); }

BOOL CopyMessage(const std::string& text, char* buffer, UINT size, PUINT help) {
    if (help)
        *help = 0;
    if (buffer && size) {
        size_t n = std::min(text.size(), static_cast<size_t>(size - 1));
        memcpy(buffer, text.data(), n);
        buffer[n] = 0;
    }
    return TRUE;
}

std::string Substitute(const char* tmpl, const std::string& name) {
    std::string s = tmpl;
    size_t pos = s.find("%1");
    if (pos != std::string::npos)
        s.replace(pos, 2, name);
    return s;
}

} // namespace

AFX_CLASSINIT::AFX_CLASSINIT(CRuntimeClass* pNewClass) {
    pNewClass->m_pNextClass = g_firstClass;
    g_firstClass = pNewClass;
}

CObject* CRuntimeClass::CreateObject() { return m_pfnCreateObject ? m_pfnCreateObject() : nullptr; }

BOOL CRuntimeClass::IsDerivedFrom(const CRuntimeClass* pBaseClass) const {
    for (const CRuntimeClass* c = this; c; c = c->GetBaseClass())
        if (c == pBaseClass)
            return TRUE;
    return FALSE;
}

CRuntimeClass* CRuntimeClass::FromName(const char* name) {
    if (!name)
        return nullptr;
    for (CRuntimeClass* c = g_firstClass; c; c = c->m_pNextClass)
        if (strcmp(c->m_lpszClassName, name) == 0)
            return c;
    return nullptr;
}

CObject* CRuntimeClass::CreateObject(const char* name) {
    CRuntimeClass* c = FromName(name);
    return c ? c->CreateObject() : nullptr;
}

CRuntimeClass CObject::classCObject = {"CObject", sizeof(CObject), 0xFFFF, nullptr, nullptr, nullptr};
static AFX_CLASSINIT _init_CObject(&CObject::classCObject);

CRuntimeClass* CObject::GetThisClass() { return &classCObject; }

CRuntimeClass* CObject::GetRuntimeClass() const { return &classCObject; }

BOOL CObject::IsKindOf(const CRuntimeClass* pClass) const { return GetRuntimeClass()->IsDerivedFrom(pClass); }

BOOL CObject::IsSerializable() const { return GetRuntimeClass()->m_wSchema != 0xFFFF; }

void CObject::Serialize(CArchive&) {}

CDumpContext afxDump;

void AfxAssertFailedLine(const char* file, int line, const char* expr) {
    fprintf(stderr, "mfcwx: assertion failed: %s (%s:%d)\n", expr ? expr : "", file ? file : "?", line);
}

void AfxDebugBreak() { fprintf(stderr, "mfcwx: AfxDebugBreak()\n"); }

// ---------------------------------------------------------------------------------------------
// Exceptions

IMPLEMENT_DYNAMIC(CException, CObject)
IMPLEMENT_DYNAMIC(CSimpleException, CException)
IMPLEMENT_DYNAMIC(CMemoryException, CSimpleException)
IMPLEMENT_DYNAMIC(CNotSupportedException, CSimpleException)
IMPLEMENT_DYNAMIC(CInvalidArgException, CSimpleException)
IMPLEMENT_DYNAMIC(CUserException, CSimpleException)
IMPLEMENT_DYNAMIC(CResourceException, CSimpleException)
IMPLEMENT_DYNAMIC(CArchiveException, CException)
IMPLEMENT_DYNAMIC(CFileException, CException)

void CException::Delete() {
    if (m_bAutoDelete > 0)
        delete this;
}

BOOL CException::GetErrorMessage(char* lpszError, UINT nMaxError, PUINT pnHelpContext) const {
    const char* text = nullptr;
    if (IsKindOf(RUNTIME_CLASS(CNotSupportedException)))
        text = "An unsupported operation was attempted.";
    else if (IsKindOf(RUNTIME_CLASS(CInvalidArgException)))
        text = "Encountered an improper argument.";
    else if (IsKindOf(RUNTIME_CLASS(CResourceException)))
        text = "A required resource was unavailable.";
    if (text)
        return CopyMessage(text, lpszError, nMaxError, pnHelpContext);
    if (pnHelpContext)
        *pnHelpContext = 0;
    if (lpszError && nMaxError)
        lpszError[0] = 0;
    return FALSE;
}

int CException::ReportError(UINT nType, UINT nMessageID) {
    char msg[1024];
    if (GetErrorMessage(msg, sizeof msg))
        return AfxMessageBox(msg, nType);
    if (nMessageID)
        return AfxMessageBox(nMessageID, nType);
    return AfxMessageBox("No error message is available.", nType);
}

BOOL CMemoryException::GetErrorMessage(char* lpszError, UINT nMaxError, PUINT pnHelpContext) const {
    return CopyMessage("Out of memory.", lpszError, nMaxError, pnHelpContext);
}

BOOL CArchiveException::GetErrorMessage(char* lpszError, UINT nMaxError, PUINT pnHelpContext) const {
    static const char* const texts[] = {
        "No error occurred.",
        "An unknown error occurred while accessing %1.",
        "An attempt was made to write to the reading %1.",
        "An attempt was made to access %1 past its end.",
        "An attempt was made to read from the writing %1.",
        "%1 has a bad format.",
        "%1 contained an unexpected object.",
        "%1 contains an incorrect schema.",
        "The archive buffer of %1 is full.",
    };
    int cause = m_cause >= 0 && m_cause <= bufferFull ? m_cause : genericException;
    return CopyMessage(Substitute(texts[cause], FileNameOrUnnamed(m_strFileName)), lpszError, nMaxError, pnHelpContext);
}

BOOL CFileException::GetErrorMessage(char* lpszError, UINT nMaxError, PUINT pnHelpContext) const {
    static const char* const texts[] = {
        "No error occurred.",
        "An unknown error occurred while accessing %1.",
        "%1 was not found.",
        "%1 contains an invalid path.",
        "%1 could not be opened because there are too many open files.",
        "Access to %1 was denied.",
        "An invalid file handle was associated with %1.",
        "%1 could not be removed because it is the current directory.",
        "%1 could not be created because the directory is full.",
        "Seek failed on %1",
        "A hardware I/O error was reported while accessing %1.",
        "A sharing violation occurred while accessing %1.",
        "A locking violation occurred while accessing %1.",
        "Disk full while accessing %1.",
        "An attempt was made to access %1 past its end.",
    };
    int cause = m_cause >= 0 && m_cause <= endOfFile ? m_cause : genericException;
    return CopyMessage(Substitute(texts[cause], FileNameOrUnnamed(m_strFileName)), lpszError, nMaxError, pnHelpContext);
}

int CFileException::OsErrorToException(LONG lOsError) {
    switch (lOsError) {
    case 0:
        return none;
    case 2:
    case 6:
    case 18:
        return fileNotFound;
    case 3:
    case 15:
    case 17:
    case 123:
    case 206:
        return badPath;
    case 4:
        return tooManyOpenFiles;
    case 5:
    case 12:
    case 19:
    case 80:
    case 183:
        return accessDenied;
    case 11:
        return invalidFile;
    case 16:
    case 145:
        return removeCurrentDir;
    case 20:
    case 21:
    case 1117:
        return hardIO;
    case 32:
        return sharingViolation;
    case 33:
        return lockViolation;
    case 38:
        return endOfFile;
    case 39:
    case 112:
        return diskFull;
    case 131:
    case 132:
        return badSeek;
    default:
        return genericException;
    }
}

int CFileException::ErrnoToException(int nErrno) {
    switch (nErrno) {
    case 0:
        return none;
    case EPERM:
    case EACCES:
    case EEXIST:
    case EISDIR:
    case EROFS:
        return accessDenied;
    case ENOENT:
        return fileNotFound;
    case EBADF:
        return invalidFile;
    case EMFILE:
    case ENFILE:
        return tooManyOpenFiles;
    case ENOSPC:
        return diskFull;
    case ENOTDIR:
    case ENAMETOOLONG:
    case ELOOP:
        return badPath;
    case EIO:
        return hardIO;
    case ESPIPE:
        return badSeek;
    case EBUSY:
        return sharingViolation;
    default:
        return genericException;
    }
}

void CFileException::ThrowOsError(LONG lOsError, const char* lpszFileName) {
    if (lOsError != 0)
        throw new CFileException(OsErrorToException(lOsError), lOsError, lpszFileName);
}

void CFileException::ThrowErrno(int nErrno, const char* lpszFileName) {
    if (nErrno != 0)
        throw new CFileException(ErrnoToException(nErrno), static_cast<LONG>(ErrnoToWin32(nErrno)), lpszFileName);
}

void AfxThrowMemoryException() { throw new CMemoryException; }

void AfxThrowNotSupportedException() { throw new CNotSupportedException; }

void AfxThrowInvalidArgException() { throw new CInvalidArgException; }

void AfxThrowUserException() { throw new CUserException; }

void AfxThrowResourceException() { throw new CResourceException; }

void AfxThrowArchiveException(int cause, const char* lpszArchiveName) {
    throw new CArchiveException(cause, lpszArchiveName);
}

void AfxThrowFileException(int cause, LONG lOsError, const char* lpszFileName) {
    throw new CFileException(cause, lOsError, lpszFileName);
}

void AfxAbort() {
    fprintf(stderr, "mfcwx: AfxAbort()\n");
    abort();
}

// ---------------------------------------------------------------------------------------------
// Collections

IMPLEMENT_SERIAL(CStringArray, CObject, 0)
IMPLEMENT_SERIAL(CPtrArray, CObject, 0)
IMPLEMENT_SERIAL(CObArray, CObject, 0)
IMPLEMENT_SERIAL(CDWordArray, CObject, 0)
IMPLEMENT_SERIAL(CUIntArray, CObject, 0)
IMPLEMENT_SERIAL(CWordArray, CObject, 0)
IMPLEMENT_SERIAL(CByteArray, CObject, 0)
IMPLEMENT_SERIAL(CStringList, CObject, 0)
IMPLEMENT_SERIAL(CPtrList, CObject, 0)
IMPLEMENT_SERIAL(CObList, CObject, 0)
IMPLEMENT_SERIAL(CMapStringToString, CObject, 0)
IMPLEMENT_SERIAL(CMapStringToPtr, CObject, 0)
IMPLEMENT_SERIAL(CMapStringToOb, CObject, 0)
IMPLEMENT_SERIAL(CMapPtrToPtr, CObject, 0)
IMPLEMENT_SERIAL(CMapPtrToWord, CObject, 0)
IMPLEMENT_SERIAL(CMapWordToPtr, CObject, 0)
IMPLEMENT_SERIAL(CMapWordToOb, CObject, 0)

// ---------------------------------------------------------------------------------------------
// Time

CString CTimeSpan::Format(const char* pFormat) const {
    std::string out;
    char buf[32];
    for (const char* p = pFormat ? pFormat : ""; *p; ++p) {
        if (*p != '%' || !p[1]) {
            out += *p;
            continue;
        }
        switch (*++p) {
        case 'D':
            snprintf(buf, sizeof buf, "%lld", static_cast<long long>(GetDays()));
            out += buf;
            break;
        case 'H':
            snprintf(buf, sizeof buf, "%02ld", static_cast<long>(GetHours()));
            out += buf;
            break;
        case 'M':
            snprintf(buf, sizeof buf, "%02ld", static_cast<long>(GetMinutes()));
            out += buf;
            break;
        case 'S':
            snprintf(buf, sizeof buf, "%02ld", static_cast<long>(GetSeconds()));
            out += buf;
            break;
        default:
            out += *p;
            break;
        }
    }
    return CString(out.c_str(), static_cast<int>(out.size()));
}

CTime::CTime(int nYear, int nMonth, int nDay, int nHour, int nMin, int nSec, int nDST) {
    struct tm tm;
    memset(&tm, 0, sizeof tm);
    tm.tm_year = nYear - 1900;
    tm.tm_mon = nMonth - 1;
    tm.tm_mday = nDay;
    tm.tm_hour = nHour;
    tm.tm_min = nMin;
    tm.tm_sec = nSec;
    tm.tm_isdst = nDST;
    m_time = mktime(&tm);
}

CTime::CTime(const SYSTEMTIME& st, int nDST) : m_time(0) {
    if (st.wYear >= 1900)
        *this = CTime(st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, nDST);
}

CTime::CTime(const FILETIME& ft, int) : m_time(FileTimeToUnix(ft)) {}

CTime CTime::GetCurrentTime() { return CTime(time(nullptr)); }

struct tm* CTime::GetLocalTm(struct tm* ptm) const {
    static thread_local struct tm local;
    if (!ptm)
        ptm = &local;
    return localtime_r(&m_time, ptm);
}

struct tm* CTime::GetGmtTm(struct tm* ptm) const {
    static thread_local struct tm gmt;
    if (!ptm)
        ptm = &gmt;
    return gmtime_r(&m_time, ptm);
}

BOOL CTime::GetAsSystemTime(SYSTEMTIME& st) const {
    struct tm tm;
    if (!GetLocalTm(&tm))
        return FALSE;
    st.wYear = static_cast<WORD>(tm.tm_year + 1900);
    st.wMonth = static_cast<WORD>(tm.tm_mon + 1);
    st.wDayOfWeek = static_cast<WORD>(tm.tm_wday);
    st.wDay = static_cast<WORD>(tm.tm_mday);
    st.wHour = static_cast<WORD>(tm.tm_hour);
    st.wMinute = static_cast<WORD>(tm.tm_min);
    st.wSecond = static_cast<WORD>(tm.tm_sec);
    st.wMilliseconds = 0;
    return TRUE;
}

int CTime::GetYear() const {
    struct tm tm;
    return GetLocalTm(&tm) ? tm.tm_year + 1900 : 0;
}

int CTime::GetMonth() const {
    struct tm tm;
    return GetLocalTm(&tm) ? tm.tm_mon + 1 : 0;
}

int CTime::GetDay() const {
    struct tm tm;
    return GetLocalTm(&tm) ? tm.tm_mday : 0;
}

int CTime::GetHour() const {
    struct tm tm;
    return GetLocalTm(&tm) ? tm.tm_hour : 0;
}

int CTime::GetMinute() const {
    struct tm tm;
    return GetLocalTm(&tm) ? tm.tm_min : 0;
}

int CTime::GetSecond() const {
    struct tm tm;
    return GetLocalTm(&tm) ? tm.tm_sec : 0;
}

int CTime::GetDayOfWeek() const {
    struct tm tm;
    return GetLocalTm(&tm) ? tm.tm_wday + 1 : 0;
}

CString CTime::Format(const char* pFormat) const {
    struct tm tm;
    if (!GetLocalTm(&tm))
        return CString();
    return FormatTm(pFormat, tm);
}

CString CTime::Format(UINT nFormatID) const {
    CString fmt;
    fmt.LoadString(nFormatID);
    return Format(fmt.GetString());
}

CString CTime::FormatGmt(const char* pFormat) const {
    struct tm tm;
    if (!GetGmtTm(&tm))
        return CString();
    return FormatTm(pFormat, tm);
}

COleDateTime::COleDateTime(int nYear, int nMonth, int nDay, int nHour, int nMin, int nSec) : m_dt(0), m_status(valid) {
    if (!OleFromParts(nYear, nMonth, nDay, nHour, nMin, nSec, m_dt)) {
        m_dt = 0;
        m_status = invalid;
    }
}

COleDateTime COleDateTime::GetCurrentTime() {
    time_t now = time(nullptr);
    struct tm tm;
    localtime_r(&now, &tm);
    return COleDateTime(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

int COleDateTime::GetYear() const {
    if (m_status != valid)
        return -1;
    struct tm tm;
    TmFromOle(m_dt, tm);
    return tm.tm_year + 1900;
}

int COleDateTime::GetMonth() const {
    if (m_status != valid)
        return -1;
    struct tm tm;
    TmFromOle(m_dt, tm);
    return tm.tm_mon + 1;
}

int COleDateTime::GetDay() const {
    if (m_status != valid)
        return -1;
    struct tm tm;
    TmFromOle(m_dt, tm);
    return tm.tm_mday;
}

int COleDateTime::GetHour() const {
    if (m_status != valid)
        return -1;
    struct tm tm;
    TmFromOle(m_dt, tm);
    return tm.tm_hour;
}

int COleDateTime::GetMinute() const {
    if (m_status != valid)
        return -1;
    struct tm tm;
    TmFromOle(m_dt, tm);
    return tm.tm_min;
}

int COleDateTime::GetSecond() const {
    if (m_status != valid)
        return -1;
    struct tm tm;
    TmFromOle(m_dt, tm);
    return tm.tm_sec;
}

int COleDateTime::GetDayOfWeek() const {
    if (m_status != valid)
        return -1;
    struct tm tm;
    TmFromOle(m_dt, tm);
    return tm.tm_wday + 1;
}

CString COleDateTime::Format(const char* pFormat) const {
    if (m_status == null)
        return CString();
    if (m_status != valid)
        return CString("Invalid DateTime");
    struct tm tm;
    TmFromOle(m_dt, tm);
    return FormatTm(pFormat, tm);
}

CString COleDateTime::Format(DWORD dwFlags, LCID) const {
    if (m_status == null)
        return CString();
    if (m_status != valid)
        return CString("Invalid DateTime");
    struct tm tm;
    TmFromOle(m_dt, tm);
    bool hasTime = tm.tm_hour || tm.tm_min || tm.tm_sec;
    bool hasDate = static_cast<long long>(std::trunc(m_dt)) != 0;
    bool timeOnly = (dwFlags & 0x2) != 0 || (!hasDate && hasTime);
    bool dateOnly = (dwFlags & 0x4) != 0 || !hasTime;
    if (timeOnly && !(dwFlags & 0x4))
        return FormatTm("%X", tm);
    if (dateOnly)
        return FormatTm("%x", tm);
    return FormatTm("%x %X", tm);
}
