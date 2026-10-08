#include "controls_internal.h"

#include "afxcmn.h"
#include "afxext.h"

#include <wx/checklst.h>
#include <wx/renderer.h>
#include <wx/tooltip.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>

namespace mfcwx {

namespace {

struct RtfFormat {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    int color = 0;
    int font = -1;
    int halfPoints = 0;
    bool skip = false;
    bool colorTable = false;
    bool fontTable = false;
};

class RtfWriter {
public:
    RtfWriter(wxTextCtrl* t, bool multiline) : m_t(t), m_multiline(multiline), m_default(t->GetDefaultStyle()) {}
    ~RtfWriter() { m_t->SetDefaultStyle(m_default); }

    void Parse(const std::string& rtf) {
        std::vector<RtfFormat> stack;
        RtfFormat cur;
        size_t i = 0;
        while (i < rtf.size()) {
            char c = rtf[i];
            if (c == '{') {
                stack.push_back(cur);
                ++i;
            } else if (c == '}') {
                Flush();
                if (cur.fontTable && m_fontId >= 0)
                    EndFont();
                if (stack.empty())
                    break;
                cur = stack.back();
                stack.pop_back();
                ++i;
            } else if (c == '\\') {
                i = Control(rtf, i + 1, cur);
            } else if (c == '\r' || c == '\n') {
                ++i;
            } else {
                Text(cur, c);
                ++i;
            }
        }
        Flush();
    }

private:
    size_t Control(const std::string& rtf, size_t i, RtfFormat& cur) {
        if (i >= rtf.size())
            return i;
        char c = rtf[i];
        if (!isalpha(static_cast<unsigned char>(c))) {
            switch (c) {
            case '\'': {
                if (i + 2 < rtf.size()) {
                    std::string hex = rtf.substr(i + 1, 2);
                    Text(cur, static_cast<char>(strtol(hex.c_str(), nullptr, 16)));
                }
                return i + 3;
            }
            case '*':
                cur.skip = true;
                return i + 1;
            case '~':
                Text(cur, static_cast<char>(0xA0));
                return i + 1;
            case '_':
                Text(cur, '-');
                return i + 1;
            case '\r':
            case '\n':
                Text(cur, '\n');
                return i + 1;
            default:
                Text(cur, c);
                return i + 1;
            }
        }
        size_t start = i;
        while (i < rtf.size() && isalpha(static_cast<unsigned char>(rtf[i])))
            ++i;
        std::string word = rtf.substr(start, i - start);
        bool hasParam = false;
        long param = 0;
        size_t numStart = i;
        if (i < rtf.size() && (rtf[i] == '-' || isdigit(static_cast<unsigned char>(rtf[i])))) {
            ++i;
            while (i < rtf.size() && isdigit(static_cast<unsigned char>(rtf[i])))
                ++i;
            param = strtol(rtf.substr(numStart, i - numStart).c_str(), nullptr, 10);
            hasParam = true;
        }
        if (i < rtf.size() && rtf[i] == ' ')
            ++i;
        Word(word, hasParam, param, cur);
        return i;
    }

    void Word(const std::string& w, bool hasParam, long param, RtfFormat& cur) {
        bool on = !hasParam || param != 0;
        if (cur.colorTable) {
            int* component = w == "red" ? &m_r : w == "green" ? &m_g : w == "blue" ? &m_b : nullptr;
            if (component) {
                *component = static_cast<int>(param);
                m_hasColor = true;
            }
            return;
        }
        if (cur.fontTable) {
            if (w == "f") {
                m_fontId = static_cast<int>(param);
                m_fontName.clear();
            }
            return;
        }
        if (w == "par" || w == "line") {
            Text(cur, '\n');
        } else if (w == "tab") {
            Text(cur, '\t');
        } else if (w == "colortbl") {
            Flush();
            cur.colorTable = true;
        } else if (w == "fonttbl") {
            Flush();
            cur.fontTable = true;
        } else if (w == "stylesheet" || w == "info" || w == "pict" || w == "header" || w == "footer" ||
                   w == "object" || w == "listtable" || w == "generator") {
            Flush();
            cur.skip = true;
        } else if (w == "b") {
            Flush();
            cur.bold = on;
        } else if (w == "i") {
            Flush();
            cur.italic = on;
        } else if (w == "ul") {
            Flush();
            cur.underline = on;
        } else if (w == "ulnone") {
            Flush();
            cur.underline = false;
        } else if (w == "cf") {
            Flush();
            cur.color = static_cast<int>(param);
        } else if (w == "f") {
            Flush();
            cur.font = static_cast<int>(param);
        } else if (w == "fs") {
            Flush();
            cur.halfPoints = static_cast<int>(param);
        } else if (w == "plain") {
            Flush();
            cur.bold = cur.italic = cur.underline = false;
            cur.color = 0;
            cur.halfPoints = 0;
        }
    }

    void Text(RtfFormat& cur, char c) {
        if (cur.skip)
            return;
        if (cur.colorTable) {
            if (c == ';') {
                m_colors.push_back(m_hasColor ? wxColour(m_r, m_g, m_b) : wxColour());
                m_r = m_g = m_b = 0;
                m_hasColor = false;
            }
            return;
        }
        if (cur.fontTable) {
            if (c == ';')
                EndFont();
            else if (m_fontId >= 0)
                m_fontName += c;
            return;
        }
        if (c == '\n' && !m_multiline)
            c = ' ';
        if (!m_run.empty() && !SameFormat(cur))
            Flush();
        if (m_run.empty())
            m_runFormat = cur;
        m_run += c;
    }

    void EndFont() {
        if (m_fontId >= 0)
            m_fonts[m_fontId] = ToWx(m_fontName.c_str());
        m_fontId = -1;
        m_fontName.clear();
    }

    bool SameFormat(const RtfFormat& f) const {
        return f.bold == m_runFormat.bold && f.italic == m_runFormat.italic && f.underline == m_runFormat.underline &&
               f.color == m_runFormat.color && f.font == m_runFormat.font && f.halfPoints == m_runFormat.halfPoints;
    }

    void Flush() {
        if (m_run.empty())
            return;
        const RtfFormat& f = m_runFormat;
        wxTextAttr attr = m_default;
        if (f.color > 0 && f.color < static_cast<int>(m_colors.size()) && m_colors[f.color].IsOk())
            attr.SetTextColour(m_colors[f.color]);
        else
            attr.SetTextColour(m_default.HasTextColour() ? m_default.GetTextColour() : m_t->GetForegroundColour());
        attr.SetFontWeight(f.bold ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL);
        attr.SetFontStyle(f.italic ? wxFONTSTYLE_ITALIC : wxFONTSTYLE_NORMAL);
        attr.SetFontUnderlined(f.underline);
        if (f.halfPoints > 0)
            attr.SetFontPointSize(std::max(1, f.halfPoints / 2));
        auto face = m_fonts.find(f.font);
        if (face != m_fonts.end() && !face->second.empty())
            attr.SetFontFaceName(face->second);
        m_t->SetDefaultStyle(attr);
        m_t->WriteText(ToWx(m_run.data(), static_cast<int>(m_run.size())));
        m_run.clear();
    }

    wxTextCtrl* m_t;
    bool m_multiline;
    wxTextAttr m_default;
    std::string m_run;
    RtfFormat m_runFormat;
    std::vector<wxColour> m_colors;
    std::map<int, wxString> m_fonts;
    int m_fontId = -1;
    std::string m_fontName;
    int m_r = 0;
    int m_g = 0;
    int m_b = 0;
    bool m_hasColor = false;
};

} // namespace

void InsertRtf(wxTextCtrl* t, bool multiline, const std::string& rtf) {
    if (!t)
        return;
    long from, to;
    t->GetSelection(&from, &to);
    if (from != to)
        t->Remove(from, to);
    t->SetInsertionPoint(from);
    RtfWriter writer(t, multiline);
    writer.Parse(rtf);
}

} // namespace mfcwx

using namespace mfcwx;

namespace {

LRESULT Send(const CWnd* w, UINT msg, WPARAM wParam = 0, LPARAM lParam = 0) {
    return ::SendMessage(w->m_hWnd, msg, wParam, lParam);
}

int CopyText(const wxString& s, char* buf, int cch) {
    if (!buf || cch <= 0)
        return 0;
    std::string a = FromWx(s);
    size_t n = std::min(a.size(), static_cast<size_t>(cch - 1));
    memcpy(buf, a.data(), n);
    buf[n] = 0;
    return static_cast<int>(n);
}

TabExtra* TabsOf(const CWnd* w) {
    WindowState* st = w->m_hWnd ? GetState(w->GetWx()) : nullptr;
    return st && st->kind == ControlKind::Tab ? &Extra<TabExtra>(*st) : nullptr;
}

struct ToolTipExtra {
    struct Tool {
        HWND hwnd;
        UINT_PTR id;
        wxString text;
    };
    std::vector<Tool> tools;
    bool active = true;
    int maxWidth = -1;
};

ToolTipExtra* TipsOf(const CWnd* w) {
    WindowState* st = w->m_hWnd ? GetState(w->GetWx()) : nullptr;
    return st && st->kind == ControlKind::Generic && !st->customPaint ? &Extra<ToolTipExtra>(*st) : nullptr;
}

void ApplyTool(const ToolTipExtra::Tool& tool, bool active) {
    wxWindow* w = ToWx(tool.hwnd);
    if (!IsManagedWindow(w))
        return;
    if (active && !tool.text.empty())
        w->SetToolTip(tool.text);
    else
        w->UnsetToolTip();
}

wxString ResourceText(UINT id) {
    std::string s;
    return LoadResourceString(id, s) ? ToWx(s.c_str()) : wxString();
}

wxString CallbackToolText(HWND hwndTool, HWND tooltip, UINT_PTR id) {
    wxWindow* parent = LogicalParent(ToWx(hwndTool));
    if (!parent)
        return wxString();
    NMTTDISPINFO di;
    memset(&di, 0, sizeof di);
    di.hdr.hwndFrom = tooltip;
    di.hdr.idFrom = id ? id : reinterpret_cast<UINT_PTR>(hwndTool);
    di.hdr.code = TTN_NEEDTEXT;
    di.uFlags = id ? 0 : TTF_IDISHWND;
    DispatchMessageTo(ToHwnd(parent), WM_NOTIFY, di.hdr.idFrom, reinterpret_cast<LPARAM>(&di));
    if (di.lpszText && IS_INTRESOURCE(di.lpszText))
        return ResourceText(static_cast<UINT>(reinterpret_cast<UINT_PTR>(di.lpszText)));
    if (di.lpszText && di.lpszText != LPSTR_TEXTCALLBACK)
        return ToWx(di.lpszText);
    return ToWx(di.szText);
}

} // namespace

IMPLEMENT_DYNAMIC(CStatic, CWnd)
IMPLEMENT_DYNAMIC(CButton, CWnd)
IMPLEMENT_DYNAMIC(CEdit, CWnd)
IMPLEMENT_DYNAMIC(CListBox, CWnd)
IMPLEMENT_DYNAMIC(CCheckListBox, CListBox)
IMPLEMENT_DYNAMIC(CComboBox, CWnd)
IMPLEMENT_DYNAMIC(CScrollBar, CWnd)
IMPLEMENT_DYNAMIC(CProgressCtrl, CWnd)
IMPLEMENT_DYNAMIC(CSliderCtrl, CWnd)
IMPLEMENT_DYNAMIC(CSpinButtonCtrl, CWnd)
IMPLEMENT_DYNAMIC(CTreeCtrl, CWnd)
IMPLEMENT_DYNAMIC(CTabCtrl, CWnd)
IMPLEMENT_DYNAMIC(CToolTipCtrl, CWnd)
IMPLEMENT_DYNAMIC(CRichEditCtrl, CWnd)
IMPLEMENT_DYNAMIC(CBitmapButton, CButton)

// ---------------------------------------------------------------------------------------------
// CStatic

BOOL CStatic::Create(const char* lpszText, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID) {
    return CWnd::Create("Static", lpszText, dwStyle, rect, pParentWnd, nID);
}

HICON CStatic::SetIcon(HICON hIcon) {
    return reinterpret_cast<HICON>(SendMessage(STM_SETICON, reinterpret_cast<WPARAM>(hIcon), 0));
}

HICON CStatic::GetIcon() const { return reinterpret_cast<HICON>(Send(this, STM_GETICON)); }

HBITMAP CStatic::SetBitmap(HBITMAP hBitmap) {
    return reinterpret_cast<HBITMAP>(SendMessage(STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(hBitmap)));
}

HBITMAP CStatic::GetBitmap() const { return reinterpret_cast<HBITMAP>(Send(this, STM_GETIMAGE, IMAGE_BITMAP)); }

HCURSOR CStatic::SetCursor(HCURSOR hCursor) {
    return reinterpret_cast<HCURSOR>(SendMessage(STM_SETIMAGE, IMAGE_CURSOR, reinterpret_cast<LPARAM>(hCursor)));
}

HCURSOR CStatic::GetCursor() { return reinterpret_cast<HCURSOR>(SendMessage(STM_GETIMAGE, IMAGE_CURSOR)); }

void CStatic::DrawItem(LPDRAWITEMSTRUCT) {}

BOOL CStatic::OnChildNotify(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult) {
    if (CWnd::OnChildNotify(message, wParam, lParam, pResult))
        return TRUE;
    if (message != WM_DRAWITEM)
        return FALSE;
    DrawItem(reinterpret_cast<LPDRAWITEMSTRUCT>(lParam));
    return TRUE;
}

// ---------------------------------------------------------------------------------------------
// CButton

BOOL CButton::Create(const char* lpszCaption, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID) {
    return CWnd::Create("Button", lpszCaption, dwStyle, rect, pParentWnd, nID);
}

UINT CButton::GetState() const { return static_cast<UINT>(Send(this, BM_GETSTATE)); }
void CButton::SetState(BOOL bHighlight) { SendMessage(BM_SETSTATE, static_cast<WPARAM>(bHighlight)); }
int CButton::GetCheck() const { return static_cast<int>(Send(this, BM_GETCHECK)); }
void CButton::SetCheck(int nCheck) { SendMessage(BM_SETCHECK, static_cast<WPARAM>(nCheck)); }
UINT CButton::GetButtonStyle() const { return static_cast<UINT>(GetStyle() & 0xFFFF); }

void CButton::SetButtonStyle(UINT nStyle, BOOL bRedraw) {
    SendMessage(BM_SETSTYLE, static_cast<WPARAM>(nStyle), static_cast<LPARAM>(bRedraw));
}

HICON CButton::SetIcon(HICON hIcon) {
    return reinterpret_cast<HICON>(SendMessage(BM_SETIMAGE, IMAGE_ICON, reinterpret_cast<LPARAM>(hIcon)));
}

HICON CButton::GetIcon() const { return reinterpret_cast<HICON>(Send(this, BM_GETIMAGE, IMAGE_ICON)); }

HBITMAP CButton::SetBitmap(HBITMAP hBitmap) {
    return reinterpret_cast<HBITMAP>(SendMessage(BM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(hBitmap)));
}

HBITMAP CButton::GetBitmap() const { return reinterpret_cast<HBITMAP>(Send(this, BM_GETIMAGE, IMAGE_BITMAP)); }

HCURSOR CButton::SetCursor(HCURSOR hCursor) {
    return reinterpret_cast<HCURSOR>(SendMessage(BM_SETIMAGE, IMAGE_CURSOR, reinterpret_cast<LPARAM>(hCursor)));
}

HCURSOR CButton::GetCursor() { return reinterpret_cast<HCURSOR>(SendMessage(BM_GETIMAGE, IMAGE_CURSOR)); }

void CButton::DrawItem(LPDRAWITEMSTRUCT) {}

BOOL CButton::OnChildNotify(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult) {
    if (CWnd::OnChildNotify(message, wParam, lParam, pResult))
        return TRUE;
    if (message != WM_DRAWITEM)
        return FALSE;
    DrawItem(reinterpret_cast<LPDRAWITEMSTRUCT>(lParam));
    return TRUE;
}

// ---------------------------------------------------------------------------------------------
// CEdit

BOOL CEdit::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID) {
    return CWnd::Create("Edit", nullptr, dwStyle, rect, pParentWnd, nID);
}

BOOL CEdit::CanUndo() const { return static_cast<BOOL>(Send(this, EM_CANUNDO)); }
int CEdit::GetLineCount() const { return static_cast<int>(Send(this, EM_GETLINECOUNT)); }
BOOL CEdit::GetModify() const { return static_cast<BOOL>(Send(this, EM_GETMODIFY)); }
void CEdit::SetModify(BOOL bModified) { SendMessage(EM_SETMODIFY, static_cast<WPARAM>(bModified)); }
void CEdit::GetRect(LPRECT lpRect) const { Send(this, EM_GETRECT, 0, reinterpret_cast<LPARAM>(lpRect)); }
DWORD CEdit::GetSel() const { return static_cast<DWORD>(Send(this, EM_GETSEL)); }

void CEdit::GetSel(int& nStartChar, int& nEndChar) const {
    DWORD start = 0;
    DWORD end = 0;
    Send(this, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
    nStartChar = static_cast<int>(start);
    nEndChar = static_cast<int>(end);
}

int CEdit::GetLine(int nIndex, char* lpszBuffer) const {
    return static_cast<int>(Send(this, EM_GETLINE, static_cast<WPARAM>(nIndex), reinterpret_cast<LPARAM>(lpszBuffer)));
}

int CEdit::GetLine(int nIndex, char* lpszBuffer, int nMaxLength) const {
    if (!lpszBuffer || nMaxLength < static_cast<int>(sizeof(WORD)))
        return 0;
    *reinterpret_cast<WORD*>(lpszBuffer) = static_cast<WORD>(nMaxLength);
    return GetLine(nIndex, lpszBuffer);
}

void CEdit::EmptyUndoBuffer() { SendMessage(EM_EMPTYUNDOBUFFER); }
void CEdit::LimitText(int nChars) { SendMessage(EM_LIMITTEXT, static_cast<WPARAM>(nChars)); }
int CEdit::LineFromChar(int nIndex) const { return static_cast<int>(Send(this, EM_LINEFROMCHAR, static_cast<WPARAM>(nIndex))); }
int CEdit::LineIndex(int nLine) const { return static_cast<int>(Send(this, EM_LINEINDEX, static_cast<WPARAM>(nLine))); }
int CEdit::LineLength(int nLine) const { return static_cast<int>(Send(this, EM_LINELENGTH, static_cast<WPARAM>(nLine))); }

void CEdit::LineScroll(int nLines, int nChars) {
    SendMessage(EM_LINESCROLL, static_cast<WPARAM>(nChars), static_cast<LPARAM>(nLines));
}

void CEdit::ReplaceSel(const char* lpszNewText, BOOL bCanUndo) {
    SendMessage(EM_REPLACESEL, static_cast<WPARAM>(bCanUndo), reinterpret_cast<LPARAM>(lpszNewText ? lpszNewText : ""));
}

void CEdit::SetPasswordChar(char ch) { SendMessage(EM_SETPASSWORDCHAR, static_cast<unsigned char>(ch)); }

void CEdit::SetSel(DWORD dwSelection, BOOL bNoScroll) {
    SendMessage(EM_SETSEL, LOWORD(dwSelection), HIWORD(dwSelection));
    if (!bNoScroll)
        SendMessage(EM_SCROLLCARET);
}

void CEdit::SetSel(int nStartChar, int nEndChar, BOOL bNoScroll) {
    SendMessage(EM_SETSEL, static_cast<WPARAM>(nStartChar), static_cast<LPARAM>(nEndChar));
    if (!bNoScroll)
        SendMessage(EM_SCROLLCARET);
}

BOOL CEdit::SetTabStops(int nTabStops, LPINT rgTabStops) {
    return static_cast<BOOL>(SendMessage(EM_SETTABSTOPS, static_cast<WPARAM>(nTabStops), reinterpret_cast<LPARAM>(rgTabStops)));
}

BOOL CEdit::Undo() { return static_cast<BOOL>(SendMessage(EM_UNDO)); }
void CEdit::Clear() { SendMessage(WM_CLEAR); }
void CEdit::Copy() { SendMessage(WM_COPY); }
void CEdit::Cut() { SendMessage(WM_CUT); }
void CEdit::Paste() { SendMessage(WM_PASTE); }
BOOL CEdit::SetReadOnly(BOOL bReadOnly) { return static_cast<BOOL>(SendMessage(EM_SETREADONLY, static_cast<WPARAM>(bReadOnly))); }
int CEdit::GetFirstVisibleLine() const { return static_cast<int>(Send(this, EM_GETFIRSTVISIBLELINE)); }
char CEdit::GetPasswordChar() const { return static_cast<char>(Send(this, EM_GETPASSWORDCHAR)); }
void CEdit::SetLimitText(UINT nMax) { SendMessage(EM_SETLIMITTEXT, static_cast<WPARAM>(nMax)); }
UINT CEdit::GetLimitText() const { return static_cast<UINT>(Send(this, EM_GETLIMITTEXT)); }

CPoint CEdit::PosFromChar(UINT nChar) const {
    LRESULT r = Send(this, EM_POSFROMCHAR, static_cast<WPARAM>(nChar));
    return CPoint(static_cast<short>(LOWORD(r)), static_cast<short>(HIWORD(r)));
}

int CEdit::CharFromPos(CPoint pt) const {
    return static_cast<int>(Send(this, EM_CHARFROMPOS, 0, MAKELPARAM(pt.x, pt.y)));
}

// ---------------------------------------------------------------------------------------------
// CListBox

BOOL CListBox::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID) {
    return CWnd::Create("ListBox", nullptr, dwStyle, rect, pParentWnd, nID);
}

int CListBox::GetCount() const { return static_cast<int>(Send(this, LB_GETCOUNT)); }
int CListBox::GetTopIndex() const { return static_cast<int>(Send(this, LB_GETTOPINDEX)); }
int CListBox::SetTopIndex(int nIndex) { return static_cast<int>(SendMessage(LB_SETTOPINDEX, static_cast<WPARAM>(nIndex))); }
DWORD_PTR CListBox::GetItemData(int nIndex) const { return static_cast<DWORD_PTR>(Send(this, LB_GETITEMDATA, static_cast<WPARAM>(nIndex))); }

int CListBox::SetItemData(int nIndex, DWORD_PTR dwItemData) {
    return static_cast<int>(SendMessage(LB_SETITEMDATA, static_cast<WPARAM>(nIndex), static_cast<LPARAM>(dwItemData)));
}

int CListBox::GetItemRect(int nIndex, LPRECT lpRect) const {
    return static_cast<int>(Send(this, LB_GETITEMRECT, static_cast<WPARAM>(nIndex), reinterpret_cast<LPARAM>(lpRect)));
}

int CListBox::GetSel(int nIndex) const { return static_cast<int>(Send(this, LB_GETSEL, static_cast<WPARAM>(nIndex))); }

int CListBox::GetText(int nIndex, char* lpszBuffer) const {
    return static_cast<int>(Send(this, LB_GETTEXT, static_cast<WPARAM>(nIndex), reinterpret_cast<LPARAM>(lpszBuffer)));
}

void CListBox::GetText(int nIndex, CString& rString) const {
    int len = GetTextLen(nIndex);
    if (len == LB_ERR) {
        rString.Empty();
        return;
    }
    GetText(nIndex, rString.GetBufferSetLength(len));
    rString.ReleaseBuffer();
}

int CListBox::GetTextLen(int nIndex) const { return static_cast<int>(Send(this, LB_GETTEXTLEN, static_cast<WPARAM>(nIndex))); }

int CListBox::FindStringExact(int nIndexStart, const char* lpszFind) const {
    return static_cast<int>(Send(this, LB_FINDSTRINGEXACT, static_cast<WPARAM>(nIndexStart), reinterpret_cast<LPARAM>(lpszFind)));
}

int CListBox::GetCaretIndex() const { return static_cast<int>(Send(this, LB_GETCARETINDEX)); }

int CListBox::SetCaretIndex(int nIndex, BOOL bScroll) {
    return static_cast<int>(SendMessage(LB_SETCARETINDEX, static_cast<WPARAM>(nIndex), MAKELONG(bScroll, 0)));
}

int CListBox::GetCurSel() const { return static_cast<int>(Send(this, LB_GETCURSEL)); }
int CListBox::SetCurSel(int nSelect) { return static_cast<int>(SendMessage(LB_SETCURSEL, static_cast<WPARAM>(nSelect))); }

int CListBox::SetSel(int nIndex, BOOL bSelect) {
    return static_cast<int>(SendMessage(LB_SETSEL, static_cast<WPARAM>(bSelect), static_cast<LPARAM>(nIndex)));
}

int CListBox::GetSelCount() const { return static_cast<int>(Send(this, LB_GETSELCOUNT)); }

int CListBox::GetSelItems(int nMaxItems, LPINT rgIndex) const {
    return static_cast<int>(Send(this, LB_GETSELITEMS, static_cast<WPARAM>(nMaxItems), reinterpret_cast<LPARAM>(rgIndex)));
}

int CListBox::SelItemRange(BOOL bSelect, int nFirstItem, int nLastItem) {
    if (bSelect)
        return static_cast<int>(SendMessage(LB_SELITEMRANGEEX, static_cast<WPARAM>(nFirstItem), static_cast<LPARAM>(nLastItem)));
    return static_cast<int>(SendMessage(LB_SELITEMRANGEEX, static_cast<WPARAM>(nLastItem), static_cast<LPARAM>(nFirstItem)));
}

int CListBox::AddString(const char* lpszItem) {
    return static_cast<int>(SendMessage(LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(lpszItem ? lpszItem : "")));
}

int CListBox::DeleteString(UINT nIndex) { return static_cast<int>(SendMessage(LB_DELETESTRING, nIndex)); }

int CListBox::InsertString(int nIndex, const char* lpszItem) {
    return static_cast<int>(SendMessage(LB_INSERTSTRING, static_cast<WPARAM>(nIndex), reinterpret_cast<LPARAM>(lpszItem ? lpszItem : "")));
}

void CListBox::ResetContent() { SendMessage(LB_RESETCONTENT); }

int CListBox::FindString(int nStartAfter, const char* lpszItem) const {
    return static_cast<int>(Send(this, LB_FINDSTRING, static_cast<WPARAM>(nStartAfter), reinterpret_cast<LPARAM>(lpszItem)));
}

int CListBox::SelectString(int nStartAfter, const char* lpszItem) {
    return static_cast<int>(SendMessage(LB_SELECTSTRING, static_cast<WPARAM>(nStartAfter), reinterpret_cast<LPARAM>(lpszItem)));
}

UINT CListBox::ItemFromPoint(CPoint pt, BOOL& bOutside) const {
    return OnMain([&]() -> UINT {
        bOutside = TRUE;
        auto* lb = m_hWnd ? wxDynamicCast(GetWx(), wxListBox) : nullptr;
        if (!lb || lb->GetCount() == 0)
            return 0;
        int hit = lb->HitTest(wxPoint(pt.x, pt.y));
        if (hit != wxNOT_FOUND) {
            bOutside = FALSE;
            return static_cast<UINT>(hit);
        }
        return pt.y < 0 ? 0 : lb->GetCount() - 1;
    });
}

// ---------------------------------------------------------------------------------------------
// CCheckListBox

void CCheckListBox::SetCheck(int nIndex, int nCheck) {
    OnMain([&] {
        auto* c = m_hWnd ? wxDynamicCast(GetWx(), wxCheckListBox) : nullptr;
        if (c && nIndex >= 0 && nIndex < static_cast<int>(c->GetCount()))
            c->Check(static_cast<unsigned>(nIndex), nCheck != 0);
    });
}

int CCheckListBox::GetCheck(int nIndex) {
    return OnMain([&]() -> int {
        auto* c = m_hWnd ? wxDynamicCast(GetWx(), wxCheckListBox) : nullptr;
        if (!c || nIndex < 0 || nIndex >= static_cast<int>(c->GetCount()))
            return 0;
        return c->IsChecked(static_cast<unsigned>(nIndex)) ? 1 : 0;
    });
}

void CCheckListBox::Enable(int, BOOL) {}

BOOL CCheckListBox::IsEnabled(int) { return TRUE; }

// ---------------------------------------------------------------------------------------------
// CComboBox

BOOL CComboBox::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID) {
    return CWnd::Create("ComboBox", nullptr, dwStyle, rect, pParentWnd, nID);
}

int CComboBox::GetCount() const { return static_cast<int>(Send(this, CB_GETCOUNT)); }
int CComboBox::GetCurSel() const { return static_cast<int>(Send(this, CB_GETCURSEL)); }
int CComboBox::SetCurSel(int nSelect) { return static_cast<int>(SendMessage(CB_SETCURSEL, static_cast<WPARAM>(nSelect))); }
DWORD CComboBox::GetEditSel() const { return static_cast<DWORD>(Send(this, CB_GETEDITSEL)); }
BOOL CComboBox::LimitText(int nMaxChars) { return static_cast<BOOL>(SendMessage(CB_LIMITTEXT, static_cast<WPARAM>(nMaxChars))); }

BOOL CComboBox::SetEditSel(int nStartChar, int nEndChar) {
    return static_cast<BOOL>(SendMessage(CB_SETEDITSEL, 0, MAKELONG(nStartChar, nEndChar)));
}

DWORD_PTR CComboBox::GetItemData(int nIndex) const {
    return static_cast<DWORD_PTR>(Send(this, CB_GETITEMDATA, static_cast<WPARAM>(nIndex)));
}

int CComboBox::SetItemData(int nIndex, DWORD_PTR dwItemData) {
    return static_cast<int>(SendMessage(CB_SETITEMDATA, static_cast<WPARAM>(nIndex), static_cast<LPARAM>(dwItemData)));
}

int CComboBox::GetLBText(int nIndex, char* lpszText) const {
    return static_cast<int>(Send(this, CB_GETLBTEXT, static_cast<WPARAM>(nIndex), reinterpret_cast<LPARAM>(lpszText)));
}

void CComboBox::GetLBText(int nIndex, CString& rString) const {
    int len = GetLBTextLen(nIndex);
    if (len == CB_ERR) {
        rString.Empty();
        return;
    }
    GetLBText(nIndex, rString.GetBufferSetLength(len));
    rString.ReleaseBuffer();
}

int CComboBox::GetLBTextLen(int nIndex) const { return static_cast<int>(Send(this, CB_GETLBTEXTLEN, static_cast<WPARAM>(nIndex))); }

int CComboBox::FindStringExact(int nIndexStart, const char* lpszFind) const {
    return static_cast<int>(Send(this, CB_FINDSTRINGEXACT, static_cast<WPARAM>(nIndexStart), reinterpret_cast<LPARAM>(lpszFind)));
}

void CComboBox::GetDroppedControlRect(LPRECT lprect) const {
    Send(this, CB_GETDROPPEDCONTROLRECT, 0, reinterpret_cast<LPARAM>(lprect));
}

void CComboBox::ShowDropDown(BOOL bShowIt) { SendMessage(CB_SHOWDROPDOWN, static_cast<WPARAM>(bShowIt)); }

int CComboBox::AddString(const char* lpszString) {
    return static_cast<int>(SendMessage(CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(lpszString ? lpszString : "")));
}

int CComboBox::DeleteString(UINT nIndex) { return static_cast<int>(SendMessage(CB_DELETESTRING, nIndex)); }

int CComboBox::InsertString(int nIndex, const char* lpszString) {
    return static_cast<int>(SendMessage(CB_INSERTSTRING, static_cast<WPARAM>(nIndex), reinterpret_cast<LPARAM>(lpszString ? lpszString : "")));
}

void CComboBox::ResetContent() { SendMessage(CB_RESETCONTENT); }

int CComboBox::FindString(int nStartAfter, const char* lpszString) const {
    return static_cast<int>(Send(this, CB_FINDSTRING, static_cast<WPARAM>(nStartAfter), reinterpret_cast<LPARAM>(lpszString)));
}

int CComboBox::SelectString(int nStartAfter, const char* lpszString) {
    return static_cast<int>(SendMessage(CB_SELECTSTRING, static_cast<WPARAM>(nStartAfter), reinterpret_cast<LPARAM>(lpszString)));
}

void CComboBox::Clear() { SendMessage(WM_CLEAR); }
void CComboBox::Copy() { SendMessage(WM_COPY); }
void CComboBox::Cut() { SendMessage(WM_CUT); }
void CComboBox::Paste() { SendMessage(WM_PASTE); }

// ---------------------------------------------------------------------------------------------
// CScrollBar

BOOL CScrollBar::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID) {
    return CWnd::Create("ScrollBar", nullptr, dwStyle, rect, pParentWnd, nID);
}

int CScrollBar::GetScrollPos() const { return static_cast<int>(Send(this, SBM_GETPOS)); }

int CScrollBar::SetScrollPos(int nPos, BOOL bRedraw) {
    return static_cast<int>(SendMessage(SBM_SETPOS, static_cast<WPARAM>(nPos), static_cast<LPARAM>(bRedraw)));
}

void CScrollBar::GetScrollRange(LPINT lpMinPos, LPINT lpMaxPos) const {
    Send(this, SBM_GETRANGE, reinterpret_cast<WPARAM>(lpMinPos), reinterpret_cast<LPARAM>(lpMaxPos));
}

void CScrollBar::SetScrollRange(int nMinPos, int nMaxPos, BOOL bRedraw) {
    SendMessage(bRedraw ? SBM_SETRANGEREDRAW : SBM_SETRANGE, static_cast<WPARAM>(nMinPos), static_cast<LPARAM>(nMaxPos));
}

void CScrollBar::ShowScrollBar(BOOL bShow) { ShowWindow(bShow ? SW_SHOW : SW_HIDE); }

BOOL CScrollBar::EnableScrollBar(UINT nArrowFlags) {
    EnableWindow((nArrowFlags & ESB_DISABLE_BOTH) != ESB_DISABLE_BOTH);
    return TRUE;
}

BOOL CScrollBar::SetScrollInfo(LPSCROLLINFO lpScrollInfo, BOOL bRedraw) {
    if (!lpScrollInfo)
        return FALSE;
    lpScrollInfo->cbSize = sizeof(SCROLLINFO);
    SendMessage(SBM_SETSCROLLINFO, static_cast<WPARAM>(bRedraw), reinterpret_cast<LPARAM>(lpScrollInfo));
    return TRUE;
}

BOOL CScrollBar::GetScrollInfo(LPSCROLLINFO lpScrollInfo, UINT nMask) {
    if (!lpScrollInfo)
        return FALSE;
    lpScrollInfo->cbSize = sizeof(SCROLLINFO);
    lpScrollInfo->fMask = nMask;
    return static_cast<BOOL>(SendMessage(SBM_GETSCROLLINFO, 0, reinterpret_cast<LPARAM>(lpScrollInfo)));
}

int CScrollBar::GetScrollLimit() {
    int nMin = 0;
    int nMax = 0;
    GetScrollRange(&nMin, &nMax);
    SCROLLINFO info;
    memset(&info, 0, sizeof info);
    if (GetScrollInfo(&info, SIF_PAGE))
        nMax -= std::max(static_cast<int>(info.nPage) - 1, 0);
    return nMax;
}

// ---------------------------------------------------------------------------------------------
// CProgressCtrl

BOOL CProgressCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID) {
    return CWnd::Create("msctls_progress32", nullptr, dwStyle, rect, pParentWnd, nID);
}

void CProgressCtrl::SetRange(short nLower, short nUpper) { SendMessage(PBM_SETRANGE, 0, MAKELPARAM(nLower, nUpper)); }

void CProgressCtrl::SetRange32(int nLower, int nUpper) {
    SendMessage(PBM_SETRANGE32, static_cast<WPARAM>(nLower), static_cast<LPARAM>(nUpper));
}

void CProgressCtrl::GetRange(int& nLower, int& nUpper) const {
    int range[2] = {0, 0};
    Send(this, PBM_GETRANGE, FALSE, reinterpret_cast<LPARAM>(range));
    nLower = range[0];
    nUpper = range[1];
}

int CProgressCtrl::GetPos() const { return static_cast<int>(Send(this, PBM_GETPOS)); }
int CProgressCtrl::SetPos(int nPos) { return static_cast<int>(SendMessage(PBM_SETPOS, static_cast<WPARAM>(nPos))); }
int CProgressCtrl::OffsetPos(int nPos) { return static_cast<int>(SendMessage(PBM_DELTAPOS, static_cast<WPARAM>(nPos))); }
int CProgressCtrl::SetStep(int nStep) { return static_cast<int>(SendMessage(PBM_SETSTEP, static_cast<WPARAM>(nStep))); }
int CProgressCtrl::StepIt() { return static_cast<int>(SendMessage(PBM_STEPIT)); }
COLORREF CProgressCtrl::SetBkColor(COLORREF clrNew) { return static_cast<COLORREF>(SendMessage(PBM_SETBKCOLOR, 0, static_cast<LPARAM>(clrNew))); }
COLORREF CProgressCtrl::SetBarColor(COLORREF clrBar) { return static_cast<COLORREF>(SendMessage(PBM_SETBARCOLOR, 0, static_cast<LPARAM>(clrBar))); }

BOOL CProgressCtrl::SetMarquee(BOOL fMarqueeMode, int nInterval) {
    return static_cast<BOOL>(SendMessage(PBM_SETMARQUEE, static_cast<WPARAM>(fMarqueeMode), static_cast<LPARAM>(nInterval)));
}

// ---------------------------------------------------------------------------------------------
// CSliderCtrl

BOOL CSliderCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID) {
    return CWnd::Create("msctls_trackbar32", nullptr, dwStyle, rect, pParentWnd, nID);
}

int CSliderCtrl::GetLineSize() const { return static_cast<int>(Send(this, TBM_GETLINESIZE)); }
int CSliderCtrl::SetLineSize(int nSize) { return static_cast<int>(SendMessage(TBM_SETLINESIZE, 0, static_cast<LPARAM>(nSize))); }
int CSliderCtrl::GetPageSize() const { return static_cast<int>(Send(this, TBM_GETPAGESIZE)); }
int CSliderCtrl::SetPageSize(int nSize) { return static_cast<int>(SendMessage(TBM_SETPAGESIZE, 0, static_cast<LPARAM>(nSize))); }
int CSliderCtrl::GetRangeMax() const { return static_cast<int>(Send(this, TBM_GETRANGEMAX)); }
int CSliderCtrl::GetRangeMin() const { return static_cast<int>(Send(this, TBM_GETRANGEMIN)); }

void CSliderCtrl::GetRange(int& nMin, int& nMax) const {
    nMin = GetRangeMin();
    nMax = GetRangeMax();
}

void CSliderCtrl::SetRangeMin(int nMin, BOOL bRedraw) {
    SendMessage(TBM_SETRANGEMIN, static_cast<WPARAM>(bRedraw), static_cast<LPARAM>(nMin));
}

void CSliderCtrl::SetRangeMax(int nMax, BOOL bRedraw) {
    SendMessage(TBM_SETRANGEMAX, static_cast<WPARAM>(bRedraw), static_cast<LPARAM>(nMax));
}

void CSliderCtrl::SetRange(int nMin, int nMax, BOOL bRedraw) {
    SendMessage(TBM_SETRANGEMIN, FALSE, static_cast<LPARAM>(nMin));
    SendMessage(TBM_SETRANGEMAX, static_cast<WPARAM>(bRedraw), static_cast<LPARAM>(nMax));
}

void CSliderCtrl::GetSelection(int& nMin, int& nMax) const {
    nMin = static_cast<int>(Send(this, TBM_GETSELSTART));
    nMax = static_cast<int>(Send(this, TBM_GETSELEND));
}

void CSliderCtrl::SetSelection(int nMin, int nMax) {
    SendMessage(TBM_SETSELSTART, FALSE, static_cast<LPARAM>(nMin));
    SendMessage(TBM_SETSELEND, TRUE, static_cast<LPARAM>(nMax));
}

void CSliderCtrl::GetChannelRect(LPRECT lprc) const {
    OnMain([&] {
        if (!lprc || !m_hWnd)
            return;
        wxSize sz = GetWx()->GetClientSize();
        bool vertical = (GetStyle() & TBS_VERT) != 0;
        if (vertical) {
            lprc->left = sz.x / 2 - 2;
            lprc->right = sz.x / 2 + 2;
            lprc->top = 8;
            lprc->bottom = std::max(8, sz.y - 8);
        } else {
            lprc->left = 8;
            lprc->right = std::max(8, sz.x - 8);
            lprc->top = sz.y / 2 - 2;
            lprc->bottom = sz.y / 2 + 2;
        }
    });
}

void CSliderCtrl::GetThumbRect(LPRECT lprc) const {
    if (!lprc)
        return;
    RECT channel;
    GetChannelRect(&channel);
    int lo = GetRangeMin();
    int hi = GetRangeMax();
    int pos = GetPos();
    double f = hi > lo ? static_cast<double>(pos - lo) / (hi - lo) : 0.0;
    if (GetStyle() & TBS_VERT) {
        int y = channel.top + static_cast<int>(f * (channel.bottom - channel.top));
        lprc->left = channel.left - 8;
        lprc->right = channel.right + 8;
        lprc->top = y - 5;
        lprc->bottom = y + 5;
    } else {
        int x = channel.left + static_cast<int>(f * (channel.right - channel.left));
        lprc->left = x - 5;
        lprc->right = x + 5;
        lprc->top = channel.top - 8;
        lprc->bottom = channel.bottom + 8;
    }
}

int CSliderCtrl::GetPos() const { return static_cast<int>(Send(this, TBM_GETPOS)); }
void CSliderCtrl::SetPos(int nPos) { SendMessage(TBM_SETPOS, TRUE, static_cast<LPARAM>(nPos)); }
UINT CSliderCtrl::GetNumTics() const { return static_cast<UINT>(Send(this, TBM_GETNUMTICS)); }
int CSliderCtrl::GetTic(int nTic) const { return static_cast<int>(Send(this, TBM_GETTIC, static_cast<WPARAM>(nTic))); }
BOOL CSliderCtrl::SetTic(int nTic) { return static_cast<BOOL>(SendMessage(TBM_SETTIC, 0, static_cast<LPARAM>(nTic))); }
void CSliderCtrl::SetTicFreq(int nFreq) { SendMessage(TBM_SETTICFREQ, static_cast<WPARAM>(nFreq)); }
void CSliderCtrl::ClearSel(BOOL bRedraw) { SendMessage(TBM_CLEARSEL, static_cast<WPARAM>(bRedraw)); }
void CSliderCtrl::ClearTics(BOOL bRedraw) { SendMessage(TBM_CLEARTICS, static_cast<WPARAM>(bRedraw)); }

// ---------------------------------------------------------------------------------------------
// CSpinButtonCtrl

BOOL CSpinButtonCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID) {
    return CWnd::Create("msctls_updown32", nullptr, dwStyle, rect, pParentWnd, nID);
}

BOOL CSpinButtonCtrl::SetAccel(int nAccel, UDACCEL* pAccel) {
    return static_cast<BOOL>(SendMessage(UDM_SETACCEL, static_cast<WPARAM>(nAccel), reinterpret_cast<LPARAM>(pAccel)));
}

UINT CSpinButtonCtrl::GetAccel(int nAccel, UDACCEL* pAccel) const {
    return static_cast<UINT>(Send(this, UDM_GETACCEL, static_cast<WPARAM>(nAccel), reinterpret_cast<LPARAM>(pAccel)));
}

int CSpinButtonCtrl::SetBase(int nBase) { return static_cast<int>(SendMessage(UDM_SETBASE, static_cast<WPARAM>(nBase))); }
UINT CSpinButtonCtrl::GetBase() const { return static_cast<UINT>(Send(this, UDM_GETBASE)); }

CWnd* CSpinButtonCtrl::SetBuddy(CWnd* pWndBuddy) {
    return CWnd::FromHandle(reinterpret_cast<HWND>(
        SendMessage(UDM_SETBUDDY, reinterpret_cast<WPARAM>(pWndBuddy ? pWndBuddy->m_hWnd : nullptr))));
}

CWnd* CSpinButtonCtrl::GetBuddy() const { return CWnd::FromHandle(reinterpret_cast<HWND>(Send(this, UDM_GETBUDDY))); }
int CSpinButtonCtrl::SetPos(int nPos) { return static_cast<int>(SendMessage(UDM_SETPOS32, 0, static_cast<LPARAM>(nPos))); }
int CSpinButtonCtrl::GetPos() const { return static_cast<int>(Send(this, UDM_GETPOS)); }
int CSpinButtonCtrl::GetPos32(BOOL* lpbError) const { return static_cast<int>(Send(this, UDM_GETPOS32, 0, reinterpret_cast<LPARAM>(lpbError))); }
void CSpinButtonCtrl::SetRange(short nLower, short nUpper) { SendMessage(UDM_SETRANGE, 0, MAKELPARAM(nUpper, nLower)); }

void CSpinButtonCtrl::SetRange32(int nLower, int nUpper) {
    SendMessage(UDM_SETRANGE32, static_cast<WPARAM>(nLower), static_cast<LPARAM>(nUpper));
}

DWORD CSpinButtonCtrl::GetRange() const { return static_cast<DWORD>(Send(this, UDM_GETRANGE)); }

void CSpinButtonCtrl::GetRange(int& lower, int& upper) const {
    Send(this, UDM_GETRANGE32, reinterpret_cast<WPARAM>(&lower), reinterpret_cast<LPARAM>(&upper));
}

// ---------------------------------------------------------------------------------------------
// CTabCtrl

BOOL CTabCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID) {
    return CWnd::Create("SysTabControl32", nullptr, dwStyle, rect, pParentWnd, nID);
}

int CTabCtrl::GetItemCount() const {
    return OnMain([&]() -> int {
        TabExtra* tx = TabsOf(this);
        return tx ? static_cast<int>(tx->items.size()) : 0;
    });
}

BOOL CTabCtrl::GetItem(int nItem, TCITEM* pTabCtrlItem) const {
    return OnMain([&]() -> BOOL {
        TabExtra* tx = TabsOf(this);
        if (!tx || !pTabCtrlItem || nItem < 0 || nItem >= static_cast<int>(tx->items.size()))
            return FALSE;
        const auto& item = tx->items[static_cast<size_t>(nItem)];
        if (pTabCtrlItem->mask & TCIF_TEXT)
            CopyText(item.first, pTabCtrlItem->pszText, pTabCtrlItem->cchTextMax);
        if (pTabCtrlItem->mask & TCIF_PARAM)
            pTabCtrlItem->lParam = item.second;
        if (pTabCtrlItem->mask & TCIF_IMAGE)
            pTabCtrlItem->iImage = -1;
        if (pTabCtrlItem->mask & TCIF_STATE)
            pTabCtrlItem->dwState = 0;
        return TRUE;
    });
}

BOOL CTabCtrl::SetItem(int nItem, TCITEM* pTabCtrlItem) {
    return OnMain([&]() -> BOOL {
        TabExtra* tx = TabsOf(this);
        if (!tx || !pTabCtrlItem || nItem < 0 || nItem >= static_cast<int>(tx->items.size()))
            return FALSE;
        auto& item = tx->items[static_cast<size_t>(nItem)];
        if ((pTabCtrlItem->mask & TCIF_TEXT) && pTabCtrlItem->pszText)
            item.first = ToWx(pTabCtrlItem->pszText);
        if (pTabCtrlItem->mask & TCIF_PARAM)
            item.second = pTabCtrlItem->lParam;
        GetWx()->Refresh();
        return TRUE;
    });
}

BOOL CTabCtrl::GetItemRect(int nItem, LPRECT lpRect) const {
    return OnMain([&]() -> BOOL {
        if (!TabsOf(this) || !lpRect)
            return FALSE;
        std::vector<wxRect> rects = TabRects(GetWx());
        if (nItem < 0 || nItem >= static_cast<int>(rects.size()))
            return FALSE;
        const wxRect& r = rects[static_cast<size_t>(nItem)];
        lpRect->left = r.x;
        lpRect->top = r.y;
        lpRect->right = r.x + r.width;
        lpRect->bottom = r.y + r.height;
        return TRUE;
    });
}

int CTabCtrl::GetCurSel() const {
    return OnMain([&]() -> int {
        TabExtra* tx = TabsOf(this);
        return tx ? tx->current : -1;
    });
}

int CTabCtrl::SetCurSel(int nItem) {
    return OnMain([&]() -> int {
        TabExtra* tx = TabsOf(this);
        if (!tx || nItem < 0 || nItem >= static_cast<int>(tx->items.size()))
            return -1;
        int old = tx->current;
        tx->current = nItem;
        GetWx()->Refresh();
        return old;
    });
}

void CTabCtrl::SetCurFocus(int nItem) {
    OnMain([&] {
        TabExtra* tx = TabsOf(this);
        if (!tx || nItem < 0 || nItem >= static_cast<int>(tx->items.size()) || nItem == tx->current)
            return;
        wxWindow* w = GetWx();
        NMHDR hdr;
        hdr.code = TCN_SELCHANGING;
        if (NotifyParentNM(w, &hdr))
            return;
        if (!(tx = TabsOf(this)))
            return;
        tx->current = nItem;
        w->Refresh();
        NMHDR changed;
        changed.code = TCN_SELCHANGE;
        NotifyParentNM(w, &changed);
    });
}

CSize CTabCtrl::SetItemSize(CSize size) {
    return OnMain([&]() -> CSize {
        TabExtra* tx = TabsOf(this);
        if (!tx)
            return CSize(0, 0);
        wxSize old = tx->itemSize;
        if (GetStyle() & TCS_FIXEDWIDTH)
            tx->itemSize.x = size.cx;
        tx->itemSize.y = size.cy;
        GetWx()->Refresh();
        return CSize(old.x, old.y);
    });
}

LONG CTabCtrl::InsertItem(int nItem, TCITEM* pTabCtrlItem) {
    return OnMain([&]() -> LONG {
        TabExtra* tx = TabsOf(this);
        if (!tx || !pTabCtrlItem || nItem < 0)
            return -1;
        int at = std::min(nItem, static_cast<int>(tx->items.size()));
        wxString text = (pTabCtrlItem->mask & TCIF_TEXT) && pTabCtrlItem->pszText ? ToWx(pTabCtrlItem->pszText) : wxString();
        LPARAM param = (pTabCtrlItem->mask & TCIF_PARAM) ? pTabCtrlItem->lParam : 0;
        tx->items.insert(tx->items.begin() + at, std::make_pair(text, param));
        if (tx->current >= at)
            ++tx->current;
        if (tx->current < 0)
            tx->current = 0;
        GetWx()->Refresh();
        return at;
    });
}

LONG CTabCtrl::InsertItem(int nItem, const char* lpszItem) {
    TCITEM item;
    memset(&item, 0, sizeof item);
    item.mask = TCIF_TEXT;
    item.pszText = const_cast<char*>(lpszItem);
    return InsertItem(nItem, &item);
}

LONG CTabCtrl::InsertItem(int nItem, const char* lpszItem, int nImage) {
    TCITEM item;
    memset(&item, 0, sizeof item);
    item.mask = TCIF_TEXT | TCIF_IMAGE;
    item.pszText = const_cast<char*>(lpszItem);
    item.iImage = nImage;
    return InsertItem(nItem, &item);
}

BOOL CTabCtrl::DeleteItem(int nItem) {
    return OnMain([&]() -> BOOL {
        TabExtra* tx = TabsOf(this);
        if (!tx || nItem < 0 || nItem >= static_cast<int>(tx->items.size()))
            return FALSE;
        tx->items.erase(tx->items.begin() + nItem);
        if (tx->current == nItem)
            tx->current = -1;
        else if (tx->current > nItem)
            --tx->current;
        GetWx()->Refresh();
        return TRUE;
    });
}

BOOL CTabCtrl::DeleteAllItems() {
    return OnMain([&]() -> BOOL {
        TabExtra* tx = TabsOf(this);
        if (!tx)
            return FALSE;
        tx->items.clear();
        tx->current = -1;
        GetWx()->Refresh();
        return TRUE;
    });
}

void CTabCtrl::AdjustRect(BOOL bLarger, LPRECT lpRect) {
    if (!lpRect)
        return;
    int header = OnMain([&]() -> int { return m_hWnd ? TabHeaderHeight(GetWx()) : 0; });
    int d = bLarger ? -1 : 1;
    lpRect->left += 2 * d;
    lpRect->top += (header + 2) * d;
    lpRect->right -= 2 * d;
    lpRect->bottom -= 2 * d;
}

// ---------------------------------------------------------------------------------------------
// CToolTipCtrl

BOOL CToolTipCtrl::Create(CWnd* pParentWnd, DWORD dwStyle) {
    if (!pParentWnd || !pParentWnd->m_hWnd)
        return FALSE;
    RECT r = {0, 0, 0, 0};
    return CWnd::CreateEx(0, "tooltips_class32", nullptr, WS_CHILD | (dwStyle & ~(WS_VISIBLE | WS_POPUP | WS_TABSTOP)),
                          r, pParentWnd, 0);
}

BOOL CToolTipCtrl::AddTool(CWnd* pWnd, UINT nIDText, LPCRECT lpRectTool, UINT_PTR nIDTool) {
    std::string text;
    if (!LoadResourceString(nIDText, text))
        return FALSE;
    return AddTool(pWnd, text.c_str(), lpRectTool, nIDTool);
}

BOOL CToolTipCtrl::AddTool(CWnd* pWnd, const char* lpszText, LPCRECT, UINT_PTR nIDTool) {
    if (!pWnd || !pWnd->m_hWnd)
        return FALSE;
    HWND tool = pWnd->m_hWnd;
    return OnMain([&]() -> BOOL {
        ToolTipExtra* tips = TipsOf(this);
        if (!tips)
            return FALSE;
        wxString text = lpszText == LPSTR_TEXTCALLBACK ? CallbackToolText(tool, m_hWnd, nIDTool)
                                                       : (lpszText ? ToWx(lpszText) : wxString());
        auto it = std::find_if(tips->tools.begin(), tips->tools.end(),
                               [&](const ToolTipExtra::Tool& t) { return t.hwnd == tool && t.id == nIDTool; });
        if (it == tips->tools.end())
            it = tips->tools.insert(tips->tools.end(), ToolTipExtra::Tool{tool, nIDTool, text});
        else
            it->text = text;
        ApplyTool(*it, tips->active);
        return TRUE;
    });
}

void CToolTipCtrl::DelTool(CWnd* pWnd, UINT_PTR nIDTool) {
    if (!pWnd)
        return;
    HWND tool = pWnd->m_hWnd;
    OnMain([&] {
        ToolTipExtra* tips = TipsOf(this);
        if (!tips)
            return;
        auto it = std::find_if(tips->tools.begin(), tips->tools.end(),
                               [&](const ToolTipExtra::Tool& t) { return t.hwnd == tool && t.id == nIDTool; });
        if (it == tips->tools.end())
            return;
        ApplyTool(ToolTipExtra::Tool{it->hwnd, it->id, wxString()}, false);
        tips->tools.erase(it);
    });
}

void CToolTipCtrl::UpdateTipText(const char* lpszText, CWnd* pWnd, UINT_PTR nIDTool) {
    if (!pWnd)
        return;
    HWND tool = pWnd->m_hWnd;
    OnMain([&] {
        ToolTipExtra* tips = TipsOf(this);
        if (!tips)
            return;
        auto it = std::find_if(tips->tools.begin(), tips->tools.end(),
                               [&](const ToolTipExtra::Tool& t) { return t.hwnd == tool && t.id == nIDTool; });
        if (it == tips->tools.end())
            it = std::find_if(tips->tools.begin(), tips->tools.end(),
                              [&](const ToolTipExtra::Tool& t) { return t.hwnd == tool; });
        if (it == tips->tools.end())
            return;
        it->text = lpszText == LPSTR_TEXTCALLBACK ? CallbackToolText(tool, m_hWnd, nIDTool)
                                                  : (lpszText ? ToWx(lpszText) : wxString());
        ApplyTool(*it, tips->active);
    });
}

void CToolTipCtrl::UpdateTipText(UINT nIDText, CWnd* pWnd, UINT_PTR nIDTool) {
    std::string text;
    LoadResourceString(nIDText, text);
    UpdateTipText(text.c_str(), pWnd, nIDTool);
}

void CToolTipCtrl::RelayEvent(LPMSG) {}

void CToolTipCtrl::Activate(BOOL bActivate) {
    OnMain([&] {
        ToolTipExtra* tips = TipsOf(this);
        if (!tips || tips->active == (bActivate != FALSE))
            return;
        tips->active = bActivate != FALSE;
        for (const auto& t : tips->tools)
            ApplyTool(t, tips->active);
    });
}

void CToolTipCtrl::SetDelayTime(UINT nDelay) { SetDelayTime(nDelay, TTDT_AUTOMATIC); }

void CToolTipCtrl::SetDelayTime(DWORD dwDuration, int iTime) {
    OnMain([&] {
        long ms = static_cast<long>(dwDuration);
        switch (iTime) {
        case TTDT_AUTOPOP:
            wxToolTip::SetAutoPop(ms);
            break;
        case TTDT_RESHOW:
            wxToolTip::SetReshow(ms);
            break;
        default:
            wxToolTip::SetDelay(ms);
            break;
        }
    });
}

int CToolTipCtrl::SetMaxTipWidth(int iWidth) {
    return OnMain([&]() -> int {
        ToolTipExtra* tips = TipsOf(this);
        if (!tips)
            return -1;
        int old = tips->maxWidth;
        tips->maxWidth = iWidth;
        return old;
    });
}

int CToolTipCtrl::GetToolCount() const {
    return OnMain([&]() -> int {
        ToolTipExtra* tips = TipsOf(this);
        return tips ? static_cast<int>(tips->tools.size()) : 0;
    });
}

// ---------------------------------------------------------------------------------------------
// CRichEditCtrl

BOOL CRichEditCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID) {
    return CWnd::Create("RichEdit20A", nullptr, dwStyle, rect, pParentWnd, nID);
}

BOOL CRichEditCtrl::CanUndo() const { return static_cast<BOOL>(Send(this, EM_CANUNDO)); }
BOOL CRichEditCtrl::CanRedo() const { return static_cast<BOOL>(Send(this, EM_CANREDO)); }
int CRichEditCtrl::GetLineCount() const { return static_cast<int>(Send(this, EM_GETLINECOUNT)); }
BOOL CRichEditCtrl::GetModify() const { return static_cast<BOOL>(Send(this, EM_GETMODIFY)); }
void CRichEditCtrl::SetModify(BOOL bModified) { SendMessage(EM_SETMODIFY, static_cast<WPARAM>(bModified)); }
void CRichEditCtrl::GetRect(LPRECT lpRect) const { Send(this, EM_GETRECT, 0, reinterpret_cast<LPARAM>(lpRect)); }

CPoint CRichEditCtrl::GetCharPos(long lChar) const {
    POINT pt = {0, 0};
    Send(this, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&pt), static_cast<LPARAM>(lChar));
    return CPoint(pt.x, pt.y);
}

int CRichEditCtrl::GetLine(int nIndex, char* lpszBuffer) const {
    return static_cast<int>(Send(this, EM_GETLINE, static_cast<WPARAM>(nIndex), reinterpret_cast<LPARAM>(lpszBuffer)));
}

int CRichEditCtrl::GetLine(int nIndex, char* lpszBuffer, int nMaxLength) const {
    if (!lpszBuffer || nMaxLength < static_cast<int>(sizeof(WORD)))
        return 0;
    *reinterpret_cast<WORD*>(lpszBuffer) = static_cast<WORD>(nMaxLength);
    return GetLine(nIndex, lpszBuffer);
}

BOOL CRichEditCtrl::CanPaste(UINT nFormat) const { return static_cast<BOOL>(Send(this, EM_CANPASTE, nFormat)); }

void CRichEditCtrl::GetSel(long& nStartChar, long& nEndChar) const {
    CHARRANGE cr = {0, 0};
    Send(this, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&cr));
    nStartChar = cr.cpMin;
    nEndChar = cr.cpMax;
}

void CRichEditCtrl::GetSel(CHARRANGE& cr) const { Send(this, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&cr)); }
void CRichEditCtrl::LimitText(long nChars) { SendMessage(EM_EXLIMITTEXT, 0, static_cast<LPARAM>(nChars)); }
long CRichEditCtrl::LineFromChar(long nIndex) const { return static_cast<long>(Send(this, EM_EXLINEFROMCHAR, 0, static_cast<LPARAM>(nIndex))); }

void CRichEditCtrl::SetSel(long nStartChar, long nEndChar) {
    CHARRANGE cr = {static_cast<LONG>(nStartChar), static_cast<LONG>(nEndChar)};
    SendMessage(EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&cr));
}

void CRichEditCtrl::SetSel(CHARRANGE& cr) { SendMessage(EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&cr)); }

DWORD CRichEditCtrl::GetDefaultCharFormat(CHARFORMAT& cf) const {
    cf.cbSize = sizeof(CHARFORMAT);
    return static_cast<DWORD>(Send(this, EM_GETCHARFORMAT, 0, reinterpret_cast<LPARAM>(&cf)));
}

DWORD CRichEditCtrl::GetSelectionCharFormat(CHARFORMAT& cf) const {
    cf.cbSize = sizeof(CHARFORMAT);
    return static_cast<DWORD>(Send(this, EM_GETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf)));
}

long CRichEditCtrl::GetLimitText() const { return static_cast<long>(Send(this, EM_GETLIMITTEXT)); }

DWORD CRichEditCtrl::GetParaFormat(PARAFORMAT& pf) const {
    pf.cbSize = sizeof(PARAFORMAT);
    return static_cast<DWORD>(Send(this, EM_GETPARAFORMAT, 0, reinterpret_cast<LPARAM>(&pf)));
}

long CRichEditCtrl::GetSelText(char* lpBuf) const {
    return static_cast<long>(Send(this, EM_GETSELTEXT, 0, reinterpret_cast<LPARAM>(lpBuf)));
}

CString CRichEditCtrl::GetSelText() const {
    CHARRANGE cr = {0, 0};
    Send(this, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&cr));
    int len = std::max(0, static_cast<int>(cr.cpMax - cr.cpMin));
    CString s;
    char* buf = s.GetBuffer(len + 1);
    buf[0] = 0;
    long n = static_cast<long>(Send(this, EM_GETSELTEXT, 0, reinterpret_cast<LPARAM>(buf)));
    s.ReleaseBuffer(static_cast<int>(std::min<long>(std::max<long>(n, 0), len)));
    return s;
}

WORD CRichEditCtrl::GetSelectionType() const { return static_cast<WORD>(Send(this, EM_SELECTIONTYPE)); }

COLORREF CRichEditCtrl::SetBackgroundColor(BOOL bSysColor, COLORREF cr) {
    return static_cast<COLORREF>(SendMessage(EM_SETBKGNDCOLOR, static_cast<WPARAM>(bSysColor), static_cast<LPARAM>(cr)));
}

BOOL CRichEditCtrl::SetDefaultCharFormat(CHARFORMAT& cf) {
    cf.cbSize = sizeof(CHARFORMAT);
    return static_cast<BOOL>(SendMessage(EM_SETCHARFORMAT, 0, reinterpret_cast<LPARAM>(&cf)));
}

BOOL CRichEditCtrl::SetSelectionCharFormat(CHARFORMAT& cf) {
    cf.cbSize = sizeof(CHARFORMAT);
    return static_cast<BOOL>(SendMessage(EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf)));
}

BOOL CRichEditCtrl::SetWordCharFormat(CHARFORMAT& cf) {
    cf.cbSize = sizeof(CHARFORMAT);
    return static_cast<BOOL>(SendMessage(EM_SETCHARFORMAT, SCF_SELECTION | SCF_WORD, reinterpret_cast<LPARAM>(&cf)));
}

BOOL CRichEditCtrl::SetParaFormat(PARAFORMAT& pf) {
    pf.cbSize = sizeof(PARAFORMAT);
    return static_cast<BOOL>(SendMessage(EM_SETPARAFORMAT, 0, reinterpret_cast<LPARAM>(&pf)));
}

BOOL CRichEditCtrl::SetReadOnly(BOOL bReadOnly) {
    return static_cast<BOOL>(SendMessage(EM_SETREADONLY, static_cast<WPARAM>(bReadOnly)));
}

long CRichEditCtrl::GetTextLength() const { return static_cast<long>(Send(this, WM_GETTEXTLENGTH)); }
void CRichEditCtrl::EmptyUndoBuffer() { SendMessage(EM_EMPTYUNDOBUFFER); }
int CRichEditCtrl::LineIndex(int nLine) const { return static_cast<int>(Send(this, EM_LINEINDEX, static_cast<WPARAM>(nLine))); }
int CRichEditCtrl::LineLength(int nLine) const { return static_cast<int>(Send(this, EM_LINELENGTH, static_cast<WPARAM>(nLine))); }

void CRichEditCtrl::LineScroll(int nLines, int nChars) {
    SendMessage(EM_LINESCROLL, static_cast<WPARAM>(nChars), static_cast<LPARAM>(nLines));
}

void CRichEditCtrl::ReplaceSel(const char* lpszNewText, BOOL bCanUndo) {
    SendMessage(EM_REPLACESEL, static_cast<WPARAM>(bCanUndo), reinterpret_cast<LPARAM>(lpszNewText ? lpszNewText : ""));
}

long CRichEditCtrl::StreamIn(int nFormat, EDITSTREAM& es) {
    return static_cast<long>(SendMessage(EM_STREAMIN, static_cast<WPARAM>(nFormat), reinterpret_cast<LPARAM>(&es)));
}

long CRichEditCtrl::StreamOut(int nFormat, EDITSTREAM& es) {
    return static_cast<long>(SendMessage(EM_STREAMOUT, static_cast<WPARAM>(nFormat), reinterpret_cast<LPARAM>(&es)));
}

long CRichEditCtrl::FindText(DWORD dwFlags, FINDTEXTEX* pFindText) const {
    return static_cast<long>(Send(this, EM_FINDTEXTEX, dwFlags, reinterpret_cast<LPARAM>(pFindText)));
}

BOOL CRichEditCtrl::Undo() { return static_cast<BOOL>(SendMessage(EM_UNDO)); }
BOOL CRichEditCtrl::Redo() { return static_cast<BOOL>(SendMessage(EM_REDO)); }
void CRichEditCtrl::Clear() { SendMessage(WM_CLEAR); }
void CRichEditCtrl::Copy() { SendMessage(WM_COPY); }
void CRichEditCtrl::Cut() { SendMessage(WM_CUT); }
void CRichEditCtrl::Paste() { SendMessage(WM_PASTE); }
int CRichEditCtrl::GetFirstVisibleLine() const { return static_cast<int>(Send(this, EM_GETFIRSTVISIBLELINE)); }

// ---------------------------------------------------------------------------------------------
// CBitmapButton

BOOL CBitmapButton::LoadBitmaps(const char* lpszBitmapResource, const char* lpszBitmapResourceSel,
                                const char* lpszBitmapResourceFocus, const char* lpszBitmapResourceDisabled) {
    m_bitmap.DeleteObject();
    m_bitmapSel.DeleteObject();
    m_bitmapFocus.DeleteObject();
    m_bitmapDisabled.DeleteObject();
    if (!lpszBitmapResource || !m_bitmap.LoadBitmap(lpszBitmapResource))
        return FALSE;
    BOOL all = TRUE;
    if (lpszBitmapResourceSel && !m_bitmapSel.LoadBitmap(lpszBitmapResourceSel))
        all = FALSE;
    if (lpszBitmapResourceFocus && !m_bitmapFocus.LoadBitmap(lpszBitmapResourceFocus))
        all = FALSE;
    if (lpszBitmapResourceDisabled && !m_bitmapDisabled.LoadBitmap(lpszBitmapResourceDisabled))
        all = FALSE;
    if (m_hWnd)
        Invalidate();
    return all;
}

BOOL CBitmapButton::LoadBitmaps(UINT nIDBitmapResource, UINT nIDBitmapResourceSel, UINT nIDBitmapResourceFocus,
                                UINT nIDBitmapResourceDisabled) {
    m_bitmap.DeleteObject();
    m_bitmapSel.DeleteObject();
    m_bitmapFocus.DeleteObject();
    m_bitmapDisabled.DeleteObject();
    if (!nIDBitmapResource || !m_bitmap.LoadBitmap(nIDBitmapResource))
        return FALSE;
    BOOL all = TRUE;
    if (nIDBitmapResourceSel && !m_bitmapSel.LoadBitmap(nIDBitmapResourceSel))
        all = FALSE;
    if (nIDBitmapResourceFocus && !m_bitmapFocus.LoadBitmap(nIDBitmapResourceFocus))
        all = FALSE;
    if (nIDBitmapResourceDisabled && !m_bitmapDisabled.LoadBitmap(nIDBitmapResourceDisabled))
        all = FALSE;
    if (m_hWnd)
        Invalidate();
    return all;
}

BOOL CBitmapButton::AutoLoad(UINT nID, CWnd* pParent) {
    if (!SubclassDlgItem(nID, pParent))
        return FALSE;
    CString name;
    GetWindowText(name);
    LoadBitmaps(name + "U", name + "D", name + "F", name + "X");
    if (!m_bitmap.m_hObject)
        return FALSE;
    SizeToContent();
    return TRUE;
}

void CBitmapButton::SizeToContent() {
    BITMAP bm;
    memset(&bm, 0, sizeof bm);
    if (!m_bitmap.m_hObject || m_bitmap.GetObject(sizeof bm, &bm) <= 0)
        return;
    SetWindowPos(nullptr, -1, -1, bm.bmWidth, bm.bmHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE);
}

void CBitmapButton::DrawItem(LPDRAWITEMSTRUCT lpDIS) {
    if (!lpDIS)
        return;
    CDC* pDC = CDC::FromHandle(lpDIS->hDC);
    if (!pDC)
        return;
    CRect rect(lpDIS->rcItem.left, lpDIS->rcItem.top, lpDIS->rcItem.right, lpDIS->rcItem.bottom);
    if (!m_bitmap.m_hObject) {
        wxDC* dc = pDC->GetWx();
        if (!dc || !m_hWnd)
            return;
        wxRect r(rect.left, rect.top, rect.Width(), rect.Height());
        int flags = 0;
        if (lpDIS->itemState & ODS_SELECTED)
            flags |= wxCONTROL_PRESSED;
        if (lpDIS->itemState & ODS_DISABLED)
            flags |= wxCONTROL_DISABLED;
        if (lpDIS->itemState & ODS_FOCUS)
            flags |= wxCONTROL_FOCUSED;
        wxRendererNative::Get().DrawPushButton(GetWx(), *dc, r, flags);
        CString text;
        GetWindowText(text);
        dc->SetFont(GetWx()->GetFont());
        dc->SetTextForeground(wxSystemSettings::GetColour((lpDIS->itemState & ODS_DISABLED) ? wxSYS_COLOUR_GRAYTEXT
                                                                                             : wxSYS_COLOUR_BTNTEXT));
        dc->DrawLabel(wxStripMenuCodes(ToWx(text), wxStrip_Mnemonics), r, wxALIGN_CENTER);
        return;
    }
    CBitmap* pBitmap = &m_bitmap;
    UINT state = lpDIS->itemState;
    if ((state & ODS_SELECTED) && m_bitmapSel.m_hObject)
        pBitmap = &m_bitmapSel;
    else if ((state & ODS_FOCUS) && m_bitmapFocus.m_hObject)
        pBitmap = &m_bitmapFocus;
    else if ((state & ODS_DISABLED) && m_bitmapDisabled.m_hObject)
        pBitmap = &m_bitmapDisabled;
    CDC memDC;
    if (!memDC.CreateCompatibleDC(pDC))
        return;
    CBitmap* pOld = memDC.SelectObject(pBitmap);
    if (!pOld)
        return;
    pDC->BitBlt(rect.left, rect.top, rect.Width(), rect.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOld);
}
