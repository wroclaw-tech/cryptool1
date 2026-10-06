#include <wx/fileconf.h>
#include <wx/log.h>
#include <wx/string.h>

#include "afxwin.h"
#include "bridge.h"
#include "runtime.h"

#include <algorithm>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

#undef fopen
#undef rename
#undef remove

namespace mfcwx {
namespace {

// ---------------------------------------------------------------------------------------------
// INI files

struct IniFile {
    std::vector<std::string> lines;
    std::string eol = "\r\n";
};

std::mutex& IniLock() {
    static std::mutex* m = new std::mutex;
    return *m;
}

std::string Trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos)
        return std::string();
    size_t e = s.find_last_not_of(" \t");
    return s.substr(b, e - b + 1);
}

void MakeDirectories(const std::string& dir) {
    for (size_t pos = 1; pos != std::string::npos;) {
        pos = dir.find('/', pos + 1);
        mkdir(dir.substr(0, pos).c_str(), 0755);
    }
}

std::string IniPath(LPCSTR file) {
    std::string f = file ? file : "win.ini";
    if (f.find_first_of("\\/") == std::string::npos) {
        std::string dir = UserConfigDirectory();
        MakeDirectories(dir);
        return dir + "/" + NativePath(f.c_str());
    }
    return FsPath(f.c_str());
}

bool LoadIni(const std::string& path, IniFile& ini) {
    FILE* f = ::fopen(path.c_str(), "rb");
    if (!f)
        return false;
    std::string data;
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0)
        data.append(buf, n);
    fclose(f);
    if (data.size() >= 3 && static_cast<unsigned char>(data[0]) == 0xEF && static_cast<unsigned char>(data[1]) == 0xBB &&
        static_cast<unsigned char>(data[2]) == 0xBF)
        data.erase(0, 3);
    size_t firstNl = data.find('\n');
    ini.eol = firstNl != std::string::npos && firstNl > 0 && data[firstNl - 1] == '\r' ? "\r\n"
              : firstNl != std::string::npos                                      ? "\n"
                                                                                  : "\r\n";
    size_t start = 0;
    while (start < data.size()) {
        size_t nl = data.find('\n', start);
        std::string line = data.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        ini.lines.push_back(line);
        if (nl == std::string::npos)
            break;
        start = nl + 1;
    }
    return true;
}

bool SaveIni(const std::string& path, const IniFile& ini) {
    FILE* f = ::fopen(path.c_str(), "wb");
    if (!f) {
        SetLastErrorFromErrno(errno);
        return false;
    }
    for (const std::string& line : ini.lines) {
        fwrite(line.data(), 1, line.size(), f);
        fwrite(ini.eol.data(), 1, ini.eol.size(), f);
    }
    return fclose(f) == 0;
}

bool SectionName(const std::string& line, std::string& name) {
    std::string t = Trim(line);
    if (t.empty() || t[0] != '[')
        return false;
    size_t close = t.find(']');
    name = Trim(t.substr(1, close == std::string::npos ? std::string::npos : close - 1));
    return true;
}

bool KeyValue(const std::string& line, std::string& key, std::string& value) {
    std::string t = Trim(line);
    if (t.empty() || t[0] == ';')
        return false;
    size_t eq = t.find('=');
    key = Trim(t.substr(0, eq));
    value = eq == std::string::npos ? std::string() : Trim(t.substr(eq + 1));
    return true;
}

// Returns the index of the section header line and sets end to the first line after the section.
long FindSection(const IniFile& ini, const char* app, size_t& end) {
    std::string name;
    for (size_t i = 0; i < ini.lines.size(); ++i) {
        if (SectionName(ini.lines[i], name) && strcasecmp(name.c_str(), app) == 0) {
            end = i + 1;
            std::string other;
            while (end < ini.lines.size() && !SectionName(ini.lines[end], other))
                ++end;
            return static_cast<long>(i);
        }
    }
    return -1;
}

DWORD CopyList(const std::vector<std::string>& items, LPSTR ret, DWORD size) {
    if (!ret || size == 0)
        return 0;
    if (size == 1) {
        ret[0] = 0;
        return 0;
    }
    DWORD o = 0;
    for (const std::string& item : items) {
        if (o + item.size() + 1 > size - 1) {
            size_t room = size - 1 - o;
            if (room > 0) {
                memcpy(ret + o, item.data(), room - 1);
                ret[o + room - 1] = 0;
            }
            ret[size - 2] = 0;
            ret[size - 1] = 0;
            return size - 2;
        }
        memcpy(ret + o, item.c_str(), item.size() + 1);
        o += static_cast<DWORD>(item.size() + 1);
    }
    ret[o] = 0;
    return o;
}

DWORD CopyString(const std::string& s, LPSTR ret, DWORD size) {
    if (!ret || size == 0)
        return 0;
    size_t n = std::min(s.size(), static_cast<size_t>(size - 1));
    memcpy(ret, s.data(), n);
    ret[n] = 0;
    return static_cast<DWORD>(n);
}

// ---------------------------------------------------------------------------------------------
// Registry

constexpr unsigned kKeyMagic = 0x5245474Bu;

struct RegKey {
    unsigned magic;
    std::string path;
};

std::mutex& RegLock() {
    static std::mutex* m = new std::mutex;
    return *m;
}

std::set<RegKey*>& RegKeys() {
    static auto* s = new std::set<RegKey*>;
    return *s;
}

std::string RegistryFile() {
    const char* env = getenv("MFCWX_REGISTRY_FILE");
    if (env && *env)
        return env;
#ifdef __APPLE__
    return UserConfigDirectory() + "/CrypTool1.ini";
#else
    return UserConfigDirectory() + "/cryptool1.ini";
#endif
}

wxFileConfig* Config() {
    static wxFileConfig* config = nullptr;
    if (!config) {
        std::string path = RegistryFile();
        size_t slash = path.rfind('/');
        if (slash != std::string::npos && slash > 0)
            MakeDirectories(path.substr(0, slash));
        wxLogNull noLog;
        config = new wxFileConfig(wxEmptyString, wxEmptyString, wxString::FromUTF8(path.c_str()), wxEmptyString,
                                  wxCONFIG_USE_LOCAL_FILE);
        config->SetExpandEnvVars(false);
        config->SetRecordDefaults(false);
    }
    return config;
}

void Flush() {
    wxLogNull noLog;
    Config()->Flush();
}

std::string Escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '%')
            out += "%25";
        else if (c == '/')
            out += "%2F";
        else
            out += c;
    }
    return out;
}

std::string Unescape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            if (s.compare(i, 3, "%25") == 0) {
                out += '%';
                i += 2;
                continue;
            }
            if (s.compare(i, 3, "%2F") == 0) {
                out += '/';
                i += 2;
                continue;
            }
        }
        out += s[i];
    }
    return out;
}

const char* RootName(HKEY key) {
    switch (reinterpret_cast<uintptr_t>(key)) {
    case 0x80000000u:
        return "HKEY_CLASSES_ROOT";
    case 0x80000001u:
        return "HKEY_CURRENT_USER";
    case 0x80000002u:
        return "HKEY_LOCAL_MACHINE";
    case 0x80000003u:
        return "HKEY_USERS";
    case 0x80000005u:
        return "HKEY_CURRENT_CONFIG";
    default:
        return nullptr;
    }
}

// Must be called with RegLock held. Returns false for an invalid handle.
bool BasePath(HKEY key, std::string& path) {
    if (const char* root = RootName(key)) {
        path = std::string("/") + root;
        return true;
    }
    auto* k = reinterpret_cast<RegKey*>(key);
    if (!RegKeys().count(k))
        return false;
    path = k->path;
    return true;
}

std::string Append(const std::string& base, LPCSTR sub) {
    std::string path = base;
    if (!sub)
        return path;
    std::string s = sub;
    size_t start = 0;
    while (start <= s.size()) {
        size_t sep = s.find('\\', start);
        std::string part = s.substr(start, sep == std::string::npos ? std::string::npos : sep - start);
        if (!part.empty())
            path += "/" + Escape(part);
        if (sep == std::string::npos)
            break;
        start = sep + 1;
    }
    return path;
}

bool IsRootPath(const std::string& path) { return std::count(path.begin(), path.end(), '/') == 1; }

wxString WxPath(const std::string& path) { return ToWx(path.c_str()); }

std::string ValueEntry(const std::string& keyPath, LPCSTR name) {
    return keyPath + "/" + (name && *name ? Escape(name) : std::string("@"));
}

HKEY NewKey(const std::string& path) {
    auto* k = new RegKey{kKeyMagic, path};
    RegKeys().insert(k);
    return reinterpret_cast<HKEY>(k);
}

std::string ToHex(const BYTE* data, size_t n) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < n; ++i) {
        out += digits[data[i] >> 4];
        out += digits[data[i] & 15];
    }
    return out;
}

std::vector<BYTE> FromHex(const std::string& s) {
    std::vector<BYTE> out;
    auto val = [](char c) { return c >= 'a' ? c - 'a' + 10 : c >= 'A' ? c - 'A' + 10 : c - '0'; };
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back(static_cast<BYTE>(val(s[i]) * 16 + val(s[i + 1])));
    return out;
}

void Decode(const wxString& raw, DWORD& type, std::vector<BYTE>& bytes) {
    std::string utf8(raw.utf8_str());
    auto text = [&](size_t prefix) {
        std::string ansi = FromWx(raw.Mid(prefix));
        bytes.assign(ansi.begin(), ansi.end());
        bytes.push_back(0);
    };
    if (utf8.compare(0, 3, "sz:") == 0) {
        type = REG_SZ;
        text(3);
    } else if (utf8.compare(0, 7, "expand:") == 0) {
        type = REG_EXPAND_SZ;
        text(7);
    } else if (utf8.compare(0, 6, "dword:") == 0) {
        type = REG_DWORD;
        DWORD v = static_cast<DWORD>(strtoul(utf8.c_str() + 6, nullptr, 16));
        bytes.resize(4);
        memcpy(bytes.data(), &v, 4);
    } else if (utf8.compare(0, 4, "hex:") == 0) {
        type = REG_BINARY;
        bytes = FromHex(utf8.substr(4));
    } else if (utf8.size() > 1 && utf8[0] == 't' && utf8.find(':') != std::string::npos) {
        type = static_cast<DWORD>(strtoul(utf8.c_str() + 1, nullptr, 10));
        bytes = FromHex(utf8.substr(utf8.find(':') + 1));
    } else {
        type = REG_SZ;
        text(0);
    }
}

wxString Encode(DWORD type, const BYTE* data, DWORD size) {
    if (type == REG_SZ || type == REG_EXPAND_SZ) {
        const char* s = reinterpret_cast<const char*>(data);
        size_t n = s ? strnlen(s, size) : 0;
        return wxString(type == REG_SZ ? "sz:" : "expand:") + ToWx(s ? s : "", static_cast<int>(n));
    }
    if (type == REG_DWORD) {
        DWORD v = 0;
        memcpy(&v, data, 4);
        return wxString::Format("dword:%08x", static_cast<unsigned>(v));
    }
    std::string hex = data ? ToHex(data, size) : std::string();
    if (type == REG_BINARY)
        return wxString("hex:") + wxString::FromUTF8(hex.c_str());
    return wxString::Format("t%u:", static_cast<unsigned>(type)) + wxString::FromUTF8(hex.c_str());
}

} // namespace
} // namespace mfcwx

using namespace mfcwx;

DWORD GetPrivateProfileStringA(LPCSTR app, LPCSTR key, LPCSTR def, LPSTR ret, DWORD size, LPCSTR file) {
    std::lock_guard<std::mutex> lk(IniLock());
    IniFile ini;
    LoadIni(IniPath(file), ini);
    if (!app) {
        std::vector<std::string> sections;
        std::string name;
        for (const std::string& line : ini.lines)
            if (SectionName(line, name))
                sections.push_back(name);
        return CopyList(sections, ret, size);
    }
    size_t end = 0;
    long start = FindSection(ini, app, end);
    if (!key) {
        std::vector<std::string> keys;
        std::string k, v;
        for (size_t i = start < 0 ? end : static_cast<size_t>(start) + 1; start >= 0 && i < end; ++i)
            if (KeyValue(ini.lines[i], k, v))
                keys.push_back(k);
        return CopyList(keys, ret, size);
    }
    if (start >= 0) {
        std::string k, v;
        for (size_t i = static_cast<size_t>(start) + 1; i < end; ++i) {
            if (KeyValue(ini.lines[i], k, v) && strcasecmp(k.c_str(), key) == 0) {
                if (v.size() >= 2 && (v[0] == '"' || v[0] == '\'') && v.back() == v[0])
                    v = v.substr(1, v.size() - 2);
                return CopyString(v, ret, size);
            }
        }
    }
    std::string d = def ? def : "";
    while (!d.empty() && d.back() == ' ')
        d.pop_back();
    return CopyString(d, ret, size);
}

UINT GetPrivateProfileIntA(LPCSTR app, LPCSTR key, INT def, LPCSTR file) {
    char buf[64];
    GetPrivateProfileStringA(app, key, "", buf, sizeof buf, file);
    if (!buf[0])
        return static_cast<UINT>(def);
    return static_cast<UINT>(strtol(buf, nullptr, 10));
}

DWORD GetPrivateProfileSectionA(LPCSTR app, LPSTR ret, DWORD size, LPCSTR file) {
    std::lock_guard<std::mutex> lk(IniLock());
    IniFile ini;
    LoadIni(IniPath(file), ini);
    std::vector<std::string> items;
    size_t end = 0;
    long start = app ? FindSection(ini, app, end) : -1;
    for (size_t i = start < 0 ? end : static_cast<size_t>(start) + 1; start >= 0 && i < end; ++i) {
        std::string t = Trim(ini.lines[i]);
        if (!t.empty() && t[0] != ';')
            items.push_back(t);
    }
    return CopyList(items, ret, size);
}

BOOL WritePrivateProfileStringA(LPCSTR app, LPCSTR key, LPCSTR value, LPCSTR file) {
    if (!app) {
        if (!key && !value)
            return TRUE;
        SetLastError(87);
        return FALSE;
    }
    std::lock_guard<std::mutex> lk(IniLock());
    std::string path = IniPath(file);
    IniFile ini;
    LoadIni(path, ini);
    size_t end = 0;
    long start = FindSection(ini, app, end);
    if (!key) {
        if (start < 0)
            return TRUE;
        ini.lines.erase(ini.lines.begin() + start, ini.lines.begin() + static_cast<long>(end));
        return SaveIni(path, ini) ? TRUE : FALSE;
    }
    std::string entry = std::string(key) + "=" + (value ? value : "");
    if (start < 0) {
        if (!value)
            return TRUE;
        ini.lines.push_back(std::string("[") + app + "]");
        ini.lines.push_back(entry);
        return SaveIni(path, ini) ? TRUE : FALSE;
    }
    size_t insertAt = static_cast<size_t>(start) + 1;
    std::string k, v;
    for (size_t i = static_cast<size_t>(start) + 1; i < end; ++i) {
        if (!Trim(ini.lines[i]).empty())
            insertAt = i + 1;
        if (KeyValue(ini.lines[i], k, v) && strcasecmp(k.c_str(), key) == 0) {
            if (value)
                ini.lines[i] = entry;
            else
                ini.lines.erase(ini.lines.begin() + static_cast<long>(i));
            return SaveIni(path, ini) ? TRUE : FALSE;
        }
    }
    if (!value)
        return TRUE;
    ini.lines.insert(ini.lines.begin() + static_cast<long>(insertAt), entry);
    return SaveIni(path, ini) ? TRUE : FALSE;
}

LONG RegOpenKeyExA(HKEY key, LPCSTR sub, DWORD, REGSAM, PHKEY result) {
    if (!result)
        return 87;
    std::lock_guard<std::mutex> lk(RegLock());
    std::string base;
    if (!BasePath(key, base))
        return ERROR_INVALID_HANDLE;
    std::string path = Append(base, sub);
    if (!IsRootPath(path) && !Config()->HasGroup(WxPath(path)))
        return ERROR_FILE_NOT_FOUND;
    *result = NewKey(path);
    return ERROR_SUCCESS;
}

LONG RegOpenKeyA(HKEY key, LPCSTR sub, PHKEY result) { return RegOpenKeyExA(key, sub, 0, KEY_ALL_ACCESS, result); }

LONG RegCreateKeyExA(HKEY key, LPCSTR sub, DWORD, LPSTR, DWORD, REGSAM, LPSECURITY_ATTRIBUTES, PHKEY result,
                     LPDWORD disposition) {
    if (!result)
        return 87;
    std::lock_guard<std::mutex> lk(RegLock());
    std::string base;
    if (!BasePath(key, base))
        return ERROR_INVALID_HANDLE;
    std::string path = Append(base, sub);
    bool existed = IsRootPath(path) || Config()->HasGroup(WxPath(path));
    if (!existed) {
        wxString old = Config()->GetPath();
        Config()->SetPath(WxPath(path));
        Config()->SetPath(old.empty() ? wxString("/") : old);
        Flush();
    }
    if (disposition)
        *disposition = existed ? REG_OPENED_EXISTING_KEY : REG_CREATED_NEW_KEY;
    *result = NewKey(path);
    return ERROR_SUCCESS;
}

LONG RegCreateKeyA(HKEY key, LPCSTR sub, PHKEY result) {
    return RegCreateKeyExA(key, sub, 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, result, nullptr);
}

LONG RegCloseKey(HKEY key) {
    if (RootName(key))
        return ERROR_SUCCESS;
    std::lock_guard<std::mutex> lk(RegLock());
    auto* k = reinterpret_cast<RegKey*>(key);
    if (!RegKeys().erase(k))
        return ERROR_INVALID_HANDLE;
    k->magic = 0;
    delete k;
    return ERROR_SUCCESS;
}

LONG RegQueryValueExA(HKEY key, LPCSTR name, LPDWORD, LPDWORD type, LPBYTE data, LPDWORD size) {
    std::lock_guard<std::mutex> lk(RegLock());
    std::string base;
    if (!BasePath(key, base))
        return ERROR_INVALID_HANDLE;
    wxString entry = WxPath(ValueEntry(base, name));
    if (!Config()->HasEntry(entry))
        return ERROR_FILE_NOT_FOUND;
    wxString raw;
    Config()->Read(entry, &raw);
    DWORD t;
    std::vector<BYTE> bytes;
    Decode(raw, t, bytes);
    if (type)
        *type = t;
    if (!size)
        return data ? 87 : ERROR_SUCCESS;
    DWORD needed = static_cast<DWORD>(bytes.size());
    if (!data) {
        *size = needed;
        return ERROR_SUCCESS;
    }
    if (*size < needed) {
        *size = needed;
        return ERROR_MORE_DATA;
    }
    if (needed)
        memcpy(data, bytes.data(), needed);
    *size = needed;
    return ERROR_SUCCESS;
}

LONG RegSetValueExA(HKEY key, LPCSTR name, DWORD, DWORD type, const BYTE* data, DWORD size) {
    if (type == REG_DWORD && (!data || size < 4))
        return 87;
    if (!data && size)
        return 87;
    std::lock_guard<std::mutex> lk(RegLock());
    std::string base;
    if (!BasePath(key, base))
        return ERROR_INVALID_HANDLE;
    wxLogNull noLog;
    if (!Config()->Write(WxPath(ValueEntry(base, name)), Encode(type, data, size)))
        return ERROR_ACCESS_DENIED;
    Config()->SetPath("/");
    Flush();
    return ERROR_SUCCESS;
}

LONG RegDeleteValueA(HKEY key, LPCSTR name) {
    std::lock_guard<std::mutex> lk(RegLock());
    std::string base;
    if (!BasePath(key, base))
        return ERROR_INVALID_HANDLE;
    wxString entry = WxPath(ValueEntry(base, name));
    if (!Config()->HasEntry(entry))
        return ERROR_FILE_NOT_FOUND;
    Config()->DeleteEntry(entry, false);
    Config()->SetPath("/");
    Flush();
    return ERROR_SUCCESS;
}

LONG RegDeleteKeyA(HKEY key, LPCSTR sub) {
    if (!sub || !*sub)
        return 87;
    std::lock_guard<std::mutex> lk(RegLock());
    std::string base;
    if (!BasePath(key, base))
        return ERROR_INVALID_HANDLE;
    std::string path = Append(base, sub);
    if (IsRootPath(path) || !Config()->HasGroup(WxPath(path)))
        return ERROR_FILE_NOT_FOUND;
    Config()->DeleteGroup(WxPath(path));
    Config()->SetPath("/");
    Flush();
    return ERROR_SUCCESS;
}

LONG RegEnumKeyExA(HKEY key, DWORD index, LPSTR name, LPDWORD nameSize, LPDWORD, LPSTR cls, LPDWORD clsSize,
                   PFILETIME lastWrite) {
    if (!name || !nameSize)
        return 87;
    std::vector<std::string> names;
    {
        std::lock_guard<std::mutex> lk(RegLock());
        std::string base;
        if (!BasePath(key, base))
            return ERROR_INVALID_HANDLE;
        wxFileConfig* config = Config();
        if (!config->HasGroup(WxPath(base)))
            return 259;
        wxString old = config->GetPath();
        config->SetPath(WxPath(base));
        wxString group;
        long cookie;
        for (bool more = config->GetFirstGroup(group, cookie); more; more = config->GetNextGroup(group, cookie))
            names.push_back(Unescape(FromWx(group)));
        config->SetPath(old.empty() ? wxString("/") : old);
    }
    std::sort(names.begin(), names.end(),
              [](const std::string& a, const std::string& b) { return strcasecmp(a.c_str(), b.c_str()) < 0; });
    if (index >= names.size())
        return 259;
    const std::string& n = names[index];
    if (*nameSize <= n.size()) {
        *nameSize = static_cast<DWORD>(n.size() + 1);
        return ERROR_MORE_DATA;
    }
    memcpy(name, n.c_str(), n.size() + 1);
    *nameSize = static_cast<DWORD>(n.size());
    if (cls && clsSize && *clsSize)
        cls[0] = 0;
    if (clsSize)
        *clsSize = 0;
    if (lastWrite)
        GetSystemTimeAsFileTime(lastWrite);
    return ERROR_SUCCESS;
}

LONG CRegKey::Create(HKEY hKeyParent, const char* lpszKeyName, LPSTR lpszClass, DWORD dwOptions, REGSAM samDesired,
                     LPSECURITY_ATTRIBUTES lpSecAttr, LPDWORD lpdwDisposition) {
    HKEY hKey = nullptr;
    LONG r = RegCreateKeyExA(hKeyParent, lpszKeyName, 0, lpszClass, dwOptions, samDesired, lpSecAttr, &hKey,
                             lpdwDisposition);
    if (r == ERROR_SUCCESS) {
        Close();
        m_hKey = hKey;
    }
    return r;
}

LONG CRegKey::Open(HKEY hKeyParent, const char* lpszKeyName, REGSAM samDesired) {
    HKEY hKey = nullptr;
    LONG r = RegOpenKeyExA(hKeyParent, lpszKeyName, 0, samDesired, &hKey);
    if (r == ERROR_SUCCESS) {
        Close();
        m_hKey = hKey;
    }
    return r;
}

LONG CRegKey::Close() {
    LONG r = ERROR_SUCCESS;
    if (m_hKey) {
        r = RegCloseKey(m_hKey);
        m_hKey = nullptr;
    }
    return r;
}

LONG CRegKey::QueryDWORDValue(const char* pszValueName, DWORD& dwValue) {
    DWORD type = 0;
    DWORD size = sizeof(DWORD);
    DWORD value = 0;
    LONG r = RegQueryValueExA(m_hKey, pszValueName, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size);
    if (r != ERROR_SUCCESS)
        return r;
    if (type != REG_DWORD)
        return 13;
    dwValue = value;
    return ERROR_SUCCESS;
}

LONG CRegKey::QueryStringValue(const char* pszValueName, LPSTR pszValue, ULONG* pnChars) {
    if (!pnChars)
        return 87;
    DWORD type = 0;
    DWORD size = *pnChars;
    LONG r = RegQueryValueExA(m_hKey, pszValueName, nullptr, &type, reinterpret_cast<LPBYTE>(pszValue), &size);
    if (r != ERROR_SUCCESS && r != ERROR_MORE_DATA)
        return r;
    if (type != REG_SZ && type != REG_EXPAND_SZ)
        return 13;
    *pnChars = size;
    return r;
}

LONG CRegKey::QueryValue(LPSTR szValue, const char* lpszValueName, DWORD* pdwCount) {
    if (!pdwCount)
        return 87;
    DWORD type = 0;
    LONG r = RegQueryValueExA(m_hKey, lpszValueName, nullptr, &type, reinterpret_cast<LPBYTE>(szValue), pdwCount);
    if (r == ERROR_SUCCESS && type != REG_SZ && type != REG_EXPAND_SZ && type != 7)
        return 13;
    return r;
}

LONG CRegKey::QueryBinaryValue(const char* pszValueName, void* pValue, ULONG* pnBytes) {
    if (!pnBytes)
        return 87;
    DWORD type = 0;
    DWORD size = *pnBytes;
    LONG r = RegQueryValueExA(m_hKey, pszValueName, nullptr, &type, static_cast<LPBYTE>(pValue), &size);
    if (r != ERROR_SUCCESS && r != ERROR_MORE_DATA)
        return r;
    if (type != REG_BINARY)
        return 13;
    *pnBytes = size;
    return r;
}

LONG CRegKey::SetDWORDValue(const char* pszValueName, DWORD dwValue) {
    return RegSetValueExA(m_hKey, pszValueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwValue), sizeof dwValue);
}

LONG CRegKey::SetStringValue(const char* pszValueName, const char* pszValue, DWORD dwType) {
    if (!pszValue)
        return 87;
    return RegSetValueExA(m_hKey, pszValueName, 0, dwType, reinterpret_cast<const BYTE*>(pszValue),
                          static_cast<DWORD>(strlen(pszValue) + 1));
}

LONG CRegKey::SetBinaryValue(const char* pszValueName, const void* pValue, ULONG nBytes) {
    return RegSetValueExA(m_hKey, pszValueName, 0, REG_BINARY, static_cast<const BYTE*>(pValue), nBytes);
}

LONG CRegKey::DeleteValue(const char* lpszValue) { return RegDeleteValueA(m_hKey, lpszValue); }

LONG CRegKey::DeleteSubKey(const char* lpszSubKey) { return RegDeleteKeyA(m_hKey, lpszSubKey); }
