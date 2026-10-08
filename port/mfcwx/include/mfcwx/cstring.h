#pragma once

#include "mfcwx/win32.h"

#include <utility>

// MBCS CString: a byte string in the application's ANSI code page (see mfcwx/codepage.h).
class CString {
public:
    CString() noexcept;
    CString(const CString& s);
    CString(CString&& s) noexcept;
    CString(const char* s);
    CString(const unsigned char* s);
    CString(const char* s, int length);
    CString(char ch, int repeat = 1);
    CString(const wchar_t* s);
    ~CString();

    CString& operator=(const CString& s);
    CString& operator=(CString&& s) noexcept;
    CString& operator=(const char* s);
    CString& operator=(const unsigned char* s);
    CString& operator=(const wchar_t* s);
    CString& operator=(char ch);

    int GetLength() const { return m_len; }
    int GetAllocLength() const { return m_cap; }
    bool IsEmpty() const { return m_len == 0; }
    void Empty();
    char GetAt(int i) const { return m_p[i]; }
    void SetAt(int i, char ch) { m_p[i] = ch; }
    char operator[](int i) const { return m_p[i]; }
    operator const char*() const { return m_p; }
    const char* GetString() const { return m_p; }

    char* GetBuffer();
    char* GetBuffer(int minLength);
    char* GetBufferSetLength(int length);
    void ReleaseBuffer(int newLength = -1);
    void ReleaseBufferSetLength(int newLength) { ReleaseBuffer(newLength); }
    char* LockBuffer() { return GetBuffer(); }
    void UnlockBuffer() {}
    void FreeExtra() {}
    void Preallocate(int length) { Reserve(length); }
    void Truncate(int length);
    void SetString(const char* s);
    void SetString(const char* s, int length);

    CString& operator+=(const CString& s);
    CString& operator+=(const char* s);
    CString& operator+=(const unsigned char* s) { return *this += reinterpret_cast<const char*>(s); }
    CString& operator+=(char ch);
    CString& operator+=(unsigned char ch) { return *this += static_cast<char>(ch); }
    CString& operator+=(const wchar_t* s);
    void Append(const char* s);
    void Append(const char* s, int length);
    void Append(const CString& s) { *this += s; }
    void AppendChar(char ch) { *this += ch; }

    int Compare(const char* s) const;
    int CompareNoCase(const char* s) const;
    int Collate(const char* s) const { return Compare(s); }
    int CollateNoCase(const char* s) const { return CompareNoCase(s); }

    CString Mid(int first) const;
    CString Mid(int first, int count) const;
    CString Left(int count) const;
    CString Right(int count) const;
    CString SpanIncluding(const char* charSet) const;
    CString SpanExcluding(const char* charSet) const;
    CString Tokenize(const char* tokens, int& start) const;

    CString& MakeUpper();
    CString& MakeLower();
    CString& MakeReverse();
    int Replace(char oldCh, char newCh);
    int Replace(const char* oldStr, const char* newStr);
    int Remove(char ch);
    int Insert(int index, char ch);
    int Insert(int index, const char* s);
    int Delete(int index, int count = 1);

    CString& TrimLeft();
    CString& TrimLeft(char target);
    CString& TrimLeft(const char* targets);
    CString& TrimRight();
    CString& TrimRight(char target);
    CString& TrimRight(const char* targets);
    CString& Trim() { TrimRight(); return TrimLeft(); }
    CString& Trim(char target) { TrimRight(target); return TrimLeft(target); }
    CString& Trim(const char* targets) { TrimRight(targets); return TrimLeft(targets); }

    int Find(char ch, int start = 0) const;
    int Find(const char* sub, int start = 0) const;
    int ReverseFind(char ch) const;
    int FindOneOf(const char* charSet) const;

    void Format(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    void Format(UINT formatId, ...);
    void FormatV(const char* fmt, va_list args);
    void AppendFormat(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    void AppendFormat(UINT formatId, ...);
    void AppendFormatV(const char* fmt, va_list args);
    void FormatMessage(const char* fmt, ...);
    void FormatMessage(UINT formatId, ...);

    BOOL LoadString(UINT id);
    BOOL LoadStringA(UINT id) { return LoadString(id); }
    BSTR AllocSysString() const;
    BSTR SetSysString(BSTR* bstr) const;
    void AnsiToOem() {}
    void OemToAnsi() {}

    static int StringLength(const char* s) { return s ? static_cast<int>(strlen(s)) : 0; }

    friend void swap(CString& a, CString& b) noexcept {
        std::swap(a.m_p, b.m_p);
        std::swap(a.m_len, b.m_len);
        std::swap(a.m_cap, b.m_cap);
    }

private:
    void Reserve(int capacity);
    void Assign(const char* s, int length);

    char* m_p;
    int m_len;
    int m_cap;
};

CString operator+(const CString& a, const CString& b);
CString operator+(const CString& a, const char* b);
CString operator+(const char* a, const CString& b);
CString operator+(const CString& a, char b);
CString operator+(char a, const CString& b);
CString operator+(const CString& a, const wchar_t* b);

inline bool operator==(const CString& a, const CString& b) { return a.Compare(b) == 0; }
inline bool operator==(const CString& a, const char* b) { return a.Compare(b) == 0; }
inline bool operator==(const char* a, const CString& b) { return b.Compare(a) == 0; }
inline bool operator==(const CString& a, char b) { return a.GetLength() == 1 && a[0] == b; }
inline bool operator!=(const CString& a, const CString& b) { return a.Compare(b) != 0; }
inline bool operator!=(const CString& a, const char* b) { return a.Compare(b) != 0; }
inline bool operator!=(const char* a, const CString& b) { return b.Compare(a) != 0; }
inline bool operator!=(const CString& a, char b) { return !(a == b); }
inline bool operator<(const CString& a, const CString& b) { return a.Compare(b) < 0; }
inline bool operator<(const CString& a, const char* b) { return a.Compare(b) < 0; }
inline bool operator<(const char* a, const CString& b) { return b.Compare(a) > 0; }
inline bool operator>(const CString& a, const CString& b) { return a.Compare(b) > 0; }
inline bool operator>(const CString& a, const char* b) { return a.Compare(b) > 0; }
inline bool operator>(const char* a, const CString& b) { return b.Compare(a) < 0; }
inline bool operator<=(const CString& a, const CString& b) { return a.Compare(b) <= 0; }
inline bool operator<=(const CString& a, const char* b) { return a.Compare(b) <= 0; }
inline bool operator<=(const char* a, const CString& b) { return b.Compare(a) >= 0; }
inline bool operator>=(const CString& a, const CString& b) { return a.Compare(b) >= 0; }
inline bool operator>=(const CString& a, const char* b) { return a.Compare(b) >= 0; }
inline bool operator>=(const char* a, const CString& b) { return b.Compare(a) <= 0; }

typedef CString CStringA;
typedef CString CStringT;

// Translates Microsoft-specific printf conversions (%I64d, %I32u, ...) to their C99 forms.
const char* AfxTranslateFormat(const char* fmt, char* buffer, size_t size);
