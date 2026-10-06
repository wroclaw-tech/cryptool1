#include <wx/intl.h>
#include <wx/strconv.h>
#include <wx/string.h>
#include <wx/utils.h>

#include "afx.h"
#include "globalmem.h"
#include "runtime.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <dlfcn.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <crt_externs.h>
#include <mach/mach.h>
#include <sys/sysctl.h>
#endif

#undef fopen
#undef rename
#undef remove

extern char** environ;

namespace mfcwx {

extern const uint16_t kCp1250High[128];
extern const uint16_t kCp1251High[128];
extern const uint16_t kCp1252High[128];
extern const uint16_t kCp1253High[128];
extern const uint16_t kCp1254High[128];
extern const uint16_t kCp1257High[128];

namespace {

wchar_t UniUpper(wchar_t c) {
    if (c < 0x80)
        return c >= 'a' && c <= 'z' ? c - 0x20 : c;
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7)
        return c - 0x20;
    if (c == 0xFF)
        return 0x178;
    if (c == 0x131)
        return 'I';
    if ((c >= 0x100 && c <= 0x137) || (c >= 0x14A && c <= 0x177))
        return (c & 1) ? c - 1 : c;
    if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E))
        return (c & 1) ? c : c - 1;
    if (c == 0x192)
        return 0x191;
    if ((c >= 0x3B1 && c <= 0x3CB && c != 0x3C2))
        return c - 0x20;
    if (c == 0x3C2)
        return 0x3A3;
    if (c == 0x3AC)
        return 0x386;
    if (c >= 0x3AD && c <= 0x3AF)
        return c - 0x25;
    if (c == 0x3CC)
        return 0x38C;
    if (c == 0x3CD || c == 0x3CE)
        return c - 0x3F;
    if (c >= 0x430 && c <= 0x44F)
        return c - 0x20;
    if (c >= 0x450 && c <= 0x45F)
        return c - 0x50;
    if (c == 0x491)
        return 0x490;
    return c;
}

wchar_t UniLower(wchar_t c) {
    if (c < 0x80)
        return c >= 'A' && c <= 'Z' ? c + 0x20 : c;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7)
        return c + 0x20;
    if (c == 0x178)
        return 0xFF;
    if (c == 0x130)
        return 'i';
    if ((c >= 0x100 && c <= 0x137) || (c >= 0x14A && c <= 0x177))
        return (c & 1) ? c : c + 1;
    if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E))
        return (c & 1) ? c + 1 : c;
    if (c == 0x191)
        return 0x192;
    if (c >= 0x391 && c <= 0x3AB && c != 0x3A2)
        return c + 0x20;
    if (c == 0x386)
        return 0x3AC;
    if (c >= 0x388 && c <= 0x38A)
        return c + 0x25;
    if (c == 0x38C)
        return 0x3CC;
    if (c == 0x38E || c == 0x38F)
        return c + 0x3F;
    if (c >= 0x410 && c <= 0x42F)
        return c + 0x20;
    if (c >= 0x400 && c <= 0x40F)
        return c + 0x50;
    if (c == 0x490)
        return 0x491;
    return c;
}

bool UniIsCaselessLetter(wchar_t c) {
    return c == 0xAA || c == 0xB5 || c == 0xBA || c == 0xDF || c == 0x138 || c == 0x149 || c == 0x17F ||
           c == 0x390 || c == 0x3B0;
}

const uint16_t* CodePageTable(UINT cp) {
    switch (cp) {
    case 1250:
        return kCp1250High;
    case 1251:
        return kCp1251High;
    case 1252:
        return kCp1252High;
    case 1253:
        return kCp1253High;
    case 1254:
        return kCp1254High;
    case 1257:
        return kCp1257High;
    default:
        return nullptr;
    }
}

UINT EffectiveCodePage(UINT cp) {
    if (cp == CP_ACP || cp == CP_OEMCP || cp == 2 || cp == 3)
        return static_cast<UINT>(GetAnsiCodePage());
    return cp;
}

bool DecodeUtf8(const unsigned char* s, size_t n, std::wstring& out, bool strict) {
    for (size_t i = 0; i < n;) {
        unsigned char c = s[i];
        uint32_t cp;
        size_t len;
        if (c < 0x80) {
            cp = c;
            len = 1;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            len = 4;
        } else {
            if (strict)
                return false;
            out.push_back(0xFFFD);
            ++i;
            continue;
        }
        bool ok = i + len <= n;
        for (size_t k = 1; ok && k < len; ++k) {
            if ((s[i + k] & 0xC0) != 0x80)
                ok = false;
            else
                cp = (cp << 6) | (s[i + k] & 0x3F);
        }
        if (ok && ((len == 2 && cp < 0x80) || (len == 3 && cp < 0x800) || (len == 4 && cp < 0x10000) || cp > 0x10FFFF ||
                   (cp >= 0xD800 && cp <= 0xDFFF)))
            ok = false;
        if (!ok) {
            if (strict)
                return false;
            out.push_back(0xFFFD);
            ++i;
            continue;
        }
        out.push_back(static_cast<wchar_t>(cp));
        i += len;
    }
    return true;
}

void EncodeUtf8(uint32_t cp, std::string& out) {
    if (cp >= 0xD800 && cp <= 0xDFFF)
        cp = 0xFFFD;
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        EncodeUtf8(0xFFFD, out);
    }
}

wxString CodePageCharsetName(UINT cp) { return wxString::Format("windows-%u", cp); }

bool MultiByteToUnicode(UINT cp, const char* src, size_t n, std::wstring& out, bool strict) {
    cp = EffectiveCodePage(cp);
    if (cp == CP_UTF8)
        return DecodeUtf8(reinterpret_cast<const unsigned char*>(src), n, out, strict);
    if (cp == static_cast<UINT>(GetAnsiCodePage())) {
        for (size_t i = 0; i < n; ++i)
            out.push_back(AnsiToUnicode(static_cast<unsigned char>(src[i])));
        return true;
    }
    if (const uint16_t* table = CodePageTable(cp)) {
        for (size_t i = 0; i < n; ++i) {
            unsigned char c = static_cast<unsigned char>(src[i]);
            out.push_back(c < 0x80 ? c : static_cast<wchar_t>(table[c - 0x80]));
        }
        return true;
    }
    if (cp == 28591 || cp == 20127) {
        for (size_t i = 0; i < n; ++i) {
            unsigned char c = static_cast<unsigned char>(src[i]);
            out.push_back(cp == 20127 && c >= 0x80 ? L'?' : c);
        }
        return true;
    }
    wxCSConv conv(CodePageCharsetName(cp));
    if (!conv.IsOk())
        return false;
    std::string tmp(src, n);
    wxWCharBuffer wide = conv.cMB2WC(tmp.c_str(), n, nullptr);
    if (!wide.data())
        return false;
    out.append(wide.data());
    return true;
}

bool UnicodeToMultiByte(UINT cp, const wchar_t* src, size_t n, std::string& out, char defChar, bool& usedDef) {
    cp = EffectiveCodePage(cp);
    if (cp == CP_UTF8) {
        for (size_t i = 0; i < n; ++i)
            EncodeUtf8(static_cast<uint32_t>(src[i]), out);
        return true;
    }
    const uint16_t* table = CodePageTable(cp);
    bool current = cp == static_cast<UINT>(GetAnsiCodePage());
    if (!table && !current && cp != 28591 && cp != 20127) {
        wxCSConv conv(CodePageCharsetName(cp));
        if (!conv.IsOk())
            return false;
        for (size_t i = 0; i < n; ++i) {
            wchar_t one[2] = {src[i], 0};
            wxCharBuffer mb = conv.cWC2MB(one, 1, nullptr);
            if (mb.data() && mb.length() > 0)
                out.append(mb.data(), mb.length());
            else {
                out.push_back(defChar);
                usedDef = true;
            }
        }
        return true;
    }
    for (size_t i = 0; i < n; ++i) {
        wchar_t w = src[i];
        if (static_cast<uint32_t>(w) < 0x80) {
            out.push_back(static_cast<char>(w));
            continue;
        }
        char ch = 0;
        bool ok = false;
        if (current) {
            ok = UnicodeToAnsi(w, ch);
        } else if (table) {
            for (int k = 0; k < 128 && !ok; ++k)
                if (table[k] == static_cast<uint32_t>(w)) {
                    ch = static_cast<char>(0x80 + k);
                    ok = true;
                }
        } else if (cp == 28591 && static_cast<uint32_t>(w) <= 0xFF) {
            ch = static_cast<char>(w);
            ok = true;
        }
        if (!ok) {
            ch = defChar;
            usedDef = true;
        }
        out.push_back(ch);
    }
    return true;
}

const char* SystemMessage(DWORD code) {
    switch (code) {
    case 0:
        return "The operation completed successfully.";
    case 1:
        return "Incorrect function.";
    case 2:
        return "The system cannot find the file specified.";
    case 3:
        return "The system cannot find the path specified.";
    case 4:
        return "The system cannot open the file.";
    case 5:
        return "Access is denied.";
    case 6:
        return "The handle is invalid.";
    case 8:
        return "Not enough storage is available to process this command.";
    case 11:
        return "An attempt was made to load a program with an incorrect format.";
    case 17:
        return "The system cannot move the file to a different disk drive.";
    case 18:
        return "There are no more files.";
    case 19:
        return "The media is write protected.";
    case 31:
        return "A device attached to the system is not functioning.";
    case 32:
        return "The process cannot access the file because it is being used by another process.";
    case 50:
        return "The request is not supported.";
    case 80:
        return "The file exists.";
    case 87:
        return "The parameter is incorrect.";
    case 112:
        return "There is not enough space on the disk.";
    case 122:
        return "The data area passed to a system call is too small.";
    case 126:
        return "The specified module could not be found.";
    case 127:
        return "The specified procedure could not be found.";
    case 131:
        return "An attempt was made to move the file pointer before the beginning of the file.";
    case 145:
        return "The directory is not empty.";
    case 183:
        return "Cannot create a file when that file already exists.";
    case 193:
        return "%1 is not a valid Win32 application.";
    case 203:
        return "The system could not find the environment option that was entered.";
    case 206:
        return "The filename or extension is too long.";
    case 234:
        return "More data is available.";
    case 259:
        return "No more data is available.";
    case 288:
        return "Attempt to release mutex not owned by caller.";
    case 298:
        return "Too many posts were made to a semaphore.";
    case 1117:
        return "The request could not be performed because of an I/O device error.";
    case 1223:
        return "The operation was canceled by the user.";
    default:
        return nullptr;
    }
}

std::string ExpandInserts(const char* tmpl, bool argArray, va_list* args) {
    std::string typeSpec[100];
    int maxArg = 0;
    for (const char* p = tmpl; *p; ++p) {
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
        } else if (*p == '%' && p[1]) {
            ++p;
        }
    }
    const char* strArgs[100] = {};
    long long intArgs[100] = {};
    va_list ap;
    if (args && !argArray)
        va_copy(ap, *args);
    for (int i = 1; i <= maxArg && args; ++i) {
        bool isString = typeSpec[i].empty() || typeSpec[i].back() == 's';
        bool isWide = typeSpec[i].find("I64") != std::string::npos || typeSpec[i].find("ll") != std::string::npos;
        if (argArray) {
            DWORD_PTR v = reinterpret_cast<const DWORD_PTR*>(args)[i - 1];
            if (isString)
                strArgs[i] = reinterpret_cast<const char*>(v);
            else
                intArgs[i] = static_cast<long long>(v);
        } else if (isString) {
            strArgs[i] = va_arg(ap, const char*);
        } else if (isWide) {
            intArgs[i] = va_arg(ap, long long);
        } else {
            intArgs[i] = va_arg(ap, int);
        }
    }
    if (args && !argArray)
        va_end(ap);
    std::string out;
    for (const char* p = tmpl; *p; ++p) {
        if (*p != '%') {
            out += *p;
            continue;
        }
        char c = p[1];
        if (c >= '1' && c <= '9') {
            int n = c - '0';
            const char* q = p + 2;
            if (*q >= '0' && *q <= '9')
                n = n * 10 + (*q++ - '0');
            if (*q == '!') {
                const char* e = strchr(q + 1, '!');
                if (e)
                    q = e + 1;
            }
            if (typeSpec[n].empty() || typeSpec[n].back() == 's') {
                out += strArgs[n] ? strArgs[n] : "";
            } else {
                char fmt[64];
                char buf[128];
                std::string spec = typeSpec[n];
                size_t i64 = spec.find("I64");
                if (i64 != std::string::npos)
                    spec.replace(i64, 3, "ll");
                snprintf(fmt, sizeof fmt, "%%%s", spec.c_str());
                if (spec.find("ll") != std::string::npos)
                    snprintf(buf, sizeof buf, fmt, intArgs[n]);
                else
                    snprintf(buf, sizeof buf, fmt, static_cast<int>(intArgs[n]));
                out += buf;
            }
            p = q - 1;
        } else if (c == 'n') {
            out += "\r\n";
            ++p;
        } else if (c == '0') {
            break;
        } else if (c) {
            out += c;
            ++p;
        }
    }
    return out;
}

struct Module {
    void* dl;
    std::string path;
    int refs;
};

std::mutex& ModuleLock() {
    static std::mutex* m = new std::mutex;
    return *m;
}

std::vector<Module*>& Modules() {
    static auto* v = new std::vector<Module*>;
    return *v;
}

char g_mainModuleTag;

Module* FindModuleLocked(HMODULE h) {
    for (Module* m : Modules())
        if (reinterpret_cast<HMODULE>(m) == h)
            return m;
    return nullptr;
}

std::mutex& CommandLineLock() {
    static std::mutex* m = new std::mutex;
    return *m;
}

std::string& CommandLineStore() {
    static auto* s = new std::string;
    return *s;
}

std::string QuoteArg(const std::string& a) {
    if (!a.empty() && a.find_first_of(" \t\"") == std::string::npos)
        return a;
    std::string out = "\"";
    for (char c : a) {
        if (c == '"')
            out += '\\';
        out += c;
    }
    return out + "\"";
}

std::vector<std::string> ProcessArguments() {
    std::vector<std::string> args;
#ifdef __APPLE__
    int argc = *_NSGetArgc();
    char** argv = *_NSGetArgv();
    for (int i = 0; i < argc; ++i)
        args.push_back(argv[i] ? argv[i] : "");
#else
    if (FILE* f = ::fopen("/proc/self/cmdline", "rb")) {
        std::string cur;
        int c;
        while ((c = fgetc(f)) != EOF) {
            if (c == 0) {
                args.push_back(cur);
                cur.clear();
            } else {
                cur += static_cast<char>(c);
            }
        }
        if (!cur.empty())
            args.push_back(cur);
        fclose(f);
    }
#endif
    return args;
}

std::string BuildCommandLine() {
    std::vector<std::string> args = ProcessArguments();
    std::string exe = ExecutablePath();
    std::string line = QuoteArg(AppPath(exe.empty() ? (args.empty() ? "" : args[0].c_str()) : exe.c_str()));
    for (size_t i = 1; i < args.size(); ++i) {
        struct stat st;
        const std::string& a = args[i];
        std::string conv = !a.empty() && a[0] == '/' && stat(a.c_str(), &st) == 0 ? AppPath(a.c_str()) : Utf8ToAnsi(a.c_str());
        line += " " + QuoteArg(conv);
    }
    return line;
}

const char* FindEnv(const char* name) {
    if (!name)
        return nullptr;
    if (const char* v = getenv(name))
        return v;
    size_t len = strlen(name);
    for (char** e = environ; e && *e; ++e)
        if (strncasecmp(*e, name, len) == 0 && (*e)[len] == '=')
            return *e + len + 1;
    return nullptr;
}

WORD LanguageFromCode(const std::string& code) {
    static const struct {
        const char* code;
        WORD primary;
    } kLanguages[] = {
        {"ar", 0x01}, {"bg", 0x02}, {"ca", 0x03}, {"zh", 0x04}, {"cs", 0x05}, {"da", 0x06}, {"de", 0x07},
        {"el", 0x08}, {"en", 0x09}, {"es", 0x0a}, {"fi", 0x0b}, {"fr", 0x0c}, {"he", 0x0d}, {"hu", 0x0e},
        {"is", 0x0f}, {"it", 0x10}, {"ja", 0x11}, {"ko", 0x12}, {"nl", 0x13}, {"nb", 0x14}, {"no", 0x14},
        {"pl", 0x15}, {"pt", 0x16}, {"ro", 0x18}, {"ru", 0x19}, {"hr", 0x1a}, {"sr", 0x1a}, {"sk", 0x1b},
        {"sq", 0x1c}, {"sv", 0x1d}, {"th", 0x1e}, {"tr", 0x1f}, {"uk", 0x22}, {"sl", 0x24}, {"et", 0x25},
        {"lv", 0x26}, {"lt", 0x27},
    };
    std::string primary = code.substr(0, code.find_first_of("_-"));
    for (const auto& l : kLanguages)
        if (primary == l.code) {
            WORD sub = SUBLANG_DEFAULT;
            std::string region = code.size() > primary.size() + 1 ? code.substr(primary.size() + 1, 2) : "";
            if (primary == "en" && region == "GB")
                sub = SUBLANG_ENGLISH_UK;
            else if (primary == "de" && region == "CH")
                sub = SUBLANG_GERMAN_SWISS;
            else if (primary == "sr" && code.find("Latn") != std::string::npos)
                sub = SUBLANG_SERBIAN_LATIN;
            return static_cast<WORD>(MAKELANGID(l.primary, sub));
        }
    return static_cast<WORD>(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
}

WORD SystemLangId() {
    static const WORD id = []() {
        int lang = wxLocale::GetSystemLanguage();
        if (lang == wxLANGUAGE_UNKNOWN || lang == wxLANGUAGE_DEFAULT)
            return static_cast<WORD>(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
        wxString name = wxLocale::GetLanguageCanonicalName(lang);
        return LanguageFromCode(std::string(name.utf8_str()));
    }();
    return id;
}

thread_local LCID t_threadLocale = 0;
UINT g_errorMode = 0;

} // namespace

char AnsiToUpper(char ch) {
    wchar_t u = UniUpper(AnsiToUnicode(static_cast<unsigned char>(ch)));
    char out;
    return UnicodeToAnsi(u, out) ? out : ch;
}

char AnsiToLower(char ch) {
    wchar_t u = UniLower(AnsiToUnicode(static_cast<unsigned char>(ch)));
    char out;
    return UnicodeToAnsi(u, out) ? out : ch;
}

bool AnsiIsAlpha(char ch) {
    wchar_t u = AnsiToUnicode(static_cast<unsigned char>(ch));
    return UniUpper(u) != u || UniLower(u) != u || UniIsCaselessLetter(u);
}

bool AnsiIsUpper(char ch) {
    wchar_t u = AnsiToUnicode(static_cast<unsigned char>(ch));
    return UniLower(u) != u;
}

bool AnsiIsLower(char ch) {
    wchar_t u = AnsiToUnicode(static_cast<unsigned char>(ch));
    return UniUpper(u) != u || (UniIsCaselessLetter(u) && u != 0xAA && u != 0xBA);
}

HGLOBAL GlobalFromData(const void* data, size_t size, UINT flags) {
    HGLOBAL h = ::GlobalAlloc(flags | GMEM_ZEROINIT, size);
    if (h && data && size)
        memcpy(h, data, size);
    return h;
}

bool GlobalIsValid(HGLOBAL h) { return GlobalHeaderOf(h) != nullptr; }

void SetCommandLineOverride(const char* commandLine) {
    std::lock_guard<std::mutex> lk(CommandLineLock());
    CommandLineStore() = commandLine ? commandLine : "";
}

} // namespace mfcwx

using namespace mfcwx;

DWORD GetTickCount(void) { return static_cast<DWORD>(GetTickCount64()); }

ULONGLONG GetTickCount64(void) {
    return static_cast<ULONGLONG>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

BOOL QueryPerformanceCounter(LARGE_INTEGER* c) {
    if (!c)
        return FALSE;
    c->QuadPart = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() / 100;
    return TRUE;
}

BOOL QueryPerformanceFrequency(LARGE_INTEGER* f) {
    if (!f)
        return FALSE;
    f->QuadPart = 10000000;
    return TRUE;
}

static void FillSystemTime(const struct tm& tm, long nsec, LPSYSTEMTIME st) {
    st->wYear = static_cast<WORD>(tm.tm_year + 1900);
    st->wMonth = static_cast<WORD>(tm.tm_mon + 1);
    st->wDayOfWeek = static_cast<WORD>(tm.tm_wday);
    st->wDay = static_cast<WORD>(tm.tm_mday);
    st->wHour = static_cast<WORD>(tm.tm_hour);
    st->wMinute = static_cast<WORD>(tm.tm_min);
    st->wSecond = static_cast<WORD>(tm.tm_sec > 59 ? 59 : tm.tm_sec);
    st->wMilliseconds = static_cast<WORD>(nsec / 1000000);
}

void GetSystemTime(LPSYSTEMTIME st) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    gmtime_r(&ts.tv_sec, &tm);
    FillSystemTime(tm, ts.tv_nsec, st);
}

void GetLocalTime(LPSYSTEMTIME st) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);
    FillSystemTime(tm, ts.tv_nsec, st);
}

void GetSystemTimeAsFileTime(LPFILETIME ft) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    *ft = MakeFileTime(ts.tv_sec, ts.tv_nsec);
}

BOOL SystemTimeToFileTime(const SYSTEMTIME* st, LPFILETIME ft) {
    if (!st || !ft || st->wMonth < 1 || st->wMonth > 12 || st->wDay < 1 || st->wDay > 31 || st->wHour > 23 ||
        st->wMinute > 59 || st->wSecond > 59 || st->wMilliseconds > 999 || st->wYear < 1601 || st->wYear > 30827) {
        SetLastError(87);
        return FALSE;
    }
    struct tm tm;
    memset(&tm, 0, sizeof tm);
    tm.tm_year = st->wYear - 1900;
    tm.tm_mon = st->wMonth - 1;
    tm.tm_mday = st->wDay;
    tm.tm_hour = st->wHour;
    tm.tm_min = st->wMinute;
    tm.tm_sec = st->wSecond;
    time_t t = timegm(&tm);
    *ft = MakeFileTime(t, static_cast<long>(st->wMilliseconds) * 1000000L);
    return TRUE;
}

BOOL FileTimeToSystemTime(const FILETIME* ft, LPSYSTEMTIME st) {
    if (!ft || !st)
        return FALSE;
    LONGLONG ticks = FileTimeTicks(*ft) - kFileTimeUnixEpoch;
    LONGLONG sec = ticks >= 0 ? ticks / 10000000LL : -((-ticks + 9999999LL) / 10000000LL);
    LONGLONG rem = ticks - sec * 10000000LL;
    time_t t = static_cast<time_t>(sec);
    struct tm tm;
    if (!gmtime_r(&t, &tm))
        return FALSE;
    FillSystemTime(tm, static_cast<long>(rem * 100), st);
    return TRUE;
}

BOOL FileTimeToLocalFileTime(const FILETIME* ft, LPFILETIME lft) {
    if (!ft || !lft)
        return FALSE;
    time_t t = FileTimeToUnix(*ft);
    struct tm tm;
    localtime_r(&t, &tm);
    ULONGLONG v = static_cast<ULONGLONG>(FileTimeTicks(*ft) + static_cast<LONGLONG>(tm.tm_gmtoff) * 10000000LL);
    lft->dwLowDateTime = static_cast<DWORD>(v);
    lft->dwHighDateTime = static_cast<DWORD>(v >> 32);
    return TRUE;
}

void GetSystemInfo(LPSYSTEM_INFO si) {
    memset(si, 0, sizeof *si);
#if defined(__x86_64__)
    si->wProcessorArchitecture = 9;
    si->dwProcessorType = 8664;
#elif defined(__aarch64__)
    si->wProcessorArchitecture = 12;
#elif defined(__i386__)
    si->wProcessorArchitecture = 0;
    si->dwProcessorType = 586;
#endif
    long page = sysconf(_SC_PAGESIZE);
    si->dwPageSize = page > 0 ? static_cast<DWORD>(page) : 4096;
    si->lpMinimumApplicationAddress = reinterpret_cast<LPVOID>(0x10000);
    si->lpMaximumApplicationAddress = reinterpret_cast<LPVOID>(static_cast<uintptr_t>(0x7FFFFFFEFFFFULL));
    unsigned n = std::thread::hardware_concurrency();
    si->dwNumberOfProcessors = n ? n : 1;
    si->dwActiveProcessorMask = si->dwNumberOfProcessors >= 64 ? ~static_cast<DWORD_PTR>(0)
                                                               : (static_cast<DWORD_PTR>(1) << si->dwNumberOfProcessors) - 1;
    si->dwAllocationGranularity = 65536;
}

BOOL GetVersionEx(LPOSVERSIONINFO vi) {
    if (!vi)
        return FALSE;
    vi->dwMajorVersion = 5;
    vi->dwMinorVersion = 1;
    vi->dwBuildNumber = 2600;
    vi->dwPlatformId = 2;
    strcpy(vi->szCSDVersion, "Service Pack 3");
    return TRUE;
}

DWORD GetVersion(void) { return 5u | (1u << 8) | (2600u << 16); }

void GlobalMemoryStatus(LPMEMORYSTATUS ms) {
    ULONGLONG total = 0, avail = 0;
#ifdef __APPLE__
    uint64_t mem = 0;
    size_t len = sizeof mem;
    if (sysctlbyname("hw.memsize", &mem, &len, nullptr, 0) == 0)
        total = mem;
    vm_statistics64_data_t vm;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm), &count) == KERN_SUCCESS)
        avail = (static_cast<ULONGLONG>(vm.free_count) + vm.inactive_count) * static_cast<ULONGLONG>(sysconf(_SC_PAGESIZE));
#else
    long pages = sysconf(_SC_PHYS_PAGES);
    long availPages = sysconf(_SC_AVPHYS_PAGES);
    long page = sysconf(_SC_PAGESIZE);
    if (pages > 0 && page > 0)
        total = static_cast<ULONGLONG>(pages) * static_cast<ULONGLONG>(page);
    if (availPages > 0 && page > 0)
        avail = static_cast<ULONGLONG>(availPages) * static_cast<ULONGLONG>(page);
#endif
    if (!total)
        total = 4ULL << 30;
    if (!avail || avail > total)
        avail = total / 2;
    memset(ms, 0, sizeof *ms);
    ms->dwLength = sizeof *ms;
    ms->dwMemoryLoad = static_cast<DWORD>(100 - avail * 100 / total);
    ms->dwTotalPhys = static_cast<SIZE_T>(total);
    ms->dwAvailPhys = static_cast<SIZE_T>(avail);
    ms->dwTotalPageFile = static_cast<SIZE_T>(total * 2);
    ms->dwAvailPageFile = static_cast<SIZE_T>(avail * 2);
    ms->dwTotalVirtual = static_cast<SIZE_T>(0x7FFFFFFEFFFFULL);
    ms->dwAvailVirtual = static_cast<SIZE_T>(0x7FFFFFFEFFFFULL);
}

DWORD FormatMessageA(DWORD flags, LPCVOID source, DWORD msgId, DWORD, LPSTR buffer, DWORD size, va_list* args) {
    std::string tmpl;
    if (flags & FORMAT_MESSAGE_FROM_STRING) {
        tmpl = source ? static_cast<const char*>(source) : "";
    } else {
        const char* known = SystemMessage(msgId);
        if (known) {
            tmpl = known;
        } else {
            const char* text = strerror(static_cast<int>(msgId));
            char buf[64];
            if (!text || strncmp(text, "Unknown error", 13) == 0) {
                snprintf(buf, sizeof buf, "Unknown error 0x%08X.", static_cast<unsigned>(msgId));
                text = buf;
            }
            tmpl = text;
            if (!tmpl.empty() && tmpl.back() != '.')
                tmpl += '.';
        }
        tmpl += "\r\n";
    }
    std::string text = (flags & FORMAT_MESSAGE_IGNORE_INSERTS) ? tmpl : ExpandInserts(tmpl.c_str(), (flags & 0x2000) != 0, args);
    if (flags & FORMAT_MESSAGE_ALLOCATE_BUFFER) {
        if (!buffer)
            return 0;
        size_t bytes = std::max<size_t>(text.size() + 1, size);
        auto* out = static_cast<char*>(LocalAlloc(GMEM_ZEROINIT, bytes));
        if (!out) {
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return 0;
        }
        memcpy(out, text.c_str(), text.size() + 1);
        *reinterpret_cast<LPSTR*>(buffer) = out;
        return static_cast<DWORD>(text.size());
    }
    if (!buffer || size < text.size() + 1) {
        SetLastError(122);
        return 0;
    }
    memcpy(buffer, text.c_str(), text.size() + 1);
    return static_cast<DWORD>(text.size());
}

HLOCAL LocalAlloc(UINT flags, SIZE_T bytes) { return GlobalAlloc(flags, bytes); }

HLOCAL LocalFree(HLOCAL mem) { return GlobalFree(mem); }

HGLOBAL GlobalAlloc(UINT flags, SIZE_T bytes) {
    size_t total = sizeof(GlobalHeader) + bytes;
    void* block = (flags & GMEM_ZEROINIT) ? calloc(1, total) : malloc(total);
    if (!block) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return nullptr;
    }
    auto* header = static_cast<GlobalHeader*>(block);
    header->magic = kGlobalMagic;
    header->flags = flags;
    header->size = bytes;
    header->lockCount = 0;
    header->reserved = 0;
    return header + 1;
}

HGLOBAL GlobalFree(HGLOBAL mem) {
    if (!mem)
        return nullptr;
    GlobalHeader* header = GlobalHeaderOf(mem);
    if (!header) {
        SetLastError(ERROR_INVALID_HANDLE);
        return mem;
    }
    header->magic = 0;
    free(header);
    return nullptr;
}

LPVOID GlobalLock(HGLOBAL mem) {
    GlobalHeader* header = GlobalHeaderOf(mem);
    if (!header) {
        SetLastError(ERROR_INVALID_HANDLE);
        return nullptr;
    }
    ++header->lockCount;
    return mem;
}

BOOL GlobalUnlock(HGLOBAL mem) {
    GlobalHeader* header = GlobalHeaderOf(mem);
    if (!header) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (header->lockCount > 0)
        --header->lockCount;
    SetLastError(0);
    return header->lockCount > 0;
}

SIZE_T GlobalSize(HGLOBAL mem) {
    GlobalHeader* header = GlobalHeaderOf(mem);
    if (!header) {
        SetLastError(ERROR_INVALID_HANDLE);
        return 0;
    }
    return header->size;
}

HGLOBAL GlobalReAlloc(HGLOBAL mem, SIZE_T bytes, UINT flags) {
    GlobalHeader* header = GlobalHeaderOf(mem);
    if (!header) {
        SetLastError(ERROR_INVALID_HANDLE);
        return nullptr;
    }
    if (flags & 0x80) {
        header->flags = (header->flags & ~GMEM_MOVEABLE) | (flags & GMEM_MOVEABLE);
        return mem;
    }
    size_t old = header->size;
    auto* fresh = static_cast<GlobalHeader*>(realloc(header, sizeof(GlobalHeader) + bytes));
    if (!fresh) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return nullptr;
    }
    if ((flags & GMEM_ZEROINIT) && bytes > old)
        memset(reinterpret_cast<char*>(fresh + 1) + old, 0, bytes - old);
    fresh->size = bytes;
    return fresh + 1;
}

DWORD GetEnvironmentVariableA(LPCSTR name, LPSTR buffer, DWORD size) {
    const char* v = FindEnv(name);
    if (!v) {
        SetLastError(203);
        return 0;
    }
    std::string ansi = Utf8ToAnsi(v);
    if (!buffer || size <= ansi.size())
        return static_cast<DWORD>(ansi.size() + 1);
    memcpy(buffer, ansi.c_str(), ansi.size() + 1);
    return static_cast<DWORD>(ansi.size());
}

BOOL SetEnvironmentVariableA(LPCSTR name, LPCSTR value) {
    if (!name || !*name || strchr(name, '=')) {
        SetLastError(87);
        return FALSE;
    }
    int rc = value ? setenv(name, AnsiToUtf8(value).c_str(), 1) : unsetenv(name);
    if (rc != 0) {
        SetLastErrorFromErrno(errno);
        return FALSE;
    }
    return TRUE;
}

DWORD ExpandEnvironmentStringsA(LPCSTR src, LPSTR dst, DWORD size) {
    std::string out;
    for (const char* p = src ? src : ""; *p;) {
        if (*p == '%') {
            const char* end = strchr(p + 1, '%');
            if (end && end > p + 1) {
                std::string name(p + 1, end);
                if (const char* v = FindEnv(name.c_str())) {
                    out += Utf8ToAnsi(v);
                    p = end + 1;
                    continue;
                }
            }
        }
        out += *p++;
    }
    if (dst && size > out.size())
        memcpy(dst, out.c_str(), out.size() + 1);
    return static_cast<DWORD>(out.size() + 1);
}

LPSTR GetCommandLineA(void) {
    std::lock_guard<std::mutex> lk(CommandLineLock());
    std::string& line = CommandLineStore();
    if (line.empty())
        line = BuildCommandLine();
    return &line[0];
}

UINT SetErrorMode(UINT mode) {
    UINT previous = g_errorMode;
    g_errorMode = mode;
    return previous;
}

void OutputDebugStringA(LPCSTR s) {
    if (s)
        fputs(s, stderr);
}

void DebugBreak(void) { fprintf(stderr, "mfcwx: DebugBreak()\n"); }

BOOL Beep(DWORD, DWORD) {
    wxBell();
    return TRUE;
}

BOOL MessageBeep(UINT) {
    wxBell();
    return TRUE;
}

int MulDiv(int number, int numerator, int denominator) {
    if (denominator == 0)
        return -1;
    long long product = static_cast<long long>(number) * numerator;
    long long half = (denominator < 0 ? -static_cast<long long>(denominator) : denominator) / 2;
    bool negative = (product < 0) != (denominator < 0);
    long long a = product < 0 ? -product : product;
    long long d = denominator < 0 ? -static_cast<long long>(denominator) : denominator;
    long long q = (a + half) / d;
    if (q > INT_MAX)
        return -1;
    return static_cast<int>(negative ? -q : q);
}

BOOL IsBadReadPtr(const void* p, UINT_PTR cb) { return !p && cb ? TRUE : FALSE; }

BOOL IsBadWritePtr(LPVOID p, UINT_PTR cb) { return !p && cb ? TRUE : FALSE; }

DWORD GetUserDefaultLangID(void) { return SystemLangId(); }

LCID GetUserDefaultLCID(void) { return MAKELCID(SystemLangId(), 0); }

LCID GetSystemDefaultLCID(void) { return MAKELCID(SystemLangId(), 0); }

LCID GetThreadLocale(void) { return t_threadLocale ? t_threadLocale : GetUserDefaultLCID(); }

BOOL SetThreadLocale(LCID lcid) {
    t_threadLocale = lcid;
    return TRUE;
}

UINT GetACP(void) { return static_cast<UINT>(GetAnsiCodePage()); }

UINT GetOEMCP(void) {
    switch (GetAnsiCodePage()) {
    case 1250:
        return 852;
    case 1251:
        return 866;
    case 1253:
        return 737;
    case 1254:
        return 857;
    case 1257:
        return 775;
    default:
        return 850;
    }
}

int MultiByteToWideChar(UINT cp, DWORD flags, LPCSTR src, int srcLen, LPWSTR dst, int dstLen) {
    if (!src || srcLen == 0 || dstLen < 0 || (dstLen > 0 && !dst)) {
        SetLastError(87);
        return 0;
    }
    size_t n = srcLen < 0 ? strlen(src) + 1 : static_cast<size_t>(srcLen);
    std::wstring wide;
    if (!MultiByteToUnicode(cp, src, n, wide, (flags & 0x8) != 0)) {
        SetLastError(EffectiveCodePage(cp) == CP_UTF8 ? 1113 : 87);
        return 0;
    }
    if (dstLen == 0)
        return static_cast<int>(wide.size());
    if (wide.size() > static_cast<size_t>(dstLen)) {
        SetLastError(122);
        return 0;
    }
    std::copy(wide.begin(), wide.end(), dst);
    return static_cast<int>(wide.size());
}

int WideCharToMultiByte(UINT cp, DWORD, LPCWSTR src, int srcLen, LPSTR dst, int dstLen, LPCSTR defChar, LPBOOL usedDef) {
    if (!src || srcLen == 0 || dstLen < 0 || (dstLen > 0 && !dst)) {
        SetLastError(87);
        return 0;
    }
    size_t n = srcLen < 0 ? wcslen(src) + 1 : static_cast<size_t>(srcLen);
    std::string out;
    bool used = false;
    if (!UnicodeToMultiByte(cp, src, n, out, defChar ? *defChar : '?', used)) {
        SetLastError(87);
        return 0;
    }
    if (usedDef)
        *usedDef = used ? TRUE : FALSE;
    if (dstLen == 0)
        return static_cast<int>(out.size());
    if (out.size() > static_cast<size_t>(dstLen)) {
        SetLastError(122);
        return 0;
    }
    memcpy(dst, out.data(), out.size());
    return static_cast<int>(out.size());
}

BOOL GetUserNameA(LPSTR buffer, LPDWORD size) {
    std::string name;
    if (struct passwd* pw = getpwuid(getuid()))
        name = pw->pw_name;
    if (name.empty()) {
        const char* env = getenv("USER");
        name = env ? env : "user";
    }
    name = Utf8ToAnsi(name.c_str());
    if (!size)
        return FALSE;
    if (!buffer || *size < name.size() + 1) {
        *size = static_cast<DWORD>(name.size() + 1);
        SetLastError(122);
        return FALSE;
    }
    memcpy(buffer, name.c_str(), name.size() + 1);
    *size = static_cast<DWORD>(name.size() + 1);
    return TRUE;
}

BOOL GetComputerNameA(LPSTR buffer, LPDWORD size) {
    char host[256] = "";
    gethostname(host, sizeof host - 1);
    std::string name(host);
    name = name.substr(0, name.find('.'));
    if (name.empty())
        name = "localhost";
    if (!size)
        return FALSE;
    if (!buffer || *size < name.size() + 1) {
        *size = static_cast<DWORD>(name.size() + 1);
        SetLastError(111);
        return FALSE;
    }
    memcpy(buffer, name.c_str(), name.size() + 1);
    *size = static_cast<DWORD>(name.size());
    return TRUE;
}

HMODULE LoadLibraryA(LPCSTR name) { return LoadLibraryExA(name, nullptr, 0); }

HMODULE LoadLibraryExA(LPCSTR name, HANDLE, DWORD) {
    if (!name || !*name) {
        SetLastError(87);
        return nullptr;
    }
    std::string path = FsPath(name);
    void* dl = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!dl) {
        SetLastError(126);
        return nullptr;
    }
    std::lock_guard<std::mutex> lk(ModuleLock());
    for (Module* m : Modules())
        if (m->dl == dl) {
            ++m->refs;
            dlclose(dl);
            return reinterpret_cast<HMODULE>(m);
        }
    auto* m = new Module{dl, path, 1};
    Modules().push_back(m);
    return reinterpret_cast<HMODULE>(m);
}

BOOL FreeLibrary(HMODULE mod) {
    std::lock_guard<std::mutex> lk(ModuleLock());
    Module* m = FindModuleLocked(mod);
    if (!m) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    if (--m->refs == 0) {
        dlclose(m->dl);
        Modules().erase(std::find(Modules().begin(), Modules().end(), m));
        delete m;
    }
    return TRUE;
}

FARPROC GetProcAddress(HMODULE mod, LPCSTR name) {
    if (!name || IS_INTRESOURCE(name)) {
        SetLastError(127);
        return nullptr;
    }
    void* dl;
    {
        std::lock_guard<std::mutex> lk(ModuleLock());
        Module* m = FindModuleLocked(mod);
        dl = m ? m->dl : RTLD_DEFAULT;
    }
    void* sym = dlsym(dl, name);
    if (!sym) {
        SetLastError(127);
        return nullptr;
    }
    return reinterpret_cast<FARPROC>(sym);
}

HMODULE GetModuleHandleA(LPCSTR name) {
    if (!name)
        return reinterpret_cast<HMODULE>(&g_mainModuleTag);
    std::string wanted = name;
    size_t slash = wanted.find_last_of("\\/");
    if (slash != std::string::npos)
        wanted = wanted.substr(slash + 1);
    std::lock_guard<std::mutex> lk(ModuleLock());
    for (Module* m : Modules()) {
        size_t s = m->path.rfind('/');
        std::string base = m->path.substr(s == std::string::npos ? 0 : s + 1);
        if (strcasecmp(base.c_str(), wanted.c_str()) == 0)
            return reinterpret_cast<HMODULE>(m);
    }
    std::string exe = ExecutablePath();
    size_t s = exe.rfind('/');
    std::string exeBase = exe.substr(s == std::string::npos ? 0 : s + 1);
    if (strcasecmp(exeBase.c_str(), wanted.c_str()) == 0 ||
        strcasecmp((exeBase + ".exe").c_str(), wanted.c_str()) == 0)
        return reinterpret_cast<HMODULE>(&g_mainModuleTag);
    SetLastError(126);
    return nullptr;
}

DWORD GetModuleFileNameA(HMODULE mod, LPSTR buffer, DWORD size) {
    std::string path;
    {
        std::lock_guard<std::mutex> lk(ModuleLock());
        if (Module* m = FindModuleLocked(mod))
            path = m->path;
    }
    if (path.empty())
        path = ExecutablePath();
    std::string app = AppPath(path.c_str());
    if (!buffer || size == 0) {
        SetLastError(122);
        return 0;
    }
    if (app.size() + 1 > size) {
        memcpy(buffer, app.c_str(), size - 1);
        buffer[size - 1] = 0;
        SetLastError(122);
        return size;
    }
    memcpy(buffer, app.c_str(), app.size() + 1);
    SetLastError(0);
    return static_cast<DWORD>(app.size());
}

HINSTANCE ShellExecuteA(HWND, LPCSTR op, LPCSTR file, LPCSTR params, LPCSTR dir, int) {
    auto result = [](int v) { return reinterpret_cast<HINSTANCE>(static_cast<intptr_t>(v)); };
    if (!file || !*file)
        return result(2);
    std::string verb = op ? op : "";
    std::transform(verb.begin(), verb.end(), verb.begin(), [](char c) { return static_cast<char>(tolower(c)); });
    if (!verb.empty() && verb != "open" && verb != "explore" && verb != "edit")
        return result(31);
    std::string f = file;
    bool url = f.find("://") != std::string::npos || strncasecmp(f.c_str(), "mailto:", 7) == 0 ||
               strncasecmp(f.c_str(), "www.", 4) == 0;
    if (url) {
        std::string target = strncasecmp(f.c_str(), "www.", 4) == 0 ? "http://" + f : f;
        return wxLaunchDefaultBrowser(wxString::FromUTF8(AnsiToUtf8(target.c_str()).c_str())) ? result(42) : result(31);
    }
    bool absolute = f[0] == '\\' || f[0] == '/' || (f.size() > 1 && f[1] == ':');
    bool hasSeparator = f.find_first_of("\\/") != std::string::npos;
    std::string native;
    struct stat st;
    if (!absolute && dir && *dir) {
        std::string inDir = FsPath((std::string(dir) + "\\" + f).c_str());
        if (stat(inDir.c_str(), &st) == 0)
            native = inDir;
    }
    if (native.empty()) {
        std::string direct = FsPath(f.c_str());
        if (stat(direct.c_str(), &st) == 0)
            native = direct;
    }
    if (native.empty() && f.size() > 4 && strcasecmp(f.c_str() + f.size() - 4, ".exe") == 0) {
        std::string stripped = FsPath(f.substr(0, f.size() - 4).c_str());
        if (stat(stripped.c_str(), &st) == 0)
            native = stripped;
    }
    bool runProgram = false;
    if (native.empty()) {
        if (hasSeparator)
            return result(2);
        runProgram = true;
    } else if (S_ISREG(st.st_mode) && access(native.c_str(), X_OK) == 0) {
        size_t slash = native.rfind('/');
        size_t dot = native.rfind('.');
        std::string ext = dot != std::string::npos && (slash == std::string::npos || dot > slash) ? native.substr(dot) : "";
        std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) { return static_cast<char>(tolower(c)); });
        runProgram = ext.empty() || ext == ".sh" || ext == ".bin" || ext == ".run" || ext == ".command" ||
                     ext == ".py" || ext == ".pl";
    }
    if (runProgram) {
        std::string cmd = "\"" + (native.empty() ? f : AppPath(native.c_str())) + "\"";
        if (params && *params)
            cmd += std::string(" ") + params;
        STARTUPINFO si;
        memset(&si, 0, sizeof si);
        si.cb = sizeof si;
        PROCESS_INFORMATION pi;
        if (!CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, dir, &si, &pi))
            return result(GetLastError() == ERROR_PATH_NOT_FOUND ? 3 : 2);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return result(42);
    }
    return wxLaunchDefaultApplication(wxString::FromUTF8(native.c_str())) ? result(42) : result(31);
}

int lstrlenA(LPCSTR s) { return s ? static_cast<int>(strlen(s)) : 0; }

LPSTR lstrcpyA(LPSTR d, LPCSTR s) {
    if (!d)
        return nullptr;
    return strcpy(d, s ? s : "");
}

LPSTR lstrcpynA(LPSTR d, LPCSTR s, int n) {
    if (!d || n <= 0)
        return d;
    int i = 0;
    for (; s && i < n - 1 && s[i]; ++i)
        d[i] = s[i];
    d[i] = 0;
    return d;
}

LPSTR lstrcatA(LPSTR d, LPCSTR s) {
    if (!d)
        return nullptr;
    return strcat(d, s ? s : "");
}

static int CompareAnsi(LPCSTR a, LPCSTR b, bool caseSensitive) {
    a = a ? a : "";
    b = b ? b : "";
    int tie = 0;
    for (;; ++a, ++b) {
        unsigned char la = static_cast<unsigned char>(AnsiToLower(*a));
        unsigned char lb = static_cast<unsigned char>(AnsiToLower(*b));
        wchar_t ua = AnsiToUnicode(la);
        wchar_t ub = AnsiToUnicode(lb);
        if (ua != ub)
            return ua < ub ? -1 : 1;
        if (!*a)
            break;
        if (caseSensitive && !tie && *a != *b)
            tie = AnsiIsLower(*a) ? -1 : 1;
    }
    return tie;
}

int lstrcmpA(LPCSTR a, LPCSTR b) { return CompareAnsi(a, b, true); }

int lstrcmpiA(LPCSTR a, LPCSTR b) { return CompareAnsi(a, b, false); }

int wvsprintfA(LPSTR buf, LPCSTR fmt, va_list args) {
    char translated[1024];
    fmt = AfxTranslateFormat(fmt, translated, sizeof translated);
    int n = vsnprintf(buf, 1025, fmt ? fmt : "", args);
    return n > 1024 ? 1024 : n;
}

int wsprintfA(LPSTR buf, LPCSTR fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = wvsprintfA(buf, fmt, args);
    va_end(args);
    return n;
}

LPSTR CharUpperA(LPSTR s) {
    if (IS_INTRESOURCE(s))
        return reinterpret_cast<LPSTR>(static_cast<uintptr_t>(static_cast<unsigned char>(
            AnsiToUpper(static_cast<char>(reinterpret_cast<uintptr_t>(s))))));
    for (char* p = s; *p; ++p)
        *p = AnsiToUpper(*p);
    return s;
}

LPSTR CharLowerA(LPSTR s) {
    if (IS_INTRESOURCE(s))
        return reinterpret_cast<LPSTR>(static_cast<uintptr_t>(static_cast<unsigned char>(
            AnsiToLower(static_cast<char>(reinterpret_cast<uintptr_t>(s))))));
    for (char* p = s; *p; ++p)
        *p = AnsiToLower(*p);
    return s;
}

DWORD CharUpperBuffA(LPSTR s, DWORD len) {
    for (DWORD i = 0; s && i < len; ++i)
        s[i] = AnsiToUpper(s[i]);
    return len;
}

DWORD CharLowerBuffA(LPSTR s, DWORD len) {
    for (DWORD i = 0; s && i < len; ++i)
        s[i] = AnsiToLower(s[i]);
    return len;
}

LPSTR CharNextA(LPCSTR s) { return const_cast<LPSTR>(s && *s ? s + 1 : s); }

LPSTR CharPrevA(LPCSTR start, LPCSTR s) { return const_cast<LPSTR>(s > start ? s - 1 : start); }

BOOL IsCharAlphaA(CHAR c) { return AnsiIsAlpha(c) ? TRUE : FALSE; }

BOOL IsCharAlphaNumericA(CHAR c) { return (AnsiIsAlpha(c) || (c >= '0' && c <= '9')) ? TRUE : FALSE; }

BOOL IsCharUpperA(CHAR c) { return AnsiIsUpper(c) ? TRUE : FALSE; }

BOOL IsCharLowerA(CHAR c) { return AnsiIsLower(c) ? TRUE : FALSE; }

BOOL IsDBCSLeadByte(BYTE) { return FALSE; }

BOOL OemToCharA(LPCSTR src, LPSTR dst) {
    if (!src || !dst)
        return FALSE;
    if (src != dst)
        memmove(dst, src, strlen(src) + 1);
    return TRUE;
}

BOOL CharToOemA(LPCSTR src, LPSTR dst) { return OemToCharA(src, dst); }
