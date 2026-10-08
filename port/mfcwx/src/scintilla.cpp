#include "windows_impl.h"

#include <wx/stc/stc.h>

// The application's Scintilla 1.77 headers; message ids are unchanged in wx's Scintilla.
#include "../../../scintilla/include/Scintilla.h"
#include "../../../scintilla/include/SciLexer.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <strings.h>
#include <unordered_map>
#include <vector>

static_assert(offsetof(SCNotification, nmhdr) == 0, "SCNotification must start with its header");
static_assert(sizeof(NotifyHeader) == sizeof(NMHDR), "NotifyHeader must match NMHDR");
static_assert(offsetof(NotifyHeader, code) == offsetof(NMHDR, code), "NotifyHeader must match NMHDR");

namespace mfcwx {
namespace {

constexpr int kModInsertCheck = 0x100000;
constexpr int kWrapWhitespace = 3;
constexpr int kCrypToolLexer = SCLEX_AUTOMATIC + 1;
constexpr int kAlwaysWatched = SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT | SC_MOD_BEFOREDELETE | kModInsertCheck;

struct Codec {
    int cp = 0;
    unsigned char encLen[256];
    char enc[256][3];
    std::vector<int16_t> dec;
};

const Codec& CurrentCodec() {
    static Codec codec;
    int cp = GetAnsiCodePage();
    if (codec.cp == cp && !codec.dec.empty())
        return codec;
    codec.cp = cp;
    codec.dec.assign(0x10000, -1);
    for (int b = 0; b < 256; ++b) {
        uint32_t u = b < 0x80 ? static_cast<uint32_t>(b)
                              : static_cast<uint32_t>(AnsiToUnicode(static_cast<unsigned char>(b))) & 0xFFFF;
        char* o = codec.enc[b];
        if (u < 0x80) {
            o[0] = static_cast<char>(u);
            codec.encLen[b] = 1;
        } else if (u < 0x800) {
            o[0] = static_cast<char>(0xC0 | (u >> 6));
            o[1] = static_cast<char>(0x80 | (u & 0x3F));
            codec.encLen[b] = 2;
        } else {
            o[0] = static_cast<char>(0xE0 | (u >> 12));
            o[1] = static_cast<char>(0x80 | ((u >> 6) & 0x3F));
            o[2] = static_cast<char>(0x80 | (u & 0x3F));
            codec.encLen[b] = 3;
        }
        codec.dec[u] = static_cast<int16_t>(b);
    }
    return codec;
}

// Like Scintilla, every byte of an invalid sequence counts as a character.
int Utf8Width(const unsigned char* s, size_t avail) {
    unsigned c = s[0];
    if (c < 0xC2)
        return 1;
    int n;
    if (c < 0xE0)
        n = 2;
    else if (c < 0xF0)
        n = 3;
    else if (c < 0xF5)
        n = 4;
    else
        return 1;
    if (static_cast<size_t>(n) > avail)
        return 1;
    for (int i = 1; i < n; ++i)
        if ((s[i] & 0xC0) != 0x80)
            return 1;
    if (n == 3) {
        if ((c == 0xE0 && s[1] < 0xA0) || (c == 0xED && s[1] >= 0xA0))
            return 1;
        if (c == 0xEF && s[1] == 0xBF && (s[2] == 0xBE || s[2] == 0xBF))
            return 1;
    } else if (n == 4) {
        if ((c == 0xF0 && s[1] < 0x90) || (c == 0xF4 && s[1] >= 0x90))
            return 1;
    }
    return n;
}

uint32_t Utf8Decode(const unsigned char* s, int n) {
    switch (n) {
    case 1:
        return s[0];
    case 2:
        return ((s[0] & 0x1Fu) << 6) | (s[1] & 0x3Fu);
    case 3:
        return ((s[0] & 0x0Fu) << 12) | ((s[1] & 0x3Fu) << 6) | (s[2] & 0x3Fu);
    default:
        return ((s[0] & 0x07u) << 18) | ((s[1] & 0x3Fu) << 12) | ((s[2] & 0x3Fu) << 6) | (s[3] & 0x3Fu);
    }
}

bool AnsiFromCodePoint(const Codec& c, uint32_t u, char& out) {
    if (u < 0x80) {
        out = static_cast<char>(u);
        return true;
    }
    if (u < 0x10000 && c.dec[u] >= 0) {
        out = static_cast<char>(c.dec[u]);
        return true;
    }
    out = '?';
    return false;
}

bool AnsiAt(const Codec& c, const unsigned char* s, int n, char& out) {
    if (n == 1 && s[0] >= 0x80) {
        out = '?';
        return false;
    }
    return AnsiFromCodePoint(c, Utf8Decode(s, n), out);
}

std::string Utf8FromAnsi(const char* s, size_t n) {
    const Codec& c = CurrentCodec();
    std::string out;
    out.reserve(n + n / 2);
    for (size_t i = 0; i < n; ++i) {
        unsigned char b = static_cast<unsigned char>(s[i]);
        if (b < 0x80)
            out.push_back(static_cast<char>(b));
        else
            out.append(c.enc[b], c.encLen[b]);
    }
    return out;
}

std::string Utf8FromAnsi(const char* s) { return s ? Utf8FromAnsi(s, strlen(s)) : std::string(); }

std::string AnsiFromUtf8(const char* s, size_t n) {
    const Codec& c = CurrentCodec();
    std::string out;
    out.reserve(n);
    const unsigned char* p = reinterpret_cast<const unsigned char*>(s);
    for (size_t i = 0; i < n;) {
        if (p[i] < 0x80) {
            out.push_back(static_cast<char>(p[i++]));
            continue;
        }
        int w = Utf8Width(p + i, n - i);
        char a;
        AnsiAt(c, p + i, w, a);
        out.push_back(a);
        i += static_cast<size_t>(w);
    }
    return out;
}

std::string AnsiFromWx(const wxString& s) {
    const Codec& c = CurrentCodec();
    std::string out;
    out.reserve(s.length());
    for (wxString::const_iterator it = s.begin(); it != s.end(); ++it) {
        char a;
        AnsiFromCodePoint(c, static_cast<uint32_t>((*it).GetValue()), a);
        out.push_back(a);
    }
    return out;
}

const unsigned char* RangeBytes(wxStyledTextCtrl* s, int pos, int len) {
    return reinterpret_cast<const unsigned char*>(s->GetRangePointer(pos, len));
}

int CountRange(wxStyledTextCtrl* s, int pos, int len, bool* high) {
    if (high)
        *high = false;
    if (len <= 0)
        return 0;
    const unsigned char* p = RangeBytes(s, pos, len);
    int chars = 0;
    for (int i = 0; i < len; ++chars) {
        if (p[i] < 0x80) {
            ++i;
            continue;
        }
        if (high)
            *high = true;
        i += Utf8Width(p + i, static_cast<size_t>(len - i));
    }
    return chars;
}

class PosMap {
public:
    static constexpr int kStride = 256;
    static constexpr int kChunk = 1 << 16;

    void Reset() { Restart(); }

    void Changed(wxStyledTextCtrl* s, int pos, int byteDelta, int charDelta, bool charsKnown, bool insertedNonAscii) {
        if (ascii_) {
            if (!insertedNonAscii)
                return;
            int before = s->GetLength() - byteDelta;
            int keep = std::max(0, std::min(pos, before)) / kStride;
            ascii_ = false;
            complete_ = false;
            firstHigh_ = INT_MAX;
            marks_.resize(static_cast<size_t>(keep) + 1);
            for (int k = 0; k <= keep; ++k)
                marks_[static_cast<size_t>(k)] = k * kStride;
            chars_ = bytes_ = keep * kStride;
            total_ = charsKnown ? before + charDelta : -1;
            return;
        }
        if (pos < 0)
            pos = 0;
        size_t k = static_cast<size_t>(std::upper_bound(marks_.begin(), marks_.end(), pos) - marks_.begin()) - 1;
        marks_.resize(k + 1);
        chars_ = static_cast<int>(k) * kStride;
        bytes_ = marks_[k];
        complete_ = false;
        if (firstHigh_ >= bytes_)
            firstHigh_ = INT_MAX;
        total_ = total_ >= 0 && charsKnown ? total_ + charDelta : -1;
    }

    int ToBytes(wxStyledTextCtrl* s, int ansi) {
        if (ansi <= 0 || ascii_)
            return ansi;
        Validate(s);
        if (!complete_ && chars_ < ansi)
            Scan(s, ansi, 0);
        if (ascii_)
            return ansi;
        if (ansi >= chars_)
            return bytes_ + (ansi - chars_);
        size_t k = static_cast<size_t>(ansi / kStride);
        int off = marks_[k];
        int count = ansi - static_cast<int>(k) * kStride;
        int span = std::min(s->GetLength() - off, count * 4 + 4);
        const unsigned char* p = RangeBytes(s, off, span);
        int i = 0;
        for (; count > 0 && i < span; --count)
            i += p[i] < 0x80 ? 1 : Utf8Width(p + i, static_cast<size_t>(span - i));
        return off + i;
    }

    int ToAnsi(wxStyledTextCtrl* s, int bytes) {
        if (bytes <= 0 || ascii_)
            return bytes;
        Validate(s);
        if (!complete_ && bytes_ < bytes)
            Scan(s, 0, bytes);
        if (ascii_)
            return bytes;
        if (bytes >= bytes_)
            return chars_ + (bytes - bytes_);
        size_t k = static_cast<size_t>(std::upper_bound(marks_.begin(), marks_.end(), bytes) - marks_.begin()) - 1;
        int off = marks_[k];
        int idx = static_cast<int>(k) * kStride;
        int span = std::min(s->GetLength() - off, bytes - off + 4);
        const unsigned char* p = RangeBytes(s, off, span);
        for (int i = 0; off + i < bytes;) {
            int w = p[i] < 0x80 ? 1 : Utf8Width(p + i, static_cast<size_t>(span - i));
            if (off + i + w > bytes)
                break;
            i += w;
            ++idx;
        }
        return idx;
    }

    int AnsiLength(wxStyledTextCtrl* s) {
        if (ascii_)
            return s->GetLength();
        Validate(s);
        if (total_ >= 0)
            return total_;
        Scan(s, INT_MAX, INT_MAX);
        return ascii_ ? s->GetLength() : chars_;
    }

private:
    void Restart() {
        ascii_ = false;
        complete_ = false;
        chars_ = bytes_ = 0;
        total_ = -1;
        firstHigh_ = INT_MAX;
        marks_.assign(1, 0);
    }

    // Safety net against changes that bypassed the modification events.
    void Validate(wxStyledTextCtrl* s) {
        int n = s->GetLength();
        if (bytes_ > n || (complete_ && bytes_ != n) || total_ > n)
            Restart();
    }

    void Scan(wxStyledTextCtrl* s, int targetChars, int targetBytes) {
        int n = s->GetLength();
        while (bytes_ < n && (chars_ < targetChars || bytes_ < targetBytes)) {
            int base = bytes_;
            int chunk = std::min(n - base, kChunk);
            const unsigned char* p = RangeBytes(s, base, chunk);
            int i = 0;
            while (i < chunk && (chars_ < targetChars || base + i < targetBytes)) {
                int w = 1;
                if (p[i] >= 0x80) {
                    if (chunk - i < 4 && base + chunk < n)
                        break;
                    w = Utf8Width(p + i, static_cast<size_t>(chunk - i));
                    if (firstHigh_ == INT_MAX)
                        firstHigh_ = base + i;
                }
                i += w;
                ++chars_;
                if (chars_ % kStride == 0)
                    marks_.push_back(base + i);
            }
            bytes_ = base + i;
        }
        if (bytes_ >= n) {
            complete_ = true;
            total_ = chars_;
            if (firstHigh_ == INT_MAX)
                ascii_ = true;
        }
    }

    bool ascii_ = true;
    bool complete_ = true;
    int chars_ = 0;
    int bytes_ = 0;
    int total_ = 0;
    int firstHigh_ = INT_MAX;
    std::vector<int> marks_{0};
};

struct SciData {
    wxStyledTextCtrl* stc = nullptr;
    PosMap map;
    int appModMask = SC_MODEVENTMASKALL;
    int appCodePage = 0;
    bool cryptoolLexer = false;
    std::map<std::string, std::string> props;
    bool alphabet[256] = {};
    bool alphabetEmpty = true;
    int nonAlphabetStyle = 0;
    int stylingAnsi = 0;
    int lengthForEncode = -1;
    std::string mirror;
    bool mirrorValid = false;
    int pendingDeletePos = -1;
    int pendingDeleteLength = 0;
    int pendingDeleteChars = 0;
};

std::unordered_map<wxWindow*, std::unique_ptr<SciData>>& Registry() {
    static std::unordered_map<wxWindow*, std::unique_ptr<SciData>> registry;
    return registry;
}

SciData* FindData(wxWindow* w) {
    auto it = Registry().find(w);
    return it == Registry().end() ? nullptr : it->second.get();
}

int ControlModMask(int appMask) { return appMask | kAlwaysWatched; }

int PosArg(uintptr_t v) { return static_cast<int>(static_cast<intptr_t>(v)); }
int PosArg(intptr_t v) { return static_cast<int>(v); }
WPARAM PosW(int pos) { return static_cast<WPARAM>(static_cast<intptr_t>(pos)); }
LPARAM PosL(int pos) { return static_cast<LPARAM>(pos); }

LRESULT Pass(SciData& d, UINT msg, WPARAM wp, LPARAM lp) {
    return static_cast<LRESULT>(d.stc->SendMsg(static_cast<int>(msg), static_cast<wxUIntPtr>(wp), static_cast<wxIntPtr>(lp)));
}

LPARAM Ptr(const std::string& s) { return reinterpret_cast<LPARAM>(s.c_str()); }

int A2B(SciData& d, int ansi) { return d.map.ToBytes(d.stc, ansi); }
int B2A(SciData& d, int bytes) { return d.map.ToAnsi(d.stc, bytes); }
LRESULT B2A(SciData& d, LRESULT bytes) { return B2A(d, static_cast<int>(bytes)); }

std::string AnsiOfBytes(SciData& d, int b0, int b1) {
    int n = d.stc->GetLength();
    b0 = std::max(0, std::min(b0, n));
    b1 = std::max(b0, std::min(b1, n));
    if (b1 == b0)
        return std::string();
    return AnsiFromUtf8(d.stc->GetRangePointer(b0, b1 - b0), static_cast<size_t>(b1 - b0));
}

std::string AnsiRange(SciData& d, int a0, int a1) {
    if (a1 <= a0)
        return std::string();
    return AnsiOfBytes(d, A2B(d, std::max(a0, 0)), A2B(d, a1));
}

LRESULT PutString(const std::string& s, LPARAM buf) {
    if (buf) {
        char* out = reinterpret_cast<char*>(buf);
        memcpy(out, s.data(), s.size());
        out[s.size()] = '\0';
    }
    return static_cast<LRESULT>(s.size());
}

std::string FetchString(SciData& d, UINT msg, WPARAM wp, bool sizeIncludesNul) {
    LRESULT n = Pass(d, msg, wp, 0);
    if (sizeIncludesNul)
        --n;
    if (n <= 0)
        return std::string();
    std::vector<char> buf(static_cast<size_t>(n) + 2, '\0');
    Pass(d, msg, wp, reinterpret_cast<LPARAM>(buf.data()));
    return AnsiFromUtf8(buf.data(), static_cast<size_t>(n));
}

void RememberCopied(std::string ansiBytes, size_t minSize = 0) {
    if (ansiBytes.empty())
        return;
    RememberClipboardText(std::move(ansiBytes), minSize);
}

void RebuildAlphabet(SciData& d) {
    const std::string& a = d.props["cryptool.alphabet"];
    std::fill(std::begin(d.alphabet), std::end(d.alphabet), false);
    for (unsigned char ch : a)
        d.alphabet[ch] = true;
    d.alphabetEmpty = a.empty();
    d.nonAlphabetStyle = atoi(d.props["cryptool.nonalphabetstyle"].c_str());
}

void Notify(wxWindow* w, SCNotification& scn) { NotifyParentNM(w, reinterpret_cast<NMHDR*>(&scn.nmhdr)); }

SCNotification MakeNotification(unsigned code) {
    SCNotification scn;
    memset(&scn, 0, sizeof scn);
    scn.nmhdr.code = code;
    return scn;
}

void LexCrypTool(SciData& d, int endPos) {
    wxStyledTextCtrl* s = d.stc;
    int length = s->GetLength();
    int end = std::min(endPos, length);
    int start = s->PositionFromLine(s->LineFromPosition(s->GetEndStyled()));
    if (end <= start)
        return;
    int n = end - start;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(s->GetRangePointer(start, n));
    std::vector<char> styles(static_cast<size_t>(n), 0);
    if (!d.alphabetEmpty) {
        const Codec& c = CurrentCodec();
        char other = static_cast<char>(d.nonAlphabetStyle);
        for (int i = 0; i < n;) {
            int w = p[i] < 0x80 ? 1 : Utf8Width(p + i, static_cast<size_t>(n - i));
            char a;
            bool known = AnsiAt(c, p + i, w, a);
            if (!known || !d.alphabet[static_cast<unsigned char>(a)])
                std::fill(styles.begin() + i, styles.begin() + i + w, other);
            i += w;
        }
    }
    s->SendMsg(SCI_STARTSTYLING, start, 0xff);
    s->SendMsg(SCI_SETSTYLINGEX, n, reinterpret_cast<wxIntPtr>(styles.data()));
}

// Text typed, pasted or dropped is reduced to the ANSI code page, as on Windows.
void SanitizeInsertion(SciData& d, wxStyledTextEvent& e) {
    wxString text = e.GetString();
    const Codec& c = CurrentCodec();
    bool bad = false;
    for (wxString::const_iterator it = text.begin(); it != text.end() && !bad; ++it) {
        char a;
        bad = !AnsiFromCodePoint(c, static_cast<uint32_t>((*it).GetValue()), a);
    }
    if (!bad)
        return;
    wxString clean;
    clean.reserve(text.length());
    for (wxString::const_iterator it = text.begin(); it != text.end(); ++it) {
        char a;
        clean += AnsiFromCodePoint(c, static_cast<uint32_t>((*it).GetValue()), a) ? *it : wxUniChar('?');
    }
    d.stc->ChangeInsertion(static_cast<int>(clean.utf8_str().length()), clean);
}

void OnModified(wxWindow* w, wxStyledTextEvent& e) {
    SciData* d = FindData(w);
    if (!d)
        return;
    int type = e.GetModificationType();
    if (type & kModInsertCheck) {
        SanitizeInsertion(*d, e);
        return;
    }
    int pos = e.GetPosition();
    int len = e.GetLength();
    int ansiLength = -1;
    if (type & SC_MOD_BEFOREDELETE) {
        d->pendingDeletePos = pos;
        d->pendingDeleteLength = len;
        d->pendingDeleteChars = ansiLength = CountRange(d->stc, pos, len, nullptr);
    } else if (type & SC_MOD_INSERTTEXT) {
        bool high = false;
        ansiLength = CountRange(d->stc, pos, len, &high);
        d->map.Changed(d->stc, pos, len, ansiLength, true, high);
        d->mirrorValid = false;
    } else if (type & SC_MOD_DELETETEXT) {
        bool known = d->pendingDeletePos == pos && d->pendingDeleteLength == len;
        if (known)
            ansiLength = d->pendingDeleteChars;
        d->pendingDeletePos = -1;
        d->map.Changed(d->stc, pos, -len, known ? -ansiLength : 0, known, false);
        d->mirrorValid = false;
    }
    if (!(type & d->appModMask))
        return;
    SCNotification scn = MakeNotification(SCN_MODIFIED);
    scn.modificationType = type;
    scn.linesAdded = e.GetLinesAdded();
    scn.line = e.GetLine();
    scn.foldLevelNow = e.GetFoldLevelNow();
    scn.foldLevelPrev = e.GetFoldLevelPrev();
    scn.position = B2A(*d, pos);
    std::string text;
    if (type & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT | SC_MOD_BEFOREINSERT)) {
        text = AnsiFromWx(e.GetString());
        scn.text = text.c_str();
        if (ansiLength < 0)
            ansiLength = text.empty() && len > 0 ? len : static_cast<int>(text.size());
    } else if (ansiLength < 0 && len > 0) {
        ansiLength = B2A(*d, pos + len) - scn.position;
    }
    scn.length = std::max(ansiLength, 0);
    if (!(type & (SC_MOD_CHANGESTYLE | SC_MOD_CHANGEINDICATOR)))
        NotifyParent(w, SCEN_CHANGE);
    Notify(w, scn);
}

void OnStyleNeeded(wxWindow* w, wxStyledTextEvent& e) {
    SciData* d = FindData(w);
    if (!d)
        return;
    if (d->cryptoolLexer) {
        LexCrypTool(*d, e.GetPosition());
        return;
    }
    SCNotification scn = MakeNotification(SCN_STYLENEEDED);
    scn.position = B2A(*d, e.GetPosition());
    Notify(w, scn);
}

int MacroLParam(const wxStyledTextEvent& e) {
    switch (e.GetMessage()) {
    case SCI_REPLACESEL:
    case SCI_ADDTEXT:
    case SCI_INSERTTEXT:
    case SCI_APPENDTEXT:
    case SCI_SEARCHNEXT:
    case SCI_SEARCHPREV:
        return 0;
    default:
        return e.GetLParam();
    }
}

void ForwardEvent(wxWindow* w, wxStyledTextEvent& e) {
    SciData* d = FindData(w);
    if (!d || w->IsBeingDeleted())
        return;
    wxEventType t = e.GetEventType();
    SCNotification scn = MakeNotification(0);
    std::string text;
    scn.modifiers = e.GetModifiers();
    if (t == wxEVT_STC_CHARADDED) {
        char a;
        AnsiFromCodePoint(CurrentCodec(), static_cast<uint32_t>(e.GetKey()), a);
        scn.nmhdr.code = SCN_CHARADDED;
        scn.ch = static_cast<unsigned char>(a);
    } else if (t == wxEVT_STC_SAVEPOINTREACHED) {
        scn.nmhdr.code = SCN_SAVEPOINTREACHED;
    } else if (t == wxEVT_STC_SAVEPOINTLEFT) {
        scn.nmhdr.code = SCN_SAVEPOINTLEFT;
    } else if (t == wxEVT_STC_ROMODIFYATTEMPT) {
        scn.nmhdr.code = SCN_MODIFYATTEMPTRO;
    } else if (t == wxEVT_STC_DOUBLECLICK) {
        scn.nmhdr.code = SCN_DOUBLECLICK;
        scn.position = B2A(*d, e.GetPosition());
        scn.line = e.GetLine();
    } else if (t == wxEVT_STC_UPDATEUI) {
        scn.nmhdr.code = SCN_UPDATEUI;
    } else if (t == wxEVT_STC_MACRORECORD) {
        scn.nmhdr.code = SCN_MACRORECORD;
        scn.message = e.GetMessage();
        scn.wParam = static_cast<uptr_t>(e.GetWParam());
        scn.lParam = MacroLParam(e);
    } else if (t == wxEVT_STC_MARGINCLICK) {
        scn.nmhdr.code = SCN_MARGINCLICK;
        scn.position = B2A(*d, e.GetPosition());
        scn.margin = e.GetMargin();
    } else if (t == wxEVT_STC_NEEDSHOWN) {
        scn.nmhdr.code = SCN_NEEDSHOWN;
        scn.position = B2A(*d, e.GetPosition());
        scn.length = B2A(*d, e.GetPosition() + e.GetLength()) - scn.position;
    } else if (t == wxEVT_STC_PAINTED) {
        scn.nmhdr.code = SCN_PAINTED;
    } else if (t == wxEVT_STC_USERLISTSELECTION || t == wxEVT_STC_AUTOCOMP_SELECTION) {
        scn.nmhdr.code = t == wxEVT_STC_USERLISTSELECTION ? SCN_USERLISTSELECTION : SCN_AUTOCSELECTION;
        scn.listType = e.GetListType();
        text = AnsiFromWx(e.GetString());
        scn.text = text.c_str();
        scn.lParam = B2A(*d, e.GetPosition());
    } else if (t == wxEVT_STC_DWELLSTART || t == wxEVT_STC_DWELLEND) {
        scn.nmhdr.code = t == wxEVT_STC_DWELLSTART ? SCN_DWELLSTART : SCN_DWELLEND;
        scn.position = B2A(*d, e.GetPosition());
        scn.x = e.GetX();
        scn.y = e.GetY();
    } else if (t == wxEVT_STC_ZOOM) {
        scn.nmhdr.code = SCN_ZOOM;
    } else if (t == wxEVT_STC_HOTSPOT_CLICK || t == wxEVT_STC_HOTSPOT_DCLICK) {
        scn.nmhdr.code = t == wxEVT_STC_HOTSPOT_CLICK ? SCN_HOTSPOTCLICK : SCN_HOTSPOTDOUBLECLICK;
        scn.position = B2A(*d, e.GetPosition());
    } else if (t == wxEVT_STC_CALLTIP_CLICK) {
        scn.nmhdr.code = SCN_CALLTIPCLICK;
        scn.position = e.GetPosition();
    } else if (t == wxEVT_STC_INDICATOR_CLICK || t == wxEVT_STC_INDICATOR_RELEASE) {
        scn.nmhdr.code = t == wxEVT_STC_INDICATOR_CLICK ? SCN_INDICATORCLICK : SCN_INDICATORRELEASE;
        scn.position = B2A(*d, e.GetPosition());
    } else if (t == wxEVT_STC_AUTOCOMP_CANCELLED) {
        scn.nmhdr.code = SCN_AUTOCCANCELLED;
    } else {
        return;
    }
    Notify(w, scn);
}

void BindEvents(wxWindow* w, wxStyledTextCtrl* s) {
    s->Bind(wxEVT_STC_MODIFIED, [w](wxStyledTextEvent& e) { OnModified(w, e); });
    s->Bind(wxEVT_STC_STYLENEEDED, [w](wxStyledTextEvent& e) { OnStyleNeeded(w, e); });
    const wxEventTypeTag<wxStyledTextEvent> forwarded[] = {
        wxEVT_STC_CHARADDED,      wxEVT_STC_SAVEPOINTREACHED, wxEVT_STC_SAVEPOINTLEFT,  wxEVT_STC_ROMODIFYATTEMPT,
        wxEVT_STC_DOUBLECLICK,    wxEVT_STC_UPDATEUI,         wxEVT_STC_MACRORECORD,    wxEVT_STC_MARGINCLICK,
        wxEVT_STC_NEEDSHOWN,      wxEVT_STC_PAINTED,          wxEVT_STC_USERLISTSELECTION, wxEVT_STC_DWELLSTART,
        wxEVT_STC_DWELLEND,       wxEVT_STC_ZOOM,             wxEVT_STC_HOTSPOT_CLICK,  wxEVT_STC_HOTSPOT_DCLICK,
        wxEVT_STC_CALLTIP_CLICK,  wxEVT_STC_AUTOCOMP_SELECTION, wxEVT_STC_INDICATOR_CLICK,
        wxEVT_STC_INDICATOR_RELEASE, wxEVT_STC_AUTOCOMP_CANCELLED,
    };
    for (const auto& t : forwarded)
        s->Bind(t, [w](wxStyledTextEvent& e) { ForwardEvent(w, e); });
    s->Bind(wxEVT_SET_FOCUS, [w](wxFocusEvent& e) {
        e.Skip();
        if (FindData(w) && !w->IsBeingDeleted())
            NotifyParent(w, SCEN_SETFOCUS);
    });
    s->Bind(wxEVT_KILL_FOCUS, [w](wxFocusEvent& e) {
        e.Skip();
        if (FindData(w) && !w->IsBeingDeleted())
            NotifyParent(w, SCEN_KILLFOCUS);
    });
    s->Bind(wxEVT_DESTROY, [w](wxWindowDestroyEvent& e) {
        e.Skip();
        if (e.GetWindow() == w)
            Registry().erase(w);
    });
}

SciData* DataFor(wxWindow* w) {
    if (SciData* d = FindData(w))
        return d;
    auto* s = wxDynamicCast(w, wxStyledTextCtrl);
    if (!s)
        return nullptr;
    std::unique_ptr<SciData> owned(new SciData());
    SciData* d = owned.get();
    d->stc = s;
    Registry()[w] = std::move(owned);
    s->SendMsg(SCI_SETCODEPAGE, SC_CP_UTF8);
    s->SendMsg(SCI_SETMODEVENTMASK, ControlModMask(d->appModMask));
    s->SendMsg(SCI_SETEOLMODE, SC_EOL_CRLF);
    d->map.Reset();
    BindEvents(w, s);
    return d;
}

struct AppRangeToFormat {
    void* hdc;
    void* hdcTarget;
    struct {
        int left, top, right, bottom;
    } rc, rcPage;
    CharacterRange chrg;
};

LRESULT GetTextRange(SciData& d, long cpMin, long cpMax, char* out) {
    if (!out)
        return 0;
    int total = d.map.AnsiLength(d.stc);
    long end = cpMax == -1 ? total : cpMax;
    long len = end - cpMin;
    if (len < 0)
        return 0;
    std::string a = AnsiRange(d, static_cast<int>(std::max(cpMin, 0L)), static_cast<int>(std::min<long>(end, total)));
    size_t lead = cpMin < 0 ? static_cast<size_t>(std::min(-cpMin, len)) : 0;
    memset(out, 0, static_cast<size_t>(len) + 1);
    memcpy(out + lead, a.data(), std::min(a.size(), static_cast<size_t>(len) - lead));
    return static_cast<LRESULT>(len);
}

LRESULT GetStyledText(SciData& d, TextRange* tr) {
    if (!tr || !tr->lpstrText)
        return 0;
    int total = d.map.AnsiLength(d.stc);
    int a0 = static_cast<int>(std::max(0L, std::min<long>(tr->chrg.cpMin, total)));
    int a1 = static_cast<int>(std::max<long>(a0, std::min<long>(tr->chrg.cpMax, total)));
    int b0 = A2B(d, a0);
    int b1 = A2B(d, a1);
    std::vector<char> cells(static_cast<size_t>(b1 - b0) * 2 + 2, '\0');
    TextRange utf;
    utf.chrg.cpMin = b0;
    utf.chrg.cpMax = b1;
    utf.lpstrText = cells.data();
    Pass(d, SCI_GETSTYLEDTEXT, 0, reinterpret_cast<LPARAM>(&utf));
    std::vector<unsigned char> chars(static_cast<size_t>(b1 - b0));
    for (size_t i = 0; i < chars.size(); ++i)
        chars[i] = static_cast<unsigned char>(cells[i * 2]);
    const Codec& c = CurrentCodec();
    char* out = tr->lpstrText;
    int place = 0;
    for (size_t i = 0; i < chars.size();) {
        int w = chars[i] < 0x80 ? 1 : Utf8Width(&chars[i], chars.size() - i);
        char a;
        AnsiAt(c, &chars[i], w, a);
        out[place++] = a;
        out[place++] = cells[i * 2 + 1];
        i += static_cast<size_t>(w);
    }
    out[place] = '\0';
    out[place + 1] = '\0';
    return place;
}

LRESULT AddStyledText(SciData& d, WPARAM length, LPARAM cellsPtr) {
    if (!cellsPtr)
        return 0;
    const char* cells = reinterpret_cast<const char*>(cellsPtr);
    const Codec& c = CurrentCodec();
    std::string out;
    size_t n = static_cast<size_t>(length) / 2;
    out.reserve(n * 2 + n);
    for (size_t i = 0; i < n; ++i) {
        unsigned char ch = static_cast<unsigned char>(cells[i * 2]);
        char style = cells[i * 2 + 1];
        if (ch < 0x80) {
            out.push_back(static_cast<char>(ch));
            out.push_back(style);
            continue;
        }
        for (int k = 0; k < c.encLen[ch]; ++k) {
            out.push_back(c.enc[ch][k]);
            out.push_back(style);
        }
    }
    return Pass(d, SCI_ADDSTYLEDTEXT, out.size(), reinterpret_cast<LPARAM>(out.data()));
}

template <class Find>
LRESULT FindTextIn(SciData& d, WPARAM flags, Find* ft) {
    if (!ft || !ft->lpstrText)
        return -1;
    std::string text = Utf8FromAnsi(ft->lpstrText);
    TextToFind utf;
    utf.chrg.cpMin = A2B(d, static_cast<int>(ft->chrg.cpMin));
    utf.chrg.cpMax = A2B(d, static_cast<int>(ft->chrg.cpMax));
    utf.lpstrText = const_cast<char*>(text.c_str());
    utf.chrgText.cpMin = utf.chrgText.cpMax = -1;
    LRESULT pos = Pass(d, SCI_FINDTEXT, flags, reinterpret_cast<LPARAM>(&utf));
    if (pos < 0)
        return pos;
    ft->chrgText.cpMin = static_cast<decltype(ft->chrgText.cpMin)>(B2A(d, static_cast<int>(utf.chrgText.cpMin)));
    ft->chrgText.cpMax = static_cast<decltype(ft->chrgText.cpMax)>(B2A(d, static_cast<int>(utf.chrgText.cpMax)));
    return B2A(d, pos);
}

wxDC* DCOf(void* hdc) {
    if (!hdc)
        return nullptr;
    CDC* dc = CDC::FromHandle(reinterpret_cast<HDC>(hdc));
    return dc ? dc->GetWx() : nullptr;
}

LRESULT FormatRange(SciData& d, bool draw, AppRangeToFormat* fr) {
    if (!fr)
        return 0;
    int total = d.map.AnsiLength(d.stc);
    int cpMin = static_cast<int>(std::max(0L, std::min<long>(fr->chrg.cpMin, total)));
    int cpMax = fr->chrg.cpMax < 0 || fr->chrg.cpMax > total ? total : static_cast<int>(fr->chrg.cpMax);
    wxDC* surface = DCOf(fr->hdc);
    if (!surface)
        return cpMax;
    wxDC* target = DCOf(fr->hdcTarget);
    wxRect render(wxPoint(fr->rc.left, fr->rc.top), wxPoint(fr->rc.right, fr->rc.bottom));
    wxRect page(wxPoint(fr->rcPage.left, fr->rcPage.top), wxPoint(fr->rcPage.right, fr->rcPage.bottom));
    int end = d.stc->FormatRange(draw, A2B(d, cpMin), A2B(d, cpMax), surface, target ? target : surface, render, page);
    return B2A(d, end);
}

LRESULT GetCharacterPointer(SciData& d) {
    if (!d.mirrorValid) {
        d.mirror = AnsiOfBytes(d, 0, d.stc->GetLength());
        d.mirrorValid = true;
    }
    return reinterpret_cast<LRESULT>(d.mirror.c_str());
}

sptr_t DirectFunction(sptr_t ptr, unsigned int msg, uptr_t wp, sptr_t lp);

LRESULT Proc(wxWindow* w, SciData& d, UINT msg, WPARAM wp, LPARAM lp, bool& handled);

// Win32 edit messages that Scintilla 1.77 (ScintillaWin) maps onto its own messages.
UINT SciFromEM(UINT msg) {
    switch (msg) {
    case EM_CANPASTE: return SCI_CANPASTE;
    case EM_CANUNDO: return SCI_CANUNDO;
    case EM_EMPTYUNDOBUFFER: return SCI_EMPTYUNDOBUFFER;
    case EM_GETFIRSTVISIBLELINE: return SCI_GETFIRSTVISIBLELINE;
    case EM_GETLINECOUNT: return SCI_GETLINECOUNT;
    case EM_GETSELTEXT: return SCI_GETSELTEXT;
    case EM_HIDESELECTION: return SCI_HIDESELECTION;
    case EM_LINEINDEX: return SCI_POSITIONFROMLINE;
    case EM_LINESCROLL: return SCI_LINESCROLL;
    case EM_REPLACESEL: return SCI_REPLACESEL;
    case EM_SCROLLCARET: return SCI_SCROLLCARET;
    case EM_SETREADONLY: return SCI_SETREADONLY;
    case WM_CLEAR: return SCI_CLEAR;
    case WM_COPY: return SCI_COPY;
    case WM_CUT: return SCI_CUT;
    case WM_SETTEXT: return SCI_SETTEXT;
    case WM_GETTEXTLENGTH: return SCI_GETTEXTLENGTH;
    case WM_PASTE: return SCI_PASTE;
    case WM_UNDO: return SCI_UNDO;
    default: return msg;
    }
}

void SetSelectionAnsi(SciData& d, int nStart, int nEnd) {
    if (nStart > nEnd)
        std::swap(nStart, nEnd);
    Pass(d, SCI_SETSEL, PosW(A2B(d, nEnd)), PosL(A2B(d, nStart)));
    Pass(d, SCI_SCROLLCARET, 0, 0);
}

LRESULT EditProc(wxWindow* w, SciData& d, UINT msg, WPARAM wp, LPARAM lp, bool& handled) {
    UINT mapped = SciFromEM(msg);
    if (mapped != msg)
        return Proc(w, d, mapped, wp, lp, handled);
    switch (msg) {
    case WM_GETTEXT: {
        if (!wp || !lp)
            return 0;
        std::string a = AnsiRange(d, 0, d.map.AnsiLength(d.stc));
        size_t n = std::min(a.size(), static_cast<size_t>(wp) - 1);
        char* out = reinterpret_cast<char*>(lp);
        memcpy(out, a.data(), n);
        out[n] = '\0';
        return static_cast<LRESULT>(n);
    }
    case EM_LINEFROMCHAR: {
        int pos = PosArg(wp);
        int b = pos < 0 ? static_cast<int>(Pass(d, SCI_GETSELECTIONSTART, 0, 0)) : A2B(d, pos);
        return Pass(d, SCI_LINEFROMPOSITION, PosW(b), 0);
    }
    case EM_EXLINEFROMCHAR:
        return Pass(d, SCI_LINEFROMPOSITION, PosW(A2B(d, PosArg(lp))), 0);
    case EM_GETSEL: {
        int a = B2A(d, static_cast<int>(Pass(d, SCI_GETSELECTIONSTART, 0, 0)));
        int b = B2A(d, static_cast<int>(Pass(d, SCI_GETSELECTIONEND, 0, 0)));
        if (wp)
            *reinterpret_cast<int*>(wp) = a;
        if (lp)
            *reinterpret_cast<int*>(lp) = b;
        return MAKELONG(a, b);
    }
    case EM_EXGETSEL: {
        if (!lp)
            return 0;
        auto* cr = reinterpret_cast<CHARRANGE*>(lp);
        cr->cpMin = B2A(d, static_cast<int>(Pass(d, SCI_GETSELECTIONSTART, 0, 0)));
        cr->cpMax = B2A(d, static_cast<int>(Pass(d, SCI_GETSELECTIONEND, 0, 0)));
        return 0;
    }
    case EM_SETSEL: {
        int nStart = PosArg(wp);
        int nEnd = PosArg(lp);
        if (nStart == 0 && nEnd == -1)
            nEnd = d.map.AnsiLength(d.stc);
        if (nStart == -1)
            nStart = nEnd;
        SetSelectionAnsi(d, nStart, nEnd);
        return 0;
    }
    case EM_EXSETSEL: {
        if (!lp)
            return 0;
        auto* cr = reinterpret_cast<CHARRANGE*>(lp);
        int nEnd = cr->cpMin == 0 && cr->cpMax == -1 ? d.map.AnsiLength(d.stc) : static_cast<int>(cr->cpMax);
        Pass(d, SCI_SETSEL, PosW(A2B(d, static_cast<int>(cr->cpMin))), PosL(A2B(d, nEnd)));
        Pass(d, SCI_SCROLLCARET, 0, 0);
        return Pass(d, SCI_LINEFROMPOSITION, static_cast<WPARAM>(Pass(d, SCI_GETSELECTIONSTART, 0, 0)), 0);
    }
    case EM_GETTEXTRANGE: {
        auto* tr = reinterpret_cast<TEXTRANGE*>(lp);
        return tr ? GetTextRange(d, tr->chrg.cpMin, tr->chrg.cpMax, tr->lpstrText) : 0;
    }
    case EM_FINDTEXTEX:
        return FindTextIn(d, wp, reinterpret_cast<FINDTEXTEX*>(lp));
    case EM_FORMATRANGE:
        return 0;
    default:
        handled = false;
        return 0;
    }
}

LRESULT Proc(wxWindow* w, SciData& d, UINT msg, WPARAM wp, LPARAM lp, bool& handled) {
    handled = true;
    if (msg < SCI_START)
        return EditProc(w, d, msg, wp, lp, handled);
    switch (msg) {
    case SCI_GETDIRECTFUNCTION:
        return reinterpret_cast<LRESULT>(&DirectFunction);
    case SCI_GETDIRECTPOINTER:
        return reinterpret_cast<LRESULT>(w);

    case SCI_SETCODEPAGE:
        d.appCodePage = static_cast<int>(wp);
        return 0;
    case SCI_GETCODEPAGE:
        return d.appCodePage;
    case SCI_SETMODEVENTMASK:
        d.appModMask = static_cast<int>(wp);
        return Pass(d, msg, static_cast<WPARAM>(ControlModMask(d.appModMask)), 0);
    case SCI_GETMODEVENTMASK:
        return d.appModMask;
    case SCI_SETWRAPMODE:
        return Pass(d, msg, wp == SC_WRAP_WORD ? static_cast<WPARAM>(kWrapWhitespace) : wp, lp);
    case SCI_GETWRAPMODE: {
        LRESULT r = Pass(d, msg, wp, lp);
        return r == kWrapWhitespace ? SC_WRAP_WORD : r;
    }
    case SCI_SETDOCPOINTER: {
        LRESULT r = Pass(d, msg, wp, lp);
        Pass(d, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
        d.map.Reset();
        d.mirrorValid = false;
        return r;
    }

    case SCI_GETLENGTH:
    case SCI_GETTEXTLENGTH:
        return d.map.AnsiLength(d.stc);
    case SCI_LINELENGTH: {
        LRESULT start = Pass(d, SCI_POSITIONFROMLINE, wp, 0);
        LRESULT len = Pass(d, msg, wp, lp);
        if (start < 0)
            return len;
        return B2A(d, static_cast<int>(start + len)) - B2A(d, static_cast<int>(start));
    }
    case SCI_GETCHARAT: {
        int b = A2B(d, PosArg(wp));
        int n = d.stc->GetLength();
        if (b < 0 || b >= n)
            return 0;
        const unsigned char* p = reinterpret_cast<const unsigned char*>(d.stc->GetRangePointer(b, std::min(4, n - b)));
        char a;
        AnsiAt(CurrentCodec(), p, p[0] < 0x80 ? 1 : Utf8Width(p, static_cast<size_t>(std::min(4, n - b))), a);
        return static_cast<signed char>(a);
    }

    case SCI_GOTOPOS:
    case SCI_SETANCHOR:
    case SCI_SETCURRENTPOS:
    case SCI_SETSELECTIONSTART:
    case SCI_SETSELECTIONEND:
    case SCI_SETTARGETSTART:
    case SCI_SETTARGETEND:
    case SCI_LINEFROMPOSITION:
    case SCI_GETCOLUMN:
    case SCI_GETSTYLEAT:
    case SCI_BRACEBADLIGHT:
    case SCI_INDICATORALLONFOR:
        return Pass(d, msg, PosW(A2B(d, PosArg(wp))), lp);
    case SCI_BRACEMATCH:
    case SCI_POSITIONBEFORE:
    case SCI_POSITIONAFTER:
    case SCI_WORDSTARTPOSITION:
    case SCI_WORDENDPOSITION:
        return B2A(d, Pass(d, msg, PosW(A2B(d, PosArg(wp))), lp));
    case SCI_SETSEL:
    case SCI_BRACEHIGHLIGHT:
    case SCI_COLOURISE:
        return Pass(d, msg, PosW(A2B(d, PosArg(wp))), PosL(A2B(d, PosArg(lp))));
    case SCI_COPYRANGE: {
        int a = PosArg(wp);
        int b = PosArg(lp);
        if (a > b)
            std::swap(a, b);
        RememberCopied(AnsiRange(d, a, b));
        return Pass(d, msg, PosW(A2B(d, PosArg(wp))), PosL(A2B(d, PosArg(lp))));
    }
    case SCI_POINTXFROMPOSITION:
    case SCI_POINTYFROMPOSITION:
    case SCI_INDICATORVALUEAT:
        return Pass(d, msg, wp, PosL(A2B(d, PosArg(lp))));
    case SCI_INDICATORSTART:
    case SCI_INDICATOREND:
        return B2A(d, Pass(d, msg, wp, PosL(A2B(d, PosArg(lp)))));
    case SCI_INDICATORFILLRANGE:
    case SCI_INDICATORCLEARRANGE: {
        int a = PosArg(wp);
        int b0 = A2B(d, a);
        int b1 = A2B(d, a + PosArg(lp));
        return Pass(d, msg, PosW(b0), PosL(b1 - b0));
    }

    case SCI_GETCURRENTPOS:
    case SCI_GETANCHOR:
    case SCI_GETSELECTIONSTART:
    case SCI_GETSELECTIONEND:
    case SCI_GETTARGETSTART:
    case SCI_GETTARGETEND:
    case SCI_GETENDSTYLED:
    case SCI_POSITIONFROMLINE:
    case SCI_GETLINEENDPOSITION:
    case SCI_GETLINEINDENTPOSITION:
    case SCI_POSITIONFROMPOINT:
    case SCI_POSITIONFROMPOINTCLOSE:
    case SCI_CALLTIPPOSSTART:
    case SCI_AUTOCPOSSTART:
    case SCI_GETLINESELSTARTPOSITION:
    case SCI_GETLINESELENDPOSITION:
    case SCI_FINDCOLUMN:
        return B2A(d, Pass(d, msg, wp, lp));

    case SCI_SETTEXT: {
        if (!lp)
            return 0;
        std::string u = Utf8FromAnsi(reinterpret_cast<const char*>(lp));
        return Pass(d, msg, 0, Ptr(u));
    }
    case SCI_ADDTEXT:
    case SCI_APPENDTEXT:
    case SCI_COPYTEXT: {
        if (!lp || static_cast<intptr_t>(wp) < 0)
            return 0;
        if (msg == SCI_COPYTEXT)
            RememberCopied(std::string(reinterpret_cast<const char*>(lp), static_cast<size_t>(wp)));
        std::string u = Utf8FromAnsi(reinterpret_cast<const char*>(lp), static_cast<size_t>(wp));
        return Pass(d, msg, u.size(), Ptr(u));
    }
    case SCI_INSERTTEXT: {
        if (!lp)
            return 0;
        std::string u = Utf8FromAnsi(reinterpret_cast<const char*>(lp));
        return Pass(d, msg, PosW(A2B(d, PosArg(wp))), Ptr(u));
    }
    case SCI_REPLACESEL: {
        if (!lp)
            return 0;
        std::string u = Utf8FromAnsi(reinterpret_cast<const char*>(lp));
        return Pass(d, msg, 0, Ptr(u));
    }
    case SCI_REPLACETARGET:
    case SCI_REPLACETARGETRE: {
        if (!lp)
            return 0;
        const char* t = reinterpret_cast<const char*>(lp);
        std::string u = static_cast<intptr_t>(wp) < 0 ? Utf8FromAnsi(t) : Utf8FromAnsi(t, static_cast<size_t>(wp));
        Pass(d, msg, u.size(), Ptr(u));
        int start = static_cast<int>(Pass(d, SCI_GETTARGETSTART, 0, 0));
        int end = static_cast<int>(Pass(d, SCI_GETTARGETEND, 0, 0));
        return B2A(d, end) - B2A(d, start);
    }
    case SCI_SEARCHINTARGET: {
        if (!lp)
            return -1;
        const char* t = reinterpret_cast<const char*>(lp);
        std::string u = static_cast<intptr_t>(wp) < 0 ? Utf8FromAnsi(t) : Utf8FromAnsi(t, static_cast<size_t>(wp));
        return B2A(d, Pass(d, msg, u.size(), Ptr(u)));
    }
    case SCI_SEARCHNEXT:
    case SCI_SEARCHPREV: {
        if (!lp)
            return -1;
        std::string u = Utf8FromAnsi(reinterpret_cast<const char*>(lp));
        return B2A(d, Pass(d, msg, wp, Ptr(u)));
    }
    case SCI_TEXTWIDTH:
    case SCI_STYLESETFONT:
    case SCI_SETKEYWORDS:
    case SCI_AUTOCSTOPS:
    case SCI_AUTOCSELECT:
    case SCI_AUTOCSETFILLUPS:
    case SCI_USERLISTSHOW: {
        if (!lp)
            return Pass(d, msg, wp, lp);
        std::string u = Utf8FromAnsi(reinterpret_cast<const char*>(lp));
        return Pass(d, msg, wp, Ptr(u));
    }
    case SCI_CALLTIPSHOW: {
        std::string u = Utf8FromAnsi(reinterpret_cast<const char*>(lp));
        return Pass(d, msg, PosW(A2B(d, PosArg(wp))), Ptr(u));
    }
    case SCI_AUTOCSHOW: {
        int caret = static_cast<int>(Pass(d, SCI_GETCURRENTPOS, 0, 0));
        int entered = caret - A2B(d, B2A(d, caret) - PosArg(wp));
        std::string u = Utf8FromAnsi(reinterpret_cast<const char*>(lp));
        return Pass(d, msg, PosW(entered), Ptr(u));
    }
    case SCI_SETWORDCHARS:
    case SCI_SETWHITESPACECHARS: {
        if (!lp)
            return Pass(d, msg, wp, lp);
        std::string ascii;
        for (const char* p = reinterpret_cast<const char*>(lp); *p; ++p)
            if (static_cast<unsigned char>(*p) < 0x80)
                ascii.push_back(*p);
        return Pass(d, msg, wp, Ptr(ascii));
    }
    case SCI_SETLENGTHFORENCODE:
        d.lengthForEncode = PosArg(wp);
        return Pass(d, msg, wp, lp);
    case SCI_ENCODEDFROMUTF8: {
        const char* src = reinterpret_cast<const char*>(wp);
        if (!src)
            return 0;
        size_t n = d.lengthForEncode < 0 ? strlen(src) : static_cast<size_t>(d.lengthForEncode);
        return PutString(AnsiFromUtf8(src, n), lp);
    }

    case SCI_GETTEXT: {
        int total = d.map.AnsiLength(d.stc);
        if (!lp)
            return total + 1;
        if (!wp)
            return 0;
        size_t want = static_cast<size_t>(wp) - 1;
        std::string a = AnsiRange(d, 0, static_cast<int>(std::min(want, static_cast<size_t>(total))));
        char* out = reinterpret_cast<char*>(lp);
        memcpy(out, a.data(), a.size());
        memset(out + a.size(), 0, want - a.size() + 1);
        return static_cast<LRESULT>(want);
    }
    case SCI_GETSELTEXT: {
        std::string a = FetchString(d, msg, 0, true);
        if (!lp)
            return static_cast<LRESULT>(a.size()) + 1;
        char* out = reinterpret_cast<char*>(lp);
        memcpy(out, a.data(), a.size());
        out[a.size()] = '\0';
        return a.empty() ? 0 : static_cast<LRESULT>(a.size()) + 1;
    }
    case SCI_GETLINE: {
        std::string a = FetchString(d, msg, wp, false);
        if (lp)
            memcpy(reinterpret_cast<char*>(lp), a.data(), a.size());
        return static_cast<LRESULT>(a.size());
    }
    case SCI_GETCURLINE: {
        int caret = static_cast<int>(Pass(d, SCI_GETCURRENTPOS, 0, 0));
        LRESULT line = Pass(d, SCI_LINEFROMPOSITION, PosW(caret), 0);
        int start = static_cast<int>(Pass(d, SCI_POSITIONFROMLINE, static_cast<WPARAM>(line), 0));
        int end = start + static_cast<int>(Pass(d, SCI_LINELENGTH, static_cast<WPARAM>(line), 0));
        std::string a = AnsiOfBytes(d, start, end);
        if (!lp)
            return static_cast<LRESULT>(a.size()) + 1;
        int caretInLine = B2A(d, caret) - B2A(d, start);
        if (!wp)
            return caretInLine;
        size_t n = std::min(a.size(), static_cast<size_t>(wp) - 1);
        char* out = reinterpret_cast<char*>(lp);
        memcpy(out, a.data(), n);
        out[n] = '\0';
        return caretInLine;
    }
    case SCI_STYLEGETFONT:
        return PutString(FetchString(d, msg, wp, false), lp);
    case SCI_GETTEXTRANGE: {
        auto* tr = reinterpret_cast<TextRange*>(lp);
        return tr ? GetTextRange(d, tr->chrg.cpMin, tr->chrg.cpMax, tr->lpstrText) : 0;
    }
    case SCI_GETSTYLEDTEXT:
        return GetStyledText(d, reinterpret_cast<TextRange*>(lp));
    case SCI_GETCHARACTERPOINTER:
        return GetCharacterPointer(d);
    case SCI_FINDTEXT:
        return FindTextIn(d, wp, reinterpret_cast<TextToFind*>(lp));
    case SCI_FORMATRANGE:
        return FormatRange(d, wp != 0, reinterpret_cast<AppRangeToFormat*>(lp));

    case SCI_STARTSTYLING:
        d.stylingAnsi = PosArg(wp);
        return Pass(d, msg, PosW(A2B(d, d.stylingAnsi)), lp);
    case SCI_SETSTYLING: {
        int len = PosArg(wp);
        int b0 = A2B(d, d.stylingAnsi);
        int b1 = A2B(d, d.stylingAnsi + len);
        d.stylingAnsi += len;
        return Pass(d, msg, PosW(b1 - b0), lp);
    }
    case SCI_SETSTYLINGEX: {
        int len = PosArg(wp);
        if (len <= 0 || !lp)
            return 0;
        const char* styles = reinterpret_cast<const char*>(lp);
        int b0 = A2B(d, d.stylingAnsi);
        int b1 = A2B(d, d.stylingAnsi + len);
        int n = d.stc->GetLength();
        int bytes = std::min(b1, n) - b0;
        std::vector<char> expanded;
        expanded.reserve(static_cast<size_t>(std::max(bytes, 0)));
        if (bytes > 0) {
            const unsigned char* p = reinterpret_cast<const unsigned char*>(d.stc->GetRangePointer(b0, bytes));
            for (int i = 0, ch = 0; i < bytes && ch < len; ++ch) {
                int width = p[i] < 0x80 ? 1 : Utf8Width(p + i, static_cast<size_t>(bytes - i));
                expanded.insert(expanded.end(), static_cast<size_t>(width), styles[ch]);
                i += width;
            }
        }
        d.stylingAnsi += len;
        return Pass(d, msg, expanded.size(), reinterpret_cast<LPARAM>(expanded.data()));
    }
    case SCI_ADDSTYLEDTEXT:
        return AddStyledText(d, wp, lp);

    case SCI_SETLEXER:
        d.cryptoolLexer = static_cast<int>(wp) == kCrypToolLexer;
        return Pass(d, msg, d.cryptoolLexer ? static_cast<WPARAM>(SCLEX_CONTAINER) : wp, lp);
    case SCI_GETLEXER:
        return d.cryptoolLexer ? kCrypToolLexer : Pass(d, msg, wp, lp);
    case SCI_SETLEXERLANGUAGE: {
        const char* name = reinterpret_cast<const char*>(lp);
        if (name && strcasecmp(name, "cryptool") == 0) {
            d.cryptoolLexer = true;
            return Pass(d, SCI_SETLEXER, SCLEX_CONTAINER, 0);
        }
        d.cryptoolLexer = false;
        std::string u = Utf8FromAnsi(name);
        return Pass(d, msg, wp, Ptr(u));
    }
    case SCI_LOADLEXERLIBRARY:
        return 0;
    case SCI_SETPROPERTY: {
        const char* key = reinterpret_cast<const char*>(wp);
        const char* value = reinterpret_cast<const char*>(lp);
        if (!key)
            return 0;
        d.props[key] = value ? value : "";
        if (!strcmp(key, "cryptool.alphabet") || !strcmp(key, "cryptool.nonalphabetstyle"))
            RebuildAlphabet(d);
        std::string k = Utf8FromAnsi(key);
        std::string v = Utf8FromAnsi(value);
        return Pass(d, msg, reinterpret_cast<WPARAM>(k.c_str()), Ptr(v));
    }
    case SCI_GETPROPERTY:
    case SCI_GETPROPERTYEXPANDED: {
        const char* key = reinterpret_cast<const char*>(wp);
        if (!key)
            return 0;
        auto it = d.props.find(key);
        if (msg == SCI_GETPROPERTY && it != d.props.end())
            return PutString(it->second, lp);
        std::string k = Utf8FromAnsi(key);
        return PutString(FetchString(d, msg, reinterpret_cast<WPARAM>(k.c_str()), false), lp);
    }
    case SCI_GETPROPERTYINT: {
        const char* key = reinterpret_cast<const char*>(wp);
        if (!key)
            return lp;
        std::string k = Utf8FromAnsi(key);
        return Pass(d, msg, reinterpret_cast<WPARAM>(k.c_str()), lp);
    }

    case SCI_COPY:
    case SCI_CUT: {
        int b0 = static_cast<int>(Pass(d, SCI_GETSELECTIONSTART, 0, 0));
        int b1 = static_cast<int>(Pass(d, SCI_GETSELECTIONEND, 0, 0));
        size_t ansiLen = static_cast<size_t>(std::max(B2A(d, b1) - B2A(d, b0), 0));
        // SCI_GETSELTEXT replaces NUL bytes, so a stream selection is read from the document
        bool rect = Pass(d, SCI_SELECTIONISRECTANGLE, 0, 0) != 0;
        RememberCopied(rect ? FetchString(d, SCI_GETSELTEXT, 0, true) : AnsiOfBytes(d, b0, b1), ansiLen);
        return Pass(d, msg, wp, lp);
    }

    case SCI_PASTE: {
        LRESULT r = Pass(d, msg, wp, lp);
        NotifyParent(w, SCEN_CHANGE);
        return r;
    }

    default:
        return Pass(d, msg, wp, lp);
    }
}

sptr_t DirectFunction(sptr_t ptr, unsigned int msg, uptr_t wp, sptr_t lp) {
    wxWindow* w = reinterpret_cast<wxWindow*>(ptr);
    return OnMain([&]() -> sptr_t {
        bool handled = false;
        return static_cast<sptr_t>(ScintillaWindowProc(w, msg, static_cast<WPARAM>(wp), static_cast<LPARAM>(lp), handled));
    });
}

} // namespace

void BindScintillaEvents(wxWindow* window) { DataFor(window); }

LRESULT ScintillaWindowProc(wxWindow* window, UINT msg, WPARAM wParam, LPARAM lParam, bool& handled) {
    SciData* d = DataFor(window);
    if (!d) {
        handled = false;
        return 0;
    }
    return Proc(window, *d, msg, wParam, lParam, handled);
}

} // namespace mfcwx
