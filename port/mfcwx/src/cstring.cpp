#include "mfcwx/cstring.h"
#include "bridge.h"

#include <algorithm>
#include <cstdlib>
#include <cwchar>
#include <string>

namespace mfcwx {
bool LoadResourceString(UINT id, std::string& out);
}

namespace {

char g_empty[1] = {0};

bool IsAsciiSpace(char ch) { return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f'; }

std::string WideToAnsi(const wchar_t* s) {
    std::string out;
    if (!s)
        return out;
    for (; *s; ++s) {
        char ch;
        mfcwx::UnicodeToAnsi(*s, ch);
        out.push_back(ch);
    }
    return out;
}

} // namespace

const char* AfxTranslateFormat(const char* fmt, char* buffer, size_t size) {
    if (!fmt || !strchr(fmt, 'I'))
        return fmt;
    size_t o = 0;
    for (const char* p = fmt; *p && o + 4 < size;) {
        if (*p != '%') {
            buffer[o++] = *p++;
            continue;
        }
        buffer[o++] = *p++;
        if (*p == '%') {
            buffer[o++] = *p++;
            continue;
        }
        while (*p && strchr("-+ #0123456789.*", *p) && o + 4 < size)
            buffer[o++] = *p++;
        if (p[0] == 'I' && p[1] == '6' && p[2] == '4') {
            buffer[o++] = 'l';
            buffer[o++] = 'l';
            p += 3;
        } else if (p[0] == 'I' && p[1] == '3' && p[2] == '2') {
            p += 3;
        } else if (p[0] == 'I' && strchr("diouxX", p[1])) {
            buffer[o++] = 'z';
            p += 1;
        }
    }
    buffer[o] = 0;
    return buffer;
}

CString::CString() noexcept : m_p(g_empty), m_len(0), m_cap(0) {}

CString::CString(const CString& s) : CString() { Assign(s.m_p, s.m_len); }

CString::CString(CString&& s) noexcept : m_p(s.m_p), m_len(s.m_len), m_cap(s.m_cap) {
    s.m_p = g_empty;
    s.m_len = 0;
    s.m_cap = 0;
}

CString::CString(const char* s) : CString() {
    if (s && IS_INTRESOURCE(s))
        LoadString(static_cast<UINT>(reinterpret_cast<uintptr_t>(s)));
    else if (s)
        Assign(s, static_cast<int>(strlen(s)));
}

CString::CString(const unsigned char* s) : CString(reinterpret_cast<const char*>(s)) {}

CString::CString(const char* s, int length) : CString() {
    if (s && length > 0)
        Assign(s, length);
}

CString::CString(char ch, int repeat) : CString() {
    if (repeat > 0) {
        Reserve(repeat);
        memset(m_p, ch, repeat);
        m_len = repeat;
        m_p[m_len] = 0;
    }
}

CString::CString(const wchar_t* s) : CString() {
    std::string a = WideToAnsi(s);
    Assign(a.data(), static_cast<int>(a.size()));
}

CString::~CString() {
    if (m_p != g_empty)
        free(m_p);
}

CString& CString::operator=(const CString& s) {
    if (this != &s)
        Assign(s.m_p, s.m_len);
    return *this;
}

CString& CString::operator=(CString&& s) noexcept {
    if (this != &s) {
        CString tmp(std::move(s));
        swap(*this, tmp);
    }
    return *this;
}

CString& CString::operator=(const char* s) {
    if (!s) {
        Empty();
        return *this;
    }
    if (s >= m_p && s <= m_p + m_len) {
        CString copy(s);
        swap(*this, copy);
        return *this;
    }
    Assign(s, static_cast<int>(strlen(s)));
    return *this;
}

CString& CString::operator=(const unsigned char* s) { return *this = reinterpret_cast<const char*>(s); }

CString& CString::operator=(const wchar_t* s) {
    std::string a = WideToAnsi(s);
    Assign(a.data(), static_cast<int>(a.size()));
    return *this;
}

CString& CString::operator=(char ch) {
    Assign(&ch, 1);
    return *this;
}

void CString::Reserve(int capacity) {
    if (capacity <= m_cap)
        return;
    int newCap = std::max(capacity, m_cap + m_cap / 2);
    if (newCap < 15)
        newCap = 15;
    char* p = static_cast<char*>(malloc(static_cast<size_t>(newCap) + 1));
    memcpy(p, m_p, static_cast<size_t>(m_len) + 1);
    if (m_p != g_empty)
        free(m_p);
    m_p = p;
    m_cap = newCap;
}

void CString::Assign(const char* s, int length) {
    if (length <= 0) {
        Empty();
        return;
    }
    if (s >= m_p && s < m_p + m_cap + 1) {
        std::string copy(s, static_cast<size_t>(length));
        Reserve(length);
        memcpy(m_p, copy.data(), static_cast<size_t>(length));
    } else {
        Reserve(length);
        memcpy(m_p, s, static_cast<size_t>(length));
    }
    m_len = length;
    m_p[m_len] = 0;
}

void CString::Empty() {
    if (m_p != g_empty)
        free(m_p);
    m_p = g_empty;
    m_len = 0;
    m_cap = 0;
}

char* CString::GetBuffer() {
    Reserve(m_len);
    if (m_p == g_empty)
        Reserve(1);
    return m_p;
}

char* CString::GetBuffer(int minLength) {
    Reserve(std::max(minLength, 1));
    return m_p;
}

char* CString::GetBufferSetLength(int length) {
    Reserve(std::max(length, 1));
    m_len = std::max(length, 0);
    m_p[m_len] = 0;
    return m_p;
}

void CString::ReleaseBuffer(int newLength) {
    if (m_p == g_empty)
        return;
    if (newLength < 0)
        newLength = static_cast<int>(strnlen(m_p, static_cast<size_t>(m_cap) + 1));
    if (newLength > m_cap)
        newLength = m_cap;
    m_len = newLength;
    m_p[m_len] = 0;
}

void CString::Truncate(int length) {
    if (length < m_len && length >= 0) {
        m_len = length;
        m_p[m_len] = 0;
    }
}

void CString::SetString(const char* s) { *this = s; }

void CString::SetString(const char* s, int length) {
    if (!s)
        Empty();
    else
        Assign(s, length);
}

void CString::Append(const char* s, int length) {
    if (!s || length <= 0)
        return;
    if (s >= m_p && s < m_p + m_cap + 1) {
        std::string copy(s, static_cast<size_t>(length));
        Append(copy.data(), length);
        return;
    }
    Reserve(m_len + length);
    memcpy(m_p + m_len, s, static_cast<size_t>(length));
    m_len += length;
    m_p[m_len] = 0;
}

void CString::Append(const char* s) {
    if (s)
        Append(s, static_cast<int>(strlen(s)));
}

CString& CString::operator+=(const CString& s) {
    Append(s.m_p, s.m_len);
    return *this;
}

CString& CString::operator+=(const char* s) {
    Append(s);
    return *this;
}

CString& CString::operator+=(char ch) {
    Append(&ch, 1);
    return *this;
}

CString& CString::operator+=(const wchar_t* s) {
    std::string a = WideToAnsi(s);
    Append(a.data(), static_cast<int>(a.size()));
    return *this;
}

int CString::Compare(const char* s) const { return strcmp(m_p, s ? s : ""); }

int CString::CompareNoCase(const char* s) const { return strcasecmp(m_p, s ? s : ""); }

CString CString::Mid(int first) const { return Mid(first, m_len - first); }

CString CString::Mid(int first, int count) const {
    if (first < 0)
        first = 0;
    if (count < 0)
        count = 0;
    if (first > m_len)
        first = m_len;
    if (first + count > m_len)
        count = m_len - first;
    return CString(m_p + first, count);
}

CString CString::Left(int count) const {
    if (count < 0)
        count = 0;
    return CString(m_p, std::min(count, m_len));
}

CString CString::Right(int count) const {
    if (count < 0)
        count = 0;
    count = std::min(count, m_len);
    return CString(m_p + m_len - count, count);
}

CString CString::SpanIncluding(const char* charSet) const {
    return Left(static_cast<int>(strspn(m_p, charSet ? charSet : "")));
}

CString CString::SpanExcluding(const char* charSet) const {
    return Left(static_cast<int>(strcspn(m_p, charSet ? charSet : "")));
}

CString CString::Tokenize(const char* tokens, int& start) const {
    if (start < 0 || start >= m_len || !tokens) {
        start = -1;
        return CString();
    }
    const char* p = m_p + start;
    p += strspn(p, tokens);
    if (*p == 0) {
        start = -1;
        return CString();
    }
    int len = static_cast<int>(strcspn(p, tokens));
    CString result(p, len);
    start = static_cast<int>(p - m_p) + len + 1;
    return result;
}

CString& CString::MakeUpper() {
    for (int i = 0; i < m_len; ++i)
        if (m_p[i] >= 'a' && m_p[i] <= 'z')
            m_p[i] = static_cast<char>(m_p[i] - 'a' + 'A');
    return *this;
}

CString& CString::MakeLower() {
    for (int i = 0; i < m_len; ++i)
        if (m_p[i] >= 'A' && m_p[i] <= 'Z')
            m_p[i] = static_cast<char>(m_p[i] - 'A' + 'a');
    return *this;
}

CString& CString::MakeReverse() {
    std::reverse(m_p, m_p + m_len);
    return *this;
}

int CString::Replace(char oldCh, char newCh) {
    int count = 0;
    for (int i = 0; i < m_len; ++i)
        if (m_p[i] == oldCh) {
            m_p[i] = newCh;
            ++count;
        }
    return count;
}

int CString::Replace(const char* oldStr, const char* newStr) {
    if (!oldStr || !*oldStr)
        return 0;
    if (!newStr)
        newStr = "";
    std::string src(m_p, static_cast<size_t>(m_len));
    std::string result;
    size_t oldLen = strlen(oldStr);
    size_t pos = 0;
    int count = 0;
    for (;;) {
        size_t hit = src.find(oldStr, pos, oldLen);
        if (hit == std::string::npos)
            break;
        result.append(src, pos, hit - pos);
        result.append(newStr);
        pos = hit + oldLen;
        ++count;
    }
    if (count) {
        result.append(src, pos, std::string::npos);
        Assign(result.data(), static_cast<int>(result.size()));
    }
    return count;
}

int CString::Remove(char ch) {
    int o = 0;
    for (int i = 0; i < m_len; ++i)
        if (m_p[i] != ch)
            m_p[o++] = m_p[i];
    int removed = m_len - o;
    m_len = o;
    if (m_p != g_empty)
        m_p[m_len] = 0;
    return removed;
}

int CString::Insert(int index, char ch) {
    char buf[2] = {ch, 0};
    return Insert(index, buf);
}

int CString::Insert(int index, const char* s) {
    if (index < 0)
        index = 0;
    if (index > m_len)
        index = m_len;
    int add = s ? static_cast<int>(strlen(s)) : 0;
    if (add) {
        std::string copy(s, static_cast<size_t>(add));
        Reserve(m_len + add);
        memmove(m_p + index + add, m_p + index, static_cast<size_t>(m_len - index) + 1);
        memcpy(m_p + index, copy.data(), static_cast<size_t>(add));
        m_len += add;
    }
    return m_len;
}

int CString::Delete(int index, int count) {
    if (index < 0)
        index = 0;
    if (count > 0 && index < m_len) {
        count = std::min(count, m_len - index);
        memmove(m_p + index, m_p + index + count, static_cast<size_t>(m_len - index - count) + 1);
        m_len -= count;
    }
    return m_len;
}

CString& CString::TrimLeft() {
    int i = 0;
    while (i < m_len && IsAsciiSpace(m_p[i]))
        ++i;
    Delete(0, i);
    return *this;
}

CString& CString::TrimLeft(char target) {
    int i = 0;
    while (i < m_len && m_p[i] == target)
        ++i;
    Delete(0, i);
    return *this;
}

CString& CString::TrimLeft(const char* targets) {
    if (!targets)
        return *this;
    int i = 0;
    while (i < m_len && strchr(targets, m_p[i]))
        ++i;
    Delete(0, i);
    return *this;
}

CString& CString::TrimRight() {
    int i = m_len;
    while (i > 0 && IsAsciiSpace(m_p[i - 1]))
        --i;
    Truncate(i);
    return *this;
}

CString& CString::TrimRight(char target) {
    int i = m_len;
    while (i > 0 && m_p[i - 1] == target)
        --i;
    Truncate(i);
    return *this;
}

CString& CString::TrimRight(const char* targets) {
    if (!targets)
        return *this;
    int i = m_len;
    while (i > 0 && strchr(targets, m_p[i - 1]))
        --i;
    Truncate(i);
    return *this;
}

int CString::Find(char ch, int start) const {
    if (start < 0)
        start = 0;
    for (int i = start; i < m_len; ++i)
        if (m_p[i] == ch)
            return i;
    return -1;
}

int CString::Find(const char* sub, int start) const {
    if (!sub || start < 0 || start > m_len)
        return -1;
    const char* hit = strstr(m_p + start, sub);
    return hit ? static_cast<int>(hit - m_p) : -1;
}

int CString::ReverseFind(char ch) const {
    for (int i = m_len - 1; i >= 0; --i)
        if (m_p[i] == ch)
            return i;
    return -1;
}

int CString::FindOneOf(const char* charSet) const {
    if (!charSet)
        return -1;
    for (int i = 0; i < m_len; ++i)
        if (strchr(charSet, m_p[i]))
            return i;
    return -1;
}

void CString::FormatV(const char* fmt, va_list args) {
    char translated[4096];
    fmt = AfxTranslateFormat(fmt, translated, sizeof translated);
    va_list copy;
    va_copy(copy, args);
    int n = vsnprintf(nullptr, 0, fmt, copy);
    va_end(copy);
    if (n <= 0) {
        Empty();
        return;
    }
    std::string buf(static_cast<size_t>(n) + 1, '\0');
    vsnprintf(&buf[0], buf.size(), fmt, args);
    Assign(buf.data(), n);
}

void CString::AppendFormatV(const char* fmt, va_list args) {
    CString tmp;
    tmp.FormatV(fmt, args);
    *this += tmp;
}

void CString::Format(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    FormatV(fmt, args);
    va_end(args);
}

void CString::Format(UINT formatId, ...) {
    CString fmt;
    fmt.LoadString(formatId);
    va_list args;
    va_start(args, formatId);
    FormatV(fmt, args);
    va_end(args);
}

void CString::AppendFormat(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    AppendFormatV(fmt, args);
    va_end(args);
}

void CString::AppendFormat(UINT formatId, ...) {
    CString fmt;
    fmt.LoadString(formatId);
    va_list args;
    va_start(args, formatId);
    AppendFormatV(fmt, args);
    va_end(args);
}

static void FormatMessageImpl(CString& out, const char* fmt, va_list args) {
    // %1..%99 insert arguments, optionally typed as %1!d!; arguments default to strings.
    const char* argv[100] = {};
    std::string result;
    std::string typeSpec[100];
    int maxArg = 0;
    for (const char* p = fmt; *p; ++p) {
        if (*p == '%' && p[1] >= '1' && p[1] <= '9') {
            int n = p[1] - '0';
            const char* q = p + 2;
            if (*q >= '0' && *q <= '9')
                n = n * 10 + (*q++ - '0');
            if (*q == '!') {
                const char* e = strchr(q + 1, '!');
                if (e)
                    typeSpec[n] = std::string(q + 1, e);
            }
            maxArg = std::max(maxArg, n);
        }
    }
    long long values[100] = {};
    for (int i = 1; i <= maxArg; ++i) {
        if (typeSpec[i].empty() || typeSpec[i].back() == 's')
            argv[i] = va_arg(args, const char*);
        else
            values[i] = va_arg(args, int);
    }
    for (const char* p = fmt; *p; ++p) {
        if (*p == '%' && p[1] >= '1' && p[1] <= '9') {
            int n = p[1] - '0';
            const char* q = p + 2;
            if (*q >= '0' && *q <= '9')
                n = n * 10 + (*q++ - '0');
            if (*q == '!') {
                const char* e = strchr(q + 1, '!');
                if (e)
                    q = e + 1;
            }
            if (typeSpec[n].empty() || typeSpec[n].back() == 's') {
                result += argv[n] ? argv[n] : "";
            } else {
                char buf[64];
                std::string f = "%" + typeSpec[n];
                snprintf(buf, sizeof buf, f.c_str(), static_cast<int>(values[n]));
                result += buf;
            }
            p = q - 1;
        } else if (*p == '%' && p[1] == '%') {
            result += '%';
            ++p;
        } else if (*p == '%' && p[1] == 'n') {
            result += '\n';
            ++p;
        } else {
            result += *p;
        }
    }
    out = result.c_str();
}

void CString::FormatMessage(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    FormatMessageImpl(*this, fmt, args);
    va_end(args);
}

void CString::FormatMessage(UINT formatId, ...) {
    CString fmt;
    fmt.LoadString(formatId);
    va_list args;
    va_start(args, formatId);
    FormatMessageImpl(*this, fmt, args);
    va_end(args);
}

BOOL CString::LoadString(UINT id) {
    std::string s;
    if (!mfcwx::LoadResourceString(id, s)) {
        Empty();
        return FALSE;
    }
    Assign(s.data(), static_cast<int>(s.size()));
    return TRUE;
}

BSTR CString::AllocSysString() const {
    wxString w = mfcwx::ToWx(m_p, m_len);
    std::wstring ws = w.ToStdWstring();
    BSTR b = static_cast<BSTR>(malloc((ws.size() + 1) * sizeof(wchar_t)));
    wcscpy(b, ws.c_str());
    return b;
}

BSTR CString::SetSysString(BSTR* bstr) const {
    if (*bstr)
        free(*bstr);
    *bstr = AllocSysString();
    return *bstr;
}

CString operator+(const CString& a, const CString& b) {
    CString r(a);
    r += b;
    return r;
}

CString operator+(const CString& a, const char* b) {
    CString r(a);
    r += b;
    return r;
}

CString operator+(const char* a, const CString& b) {
    CString r(a);
    r += b;
    return r;
}

CString operator+(const CString& a, char b) {
    CString r(a);
    r += b;
    return r;
}

CString operator+(char a, const CString& b) {
    CString r(a);
    r += b;
    return r;
}

CString operator+(const CString& a, const wchar_t* b) {
    CString r(a);
    r += b;
    return r;
}
