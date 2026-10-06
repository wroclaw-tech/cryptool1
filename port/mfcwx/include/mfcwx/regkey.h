#pragma once

// ATL CRegKey, backed by the registry emulation of mfcwx (wxConfig).
class CRegKey {
public:
    CRegKey() : m_hKey(nullptr) {}
    ~CRegKey() { Close(); }
    operator HKEY() const { return m_hKey; }
    LONG Create(HKEY hKeyParent, const char* lpszKeyName, LPSTR lpszClass = nullptr,
                DWORD dwOptions = REG_OPTION_NON_VOLATILE, REGSAM samDesired = KEY_READ | KEY_WRITE,
                LPSECURITY_ATTRIBUTES lpSecAttr = nullptr, LPDWORD lpdwDisposition = nullptr);
    LONG Open(HKEY hKeyParent, const char* lpszKeyName, REGSAM samDesired = KEY_READ | KEY_WRITE);
    LONG Close();
    LONG QueryDWORDValue(const char* pszValueName, DWORD& dwValue);
    LONG QueryStringValue(const char* pszValueName, LPSTR pszValue, ULONG* pnChars);
    LONG QueryValue(DWORD& dwValue, const char* lpszValueName) { return QueryDWORDValue(lpszValueName, dwValue); }
    LONG QueryValue(unsigned long& dwValue, const char* lpszValueName) {
        DWORD v = 0;
        LONG r = QueryDWORDValue(lpszValueName, v);
        if (r == ERROR_SUCCESS)
            dwValue = v;
        return r;
    }
    LONG QueryValue(LPSTR szValue, const char* lpszValueName, unsigned long* pdwCount) {
        DWORD n = pdwCount ? static_cast<DWORD>(*pdwCount) : 0;
        LONG r = QueryValue(szValue, lpszValueName, &n);
        if (pdwCount)
            *pdwCount = n;
        return r;
    }
    LONG QueryValue(LPSTR szValue, const char* lpszValueName, DWORD* pdwCount);
    LONG QueryBinaryValue(const char* pszValueName, void* pValue, ULONG* pnBytes);
    LONG SetDWORDValue(const char* pszValueName, DWORD dwValue);
    LONG SetStringValue(const char* pszValueName, const char* pszValue, DWORD dwType = REG_SZ);
    LONG SetValue(DWORD dwValue, const char* lpszValueName) { return SetDWORDValue(lpszValueName, dwValue); }
    LONG SetValue(unsigned long dwValue, const char* lpszValueName) { return SetDWORDValue(lpszValueName, static_cast<DWORD>(dwValue)); }
    LONG SetValue(int dwValue, const char* lpszValueName) { return SetDWORDValue(lpszValueName, static_cast<DWORD>(dwValue)); }
    LONG SetValue(const char* lpszValue, const char* lpszValueName = nullptr) { return SetStringValue(lpszValueName, lpszValue); }
    LONG SetBinaryValue(const char* pszValueName, const void* pValue, ULONG nBytes);
    LONG DeleteValue(const char* lpszValue);
    LONG DeleteSubKey(const char* lpszSubKey);
    LONG RecurseDeleteKey(const char* lpszKey) { return DeleteSubKey(lpszKey); }
    HKEY m_hKey;
};
