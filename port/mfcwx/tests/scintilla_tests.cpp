// Built without the rest of mfcwx: the window functions scintilla.cpp calls are recording stubs below.

#include "windows_impl.h"

#include <wx/stc/stc.h>

#include "../../../scintilla/include/Scintilla.h"
#include "../../../scintilla/include/SciLexer.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <vector>

namespace {

struct Note {
    unsigned code = 0;
    int position = 0;
    int length = 0;
    int modificationType = 0;
    int ch = 0;
    std::string text;
};

std::vector<Note> g_notes;
std::vector<UINT> g_commands;
std::function<void(const SCNotification&)> g_onNotify;

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        ++g_checks;                                                                    \
        if (!(cond)) {                                                                 \
            ++g_failures;                                                              \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                              \
    } while (0)

#define CHECK_EQ(a, b)                                                                                 \
    do {                                                                                               \
        ++g_checks;                                                                                    \
        long long va_ = static_cast<long long>(a);                                                     \
        long long vb_ = static_cast<long long>(b);                                                     \
        if (va_ != vb_) {                                                                              \
            ++g_failures;                                                                              \
            fprintf(stderr, "%s:%d: CHECK_EQ failed: %s (%lld) != %s (%lld)\n", __FILE__, __LINE__, #a, \
                    va_, #b, vb_);                                                                     \
        }                                                                                              \
    } while (0)

#define CHECK_STR(a, b)                                                                                    \
    do {                                                                                                   \
        ++g_checks;                                                                                        \
        std::string sa_(a);                                                                                \
        std::string sb_(b);                                                                                \
        if (sa_ != sb_) {                                                                                  \
            ++g_failures;                                                                                  \
            fprintf(stderr, "%s:%d: CHECK_STR failed: %s (\"%s\") != %s (\"%s\")\n", __FILE__, __LINE__, #a, \
                    sa_.c_str(), #b, sb_.c_str());                                                         \
        }                                                                                                  \
    } while (0)

wxStyledTextCtrl* g_stc = nullptr;

LRESULT Send(UINT msg, WPARAM wp = 0, LPARAM lp = 0) {
    bool handled = false;
    LRESULT r = mfcwx::ScintillaWindowProc(g_stc, msg, wp, lp, handled);
    if (!handled)
        fprintf(stderr, "message %u not handled\n", msg);
    return r;
}

LRESULT Send(UINT msg, WPARAM wp, const char* text) { return Send(msg, wp, reinterpret_cast<LPARAM>(text)); }
LRESULT Send(UINT msg, WPARAM wp, const void* p) { return Send(msg, wp, reinterpret_cast<LPARAM>(p)); }
WPARAM W(int v) { return static_cast<WPARAM>(static_cast<intptr_t>(v)); }

std::string GetAll() {
    LRESULT len = Send(SCI_GETLENGTH);
    std::vector<char> buf(static_cast<size_t>(len) + 1, 'x');
    Send(SCI_GETTEXT, static_cast<WPARAM>(len + 1), buf.data());
    return std::string(buf.data(), static_cast<size_t>(len));
}

std::string Range(int a, int b) {
    std::vector<char> buf(static_cast<size_t>((b < 0 ? Send(SCI_GETLENGTH) : b) - a) + 1, 'x');
    TextRange tr;
    tr.chrg.cpMin = a;
    tr.chrg.cpMax = b;
    tr.lpstrText = buf.data();
    LRESULT n = Send(SCI_GETTEXTRANGE, 0, &tr);
    return std::string(buf.data(), static_cast<size_t>(n));
}

void SetBytes(const std::string& s) {
    Send(SCI_CLEARALL);
    Send(SCI_ADDTEXT, s.size(), s.data());
}

void TestCp1252RoundTrip() {
    mfcwx::SetAnsiCodePage(1252);
    const char* t = "Gr\xFC\xDF" "e \x80\n\x89 caf\xE9";
    Send(SCI_SETTEXT, 0, t);
    CHECK_EQ(Send(SCI_GETLENGTH), strlen(t));
    CHECK_EQ(Send(SCI_GETTEXTLENGTH), strlen(t));
    CHECK_EQ(Send(WM_GETTEXTLENGTH), strlen(t));
    CHECK(g_stc->GetLength() > static_cast<int>(strlen(t)));
    CHECK(g_stc->GetText() == wxString::FromUTF8("Gr\xC3\xBC\xC3\x9F" "e \xE2\x82\xAC\n\xE2\x80\xB0 caf\xC3\xA9"));
    CHECK_STR(GetAll(), t);

    char buf[64];
    memset(buf, 'x', sizeof buf);
    CHECK_EQ(Send(SCI_GETTEXT, 5, buf), 4);
    CHECK_STR(buf, "Gr\xFC\xDF");
    memset(buf, 'x', sizeof buf);
    CHECK_EQ(Send(WM_GETTEXT, 5, buf), 4);
    CHECK_STR(buf, "Gr\xFC\xDF");

    CHECK_EQ(Send(SCI_GETCHARAT, 2), static_cast<signed char>('\xFC'));
    CHECK_EQ(Send(SCI_GETCHARAT, 1), 'r');
    CHECK_STR(Range(3, 7), "\xDF" "e \x80");
    CHECK_STR(Range(9, -1), " caf\xE9");

    CHECK_EQ(Send(SCI_LINEFROMPOSITION, 8), 1);
    CHECK_EQ(Send(SCI_LINEFROMPOSITION, 6), 0);
    CHECK_EQ(Send(SCI_POSITIONFROMLINE, 1), 8);
    CHECK_EQ(Send(SCI_GETLINEENDPOSITION, 0), 7);
    CHECK_EQ(Send(SCI_LINELENGTH, 0), 8);
    CHECK_EQ(Send(SCI_GETCOLUMN, 11), 3);
    CHECK_EQ(Send(SCI_POSITIONAFTER, 2), 3);
    CHECK_EQ(Send(SCI_POSITIONBEFORE, 7), 6);

    Send(SCI_SETSEL, 2, 7);
    CHECK_EQ(Send(SCI_GETSELECTIONSTART), 2);
    CHECK_EQ(Send(SCI_GETSELECTIONEND), 7);
    CHECK_EQ(Send(SCI_GETCURRENTPOS), 7);
    CHECK_EQ(Send(SCI_GETANCHOR), 2);
    CHECK_EQ(Send(SCI_GETSELTEXT, 0, static_cast<LPARAM>(0)), 6);
    memset(buf, 'x', sizeof buf);
    CHECK_EQ(Send(SCI_GETSELTEXT, 0, buf), 6);
    CHECK_STR(buf, "\xFC\xDF" "e \x80");
    Send(SCI_SETSEL, W(-1), W(-1));
    CHECK_EQ(Send(SCI_GETCURRENTPOS), strlen(t));
    Send(SCI_GOTOPOS, 10);
    CHECK_EQ(Send(SCI_GETCURRENTPOS), 10);
    CHECK_EQ(g_stc->GetCurrentPos(), 16);

    char line[64];
    memset(line, 'x', sizeof line);
    CHECK_EQ(Send(SCI_GETLINE, 0, line), 8);
    CHECK_STR(std::string(line, 8), "Gr\xFC\xDF" "e \x80\n");
    memset(line, 'x', sizeof line);
    CHECK_EQ(Send(SCI_GETCURLINE, sizeof line, line), 2);
    CHECK_STR(line, "\x89 caf\xE9");
    CHECK_EQ(Send(SCI_GETCURLINE, 0, static_cast<LPARAM>(0)), 7);

    const char* p = reinterpret_cast<const char*>(Send(SCI_GETCHARACTERPOINTER));
    CHECK_STR(p, t);

    Send(SCI_INSERTTEXT, 3, "\xE9");
    CHECK_STR(GetAll(), "Gr\xFC\xE9\xDF" "e \x80\n\x89 caf\xE9");
    Send(SCI_INSERTTEXT, W(-1), "\xA7");
    CHECK_EQ(Send(SCI_GETCHARAT, 11), static_cast<signed char>('\xA7'));
}

void TestAllBytes() {
    mfcwx::SetAnsiCodePage(1252);
    std::string all;
    for (int i = 0; i < 256; ++i)
        all.push_back(static_cast<char>(i));
    SetBytes(all);
    CHECK_EQ(Send(SCI_GETLENGTH), 256);
    CHECK(Range(0, -1) == all);
    CHECK(GetAll() == all);
    const char* p = reinterpret_cast<const char*>(Send(SCI_GETCHARACTERPOINTER));
    CHECK(memcmp(p, all.data(), 256) == 0);
    for (int i = 0; i < 256; i += 17)
        CHECK_EQ(Send(SCI_GETCHARAT, W(i)), static_cast<signed char>(i));

    std::vector<char> cells(256 * 2 + 2, 'x');
    TextRange tr;
    tr.chrg.cpMin = 0;
    tr.chrg.cpMax = 256;
    tr.lpstrText = cells.data();
    CHECK_EQ(Send(SCI_GETSTYLEDTEXT, 0, &tr), 512);
    bool same = true;
    for (int i = 0; i < 256; ++i)
        same = same && cells[i * 2] == static_cast<char>(i);
    CHECK(same);
    CHECK(cells[512] == 0 && cells[513] == 0);
}

void TestCp1250Positions() {
    mfcwx::SetAnsiCodePage(1250);
    const char* t = "Za\xBF\xF3\xB3\xE6 g\xEA\x9Cl\xB9 ja\x9F\xF1\r\nZA\xAF\xD3\xA3\xC6 koniec";
    size_t len = strlen(t);
    Send(SCI_SETTEXT, 0, t);
    CHECK_STR(GetAll(), t);
    CHECK(g_stc->GetText() == wxString::FromUTF8("Za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87 g\xC4\x99\xC5\x9Bl\xC4\x85 "
                                                  "ja\xC5\xBA\xC5\x84\r\nZA\xC5\xBB\xC3\x93\xC5\x81\xC4\x86 koniec"));
    CHECK_EQ(Send(SCI_GETLENGTH), len);

    TextToFind ft;
    ft.chrg.cpMin = 0;
    ft.chrg.cpMax = static_cast<long>(len);
    ft.lpstrText = const_cast<char*>("g\xEA\x9Cl\xB9");
    CHECK_EQ(Send(SCI_FINDTEXT, SCFIND_MATCHCASE, &ft), 7);
    CHECK_EQ(ft.chrgText.cpMin, 7);
    CHECK_EQ(ft.chrgText.cpMax, 12);
    CHECK_EQ(ft.chrg.cpMax, static_cast<long>(len));

    ft.chrg.cpMin = static_cast<long>(len);
    ft.chrg.cpMax = 0;
    ft.lpstrText = const_cast<char*>("ja\x9F\xF1");
    CHECK_EQ(Send(SCI_FINDTEXT, SCFIND_MATCHCASE, &ft), 13);
    CHECK_EQ(ft.chrgText.cpMax, 17);

    ft.chrg.cpMin = 0;
    ft.chrg.cpMax = static_cast<long>(len);
    ft.lpstrText = const_cast<char*>("\xAF\xD3\xA3\xC6");
    CHECK_EQ(Send(SCI_FINDTEXT, SCFIND_MATCHCASE, &ft), 21);
    // Scintilla 1.77 folds only ASCII when searching case-insensitively; so does wx's Scintilla.
    CHECK_EQ(Send(SCI_FINDTEXT, 0, &ft), 21);
    ft.lpstrText = const_cast<char*>("KONIEC");
    CHECK_EQ(Send(SCI_FINDTEXT, 0, &ft), 26);
    CHECK_EQ(ft.chrgText.cpMax, 32);

    ft.lpstrText = const_cast<char*>("nothere");
    CHECK_EQ(Send(SCI_FINDTEXT, 0, &ft), -1);

    CHECK_EQ(Send(SCI_LINEFROMPOSITION, 21), 1);
    CHECK_EQ(Send(SCI_POSITIONFROMLINE, 1), 19);
    CHECK_EQ(Send(SCI_GETLINEENDPOSITION, 0), 17);
    Send(SCI_SETSEL, 21, 25);
    char buf[64];
    Send(SCI_GETSELTEXT, 0, buf);
    CHECK_STR(buf, "\xAF\xD3\xA3\xC6");
    CHECK_STR(Range(2, 6), "\xBF\xF3\xB3\xE6");

    Send(SCI_SETSEL, 0, static_cast<LPARAM>(0));
    Send(SCI_SETSEARCHFLAGS, SCFIND_MATCHCASE);
    Send(SCI_SETTARGETSTART, 0);
    Send(SCI_SETTARGETEND, W(static_cast<int>(len)));
    CHECK_EQ(Send(SCI_SEARCHINTARGET, 2, "\xB3\xE6"), 4);
    CHECK_EQ(Send(SCI_GETTARGETSTART), 4);
    CHECK_EQ(Send(SCI_GETTARGETEND), 6);
    CHECK_EQ(Send(SCI_REPLACETARGET, W(-1), "\xA3\xC6!"), 3);
    CHECK_STR(GetAll(), "Za\xBF\xF3\xA3\xC6! g\xEA\x9Cl\xB9 ja\x9F\xF1\r\nZA\xAF\xD3\xA3\xC6 koniec");
    CHECK_EQ(Send(SCI_GETLENGTH), len + 1);

    Send(SCI_SETTEXT, 0, "\xB9\xB9 a \xB9");
    const char* find = "\xB9";
    const char* repl = "\xA5\xA5";
    int count = 0;
    long lEnd = static_cast<long>(Send(SCI_GETTEXTLENGTH));
    Send(SCI_SETTARGETSTART, 0);
    Send(SCI_SETTARGETEND, static_cast<WPARAM>(lEnd));
    long lPos = static_cast<long>(Send(SCI_SEARCHINTARGET, strlen(find), find));
    while (lPos < lEnd && lPos >= 0) {
        long lLen = static_cast<long>(Send(SCI_REPLACETARGET, strlen(repl), repl));
        lEnd = static_cast<long>(Send(SCI_GETTEXTLENGTH));
        Send(SCI_SETTARGETSTART, static_cast<WPARAM>(lPos + lLen));
        Send(SCI_SETTARGETEND, static_cast<WPARAM>(lEnd));
        lPos = static_cast<long>(Send(SCI_SEARCHINTARGET, strlen(find), find));
        ++count;
    }
    CHECK_EQ(count, 3);
    CHECK_STR(GetAll(), "\xA5\xA5\xA5\xA5 a \xA5\xA5");

    TEXTRANGE tr;
    char out[16];
    tr.chrg.cpMin = 1;
    tr.chrg.cpMax = 5;
    tr.lpstrText = out;
    bool handled = false;
    CHECK_EQ(mfcwx::ScintillaWindowProc(g_stc, EM_GETTEXTRANGE, 0, reinterpret_cast<LPARAM>(&tr), handled), 4);
    CHECK(handled);
    CHECK_STR(out, "\xA5\xA5\xA5 ");
    int s0 = -1;
    int s1 = -1;
    Send(SCI_SETSEL, 2, 4);
    mfcwx::ScintillaWindowProc(g_stc, EM_GETSEL, reinterpret_cast<WPARAM>(&s0), reinterpret_cast<LPARAM>(&s1), handled);
    CHECK_EQ(s0, 2);
    CHECK_EQ(s1, 4);
    mfcwx::ScintillaWindowProc(g_stc, EM_SETSEL, 0, -1, handled);
    CHECK_EQ(Send(SCI_GETSELECTIONEND), 9);
    CHECK(Send(SCI_CANUNDO) != 0);
    mfcwx::ScintillaWindowProc(g_stc, EM_EMPTYUNDOBUFFER, 0, 0, handled);
    CHECK(handled);
    CHECK_EQ(Send(SCI_CANUNDO), 0);
    mfcwx::SetAnsiCodePage(1252);
}

void TestDirectFunction() {
    mfcwx::SetAnsiCodePage(1252);
    auto fn = reinterpret_cast<SciFnDirect>(Send(SCI_GETDIRECTFUNCTION));
    sptr_t ptr = static_cast<sptr_t>(Send(SCI_GETDIRECTPOINTER));
    CHECK(fn != nullptr);
    CHECK(ptr == reinterpret_cast<sptr_t>(g_stc));
    fn(ptr, SCI_SETTEXT, 0, reinterpret_cast<sptr_t>("\xE9t\xE9 \x80"));
    CHECK_EQ(fn(ptr, SCI_GETLENGTH, 0, 0), 5);
    CHECK_EQ(fn(ptr, SCI_GETCHARAT, 2, 0), static_cast<signed char>('\xE9'));
    fn(ptr, SCI_GOTOPOS, 4, 0);
    CHECK_EQ(fn(ptr, SCI_GETCURRENTPOS, 0, 0), 4);
    CHECK_EQ(g_stc->GetCurrentPos(), 6);
    CHECK_STR(GetAll(), "\xE9t\xE9 \x80");
}

void TestCrypToolLexer() {
    mfcwx::SetAnsiCodePage(1252);
    Send(SCI_SETMODEVENTMASK, SC_MODEVENTMASKALL);
    Send(SCI_SETTEXT, 0, "A\xE4\xC4" "b \xD6x");
    Send(SCI_STYLESETFORE, 2, RGB(192, 192, 192));
    Send(SCI_SETPROPERTY, reinterpret_cast<WPARAM>("cryptool.nonalphabetstyle"), "2");
    Send(SCI_SETPROPERTY, reinterpret_cast<WPARAM>("cryptool.alphabet"), "ABCDEFGHIJKLMNOPQRSTUVWXYZ\xC4\xD6\xDC");
    Send(SCI_SETSTYLEBITS, 5);
    Send(SCI_SETLEXERLANGUAGE, 0, "CrypTool");
    CHECK(Send(SCI_GETLEXER) != SCLEX_NULL);
    CHECK_EQ(g_stc->GetLexer(), SCLEX_CONTAINER);
    char prop[64];
    CHECK_EQ(Send(SCI_GETPROPERTY, reinterpret_cast<WPARAM>("cryptool.alphabet"), prop), 29);
    CHECK_STR(prop, "ABCDEFGHIJKLMNOPQRSTUVWXYZ\xC4\xD6\xDC");
    Send(SCI_CLEARDOCUMENTSTYLE);
    g_notes.clear();
    Send(SCI_COLOURISE, 0, 1);
    CHECK(Send(SCI_GETENDSTYLED) >= 1);
    Send(SCI_COLOURISE, 0, W(-1));
    CHECK_EQ(Send(SCI_GETENDSTYLED), 7);
    const int expected[] = {0, 2, 0, 2, 2, 0, 2};
    for (int i = 0; i < 7; ++i)
        CHECK_EQ(Send(SCI_GETSTYLEAT, W(i)), expected[i]);
    CHECK_EQ(g_stc->GetStyleAt(2), 2);
    bool styleNote = false;
    for (const Note& n : g_notes)
        if (n.code == SCN_MODIFIED && (n.modificationType & SC_MOD_CHANGESTYLE) && n.position == 1 && n.length == 6)
            styleNote = true;
    CHECK(styleNote);
    bool styleNeededForwarded = false;
    for (const Note& n : g_notes)
        styleNeededForwarded = styleNeededForwarded || n.code == SCN_STYLENEEDED;
    CHECK(!styleNeededForwarded);

    Send(SCI_SETPROPERTY, reinterpret_cast<WPARAM>("cryptool.alphabet"), "abcdefghijklmnopqrstuvwxyz\xE4");
    Send(SCI_CLEARDOCUMENTSTYLE);
    Send(SCI_COLOURISE, 0, W(-1));
    const int expected2[] = {2, 0, 2, 0, 2, 2, 0};
    for (int i = 0; i < 7; ++i)
        CHECK_EQ(Send(SCI_GETSTYLEAT, W(i)), expected2[i]);

    Send(SCI_SETLEXER, SCLEX_NULL);
    CHECK_EQ(Send(SCI_GETLEXER), SCLEX_NULL);
    Send(SCI_CLEARDOCUMENTSTYLE);
    CHECK_EQ(Send(SCI_GETSTYLEAT, 1), 0);
    CHECK_EQ(Send(SCI_LOADLEXERLIBRARY, 0, "LexCrypTool"), 0);
}

void TestContainerStyling() {
    mfcwx::SetAnsiCodePage(1252);
    Send(SCI_SETLEXER, SCLEX_CONTAINER);
    Send(SCI_SETTEXT, 0, "\xE4\xF6\xFC" "abc");
    Send(SCI_CLEARDOCUMENTSTYLE);
    Send(SCI_STARTSTYLING, 1, 0x1f);
    Send(SCI_SETSTYLING, 2, 5);
    CHECK_EQ(Send(SCI_GETSTYLEAT, 0), 0);
    CHECK_EQ(Send(SCI_GETSTYLEAT, 1), 5);
    CHECK_EQ(Send(SCI_GETSTYLEAT, 2), 5);
    CHECK_EQ(Send(SCI_GETSTYLEAT, 3), 0);
    CHECK_EQ(Send(SCI_GETENDSTYLED), 3);
    Send(SCI_SETSTYLINGEX, 2, "\x07\x08");
    CHECK_EQ(Send(SCI_GETSTYLEAT, 3), 7);
    CHECK_EQ(Send(SCI_GETSTYLEAT, 4), 8);
    CHECK_EQ(Send(SCI_GETENDSTYLED), 5);
    Send(SCI_STARTSTYLING, 0, 0x1f);
    Send(SCI_SETSTYLINGEX, 3, "\x01\x02\x03");
    CHECK_EQ(g_stc->GetStyleAt(1), 1);
    CHECK_EQ(g_stc->GetStyleAt(3), 2);
    CHECK_EQ(g_stc->GetStyleAt(5), 3);
    CHECK_EQ(Send(SCI_GETENDSTYLED), 3);

    std::vector<char> cells(6 * 2 + 2);
    TextRange tr;
    tr.chrg.cpMin = 0;
    tr.chrg.cpMax = 6;
    tr.lpstrText = cells.data();
    CHECK_EQ(Send(SCI_GETSTYLEDTEXT, 0, &tr), 12);
    CHECK(cells[0] == '\xE4' && cells[1] == 1 && cells[2] == '\xF6' && cells[3] == 2 && cells[6] == 'a' && cells[7] == 7);

    Send(SCI_CLEARALL);
    const char styled[] = {'\xC4', 4, 'x', 6};
    Send(SCI_ADDSTYLEDTEXT, 4, styled);
    CHECK_STR(GetAll(), "\xC4x");
    CHECK_EQ(Send(SCI_GETSTYLEAT, 0), 4);
    CHECK_EQ(Send(SCI_GETSTYLEAT, 1), 6);

    Send(SCI_SETTEXT, 0, "\xE4\xF6\xFC" "abc");
    Send(SCI_CLEARDOCUMENTSTYLE);
    g_notes.clear();
    Send(SCI_COLOURISE, 0, 4);
    bool found = false;
    for (const Note& n : g_notes)
        if (n.code == SCN_STYLENEEDED && n.position == 4)
            found = true;
    CHECK(found);
    Send(SCI_SETLEXER, SCLEX_NULL);
}

void TestNotifications() {
    mfcwx::SetAnsiCodePage(1252);
    Send(SCI_SETMODEVENTMASK, SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT);
    CHECK_EQ(Send(SCI_GETMODEVENTMASK), SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT);
    Send(SCI_SETTEXT, 0, "ab");
    g_notes.clear();
    g_commands.clear();
    std::vector<std::string> seen;
    g_onNotify = [&seen](const SCNotification& scn) {
        if (scn.nmhdr.code == SCN_MODIFIED)
            seen.push_back(GetAll());
    };
    Send(SCI_INSERTTEXT, 1, "\xFC\xFC");
    CHECK_EQ(g_notes.size(), 1);
    if (!g_notes.empty()) {
        CHECK_EQ(g_notes[0].code, SCN_MODIFIED);
        CHECK(g_notes[0].modificationType & SC_MOD_INSERTTEXT);
        CHECK_EQ(g_notes[0].position, 1);
        CHECK_EQ(g_notes[0].length, 2);
        CHECK_STR(g_notes[0].text, "\xFC\xFC");
    }
    CHECK_EQ(g_commands.size(), 1);
    if (!g_commands.empty())
        CHECK_EQ(g_commands[0], SCEN_CHANGE);
    CHECK_EQ(seen.size(), 1);
    if (!seen.empty())
        CHECK_STR(seen[0], "a\xFC\xFC" "b");

    g_notes.clear();
    Send(SCI_SETSEL, 1, 3);
    Send(SCI_CLEAR);
    CHECK_EQ(g_notes.size(), 1);
    if (!g_notes.empty()) {
        CHECK(g_notes[0].modificationType & SC_MOD_DELETETEXT);
        CHECK_EQ(g_notes[0].position, 1);
        CHECK_EQ(g_notes[0].length, 2);
        CHECK_STR(g_notes[0].text, "\xFC\xFC");
    }
    g_onNotify = nullptr;

    Send(SCI_SETSAVEPOINT);
    g_notes.clear();
    Send(SCI_ADDTEXT, 1, "\xE9");
    bool left = false;
    for (const Note& n : g_notes)
        left = left || n.code == SCN_SAVEPOINTLEFT;
    CHECK(left);
    g_notes.clear();
    Send(SCI_UNDO);
    bool reached = false;
    for (const Note& n : g_notes)
        reached = reached || n.code == SCN_SAVEPOINTREACHED;
    CHECK(reached);

    Send(SCI_SETMODEVENTMASK, SC_MODEVENTMASKALL);
    g_notes.clear();
    Send(SCI_SETTEXT, 0, "\xE4x");
    bool before = false;
    for (const Note& n : g_notes)
        before = before || (n.modificationType & SC_MOD_BEFOREINSERT);
    CHECK(before);
    Send(SCI_SETMODEVENTMASK, SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT);
}

void TestSanitizedInput() {
    mfcwx::SetAnsiCodePage(1252);
    g_stc->SetText(wxString::FromUTF8("x\xE4\xB8\xAD" "y\xC5\x82\xC3\xA9"));
    CHECK_STR(GetAll(), "x?y?\xE9");
    CHECK(g_stc->GetText() == wxString::FromUTF8("x?y?\xC3\xA9"));
    g_stc->InsertText(0, wxString::FromUTF8("\xE2\x82\xAC"));
    CHECK_STR(GetAll(), "\x80x?y?\xE9");
    CHECK_EQ(Send(SCI_GETLENGTH), 6);
}

void TestMisc() {
    mfcwx::SetAnsiCodePage(1252);
    Send(SCI_SETWRAPMODE, SC_WRAP_WORD);
    CHECK_EQ(Send(SCI_GETWRAPMODE), SC_WRAP_WORD);
    CHECK_EQ(g_stc->GetWrapMode(), wxSTC_WRAP_WHITESPACE);
    Send(SCI_SETWRAPMODE, SC_WRAP_CHAR);
    CHECK_EQ(Send(SCI_GETWRAPMODE), SC_WRAP_CHAR);
    Send(SCI_SETWRAPMODE, SC_WRAP_NONE);

    Send(SCI_SETCODEPAGE, 0);
    CHECK_EQ(Send(SCI_GETCODEPAGE), 0);
    CHECK_EQ(g_stc->GetCodePage(), wxSTC_CP_UTF8);

    Send(SCI_SETTEXT, 0, "\xE4\xF6\xFC");
    struct {
        void* hdc;
        void* hdcTarget;
        int rc[4];
        int rcPage[4];
        CharacterRange chrg;
    } fr = {nullptr, nullptr, {0, 0, 100, 100}, {0, 0, 100, 100}, {0, 3}};
    CHECK_EQ(Send(SCI_FORMATRANGE, 1, &fr), 3);
    CHECK(Send(SCI_TEXTWIDTH, STYLE_LINENUMBER, "9") > 0);
    Send(SCI_STYLESETFONT, STYLE_DEFAULT, "Courier");
    char font[64];
    CHECK_EQ(Send(SCI_STYLEGETFONT, STYLE_DEFAULT, font), 7);
    CHECK_STR(font, "Courier");

    Send(SCI_EMPTYUNDOBUFFER);
    CHECK_EQ(Send(SCI_CANUNDO), 0);
    Send(SCI_SETSEL, 0, 3);
    Send(SCI_COPY);
    Send(SCI_SETUNDOCOLLECTION, 0);
    Send(SCI_SETUNDOCOLLECTION, 1);
    CHECK_EQ(Send(SCI_BRACEMATCH, W(-1)), -1);
    Send(SCI_SETTEXT, 0, "(\xE4)");
    CHECK_EQ(Send(SCI_BRACEMATCH, 0), 2);
    CHECK_EQ(Send(SCI_BRACEMATCH, 2), 0);
    Send(SCI_BRACEHIGHLIGHT, 0, 2);
}

void TestLargeDocument() {
    mfcwx::SetAnsiCodePage(1250);
    std::mt19937 rng(12345);
    auto randomText = [&rng](size_t n) {
        std::string s;
        s.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            unsigned r = rng() % 100;
            if (r < 60)
                s.push_back(static_cast<char>('a' + rng() % 26));
            else if (r < 65)
                s.append("\r\n");
            else
                s.push_back(static_cast<char>(0x80 + rng() % 128));
        }
        return s;
    };
    std::string model = randomText(300000);
    auto t0 = std::chrono::steady_clock::now();
    SetBytes(model);
    std::string copy = GetAll();
    long long sum = 0;
    for (int i = 0; i < 20000; ++i) {
        int p = static_cast<int>(rng() % (model.size() + 1));
        sum += Send(SCI_LINEFROMPOSITION, W(p)) + Send(SCI_POSITIONAFTER, W(p));
    }
    for (int i = 0; i < 2000; ++i) {
        int p = static_cast<int>(rng() % model.size());
        Send(SCI_INSERTTEXT, W(p), "\xE4");
        Send(SCI_SETTARGETSTART, W(p));
        Send(SCI_SETTARGETEND, W(p + 1));
        Send(SCI_REPLACETARGET, 0, "");
        sum += Send(SCI_GETLENGTH);
    }
    double proxyMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    auto t1 = std::chrono::steady_clock::now();
    for (int i = 0; i < 20000; ++i) {
        int p = static_cast<int>(rng() % (model.size() + 1));
        sum += g_stc->SendMsg(SCI_LINEFROMPOSITION, p) + g_stc->SendMsg(SCI_POSITIONAFTER, p);
    }
    for (int i = 0; i < 2000; ++i) {
        int p = static_cast<int>(rng() % model.size());
        g_stc->SendMsg(SCI_INSERTTEXT, p, reinterpret_cast<wxIntPtr>("\xC3\xA4"));
        g_stc->SendMsg(SCI_SETTARGETSTART, p);
        g_stc->SendMsg(SCI_SETTARGETEND, p + 2);
        g_stc->SendMsg(SCI_REPLACETARGET, 0, reinterpret_cast<wxIntPtr>(""));
        sum += g_stc->SendMsg(SCI_GETLENGTH);
    }
    double rawMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t1).count();
    printf("large document: load + 20000 lookups + 2000 edits: proxy %.1f ms, same calls sent to wx directly %.1f ms (%lld)\n",
           proxyMs, rawMs, sum);
    CHECK(copy == model);
    CHECK(GetAll() == model);
    bool ok = Send(SCI_GETLENGTH) == static_cast<LRESULT>(model.size());
    for (int i = 0; i < 300 && ok; ++i) {
        int p = static_cast<int>(rng() % (model.size() + 1));
        Send(SCI_GOTOPOS, W(p));
        ok = Send(SCI_GETCURRENTPOS) == p && g_stc->GetCurrentPos() == g_stc->PositionRelative(0, p);
        if (!ok)
            fprintf(stderr, "position %d mismatch\n", p);
        if (p < static_cast<int>(model.size()))
            ok = ok && Send(SCI_GETCHARAT, W(p)) == static_cast<signed char>(model[static_cast<size_t>(p)]);
    }
    CHECK(ok);
    for (int i = 0; i < 300 && ok; ++i) {
        int op = static_cast<int>(rng() % 3);
        int p = static_cast<int>(rng() % (model.size() + 1));
        if (op == 0) {
            std::string ins = randomText(1 + rng() % 20);
            Send(SCI_INSERTTEXT, W(p), ins.c_str());
            model.insert(static_cast<size_t>(p), ins);
        } else if (op == 1 && p < static_cast<int>(model.size())) {
            int n = std::min(static_cast<int>(1 + rng() % 30), static_cast<int>(model.size()) - p);
            Send(SCI_SETSEL, W(p), static_cast<LPARAM>(p + n));
            Send(SCI_CLEAR);
            model.erase(static_cast<size_t>(p), static_cast<size_t>(n));
        } else {
            int n = std::min(static_cast<int>(rng() % 50), static_cast<int>(model.size()) - p);
            ok = Range(p, p + n) == model.substr(static_cast<size_t>(p), static_cast<size_t>(n));
        }
        ok = ok && Send(SCI_GETLENGTH) == static_cast<LRESULT>(model.size());
        int q = static_cast<int>(rng() % (model.size() + 1));
        ok = ok && Send(SCI_LINEFROMPOSITION, W(q)) == g_stc->LineFromPosition(g_stc->PositionRelative(0, q));
    }
    CHECK(ok);
    CHECK(GetAll() == model);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    printf("large document: %zu chars, total with brute-force verification %.1f ms\n", model.size(), ms);
    mfcwx::SetAnsiCodePage(1252);
}

int RunTests() {
    wxFrame* frame = new wxFrame(nullptr, wxID_ANY, "scintilla tests");
    g_stc = new wxStyledTextCtrl(frame, wxID_ANY);
    mfcwx::BindScintillaEvents(g_stc);
    TestCp1252RoundTrip();
    TestAllBytes();
    TestCp1250Positions();
    TestDirectFunction();
    TestCrypToolLexer();
    TestContainerStyling();
    TestNotifications();
    TestSanitizedInput();
    TestMisc();
    TestLargeDocument();
    frame->Destroy();
    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}

int g_rc = 1;

class TestApp : public wxApp {
public:
    bool OnInit() override {
        g_rc = RunTests();
        return false;
    }
};

} // namespace

namespace mfcwx {

void NotifyParent(wxWindow*, UINT code) { g_commands.push_back(code); }

LRESULT NotifyParentNM(wxWindow* control, NMHDR* hdr) {
    hdr->hwndFrom = ToHwnd(control);
    const SCNotification& scn = *reinterpret_cast<const SCNotification*>(hdr);
    Note n;
    n.code = scn.nmhdr.code;
    n.position = scn.position;
    n.length = scn.length;
    n.modificationType = scn.modificationType;
    n.ch = scn.ch;
    if (scn.text)
        n.text.assign(scn.text, static_cast<size_t>(scn.length));
    g_notes.push_back(n);
    if (g_onNotify)
        g_onNotify(scn);
    return 0;
}

bool IsMainThread() { return true; }
void RunOnMainThread(const std::function<void()>& fn) { fn(); }

} // namespace mfcwx

CDC* CDC::FromHandle(HDC) { return nullptr; }
wxDC* CDC::GetWx() const { return nullptr; }

wxIMPLEMENT_APP_NO_MAIN(TestApp);

int main(int argc, char** argv) {
    wxEntry(argc, argv);
    return g_rc;
}
