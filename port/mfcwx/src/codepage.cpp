#include "bridge.h"

#include <atomic>
#include <cstdint>
#include <unordered_map>

namespace mfcwx {

extern const uint16_t kCp1250High[128];
extern const uint16_t kCp1251High[128];
extern const uint16_t kCp1252High[128];
extern const uint16_t kCp1253High[128];
extern const uint16_t kCp1254High[128];
extern const uint16_t kCp1257High[128];

namespace {

struct CodePage {
    int cp;
    const uint16_t* high;
    std::unordered_map<wchar_t, char> reverse;
};

CodePage* MakeCodePage(int cp, const uint16_t* high) {
    auto* page = new CodePage{cp, high, {}};
    for (int i = 0; i < 128; ++i)
        page->reverse.emplace(static_cast<wchar_t>(high[i]), static_cast<char>(0x80 + i));
    return page;
}

CodePage* LookupCodePage(int cp) {
    static CodePage* const pages[] = {
        MakeCodePage(1250, kCp1250High), MakeCodePage(1251, kCp1251High), MakeCodePage(1252, kCp1252High),
        MakeCodePage(1253, kCp1253High), MakeCodePage(1254, kCp1254High), MakeCodePage(1257, kCp1257High),
    };
    for (CodePage* page : pages)
        if (page->cp == cp)
            return page;
    return pages[2];
}

std::atomic<CodePage*> g_page{nullptr};

CodePage* Current() {
    CodePage* page = g_page.load(std::memory_order_acquire);
    if (!page) {
        page = LookupCodePage(1252);
        g_page.store(page, std::memory_order_release);
    }
    return page;
}

} // namespace

void SetAnsiCodePage(int cp) { g_page.store(LookupCodePage(cp), std::memory_order_release); }

int GetAnsiCodePage() { return Current()->cp; }

wchar_t AnsiToUnicode(unsigned char ch) {
    if (ch < 0x80)
        return ch;
    return static_cast<wchar_t>(Current()->high[ch - 0x80]);
}

bool UnicodeToAnsi(wchar_t ch, char& out) {
    if (static_cast<uint32_t>(ch) < 0x80) {
        out = static_cast<char>(ch);
        return true;
    }
    const auto& reverse = Current()->reverse;
    auto it = reverse.find(ch);
    if (it == reverse.end()) {
        out = '?';
        return false;
    }
    out = it->second;
    return true;
}

wxString ToWx(const char* s, int length) {
    if (!s)
        return wxString();
    if (length < 0)
        length = static_cast<int>(strlen(s));
    std::wstring w;
    w.reserve(length);
    for (int i = 0; i < length; ++i)
        w.push_back(AnsiToUnicode(static_cast<unsigned char>(s[i])));
    return wxString(w);
}

std::string FromWx(const wxString& s) {
    std::string out;
    out.reserve(s.length());
    for (wxString::const_iterator it = s.begin(); it != s.end(); ++it) {
        char ch;
        UnicodeToAnsi(static_cast<wchar_t>((*it).GetValue()), ch);
        out.push_back(ch);
    }
    return out;
}

std::string AnsiToUtf8(const char* s, int length) { return std::string(ToWx(s, length).utf8_str()); }

std::string AppPath(const char* nativePath) {
    if (!nativePath)
        return std::string();
    std::string p = Utf8ToAnsi(nativePath);
    for (char& ch : p)
        if (ch == '/')
            ch = '\\';
    return p;
}

std::string Utf8ToAnsi(const char* utf8) {
    if (!utf8)
        return std::string();
    return FromWx(wxString::FromUTF8(utf8));
}

std::string NativePath(const char* path) {
    if (!path)
        return std::string();
    std::string p(path);
    for (char& ch : p)
        if (ch == '\\')
            ch = '/';
    if (p.size() >= 2 && p[1] == ':' && ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')))
        p.erase(0, 2);
    bool ascii = true;
    for (unsigned char ch : p)
        if (ch >= 0x80) {
            ascii = false;
            break;
        }
    if (ascii)
        return p;
    return std::string(ToWx(p.c_str()).utf8_str());
}

} // namespace mfcwx
