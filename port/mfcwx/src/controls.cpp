#include "controls_internal.h"

#include <wx/checklst.h>
#include <wx/gauge.h>
#include <wx/glcanvas.h>
#include <wx/listctrl.h>
#include <wx/renderer.h>
#include <wx/scrolbar.h>
#include <wx/slider.h>
#include <wx/spinbutt.h>
#include <wx/statbmp.h>
#include <wx/statline.h>
#include <wx/stc/stc.h>
#include <wx/tglbtn.h>
#include <wx/treectrl.h>

#include <algorithm>

namespace mfcwx {

// ---------------------------------------------------------------------------------------------
// Custom-drawn window used for views, owner-drawn controls, unknown window classes and statics
// that the application paints itself.

class GenericWindow : public wxWindow {
public:
    GenericWindow(wxWindow* parent, int id, const wxPoint& pos, const wxSize& size, long style)
        : wxWindow(parent, id, pos, size, style | wxFULL_REPAINT_ON_RESIZE) {}
    bool AcceptsFocus() const override {
        WindowState* st = GetState(const_cast<GenericWindow*>(this));
        return st && (st->style & WS_TABSTOP) != 0 ? wxWindow::AcceptsFocus() : HasFlag(wxWANTS_CHARS);
    }
    bool AcceptsFocusFromKeyboard() const override {
        WindowState* st = GetState(const_cast<GenericWindow*>(this));
        return st && (st->style & WS_TABSTOP) != 0;
    }
};

namespace {

struct EditExtra {
    int limit = 0;
    bool modified = false;
    char passwordChar = 0;
};

struct RangeExtra {
    int minValue = 0;
    int maxValue = 100;
    int step = 10;
    int pos = 0;
    bool inverted = false;
    int page = 0;
    int selStart = 0;
    int selEnd = 0;
};

struct ButtonExtra {
    bool pushed = false;
    bool hover = false;
    HBITMAP image = nullptr;
    HICON icon = nullptr;
};

wxString AnsiText(const char* s) { return ToWx(s ? s : ""); }

bool IsMultilineEdit(const WindowState& st) {
    return (st.kind == ControlKind::Edit || st.kind == ControlKind::RichEdit) && (st.style & ES_MULTILINE);
}

wxTextCtrl* AsText(wxWindow* w) { return wxDynamicCast(w, wxTextCtrl); }

// Windows multiline edits count "\r\n" as two characters; wx counts one.
long WinToWxPos(wxTextCtrl* t, const WindowState& st, long winPos) {
    if (!IsMultilineEdit(st) || winPos <= 0)
        return winPos;
    wxString v = t->GetValue();
    long wxPos = 0;
    long win = 0;
    for (wxString::const_iterator it = v.begin(); it != v.end() && win < winPos; ++it) {
        win += *it == '\n' ? 2 : 1;
        if (win <= winPos)
            ++wxPos;
    }
    return wxPos;
}

long WxToWinPos(wxTextCtrl* t, const WindowState& st, long wxPos) {
    if (!IsMultilineEdit(st) || wxPos <= 0)
        return wxPos;
    wxString v = t->GetValue();
    long win = 0;
    long i = 0;
    for (wxString::const_iterator it = v.begin(); it != v.end() && i < wxPos; ++it, ++i)
        win += *it == '\n' ? 2 : 1;
    return win;
}

wxString ToEditText(const WindowState& st, const wxString& s) {
    if (!IsMultilineEdit(st))
        return s;
    wxString r = s;
    r.Replace("\r\n", "\n");
    r.Replace("\r", "\n");
    return r;
}

wxString FromEditText(const WindowState& st, const wxString& s) {
    if (!IsMultilineEdit(st))
        return s;
    wxString r = s;
    r.Replace("\n", "\r\n");
    return r;
}

wxString StripMnemonicForNoPrefix(const char* text, DWORD style) {
    wxString s = AnsiText(text);
    if (style & SS_NOPREFIX)
        s.Replace("&", "&&");
    return s;
}

int ControlId(wxWindow* w) {
    WindowState* st = GetState(w);
    return st ? st->winId : 0;
}

void WrapStatic(wxWindow* w, WindowState& st) {
    auto* text = wxDynamicCast(w, wxStaticText);
    if (!text)
        return;
    DWORD type = st.style & SS_TYPEMASK;
    wxString label = (st.style & SS_NOPREFIX) ? wxString(st.text).Clone() : st.text;
    if (st.style & SS_NOPREFIX)
        label.Replace("&", "&&");
    st.settingText = true;
    text->SetLabel(label);
    if (type != SS_LEFTNOWORDWRAP && type != SS_SIMPLE && w->GetSize().x > 0)
        text->Wrap(w->GetSize().x);
    st.settingText = false;
}

} // namespace

wxString GetControlText(wxWindow* w) {
    WindowState* st = GetState(w);
    if (!st)
        return w->GetLabel();
    switch (st->kind) {
    case ControlKind::Edit:
    case ControlKind::RichEdit:
        if (auto* t = AsText(w))
            return FromEditText(*st, t->GetValue());
        break;
    case ControlKind::ComboBox:
        if (auto* c = wxDynamicCast(w, wxComboBox))
            return c->GetValue();
        break;
    case ControlKind::ComboList:
        if (auto* c = wxDynamicCast(w, wxChoice))
            return c->GetStringSelection();
        break;
    case ControlKind::Scintilla:
        if (auto* s = wxDynamicCast(w, wxStyledTextCtrl))
            return s->GetText();
        break;
    case ControlKind::Static:
    case ControlKind::Generic:
    case ControlKind::OwnerDrawButton:
    case ControlKind::StaticBitmap:
    case ControlKind::StaticFrame:
        return st->text;
    case ControlKind::Dialog:
    case ControlKind::Frame:
    case ControlKind::MDIChild:
        return st->text.empty() ? w->GetLabel() : st->text;
    default:
        break;
    }
    return w->GetLabel();
}

void SetControlText(wxWindow* w, const wxString& text) {
    WindowState* st = GetState(w);
    if (!st) {
        w->SetLabel(text);
        return;
    }
    switch (st->kind) {
    case ControlKind::Edit:
    case ControlKind::RichEdit:
        if (auto* t = AsText(w)) {
            wxString v = ToEditText(*st, text);
            if (t->GetValue() != v)
                t->SetValue(v);
            else
                t->SetValue(v);
            Extra<EditExtra>(*st).modified = false;
        }
        return;
    case ControlKind::ComboBox:
        if (auto* c = wxDynamicCast(w, wxComboBox)) {
            st->settingText = true;
            c->ChangeValue(text);
            st->settingText = false;
        }
        return;
    case ControlKind::ComboList:
        if (auto* c = wxDynamicCast(w, wxChoice))
            c->SetStringSelection(text);
        return;
    case ControlKind::Scintilla:
        if (auto* s = wxDynamicCast(w, wxStyledTextCtrl))
            s->SetText(text);
        return;
    case ControlKind::Static:
        st->text = text;
        WrapStatic(w, *st);
        return;
    case ControlKind::Generic:
    case ControlKind::OwnerDrawButton:
    case ControlKind::StaticBitmap:
    case ControlKind::StaticFrame:
        st->text = text;
        w->Refresh();
        return;
    case ControlKind::Dialog:
    case ControlKind::Frame:
    case ControlKind::MDIChild:
        st->text = text;
        w->SetLabel(text);
        return;
    default:
        w->SetLabel(text);
        return;
    }
}

DWORD CurrentStyle(wxWindow* w) {
    WindowState* st = GetState(w);
    DWORD style = st ? st->style : 0;
    style = w->IsShown() ? (style | WS_VISIBLE) : (style & ~WS_VISIBLE);
    style = w->IsThisEnabled() ? (style & ~WS_DISABLED) : (style | WS_DISABLED);
    if (st && (st->kind == ControlKind::Edit || st->kind == ControlKind::RichEdit)) {
        if (auto* t = AsText(w))
            style = t->IsEditable() ? (style & ~ES_READONLY) : (style | ES_READONLY);
    }
    if (auto* tlw = wxDynamicCast(w, wxTopLevelWindow)) {
        if (tlw->IsMaximized())
            style |= WS_MAXIMIZE;
        if (tlw->IsIconized())
            style |= WS_MINIMIZE;
    }
    return style;
}

void ApplyStyle(wxWindow* w, DWORD oldStyle, DWORD newStyle) {
    WindowState& st = EnsureState(w);
    st.style = newStyle;
    if ((oldStyle ^ newStyle) & WS_VISIBLE)
        w->Show((newStyle & WS_VISIBLE) != 0);
    if ((oldStyle ^ newStyle) & WS_DISABLED)
        w->Enable((newStyle & WS_DISABLED) == 0);
    if (st.kind == ControlKind::Edit || st.kind == ControlKind::RichEdit) {
        if (auto* t = AsText(w)) {
            if ((oldStyle ^ newStyle) & ES_READONLY)
                t->SetEditable((newStyle & ES_READONLY) == 0);
        }
    }
    if (st.kind == ControlKind::OwnerDrawButton || st.kind == ControlKind::Generic || st.kind == ControlKind::StaticFrame)
        w->Refresh();
}

// ---------------------------------------------------------------------------------------------
// Notifications to the parent

void NotifyParent(wxWindow* control, UINT code) {
    wxWindow* parent = LogicalParent(control);
    if (!parent || !IsManagedWindow(parent))
        return;
    DispatchMessageTo(ToHwnd(parent), WM_COMMAND, MAKEWPARAM(ControlId(control), code),
                      reinterpret_cast<LPARAM>(ToHwnd(control)));
}

LRESULT NotifyParentNM(wxWindow* control, NMHDR* hdr) {
    wxWindow* parent = LogicalParent(control);
    if (!parent || !IsManagedWindow(parent))
        return 0;
    hdr->hwndFrom = ToHwnd(control);
    hdr->idFrom = static_cast<UINT_PTR>(ControlId(control));
    return DispatchMessageTo(ToHwnd(parent), WM_NOTIFY, hdr->idFrom, reinterpret_cast<LPARAM>(hdr));
}

void NotifyFocusChange(wxWindow* w, WindowState& st, bool gained) {
    switch (st.kind) {
    case ControlKind::Edit:
    case ControlKind::RichEdit:
        NotifyParent(w, gained ? EN_SETFOCUS : EN_KILLFOCUS);
        break;
    case ControlKind::ComboBox:
    case ControlKind::ComboList:
        NotifyParent(w, gained ? CBN_SETFOCUS : CBN_KILLFOCUS);
        break;
    case ControlKind::ListBox:
        if (st.style & LBS_NOTIFY)
            NotifyParent(w, gained ? LBN_SETFOCUS : LBN_KILLFOCUS);
        break;
    case ControlKind::PushButton:
    case ControlKind::CheckBox:
    case ControlKind::RadioButton:
    case ControlKind::OwnerDrawButton:
        if (st.style & BS_NOTIFY)
            NotifyParent(w, gained ? BN_SETFOCUS : BN_KILLFOCUS);
        if (st.kind == ControlKind::OwnerDrawButton)
            w->Refresh();
        break;
    case ControlKind::ListView: {
        NMHDR hdr;
        hdr.code = gained ? NM_SETFOCUS : NM_KILLFOCUS;
        NotifyParentNM(w, &hdr);
        break;
    }
    default:
        break;
    }
}

bool FilterEditChar(wxWindow* w, WindowState& st, UINT& ch, wxKeyEvent& e) {
    if (st.kind != ControlKind::Edit && st.kind != ControlKind::ComboBox)
        return false;
    if (ch >= 32) {
        if ((st.style & ES_NUMBER) && st.kind == ControlKind::Edit && !(ch >= '0' && ch <= '9')) {
            wxBell();
            return true;
        }
        if (st.style & ES_UPPERCASE)
            ch = static_cast<UINT>(toupper(static_cast<int>(ch)));
        else if (st.style & ES_LOWERCASE)
            ch = static_cast<UINT>(tolower(static_cast<int>(ch)));
        if (st.kind == ControlKind::Edit) {
            EditExtra& ex = Extra<EditExtra>(st);
            auto* t = AsText(w);
            if (t && ex.limit > 0) {
                long from, to;
                t->GetSelection(&from, &to);
                long len = static_cast<long>(FromEditText(st, t->GetValue()).length());
                if (len - (to - from) >= ex.limit) {
                    NotifyParent(w, EN_MAXTEXT);
                    return true;
                }
            }
        }
        if ((st.style & (ES_UPPERCASE | ES_LOWERCASE)) && !HasMessageHandler(st.permanent, WM_CHAR)) {
            if (auto* t = AsText(w)) {
                char a = static_cast<char>(ch);
                t->WriteText(ToWx(&a, 1));
                return true;
            }
        }
    }
    (void)e;
    return false;
}

// ---------------------------------------------------------------------------------------------
// Tab control geometry

int TabHeaderHeight(wxWindow* w) {
    WindowState* st = GetState(w);
    if (st && Extra<TabExtra>(*st).itemSize.y > 0)
        return Extra<TabExtra>(*st).itemSize.y;
    return w->GetCharHeight() + 8;
}

std::vector<wxRect> TabRects(wxWindow* w) {
    std::vector<wxRect> rects;
    WindowState* st = GetState(w);
    if (!st)
        return rects;
    TabExtra& tx = Extra<TabExtra>(*st);
    int tabH = TabHeaderHeight(w);
    int x = 2;
    for (const auto& item : tx.items) {
        int tw = tx.itemSize.x > 0 ? tx.itemSize.x
                                   : w->GetTextExtent(wxStripMenuCodes(item.first, wxStrip_Mnemonics)).x + 16;
        rects.emplace_back(x, 0, tw, tabH);
        x += tw;
    }
    return rects;
}

// ---------------------------------------------------------------------------------------------
// Owner-drawn buttons and statics

namespace {

using PaintFn = void (*)(wxWindow*, wxDC&, CDC*);

std::function<void(wxPaintEvent&)> PaintBridge(wxWindow* w, PaintFn fn) {
    return [w, fn](wxPaintEvent&) {
        wxPaintDC dc(w);
        CDC* cdc = WrapDC(&dc, w);
        fn(w, dc, cdc);
        PaintClientDCOverlay(w, dc);
        UnwrapDC(cdc);
    };
}

void SendDrawItem(wxWindow* w, WindowState& st, CDC* cdc, UINT ctlType) {
    ButtonExtra& bx = Extra<ButtonExtra>(st);
    DRAWITEMSTRUCT dis;
    memset(&dis, 0, sizeof dis);
    dis.CtlType = ctlType;
    dis.CtlID = static_cast<UINT>(st.winId);
    dis.itemID = 0;
    dis.itemAction = ODA_DRAWENTIRE;
    if (bx.pushed)
        dis.itemState |= ODS_SELECTED;
    if (!w->IsEnabled())
        dis.itemState |= ODS_DISABLED | ODS_GRAYED;
    if (w->HasFocus())
        dis.itemState |= ODS_FOCUS;
    dis.hwndItem = ToHwnd(w);
    dis.hDC = cdc->m_hDC;
    wxSize sz = w->GetClientSize();
    dis.rcItem.right = sz.x;
    dis.rcItem.bottom = sz.y;
    wxWindow* parent = LogicalParent(w);
    if (parent && IsManagedWindow(parent))
        DispatchMessageTo(ToHwnd(parent), WM_DRAWITEM, static_cast<WPARAM>(st.winId), reinterpret_cast<LPARAM>(&dis));
}

void PaintOwnerDrawButton(wxWindow* w, wxDC&, CDC* cdc) {
    WindowState* st = GetState(w);
    if (!st)
        return;
    SendDrawItem(w, *st, cdc, ODT_BUTTON);
}

void PaintStaticFrame(wxWindow* w, wxDC& dc, CDC*) {
    WindowState* st = GetState(w);
    if (!st)
        return;
    wxSize sz = w->GetClientSize();
    DWORD type = st->style & SS_TYPEMASK;
    wxColour shadow = wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW);
    wxColour light = wxSystemSettings::GetColour(wxSYS_COLOUR_3DHIGHLIGHT);
    wxColour colour;
    switch (type) {
    case SS_BLACKRECT: case SS_BLACKFRAME: colour = wxColour(0, 0, 0); break;
    case SS_GRAYRECT: case SS_GRAYFRAME: colour = shadow; break;
    case SS_WHITERECT: case SS_WHITEFRAME: colour = wxColour(255, 255, 255); break;
    default: colour = shadow; break;
    }
    switch (type) {
    case SS_BLACKRECT:
    case SS_GRAYRECT:
    case SS_WHITERECT:
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(colour));
        dc.DrawRectangle(0, 0, sz.x, sz.y);
        break;
    case SS_ETCHEDHORZ:
        dc.SetPen(wxPen(shadow));
        dc.DrawLine(0, 0, sz.x, 0);
        dc.SetPen(wxPen(light));
        dc.DrawLine(0, 1, sz.x, 1);
        break;
    case SS_ETCHEDVERT:
        dc.SetPen(wxPen(shadow));
        dc.DrawLine(0, 0, 0, sz.y);
        dc.SetPen(wxPen(light));
        dc.DrawLine(1, 0, 1, sz.y);
        break;
    case SS_ETCHEDFRAME:
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(shadow));
        dc.DrawRectangle(0, 0, sz.x - 1, sz.y - 1);
        dc.SetPen(wxPen(light));
        dc.DrawRectangle(1, 1, sz.x - 1, sz.y - 1);
        break;
    default:
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(colour));
        dc.DrawRectangle(0, 0, sz.x, sz.y);
        break;
    }
}

void PaintGenericStatic(wxWindow* w, wxDC& dc, CDC* cdc) {
    WindowState* st = GetState(w);
    if (!st)
        return;
    if ((st->style & SS_TYPEMASK) == SS_OWNERDRAW) {
        SendDrawItem(w, *st, cdc, ODT_STATIC);
        return;
    }
    wxString text = st->text;
    if (!(st->style & SS_NOPREFIX))
        text = wxStripMenuCodes(text, wxStrip_Mnemonics);
    dc.SetFont(w->GetFont());
    dc.SetTextForeground(w->GetForegroundColour());
    wxSize sz = w->GetClientSize();
    int align = wxALIGN_LEFT;
    if ((st->style & SS_TYPEMASK) == SS_CENTER)
        align = wxALIGN_CENTER_HORIZONTAL;
    else if ((st->style & SS_TYPEMASK) == SS_RIGHT)
        align = wxALIGN_RIGHT;
    if (st->style & SS_CENTERIMAGE)
        align |= wxALIGN_CENTER_VERTICAL;
    dc.DrawLabel(text, wxRect(0, 0, sz.x, sz.y), align);
}

void BindOwnerDrawButton(wxWindow* w) {
    w->Bind(wxEVT_LEFT_DOWN, [w](wxMouseEvent& e) {
        WindowState* st = GetState(w);
        if (st && w->IsEnabled()) {
            Extra<ButtonExtra>(*st).pushed = true;
            w->CaptureMouse();
            w->Refresh();
            if (st->style & WS_TABSTOP)
                w->SetFocus();
        }
        e.Skip();
    });
    w->Bind(wxEVT_LEFT_DCLICK, [w](wxMouseEvent& e) {
        WindowState* st = GetState(w);
        if (st && w->IsEnabled()) {
            Extra<ButtonExtra>(*st).pushed = true;
            if (!w->HasCapture())
                w->CaptureMouse();
            w->Refresh();
        }
        e.Skip();
    });
    w->Bind(wxEVT_LEFT_UP, [w](wxMouseEvent& e) {
        WindowState* st = GetState(w);
        if (w->HasCapture())
            w->ReleaseMouse();
        if (st) {
            ButtonExtra& bx = Extra<ButtonExtra>(*st);
            bool wasPushed = bx.pushed;
            bx.pushed = false;
            w->Refresh();
            if (wasPushed && wxRect(w->GetClientSize()).Contains(e.GetPosition()))
                NotifyParent(w, BN_CLICKED);
        }
        e.Skip();
    });
    w->Bind(wxEVT_MOUSE_CAPTURE_LOST, [w](wxMouseCaptureLostEvent&) {
        WindowState* st = GetState(w);
        if (st) {
            Extra<ButtonExtra>(*st).pushed = false;
            w->Refresh();
        }
    });
    w->Bind(wxEVT_KEY_UP, [w](wxKeyEvent& e) {
        if (e.GetKeyCode() == WXK_SPACE)
            NotifyParent(w, BN_CLICKED);
        else
            e.Skip();
    });
}

void BindStaticNotify(wxWindow* w) {
    w->Bind(wxEVT_LEFT_DOWN, [w](wxMouseEvent& e) {
        WindowState* st = GetState(w);
        if (st && (st->style & SS_NOTIFY))
            NotifyParent(w, STN_CLICKED);
        e.Skip();
    });
    w->Bind(wxEVT_LEFT_DCLICK, [w](wxMouseEvent& e) {
        WindowState* st = GetState(w);
        if (st && (st->style & SS_NOTIFY))
            NotifyParent(w, STN_DBLCLK);
        e.Skip();
    });
}

// ---------------------------------------------------------------------------------------------
// Tab control (header only, as in Windows)

void PaintTabs(wxWindow* w, wxDC& dc, CDC*) {
    WindowState* st = GetState(w);
    if (!st)
        return;
    TabExtra& tx = Extra<TabExtra>(*st);
    dc.SetFont(w->GetFont());
    wxSize sz = w->GetClientSize();
    int tabH = TabHeaderHeight(w);
    wxColour shadow = wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW);
    dc.SetPen(wxPen(shadow));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(0, tabH - 1, sz.x, sz.y - tabH + 1);
    std::vector<wxRect> rects = TabRects(w);
    for (size_t i = 0; i < rects.size() && i < tx.items.size(); ++i) {
        wxString label = wxStripMenuCodes(tx.items[i].first, wxStrip_Mnemonics);
        bool sel = static_cast<int>(i) == tx.current;
        wxRect r = rects[i];
        if (!sel) {
            r.y += 2;
            r.height -= 2;
        }
        dc.SetBrush(sel ? wxBrush(w->GetBackgroundColour()) : wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE)));
        dc.SetPen(wxPen(shadow));
        dc.DrawRectangle(r);
        if (sel) {
            dc.SetPen(wxPen(w->GetBackgroundColour()));
            dc.DrawLine(r.x + 1, r.GetBottom(), r.GetRight(), r.GetBottom());
        }
        dc.SetTextForeground(w->GetForegroundColour());
        dc.DrawLabel(label, r, wxALIGN_CENTER);
    }
}

int TabHitTest(wxWindow* w, const wxPoint& p) {
    std::vector<wxRect> rects = TabRects(w);
    for (size_t i = 0; i < rects.size(); ++i)
        if (rects[i].Contains(p))
            return static_cast<int>(i);
    return -1;
}

void BindTabs(wxWindow* w) {
    w->Bind(wxEVT_LEFT_DOWN, [w](wxMouseEvent& e) {
        int hit = TabHitTest(w, e.GetPosition());
        WindowState* st = GetState(w);
        if (hit >= 0 && st && hit != Extra<TabExtra>(*st).current) {
            NMHDR hdr;
            hdr.code = TCN_SELCHANGING;
            if (!NotifyParentNM(w, &hdr)) {
                Extra<TabExtra>(*st).current = hit;
                w->Refresh();
                NMHDR hdr2;
                hdr2.code = TCN_SELCHANGE;
                NotifyParentNM(w, &hdr2);
            }
        }
        e.Skip();
    });
}

// ---------------------------------------------------------------------------------------------
// Spin buttons

RangeExtra& SpinRange(WindowState& st) { return Extra<RangeExtra>(st); }

void UpdateSpinBuddy(wxWindow* w, WindowState& st) {
    if (!(st.style & UDS_SETBUDDYINT) || !st.buddy || !st.buddy->m_hWnd)
        return;
    RangeExtra& r = SpinRange(st);
    wxString text = wxString::Format("%d", r.pos);
    if (!(st.style & UDS_NOTHOUSANDS) && std::abs(r.pos) >= 1000) {
        text = wxString::Format("%d", r.pos);
    }
    SetControlText(st.buddy->GetWx(), text);
    (void)w;
}

int ReadSpinBuddy(WindowState& st) {
    RangeExtra& r = SpinRange(st);
    if ((st.style & UDS_SETBUDDYINT) && st.buddy && st.buddy->m_hWnd) {
        long v;
        if (GetControlText(st.buddy->GetWx()).ToLong(&v))
            r.pos = static_cast<int>(v);
    }
    return r.pos;
}

void SpinStep(wxWindow* w, int direction) {
    WindowState* st = GetState(w);
    if (!st)
        return;
    RangeExtra& r = SpinRange(*st);
    int pos = ReadSpinBuddy(*st);
    int lo = std::min(r.minValue, r.maxValue);
    int hi = std::max(r.minValue, r.maxValue);
    int delta = r.minValue > r.maxValue ? -direction : direction;
    NMUPDOWN nm;
    memset(&nm, 0, sizeof nm);
    nm.hdr.code = UDN_DELTAPOS;
    nm.iPos = pos;
    nm.iDelta = delta;
    if (NotifyParentNM(w, &nm.hdr))
        return;
    int next = pos + nm.iDelta;
    if (next > hi)
        next = (st->style & UDS_WRAP) ? lo : hi;
    if (next < lo)
        next = (st->style & UDS_WRAP) ? hi : lo;
    r.pos = next;
    UpdateSpinBuddy(w, *st);
    wxWindow* parent = LogicalParent(w);
    if (parent && IsManagedWindow(parent)) {
        UINT msg = (st->style & UDS_HORZ) ? WM_HSCROLL : WM_VSCROLL;
        DispatchMessageTo(ToHwnd(parent), msg, MAKEWPARAM(SB_THUMBPOSITION, static_cast<WORD>(next)),
                          reinterpret_cast<LPARAM>(ToHwnd(w)));
        DispatchMessageTo(ToHwnd(parent), msg, MAKEWPARAM(SB_ENDSCROLL, static_cast<WORD>(next)),
                          reinterpret_cast<LPARAM>(ToHwnd(w)));
    }
}

// ---------------------------------------------------------------------------------------------
// Scroll bars and sliders

UINT ScrollCodeFromEvent(const wxScrollEvent& e) {
    wxEventType t = e.GetEventType();
    if (t == wxEVT_SCROLL_LINEUP)
        return SB_LINEUP;
    if (t == wxEVT_SCROLL_LINEDOWN)
        return SB_LINEDOWN;
    if (t == wxEVT_SCROLL_PAGEUP)
        return SB_PAGEUP;
    if (t == wxEVT_SCROLL_PAGEDOWN)
        return SB_PAGEDOWN;
    if (t == wxEVT_SCROLL_THUMBTRACK)
        return SB_THUMBTRACK;
    if (t == wxEVT_SCROLL_THUMBRELEASE)
        return SB_THUMBPOSITION;
    if (t == wxEVT_SCROLL_TOP)
        return SB_TOP;
    if (t == wxEVT_SCROLL_BOTTOM)
        return SB_BOTTOM;
    return SB_ENDSCROLL;
}

void ForwardScroll(wxScrollEvent& e) {
    wxWindow* w = static_cast<wxWindow*>(e.GetEventObject());
    WindowState* st = GetState(w);
    wxWindow* parent = LogicalParent(w);
    if (!st || !parent || !IsManagedWindow(parent))
        return;
    bool vertical = e.GetOrientation() == wxVERTICAL;
    UINT msg = vertical ? WM_VSCROLL : WM_HSCROLL;
    UINT code = ScrollCodeFromEvent(e);
    int pos = e.GetPosition();
    if (st->kind == ControlKind::Slider) {
        RangeExtra& r = Extra<RangeExtra>(*st);
        r.pos = pos;
    } else if (st->kind == ControlKind::ScrollBar) {
        pos += Extra<RangeExtra>(*st).minValue;
    }
    DispatchMessageTo(ToHwnd(parent), msg, MAKEWPARAM(code, static_cast<WORD>(pos)), reinterpret_cast<LPARAM>(ToHwnd(w)));
    if (e.GetEventType() == wxEVT_SCROLL_CHANGED || e.GetEventType() == wxEVT_SCROLL_THUMBRELEASE)
        DispatchMessageTo(ToHwnd(parent), msg, MAKEWPARAM(SB_ENDSCROLL, static_cast<WORD>(pos)),
                          reinterpret_cast<LPARAM>(ToHwnd(w)));
}

} // namespace

// ---------------------------------------------------------------------------------------------
// Event binding for standard controls

void BindControlEvents(wxWindow* w, WindowState& st) {
    switch (st.kind) {
    case ControlKind::PushButton:
        w->Bind(wxEVT_BUTTON, [w](wxCommandEvent&) { NotifyParent(w, BN_CLICKED); });
        w->Bind(wxEVT_TOGGLEBUTTON, [w](wxCommandEvent&) { NotifyParent(w, BN_CLICKED); });
        break;
    case ControlKind::CheckBox:
        w->Bind(wxEVT_CHECKBOX, [w](wxCommandEvent&) {
            WindowState* s = GetState(w);
            if (s) {
                DWORD type = s->style & BS_TYPEMASK;
                if (type == BS_CHECKBOX || type == BS_3STATE) {
                    // Non-automatic check boxes keep their state; the application changes it.
                    auto* cb = wxDynamicCast(w, wxCheckBox);
                    if (cb)
                        cb->Set3StateValue(s->checkState == 2 ? wxCHK_UNDETERMINED
                                                              : (s->checkState ? wxCHK_CHECKED : wxCHK_UNCHECKED));
                } else if (auto* cb = wxDynamicCast(w, wxCheckBox)) {
                    s->checkState = cb->Get3StateValue() == wxCHK_UNDETERMINED ? 2 : (cb->GetValue() ? 1 : 0);
                }
            }
            NotifyParent(w, BN_CLICKED);
        });
        w->Bind(wxEVT_TOGGLEBUTTON, [w](wxCommandEvent&) { NotifyParent(w, BN_CLICKED); });
        break;
    case ControlKind::RadioButton:
        w->Bind(wxEVT_RADIOBUTTON, [w](wxCommandEvent&) {
            WindowState* s = GetState(w);
            if (s && (s->style & BS_TYPEMASK) == BS_RADIOBUTTON) {
                if (auto* rb = wxDynamicCast(w, wxRadioButton))
                    if (!s->checkState)
                        rb->SetValue(false);
            }
            NotifyParent(w, BN_CLICKED);
        });
        break;
    case ControlKind::OwnerDrawButton:
        st.customPaint = PaintBridge(w, &PaintOwnerDrawButton);
        BindOwnerDrawButton(w);
        break;
    case ControlKind::StaticFrame:
        st.customPaint = PaintBridge(w, &PaintStaticFrame);
        BindStaticNotify(w);
        break;
    case ControlKind::Static:
    case ControlKind::StaticBitmap:
        BindStaticNotify(w);
        break;
    case ControlKind::Edit:
    case ControlKind::RichEdit:
        w->Bind(wxEVT_TEXT, [w](wxCommandEvent& e) {
            WindowState* s = GetState(w);
            if (s) {
                Extra<EditExtra>(*s).modified = true;
                NotifyParent(w, EN_UPDATE);
                NotifyParent(w, EN_CHANGE);
            }
            e.Skip(false);
        });
        w->Bind(wxEVT_TEXT_MAXLEN, [w](wxCommandEvent&) { NotifyParent(w, EN_MAXTEXT); });
        break;
    case ControlKind::ComboBox:
        w->Bind(wxEVT_TEXT, [w](wxCommandEvent&) {
            WindowState* s = GetState(w);
            if (s && !s->settingText) {
                NotifyParent(w, CBN_EDITUPDATE);
                NotifyParent(w, CBN_EDITCHANGE);
            }
        });
        w->Bind(wxEVT_COMBOBOX, [w](wxCommandEvent&) {
            NotifyParent(w, CBN_SELCHANGE);
            NotifyParent(w, CBN_SELENDOK);
        });
        w->Bind(wxEVT_COMBOBOX_DROPDOWN, [w](wxCommandEvent&) { NotifyParent(w, CBN_DROPDOWN); });
        w->Bind(wxEVT_COMBOBOX_CLOSEUP, [w](wxCommandEvent&) { NotifyParent(w, CBN_CLOSEUP); });
        break;
    case ControlKind::ComboList:
        w->Bind(wxEVT_CHOICE, [w](wxCommandEvent&) {
            NotifyParent(w, CBN_SELCHANGE);
            NotifyParent(w, CBN_SELENDOK);
            NotifyParent(w, CBN_CLOSEUP);
        });
        break;
    case ControlKind::ListBox:
        w->Bind(wxEVT_LISTBOX, [w](wxCommandEvent&) {
            WindowState* s = GetState(w);
            if (s && (s->style & LBS_NOTIFY))
                NotifyParent(w, LBN_SELCHANGE);
        });
        w->Bind(wxEVT_LISTBOX_DCLICK, [w](wxCommandEvent&) {
            WindowState* s = GetState(w);
            if (s && (s->style & LBS_NOTIFY))
                NotifyParent(w, LBN_DBLCLK);
        });
        break;
    case ControlKind::ScrollBar:
    case ControlKind::Slider:
        w->Bind(wxEVT_SCROLL_TOP, &ForwardScroll);
        w->Bind(wxEVT_SCROLL_BOTTOM, &ForwardScroll);
        w->Bind(wxEVT_SCROLL_LINEUP, &ForwardScroll);
        w->Bind(wxEVT_SCROLL_LINEDOWN, &ForwardScroll);
        w->Bind(wxEVT_SCROLL_PAGEUP, &ForwardScroll);
        w->Bind(wxEVT_SCROLL_PAGEDOWN, &ForwardScroll);
        w->Bind(wxEVT_SCROLL_THUMBTRACK, &ForwardScroll);
        w->Bind(wxEVT_SCROLL_THUMBRELEASE, &ForwardScroll);
        w->Bind(wxEVT_SCROLL_CHANGED, &ForwardScroll);
        if (st.kind == ControlKind::Slider)
            w->Bind(wxEVT_LEFT_UP, [w](wxMouseEvent& e) {
                NMHDR hdr;
                hdr.code = NM_RELEASEDCAPTURE;
                NotifyParentNM(w, &hdr);
                e.Skip();
            });
        break;
    case ControlKind::Spin:
        w->Bind(wxEVT_SPIN_UP, [w](wxSpinEvent& e) {
            SpinStep(w, +1);
            e.Veto();
        });
        w->Bind(wxEVT_SPIN_DOWN, [w](wxSpinEvent& e) {
            SpinStep(w, -1);
            e.Veto();
        });
        break;
    case ControlKind::ListView:
        BindListViewEvents(w);
        break;
    case ControlKind::Tab:
        st.customPaint = PaintBridge(w, &PaintTabs);
        BindTabs(w);
        break;
    case ControlKind::Scintilla:
        BindScintillaEvents(w);
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------------------------
// Creation

namespace {

wxFont DialogFont();

bool ClassIs(const char* cls, const char* name) { return cls && strcasecmp(cls, name) == 0; }

long BorderFlags(DWORD style, DWORD exStyle, bool defaultBorder) {
    if (exStyle & (WS_EX_CLIENTEDGE | WS_EX_STATICEDGE))
        return wxBORDER_SUNKEN;
    if (style & WS_BORDER)
        return wxBORDER_SIMPLE;
    return defaultBorder ? wxBORDER_DEFAULT : wxBORDER_NONE;
}

} // namespace

wxWindow* CreateControl(wxWindow* parent, const char* cls, const char* text, int winId, const wxRect& rect,
                        DWORD style, DWORD exStyle) {
    const int id = ToWxId(winId);
    const wxPoint pos = rect.GetPosition();
    const wxSize size = rect.GetSize();
    wxWindow* w = nullptr;
    ControlKind kind = ControlKind::Generic;
    wxString label = AnsiText(text);

    if (ClassIs(cls, "Button")) {
        DWORD type = style & BS_TYPEMASK;
        if (type == BS_GROUPBOX) {
            w = new wxStaticBox(parent, id, label, pos, size);
            kind = ControlKind::GroupBox;
        } else if (type == BS_OWNERDRAW) {
            w = new GenericWindow(parent, id, pos, size, wxBORDER_NONE);
            kind = ControlKind::OwnerDrawButton;
        } else if (type == BS_CHECKBOX || type == BS_AUTOCHECKBOX || type == BS_3STATE || type == BS_AUTO3STATE) {
            if (style & BS_PUSHLIKE) {
                w = new wxToggleButton(parent, id, label, pos, size);
            } else {
                long f = (type == BS_3STATE || type == BS_AUTO3STATE) ? wxCHK_3STATE | wxCHK_ALLOW_3RD_STATE_FOR_USER
                                                                      : wxCHK_2STATE;
                if (style & BS_LEFTTEXT)
                    f |= wxALIGN_RIGHT;
                w = new wxCheckBox(parent, id, label, pos, size, f);
            }
            kind = ControlKind::CheckBox;
        } else if (type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON) {
            long f = 0;
            if (style & WS_GROUP)
                f |= wxRB_GROUP;
            else {
                wxWindowList& siblings = parent->GetChildren();
                for (auto it = siblings.rbegin(); it != siblings.rend(); ++it) {
                    WindowState* s = GetState(*it);
                    if (!s)
                        continue;
                    if (s->kind == ControlKind::RadioButton)
                        break;
                    if (s->style & WS_GROUP) {
                        f |= wxRB_GROUP;
                        break;
                    }
                }
            }
            w = new wxRadioButton(parent, id, label, pos, size, f);
            kind = ControlKind::RadioButton;
        } else {
            long f = 0;
            if ((style & BS_CENTER) == BS_LEFT)
                f |= wxBU_LEFT;
            else if ((style & BS_CENTER) == BS_RIGHT)
                f |= wxBU_RIGHT;
            if ((style & (BS_BITMAP | BS_ICON)) && label.empty())
                f |= wxBU_EXACTFIT | wxBU_NOTEXT;
            w = new wxButton(parent, id, label, pos, size, f);
            kind = ControlKind::PushButton;
        }
    } else if (ClassIs(cls, "Edit") || ClassIs(cls, "RICHEDIT") || ClassIs(cls, "RichEdit20A") ||
               ClassIs(cls, "RichEdit20W") || ClassIs(cls, "RICHEDIT50W")) {
        bool rich = !ClassIs(cls, "Edit");
        long f = BorderFlags(style, exStyle, false);
        if ((style & ES_MULTILINE) || rich) {
            f |= wxTE_MULTILINE;
            if (style & ES_AUTOHSCROLL || style & WS_HSCROLL)
                f |= wxTE_DONTWRAP | wxHSCROLL;
            else
                f |= wxTE_WORDWRAP;
            if (!(style & WS_VSCROLL) && !rich)
                f |= wxTE_NO_VSCROLL;
        }
        if (rich)
            f |= wxTE_RICH2;
        if (style & ES_READONLY)
            f |= wxTE_READONLY;
        if (style & ES_PASSWORD)
            f |= wxTE_PASSWORD;
        if ((style & 3) == ES_CENTER)
            f |= wxTE_CENTRE;
        else if ((style & 3) == ES_RIGHT)
            f |= wxTE_RIGHT;
        if (style & ES_NOHIDESEL)
            f |= wxTE_NOHIDESEL;
        w = new wxTextCtrl(parent, id, wxString(), pos, size, f);
        kind = rich ? ControlKind::RichEdit : ControlKind::Edit;
        if (!label.empty()) {
            static_cast<wxTextCtrl*>(w)->ChangeValue(label);
        }
    } else if (ClassIs(cls, "Static")) {
        DWORD type = style & SS_TYPEMASK;
        if (type == SS_BITMAP || type == SS_ICON) {
            wxBitmap bmp;
            if (type == SS_BITMAP)
                bmp = LoadBitmapResource(ResRef::From(text));
            else {
                wxIcon icon = LoadIconResource(ResRef::From(text));
                if (icon.IsOk())
                    bmp.CopyFromIcon(icon);
            }
            wxSize s = size;
            if (bmp.IsOk() && !(style & SS_REALSIZECONTROL) && !(style & SS_CENTERIMAGE))
                s = bmp.GetSize();
            w = new wxStaticBitmap(parent, id, bmp.IsOk() ? bmp : wxBitmap(1, 1), pos, s);
            kind = ControlKind::StaticBitmap;
        } else if ((type >= SS_BLACKRECT && type <= SS_WHITEFRAME) || type == SS_ETCHEDHORZ || type == SS_ETCHEDVERT ||
                   type == SS_ETCHEDFRAME) {
            w = new GenericWindow(parent, id, pos, size, wxBORDER_NONE);
            kind = ControlKind::StaticFrame;
        } else if (type == SS_OWNERDRAW || type == SS_USERITEM) {
            w = new GenericWindow(parent, id, pos, size, wxBORDER_NONE);
            kind = ControlKind::Generic;
            EnsureState(w).customPaint = PaintBridge(w, &PaintGenericStatic);
        } else {
            long f = wxST_NO_AUTORESIZE | BorderFlags(style, exStyle, false);
            if (style & SS_SUNKEN)
                f |= wxBORDER_SUNKEN;
            if (type == SS_CENTER)
                f |= wxALIGN_CENTRE_HORIZONTAL;
            else if (type == SS_RIGHT)
                f |= wxALIGN_RIGHT;
            if (style & SS_ENDELLIPSIS)
                f |= wxST_ELLIPSIZE_END;
            w = new wxStaticText(parent, id, wxString(), pos, size, f);
            kind = ControlKind::Static;
        }
    } else if (ClassIs(cls, "ComboBox")) {
        DWORD type = style & 3;
        long f = (style & CBS_SORT) ? wxCB_SORT : 0;
        if (type == CBS_DROPDOWNLIST) {
            w = new wxChoice(parent, id, pos, wxSize(size.x, -1), 0, nullptr, f);
            kind = ControlKind::ComboList;
        } else {
            w = new wxComboBox(parent, id, wxString(), pos, wxSize(size.x, -1), 0, nullptr, f);
            kind = ControlKind::ComboBox;
        }
    } else if (ClassIs(cls, "ListBox")) {
        long f = BorderFlags(style, exStyle, true);
        if (style & LBS_MULTIPLESEL)
            f |= wxLB_MULTIPLE;
        else if (style & LBS_EXTENDEDSEL)
            f |= wxLB_EXTENDED;
        else
            f |= wxLB_SINGLE;
        if (style & LBS_SORT)
            f |= wxLB_SORT;
        if (style & WS_HSCROLL)
            f |= wxLB_HSCROLL;
        if (style & LBS_DISABLENOSCROLL)
            f |= wxLB_ALWAYS_SB;
        w = new wxListBox(parent, id, pos, size, 0, nullptr, f);
        kind = ControlKind::ListBox;
    } else if (ClassIs(cls, "ScrollBar")) {
        w = new wxScrollBar(parent, id, pos, size, (style & SBS_VERT) ? wxSB_VERTICAL : wxSB_HORIZONTAL);
        kind = ControlKind::ScrollBar;
    } else if (ClassIs(cls, "msctls_progress32")) {
        w = new wxGauge(parent, id, 100, pos, size, (style & PBS_VERTICAL) ? wxGA_VERTICAL : wxGA_HORIZONTAL);
        kind = ControlKind::Progress;
    } else if (ClassIs(cls, "msctls_trackbar32")) {
        long f = (style & TBS_VERT) ? wxSL_VERTICAL : wxSL_HORIZONTAL;
        if (style & TBS_AUTOTICKS)
            f |= wxSL_AUTOTICKS;
        if (style & TBS_BOTH)
            f |= wxSL_BOTH;
        else if (style & TBS_TOP)
            f |= (style & TBS_VERT) ? wxSL_LEFT : wxSL_TOP;
        w = new wxSlider(parent, id, 0, 0, 100, pos, size, f);
        kind = ControlKind::Slider;
    } else if (ClassIs(cls, "msctls_updown32")) {
        long f = (style & UDS_HORZ) ? wxSP_HORIZONTAL : wxSP_VERTICAL;
        f |= wxSP_ARROW_KEYS;
        if (style & UDS_WRAP)
            f |= wxSP_WRAP;
        w = new wxSpinButton(parent, id, pos, size, f);
        static_cast<wxSpinButton*>(w)->SetRange(-1000000, 1000000);
        kind = ControlKind::Spin;
    } else if (ClassIs(cls, "SysListView32")) {
        long f = BorderFlags(style, exStyle, true);
        switch (style & LVS_TYPEMASK) {
        case LVS_REPORT: f |= wxLC_REPORT; break;
        case LVS_LIST: f |= wxLC_LIST; break;
        case LVS_SMALLICON: f |= wxLC_SMALL_ICON; break;
        default: f |= wxLC_ICON; break;
        }
        if (style & LVS_SINGLESEL)
            f |= wxLC_SINGLE_SEL;
        if (style & LVS_NOCOLUMNHEADER)
            f |= wxLC_NO_HEADER;
        if (style & LVS_EDITLABELS)
            f |= wxLC_EDIT_LABELS;
        if (style & LVS_SORTASCENDING)
            f |= wxLC_SORT_ASCENDING;
        else if (style & LVS_SORTDESCENDING)
            f |= wxLC_SORT_DESCENDING;
        w = new wxListCtrl(parent, id, pos, size, f);
        kind = ControlKind::ListView;
    } else if (ClassIs(cls, "SysTreeView32")) {
        long f = BorderFlags(style, exStyle, true) | wxTR_DEFAULT_STYLE;
        if (style & TVS_HASLINES)
            f |= wxTR_LINES_AT_ROOT;
        w = new wxTreeCtrl(parent, id, pos, size, f);
        kind = ControlKind::TreeView;
    } else if (ClassIs(cls, "SysTabControl32")) {
        w = new GenericWindow(parent, id, pos, size, wxBORDER_NONE);
        kind = ControlKind::Tab;
    } else if (ClassIs(cls, "Scintilla")) {
        w = new wxStyledTextCtrl(parent, id, pos, size, BorderFlags(style, exStyle, false));
        kind = ControlKind::Scintilla;
    } else {
        long f = BorderFlags(style, exStyle, false);
        w = new GenericWindow(parent, id, pos, size, f);
        kind = ControlKind::Generic;
    }

    HookWindow(w, kind, winId, style, exStyle);
    WindowState& st = EnsureState(w);
    if (kind == ControlKind::Static || kind == ControlKind::Generic || kind == ControlKind::OwnerDrawButton ||
        kind == ControlKind::StaticFrame || kind == ControlKind::StaticBitmap) {
        st.text = label;
        if (kind == ControlKind::Static)
            WrapStatic(w, st);
    }
    if (kind == ControlKind::Static || kind == ControlKind::GroupBox) {
        // Statics are never in the tab order.
        st.style &= ~WS_TABSTOP;
    }
    if (kind == ControlKind::Slider) {
        Extra<RangeExtra>(st).maxValue = 100;
    }
    if (style & WS_DISABLED)
        w->Disable();
    if (!(style & WS_VISIBLE))
        w->Hide();
    return w;
}

wxWindow* CreateWindowForClass(CWnd* pWnd, CREATESTRUCT& cs) {
    wxWindow* parent = cs.hwndParent ? ToWx(cs.hwndParent) : nullptr;
    const char* cls = cs.lpszClass && !IS_INTRESOURCE(cs.lpszClass) ? cs.lpszClass : "";
    DWORD style = static_cast<DWORD>(cs.style);
    bool child = (style & WS_CHILD) != 0;
    if (!child || !parent) {
        long f = wxDEFAULT_FRAME_STYLE;
        if (!(style & WS_THICKFRAME))
            f &= ~wxRESIZE_BORDER;
        if (!(style & WS_CAPTION))
            f &= ~wxCAPTION;
        wxFrame* frame = new wxFrame(parent ? wxGetTopLevelParent(parent) : nullptr, wxID_ANY, AnsiText(cs.lpszName),
                                     cs.x == CW_USEDEFAULT ? wxDefaultPosition : wxPoint(cs.x, cs.y),
                                     cs.cx == CW_USEDEFAULT ? wxDefaultSize : wxSize(cs.cx, cs.cy),
                                     f | (style & WS_POPUP ? wxFRAME_TOOL_WINDOW | wxFRAME_FLOAT_ON_PARENT : 0));
        HookWindow(frame, ControlKind::Frame, 0, style, cs.dwExStyle);
        EnsureState(frame).text = AnsiText(cs.lpszName);
        return frame;
    }
    int winId = static_cast<int>(reinterpret_cast<UINT_PTR>(cs.hMenu));
    wxRect rect(cs.x, cs.y, cs.cx, cs.cy);
    static const char* const standard[] = {"Button", "Edit", "Static", "ComboBox", "ListBox", "ScrollBar",
                                           "msctls_progress32", "msctls_trackbar32", "msctls_updown32",
                                           "SysListView32", "SysTreeView32", "SysTabControl32", "Scintilla",
                                           "RICHEDIT", "RichEdit20A", "RichEdit20W"};
    for (const char* s : standard)
        if (ClassIs(cls, s))
            return CreateControl(parent, cls, cs.lpszName, winId, rect, style, cs.dwExStyle);
    long f = BorderFlags(style, cs.dwExStyle, false);
    if (style & WS_VSCROLL)
        f |= wxVSCROLL;
    if (style & WS_HSCROLL)
        f |= wxHSCROLL;
    if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CView)))
        f |= wxWANTS_CHARS;
    auto* w = new GenericWindow(parent, ToWxId(winId), rect.GetPosition(), rect.GetSize(), f);
    HookWindow(w, ControlKind::Generic, winId, style, cs.dwExStyle);
    EnsureState(w).text = AnsiText(cs.lpszName);
    if (!(style & WS_VISIBLE))
        w->Hide();
    if (style & WS_DISABLED)
        w->Disable();
    return w;
}

namespace {

wxWindow* ReplaceWithCheckListBox(wxListBox* lb, WindowState& st) {
    auto* c = new wxCheckListBox(lb->GetParent(), lb->GetId(), lb->GetPosition(), lb->GetSize(), 0, nullptr,
                                 lb->GetWindowStyleFlag());
    c->MoveAfterInTabOrder(lb);
    c->SetFont(lb->GetFont());
    for (unsigned i = 0; i < lb->GetCount(); ++i)
        c->Append(lb->GetString(i), lb->HasClientUntypedData() ? lb->GetClientData(i) : nullptr);
    WindowState& cs = EnsureState(c);
    HookWindow(c, ControlKind::ListBox, st.winId, st.style, st.exStyle);
    cs.font = st.font;
    c->Bind(wxEVT_CHECKLISTBOX, [c](wxCommandEvent&) { NotifyParent(c, CLBN_CHKCHANGE); });
    c->Show(lb->IsShown());
    c->Enable(lb->IsThisEnabled());
    lb->Destroy();
    return c;
}

} // namespace

wxWindow* PrepareForSubclass(wxWindow* w, CWnd* pWnd) {
    WindowState* st = GetState(w);
    if (!st || !pWnd)
        return w;
    if (st->kind == ControlKind::ListBox && pWnd->IsKindOf(RUNTIME_CLASS(CCheckListBox)) &&
        !wxDynamicCast(w, wxCheckListBox) && wxDynamicCast(w, wxListBox))
        return ReplaceWithCheckListBox(static_cast<wxListBox*>(w), *st);
    bool paints = HasMessageHandler(pWnd, WM_PAINT) || HasMessageHandler(pWnd, WM_ERASEBKGND);
    bool nativeStatic = st->kind == ControlKind::Static || st->kind == ControlKind::StaticBitmap;
    bool ownerDrawStatic = st->kind == ControlKind::Static && (st->style & SS_TYPEMASK) == SS_OWNERDRAW;
    if (!(paints && nativeStatic) && !ownerDrawStatic)
        return w;
    wxWindow* parent = w->GetParent();
    wxRect rect = w->GetRect();
    auto* g = new GenericWindow(parent, w->GetId(), rect.GetPosition(), rect.GetSize(), wxBORDER_NONE);
    g->MoveAfterInTabOrder(w);
    g->SetFont(w->GetFont());
    if (w->GetForegroundColour().IsOk())
        g->SetForegroundColour(w->GetForegroundColour());
    WindowState& gs = EnsureState(g);
    HookWindow(g, ControlKind::Generic, st->winId, st->style, st->exStyle);
    gs.text = st->text;
    gs.font = st->font;
    if (!HasMessageHandler(pWnd, WM_PAINT))
        gs.customPaint = PaintBridge(g, &PaintGenericStatic);
    g->Show(w->IsShown());
    g->Enable(w->IsThisEnabled());
    w->Destroy();
    return g;
}

// ---------------------------------------------------------------------------------------------
// Control message processing

namespace {

int FirstVisibleLine(wxTextCtrl* t) {
    wxTextCoord col = 0, row = 0;
    if (t->HitTest(wxPoint(2, 2), &col, &row) != wxTE_HT_UNKNOWN && row > 0)
        return static_cast<int>(row);
    return 0;
}

LRESULT EditProc(wxTextCtrl* t, WindowState& st, UINT msg, WPARAM wParam, LPARAM lParam, bool& handled) {
    handled = true;
    EditExtra& ex = Extra<EditExtra>(st);
    switch (msg) {
    case EM_GETSEL: {
        long from, to;
        t->GetSelection(&from, &to);
        from = WxToWinPos(t, st, from);
        to = WxToWinPos(t, st, to);
        if (wParam)
            *reinterpret_cast<DWORD*>(wParam) = static_cast<DWORD>(from);
        if (lParam)
            *reinterpret_cast<DWORD*>(lParam) = static_cast<DWORD>(to);
        return MAKELRESULT(from, to);
    }
    case EM_SETSEL: {
        long from = static_cast<long>(static_cast<int>(wParam));
        long to = static_cast<long>(static_cast<int>(lParam));
        long len = t->GetLastPosition();
        if (from < 0) {
            t->SetInsertionPoint(t->GetInsertionPoint());
            t->SelectNone();
            return 0;
        }
        from = WinToWxPos(t, st, from);
        to = to < 0 ? len : WinToWxPos(t, st, to);
        t->SetSelection(std::min(from, len), std::min(to, len));
        return 0;
    }
    case EM_EXSETSEL: {
        auto* cr = reinterpret_cast<CHARRANGE*>(lParam);
        long len = t->GetLastPosition();
        long to = cr->cpMax < 0 ? len : WinToWxPos(t, st, cr->cpMax);
        t->SetSelection(WinToWxPos(t, st, cr->cpMin), std::min(to, len));
        return 0;
    }
    case EM_EXGETSEL: {
        long from, to;
        t->GetSelection(&from, &to);
        auto* cr = reinterpret_cast<CHARRANGE*>(lParam);
        cr->cpMin = WxToWinPos(t, st, from);
        cr->cpMax = WxToWinPos(t, st, to);
        return 0;
    }
    case EM_REPLACESEL:
        t->WriteText(ToEditText(st, AnsiText(reinterpret_cast<const char*>(lParam))));
        return 0;
    case EM_GETMODIFY:
        return ex.modified || t->IsModified();
    case EM_SETMODIFY:
        ex.modified = wParam != 0;
        if (!wParam)
            t->DiscardEdits();
        else
            t->MarkDirty();
        return 0;
    case EM_GETLINECOUNT:
        return std::max(1, t->GetNumberOfLines());
    case EM_LINEINDEX: {
        int line = static_cast<int>(static_cast<int>(wParam));
        if (line < 0) {
            long x, y;
            t->PositionToXY(t->GetInsertionPoint(), &x, &y);
            line = static_cast<int>(y);
        }
        if (line >= t->GetNumberOfLines())
            return -1;
        return WxToWinPos(t, st, t->XYToPosition(0, line));
    }
    case EM_LINELENGTH: {
        long x, y;
        long pos = static_cast<int>(wParam) < 0 ? t->GetInsertionPoint() : WinToWxPos(t, st, static_cast<long>(wParam));
        if (!t->PositionToXY(pos, &x, &y))
            return 0;
        return t->GetLineLength(y);
    }
    case EM_LINEFROMCHAR:
    case EM_EXLINEFROMCHAR: {
        long pos = static_cast<int>(wParam) < 0 ? t->GetInsertionPoint() : WinToWxPos(t, st, static_cast<long>(msg == EM_EXLINEFROMCHAR ? lParam : static_cast<LPARAM>(wParam)));
        long x, y;
        if (!t->PositionToXY(pos, &x, &y))
            return std::max(0, t->GetNumberOfLines() - 1);
        return y;
    }
    case EM_GETLINE: {
        int line = static_cast<int>(wParam);
        char* buf = reinterpret_cast<char*>(lParam);
        WORD max = *reinterpret_cast<WORD*>(buf);
        if (line >= t->GetNumberOfLines())
            return 0;
        std::string a = FromWx(t->GetLineText(line));
        size_t n = std::min<size_t>(a.size(), max);
        memcpy(buf, a.data(), n);
        return static_cast<LRESULT>(n);
    }
    case EM_LIMITTEXT:
        ex.limit = static_cast<int>(wParam);
        if (!(st.style & ES_MULTILINE))
            t->SetMaxLength(static_cast<unsigned long>(wParam));
        return 0;
    case EM_GETLIMITTEXT:
        return ex.limit > 0 ? ex.limit : 30000;
    case EM_EXLIMITTEXT:
        ex.limit = static_cast<int>(lParam);
        return 0;
    case EM_CANUNDO:
        return t->CanUndo();
    case EM_UNDO:
    case WM_UNDO:
        t->Undo();
        return TRUE;
    case EM_REDO:
        t->Redo();
        return TRUE;
    case EM_CANREDO:
        return t->CanRedo();
    case EM_EMPTYUNDOBUFFER:
        return 0;
    case EM_SETREADONLY:
        t->SetEditable(wParam == 0);
        if (wParam)
            st.style |= ES_READONLY;
        else
            st.style &= ~ES_READONLY;
        return TRUE;
    case EM_SETPASSWORDCHAR:
        ex.passwordChar = static_cast<char>(wParam);
        return 0;
    case EM_GETPASSWORDCHAR:
        return static_cast<unsigned char>(ex.passwordChar);
    case EM_GETFIRSTVISIBLELINE:
        return FirstVisibleLine(t);
    case EM_SCROLLCARET:
        t->ShowPosition(t->GetInsertionPoint());
        return TRUE;
    case EM_LINESCROLL: {
        int lines = static_cast<int>(lParam);
        if (lines && IsMultilineEdit(st)) {
            int count = std::max(1, t->GetNumberOfLines());
            int target = std::max(0, std::min(count - 1, FirstVisibleLine(t) + lines));
            t->ShowPosition(t->GetLastPosition());
            t->ShowPosition(t->XYToPosition(0, target));
        }
        return TRUE;
    }
    case EM_CANPASTE:
        return t->CanPaste();
    case EM_SELECTIONTYPE: {
        long from, to;
        t->GetSelection(&from, &to);
        return from == to ? 0 : 1;
    }
    case EM_GETTEXTLENGTHEX:
        return static_cast<LRESULT>(FromWx(FromEditText(st, t->GetValue())).size());
    case EM_SETTABSTOPS:
    case EM_SETMARGINS:
    case EM_SETRECT:
    case EM_SETRECTNP:
    case EM_SETEVENTMASK:
    case EM_SETOPTIONS:
    case EM_SETTARGETDEVICE:
    case EM_AUTOURLDETECT:
    case EM_SETUNDOLIMIT:
    case EM_HIDESELECTION:
        return 0;
    case EM_GETEVENTMASK:
        return 0;
    case EM_GETRECT: {
        auto* r = reinterpret_cast<RECT*>(lParam);
        wxSize sz = t->GetClientSize();
        r->left = r->top = 0;
        r->right = sz.x;
        r->bottom = sz.y;
        return 0;
    }
    case EM_POSFROMCHAR: {
        bool rich = st.kind == ControlKind::RichEdit && wParam > 0xFFFF;
        long pos = WinToWxPos(t, st, static_cast<long>(rich ? lParam : static_cast<LPARAM>(wParam)));
        wxPoint p = t->PositionToCoords(pos);
        if (p == wxDefaultPosition)
            p = wxPoint(0, 0);
        if (rich) {
            if (auto* pt = reinterpret_cast<POINT*>(wParam)) {
                pt->x = p.x;
                pt->y = p.y;
            }
            return 0;
        }
        return MAKELRESULT(p.x, p.y);
    }
    case EM_CHARFROMPOS: {
        long pos = 0;
        wxTextCoord col, row;
        if (t->HitTest(wxPoint(LOWORD(lParam), HIWORD(lParam)), &col, &row) != wxTE_HT_UNKNOWN)
            pos = t->XYToPosition(col, row);
        return MAKELRESULT(WxToWinPos(t, st, pos), 0);
    }
    case EM_SETBKGNDCOLOR:
        if (!wParam) {
            t->SetBackgroundColour(ToWxColour(static_cast<COLORREF>(lParam)));
            t->Refresh();
        }
        return 0;
    case EM_GETSELTEXT: {
        std::string a = FromWx(FromEditText(st, t->GetStringSelection()));
        strcpy(reinterpret_cast<char*>(lParam), a.c_str());
        return static_cast<LRESULT>(a.size());
    }
    case EM_GETTEXTRANGE: {
        auto* tr = reinterpret_cast<TEXTRANGE*>(lParam);
        wxString v = FromEditText(st, t->GetValue());
        long to = tr->chrg.cpMax < 0 ? static_cast<long>(v.length()) : tr->chrg.cpMax;
        std::string a = FromWx(v.Mid(tr->chrg.cpMin, to - tr->chrg.cpMin));
        strcpy(tr->lpstrText, a.c_str());
        return static_cast<LRESULT>(a.size());
    }
    case EM_SETCHARFORMAT: {
        auto* cf = reinterpret_cast<CHARFORMAT*>(lParam);
        wxTextAttr attr;
        if (cf->dwMask & CFM_COLOR)
            attr.SetTextColour((cf->dwEffects & CFE_AUTOCOLOR) ? t->GetForegroundColour() : ToWxColour(cf->crTextColor));
        if (cf->dwMask & CFM_BOLD)
            attr.SetFontWeight((cf->dwEffects & CFE_BOLD) ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL);
        if (cf->dwMask & CFM_ITALIC)
            attr.SetFontStyle((cf->dwEffects & CFE_ITALIC) ? wxFONTSTYLE_ITALIC : wxFONTSTYLE_NORMAL);
        if (cf->dwMask & CFM_UNDERLINE)
            attr.SetFontUnderlined((cf->dwEffects & CFE_UNDERLINE) != 0);
        if ((cf->dwMask & CFM_SIZE) && cf->yHeight > 0)
            attr.SetFontPointSize(cf->yHeight / 20);
        if ((cf->dwMask & CFM_FACE) && cf->szFaceName[0])
            attr.SetFontFaceName(AnsiText(cf->szFaceName));
        if (wParam & SCF_SELECTION) {
            long from, to;
            t->GetSelection(&from, &to);
            t->SetStyle(from, to, attr);
        } else {
            t->SetDefaultStyle(attr);
            if (wParam & SCF_ALL)
                t->SetStyle(0, t->GetLastPosition(), attr);
        }
        return TRUE;
    }
    case EM_GETCHARFORMAT: {
        auto* cf = reinterpret_cast<CHARFORMAT*>(lParam);
        cf->dwMask = CFM_COLOR | CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_FACE | CFM_SIZE;
        wxFont f = t->GetFont();
        cf->dwEffects = (f.GetWeight() >= wxFONTWEIGHT_BOLD ? CFE_BOLD : 0) |
                        (f.GetStyle() == wxFONTSTYLE_ITALIC ? CFE_ITALIC : 0) | (f.GetUnderlined() ? CFE_UNDERLINE : 0);
        cf->yHeight = f.GetPointSize() * 20;
        cf->crTextColor = FromWxColour(t->GetForegroundColour());
        std::string face = FromWx(f.GetFaceName());
        strncpy(cf->szFaceName, face.c_str(), sizeof cf->szFaceName - 1);
        cf->szFaceName[sizeof cf->szFaceName - 1] = 0;
        return cf->dwMask;
    }
    case EM_SETPARAFORMAT:
    case EM_GETPARAFORMAT:
        return TRUE;
    case EM_STREAMIN: {
        auto* es = reinterpret_cast<EDITSTREAM*>(lParam);
        std::string data;
        BYTE buf[4096];
        for (;;) {
            LONG got = 0;
            if (es->pfnCallback(es->dwCookie, buf, sizeof buf, &got) != 0 || got <= 0)
                break;
            data.append(reinterpret_cast<char*>(buf), static_cast<size_t>(got));
        }
        if (wParam & SF_RTF) {
            if (!(wParam & SFF_SELECTION))
                t->Clear();
            InsertRtf(t, IsMultilineEdit(st), data);
            return static_cast<LRESULT>(data.size());
        }
        wxString v = ToEditText(st, ToWx(data.data(), static_cast<int>(data.size())));
        if (wParam & SFF_SELECTION)
            t->WriteText(v);
        else
            t->SetValue(v);
        return static_cast<LRESULT>(data.size());
    }
    case EM_STREAMOUT: {
        auto* es = reinterpret_cast<EDITSTREAM*>(lParam);
        std::string a = FromWx(FromEditText(st, (wParam & SFF_SELECTION) ? t->GetStringSelection() : t->GetValue()));
        size_t off = 0;
        while (off < a.size()) {
            LONG done = 0;
            LONG chunk = static_cast<LONG>(std::min<size_t>(a.size() - off, 4096));
            if (es->pfnCallback(es->dwCookie, reinterpret_cast<BYTE*>(&a[off]), chunk, &done) != 0 || done <= 0)
                break;
            off += static_cast<size_t>(done);
        }
        return static_cast<LRESULT>(off);
    }
    case EM_FINDTEXTEX: {
        auto* ft = reinterpret_cast<FINDTEXTEX*>(lParam);
        wxString v = FromEditText(st, t->GetValue());
        wxString needle = AnsiText(ft->lpstrText);
        long start = ft->chrg.cpMin;
        int pos = v.Mid(static_cast<size_t>(start)).Find(needle);
        if (pos == wxNOT_FOUND)
            return -1;
        ft->chrgText.cpMin = start + pos;
        ft->chrgText.cpMax = start + pos + static_cast<long>(needle.length());
        return ft->chrgText.cpMin;
    }
    case WM_CUT:
        t->Cut();
        return 0;
    case WM_COPY:
        t->Copy();
        return 0;
    case WM_PASTE:
        t->Paste();
        return 0;
    case WM_CLEAR: {
        long from, to;
        t->GetSelection(&from, &to);
        if (from != to)
            t->Remove(from, to);
        return 0;
    }
    case WM_CHAR: {
        UINT ch = static_cast<UINT>(wParam);
        if (!t->IsEditable() && ch >= 32)
            return 0;
        long from, to;
        t->GetSelection(&from, &to);
        switch (ch) {
        case 1:
            t->SelectAll();
            return 0;
        case 3:
            t->Copy();
            return 0;
        case 22:
            if (t->IsEditable())
                t->Paste();
            return 0;
        case 24:
            if (t->IsEditable())
                t->Cut();
            return 0;
        case 26:
            if (t->IsEditable())
                t->Undo();
            return 0;
        case 8:
            if (!t->IsEditable())
                return 0;
            if (from != to)
                t->Remove(from, to);
            else if (from > 0)
                t->Remove(from - 1, from);
            return 0;
        case 13:
            if (st.style & ES_MULTILINE)
                t->WriteText("\n");
            return 0;
        case 9:
            if ((st.style & ES_MULTILINE) && (st.style & ES_WANTRETURN))
                t->WriteText("\t");
            return 0;
        default:
            if (ch < 32 || ch == 127)
                return 0;
            break;
        }
        if (ex.limit > 0) {
            long len = static_cast<long>(FromEditText(st, t->GetValue()).length());
            if (len - (to - from) >= ex.limit) {
                NotifyParent(t, EN_MAXTEXT);
                return 0;
            }
        }
        char a = static_cast<char>(ch);
        t->WriteText(ToWx(&a, 1));
        return 0;
    }
    case WM_KEYDOWN: {
        UINT vk = static_cast<UINT>(wParam);
        long from, to;
        t->GetSelection(&from, &to);
        CurrentMessage* cm = GetCurrentMessageSlot();
        if (cm && cm->event && cm->msg.message == WM_KEYDOWN && cm->msg.wParam == wParam) {
            cm->defaultCalled = true;
            return 0;
        }
        if (vk == VK_DELETE && t->IsEditable()) {
            if (from != to)
                t->Remove(from, to);
            else if (from < t->GetLastPosition())
                t->Remove(from, from + 1);
        } else if (vk == VK_LEFT && from > 0) {
            t->SetInsertionPoint(from - 1);
        } else if (vk == VK_RIGHT && to < t->GetLastPosition()) {
            t->SetInsertionPoint(to + 1);
        } else if (vk == VK_HOME) {
            t->SetInsertionPoint(0);
        } else if (vk == VK_END) {
            t->SetInsertionPointEnd();
        }
        return 0;
    }
    case WM_GETDLGCODE:
        return DLGC_WANTCHARS | DLGC_HASSETSEL | DLGC_WANTARROWS | ((st.style & ES_WANTRETURN) ? DLGC_WANTMESSAGE : 0);
    default:
        handled = false;
        return 0;
    }
}

int ItemCount(wxItemContainerImmutable* c) { return static_cast<int>(c->GetCount()); }

LRESULT ComboProc(wxWindow* w, WindowState& st, UINT msg, WPARAM wParam, LPARAM lParam, bool& handled) {
    handled = true;
    auto* items = dynamic_cast<wxItemContainer*>(w);
    auto* combo = wxDynamicCast(w, wxComboBox);
    if (!items) {
        handled = false;
        return 0;
    }
    switch (msg) {
    case CB_ADDSTRING: {
        int n = items->Append(AnsiText(reinterpret_cast<const char*>(lParam)));
        return n;
    }
    case CB_INSERTSTRING: {
        int index = static_cast<int>(wParam);
        wxString s = AnsiText(reinterpret_cast<const char*>(lParam));
        if (index < 0 || index >= ItemCount(items))
            return items->Append(s);
        return items->Insert(s, static_cast<unsigned>(index));
    }
    case CB_DELETESTRING: {
        int index = static_cast<int>(wParam);
        if (index < 0 || index >= ItemCount(items))
            return CB_ERR;
        items->Delete(static_cast<unsigned>(index));
        return ItemCount(items);
    }
    case CB_RESETCONTENT:
        items->Clear();
        if (combo)
            combo->ChangeValue(wxString());
        return CB_OKAY;
    case CB_GETCOUNT:
        return ItemCount(items);
    case CB_GETCURSEL: {
        int sel = items->GetSelection();
        return sel == wxNOT_FOUND ? CB_ERR : sel;
    }
    case CB_SETCURSEL: {
        int index = static_cast<int>(wParam);
        if (index < 0 || index >= ItemCount(items)) {
            items->SetSelection(wxNOT_FOUND);
            if (combo)
                combo->ChangeValue(wxString());
            return CB_ERR;
        }
        items->SetSelection(index);
        return index;
    }
    case CB_GETLBTEXT: {
        int index = static_cast<int>(wParam);
        if (index < 0 || index >= ItemCount(items))
            return CB_ERR;
        std::string a = FromWx(items->GetString(static_cast<unsigned>(index)));
        strcpy(reinterpret_cast<char*>(lParam), a.c_str());
        return static_cast<LRESULT>(a.size());
    }
    case CB_GETLBTEXTLEN: {
        int index = static_cast<int>(wParam);
        if (index < 0 || index >= ItemCount(items))
            return CB_ERR;
        return static_cast<LRESULT>(FromWx(items->GetString(static_cast<unsigned>(index))).size());
    }
    case CB_FINDSTRING:
    case CB_FINDSTRINGEXACT:
    case CB_SELECTSTRING: {
        wxString s = AnsiText(reinterpret_cast<const char*>(lParam));
        int n = ItemCount(items);
        int start = static_cast<int>(wParam);
        for (int k = 0; k < n; ++k) {
            int i = (start + 1 + k + n) % n;
            if (start < 0)
                i = k;
            wxString item = items->GetString(static_cast<unsigned>(i));
            bool match = msg == CB_FINDSTRINGEXACT ? item.CmpNoCase(s) == 0 : item.Lower().StartsWith(s.Lower());
            if (match) {
                if (msg == CB_SELECTSTRING)
                    items->SetSelection(i);
                return i;
            }
        }
        return CB_ERR;
    }
    case CB_GETITEMDATA: {
        int index = static_cast<int>(wParam);
        if (index < 0 || index >= ItemCount(items))
            return CB_ERR;
        return reinterpret_cast<LRESULT>(items->GetClientData(static_cast<unsigned>(index)));
    }
    case CB_SETITEMDATA: {
        int index = static_cast<int>(wParam);
        if (index < 0 || index >= ItemCount(items))
            return CB_ERR;
        items->SetClientData(static_cast<unsigned>(index), reinterpret_cast<void*>(lParam));
        return CB_OKAY;
    }
    case CB_GETEDITSEL:
        if (combo) {
            long from, to;
            combo->GetSelection(&from, &to);
            return MAKELRESULT(from, to);
        }
        return 0;
    case CB_SETEDITSEL:
        if (combo)
            combo->SetSelection(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)));
        return TRUE;
    case CB_LIMITTEXT:
        if (combo)
            combo->SetMaxLength(static_cast<unsigned long>(wParam));
        return TRUE;
    case CB_SHOWDROPDOWN:
        if (combo) {
            if (wParam)
                combo->Popup();
            else
                combo->Dismiss();
        }
        return TRUE;
    case CB_GETDROPPEDSTATE:
        return FALSE;
    case CB_GETDROPPEDCONTROLRECT: {
        auto* r = reinterpret_cast<RECT*>(lParam);
        if (r) {
            wxRect sr = w->GetScreenRect();
            int visible = std::max(1, std::min(ItemCount(items), 30));
            r->left = sr.x;
            r->top = sr.y;
            r->right = sr.x + sr.width;
            r->bottom = sr.y + sr.height + visible * (w->GetCharHeight() + 2) + 2;
        }
        return CB_OKAY;
    }
    case CB_SETITEMHEIGHT:
    case CB_SETEXTENDEDUI:
    case CB_SETDROPPEDWIDTH:
    case CB_SETHORIZONTALEXTENT:
    case CB_INITSTORAGE:
        return CB_OKAY;
    case CB_GETITEMHEIGHT:
        return w->GetCharHeight() + 2;
    case WM_CUT:
        if (combo)
            combo->Cut();
        return 0;
    case WM_COPY:
        if (combo)
            combo->Copy();
        return 0;
    case WM_PASTE:
        if (combo)
            combo->Paste();
        return 0;
    case WM_CLEAR:
        if (combo) {
            long from, to;
            combo->GetSelection(&from, &to);
            if (from != to)
                combo->Remove(from, to);
        }
        return 0;
    case WM_CHAR:
        if (combo) {
            UINT ch = static_cast<UINT>(wParam);
            if (ch == 8) {
                long from, to;
                combo->GetSelection(&from, &to);
                if (from != to)
                    combo->Remove(from, to);
                else if (from > 0)
                    combo->Remove(from - 1, from);
            } else if (ch >= 32) {
                char a = static_cast<char>(ch);
                combo->WriteText(ToWx(&a, 1));
            }
        }
        return 0;
    default:
        handled = false;
        return 0;
    }
    (void)st;
}

LRESULT ListBoxProc(wxListBox* lb, WindowState& st, UINT msg, WPARAM wParam, LPARAM lParam, bool& handled) {
    handled = true;
    int n = static_cast<int>(lb->GetCount());
    int index = static_cast<int>(wParam);
    switch (msg) {
    case LB_ADDSTRING:
        return lb->Append(AnsiText(reinterpret_cast<const char*>(lParam)));
    case LB_INSERTSTRING:
        if (index < 0 || index >= n)
            return lb->Append(AnsiText(reinterpret_cast<const char*>(lParam)));
        return lb->Insert(AnsiText(reinterpret_cast<const char*>(lParam)), static_cast<unsigned>(index));
    case LB_DELETESTRING:
        if (index < 0 || index >= n)
            return LB_ERR;
        lb->Delete(static_cast<unsigned>(index));
        return n - 1;
    case LB_RESETCONTENT:
        lb->Clear();
        return 0;
    case LB_GETCOUNT:
        return n;
    case LB_GETCURSEL: {
        if (lb->HasMultipleSelection()) {
            wxArrayInt sel;
            lb->GetSelections(sel);
            return sel.empty() ? LB_ERR : sel[0];
        }
        int s = lb->GetSelection();
        return s == wxNOT_FOUND ? LB_ERR : s;
    }
    case LB_SETCURSEL:
        if (index < 0 || index >= n) {
            lb->SetSelection(wxNOT_FOUND);
            return index < 0 ? 0 : LB_ERR;
        }
        lb->SetSelection(index);
        return index;
    case LB_GETSEL:
        return index >= 0 && index < n && lb->IsSelected(index) ? 1 : 0;
    case LB_SETSEL: {
        int item = static_cast<int>(lParam);
        if (item == -1) {
            for (int i = 0; i < n; ++i)
                wParam ? lb->SetSelection(i) : lb->Deselect(i);
            return 0;
        }
        if (item < 0 || item >= n)
            return LB_ERR;
        if (wParam)
            lb->SetSelection(item);
        else
            lb->Deselect(item);
        return 0;
    }
    case LB_GETSELCOUNT: {
        wxArrayInt sel;
        return lb->GetSelections(sel);
    }
    case LB_GETSELITEMS: {
        wxArrayInt sel;
        lb->GetSelections(sel);
        int* out = reinterpret_cast<int*>(lParam);
        int count = std::min<int>(static_cast<int>(sel.size()), index);
        for (int i = 0; i < count; ++i)
            out[i] = sel[static_cast<size_t>(i)];
        return count;
    }
    case LB_SELITEMRANGE:
    case LB_SELITEMRANGEEX: {
        int first = LOWORD(lParam);
        int last = HIWORD(lParam);
        bool select = wParam != 0;
        if (msg == LB_SELITEMRANGEEX) {
            first = static_cast<int>(wParam);
            last = static_cast<int>(lParam);
            select = first <= last;
            if (first > last)
                std::swap(first, last);
        }
        for (int i = std::max(0, first); i <= std::min(last, n - 1); ++i)
            select ? lb->SetSelection(i) : lb->Deselect(i);
        return 0;
    }
    case LB_GETTEXT: {
        if (index < 0 || index >= n)
            return LB_ERR;
        std::string a = FromWx(lb->GetString(static_cast<unsigned>(index)));
        strcpy(reinterpret_cast<char*>(lParam), a.c_str());
        return static_cast<LRESULT>(a.size());
    }
    case LB_GETTEXTLEN:
        if (index < 0 || index >= n)
            return LB_ERR;
        return static_cast<LRESULT>(FromWx(lb->GetString(static_cast<unsigned>(index))).size());
    case LB_FINDSTRING:
    case LB_FINDSTRINGEXACT:
    case LB_SELECTSTRING: {
        wxString s = AnsiText(reinterpret_cast<const char*>(lParam));
        for (int k = 0; k < n; ++k) {
            int i = index < 0 ? k : (index + 1 + k) % n;
            wxString item = lb->GetString(static_cast<unsigned>(i));
            bool match = msg == LB_FINDSTRINGEXACT ? item.CmpNoCase(s) == 0 : item.Lower().StartsWith(s.Lower());
            if (match) {
                if (msg == LB_SELECTSTRING)
                    lb->SetSelection(i);
                return i;
            }
        }
        return LB_ERR;
    }
    case LB_GETITEMDATA:
        if (index < 0 || index >= n)
            return LB_ERR;
        return reinterpret_cast<LRESULT>(lb->GetClientData(static_cast<unsigned>(index)));
    case LB_SETITEMDATA:
        if (index < 0 || index >= n)
            return LB_ERR;
        lb->SetClientData(static_cast<unsigned>(index), reinterpret_cast<void*>(lParam));
        return 0;
    case LB_GETTOPINDEX:
        return 0;
    case LB_SETTOPINDEX:
        if (index >= 0 && index < n)
            lb->SetFirstItem(index);
        return 0;
    case LB_GETCARETINDEX: {
        int s = lb->GetSelection();
        return s == wxNOT_FOUND ? 0 : s;
    }
    case LB_SETCARETINDEX:
        return 0;
    case LB_GETITEMRECT: {
        auto* r = reinterpret_cast<RECT*>(lParam);
        int h = lb->GetCharHeight() + 2;
        r->left = 0;
        r->right = lb->GetClientSize().x;
        r->top = index * h;
        r->bottom = r->top + h;
        return 0;
    }
    case LB_GETITEMHEIGHT:
        return lb->GetCharHeight() + 2;
    case LB_SETTABSTOPS:
    case LB_SETITEMHEIGHT:
    case LB_SETHORIZONTALEXTENT:
    case LB_SETCOLUMNWIDTH:
    case LB_SETANCHORINDEX:
        return 0;
    case LB_GETHORIZONTALEXTENT:
        return 0;
    default:
        handled = false;
        return 0;
    }
    (void)st;
}

LRESULT ButtonProc(wxWindow* w, WindowState& st, UINT msg, WPARAM wParam, LPARAM lParam, bool& handled) {
    handled = true;
    switch (msg) {
    case BM_GETCHECK:
        if (auto* cb = wxDynamicCast(w, wxCheckBox))
            return cb->Get3StateValue() == wxCHK_UNDETERMINED ? BST_INDETERMINATE : (cb->GetValue() ? BST_CHECKED : BST_UNCHECKED);
        if (auto* rb = wxDynamicCast(w, wxRadioButton))
            return rb->GetValue() ? BST_CHECKED : BST_UNCHECKED;
        if (auto* tb = wxDynamicCast(w, wxToggleButton))
            return tb->GetValue() ? BST_CHECKED : BST_UNCHECKED;
        return st.checkState;
    case BM_SETCHECK:
        st.checkState = static_cast<int>(wParam);
        if (auto* cb = wxDynamicCast(w, wxCheckBox)) {
            if (cb->Is3State())
                cb->Set3StateValue(wParam == BST_INDETERMINATE ? wxCHK_UNDETERMINED
                                                               : (wParam == BST_CHECKED ? wxCHK_CHECKED : wxCHK_UNCHECKED));
            else
                cb->SetValue(wParam == BST_CHECKED);
        } else if (auto* rb = wxDynamicCast(w, wxRadioButton)) {
            if (wParam == BST_CHECKED)
                rb->SetValue(true);
            else if (rb->GetValue()) {
                // wx cannot uncheck the only checked radio button of a group directly.
                rb->SetValue(false);
            }
        } else if (auto* tb = wxDynamicCast(w, wxToggleButton)) {
            tb->SetValue(wParam == BST_CHECKED);
        } else {
            w->Refresh();
        }
        return 0;
    case BM_GETSTATE: {
        LRESULT state = SendMessage(ToHwnd(w), BM_GETCHECK, 0, 0);
        if (st.kind == ControlKind::OwnerDrawButton && Extra<ButtonExtra>(st).pushed)
            state |= BST_PUSHED;
        if (w->HasFocus())
            state |= BST_FOCUS;
        return state;
    }
    case BM_SETSTATE:
        if (st.kind == ControlKind::OwnerDrawButton) {
            Extra<ButtonExtra>(st).pushed = wParam != 0;
            w->Refresh();
        }
        return 0;
    case BM_SETSTYLE: {
        DWORD old = st.style;
        st.style = (st.style & ~0xFFFFu) | static_cast<DWORD>(wParam & 0xFFFF);
        if ((old & BS_TYPEMASK) != (st.style & BS_TYPEMASK) && (st.style & BS_TYPEMASK) == BS_DEFPUSHBUTTON) {
            if (auto* b = wxDynamicCast(w, wxButton))
                b->SetDefault();
        }
        if (lParam)
            w->Refresh();
        return 0;
    }
    case BM_CLICK:
        NotifyParent(w, BN_CLICKED);
        return 0;
    case BM_SETIMAGE: {
        ButtonExtra& bx = Extra<ButtonExtra>(st);
        LRESULT old = 0;
        if (wParam == IMAGE_BITMAP) {
            old = reinterpret_cast<LRESULT>(bx.image);
            bx.image = reinterpret_cast<HBITMAP>(lParam);
            wxBitmap* bmp = BitmapFromHandle(bx.image);
            if (auto* b = wxDynamicCast(w, wxButton)) {
                if (bmp && bmp->IsOk())
                    b->SetBitmap(*bmp);
            }
        } else if (wParam == IMAGE_ICON) {
            old = reinterpret_cast<LRESULT>(bx.icon);
            bx.icon = reinterpret_cast<HICON>(lParam);
            if (auto* b = wxDynamicCast(w, wxButton)) {
                wxIcon* icon = reinterpret_cast<wxIcon*>(bx.icon);
                if (icon && icon->IsOk())
                    b->SetBitmap(*icon);
            }
        }
        w->Refresh();
        return old;
    }
    case BM_GETIMAGE: {
        ButtonExtra& bx = Extra<ButtonExtra>(st);
        return wParam == IMAGE_ICON ? reinterpret_cast<LRESULT>(bx.icon) : reinterpret_cast<LRESULT>(bx.image);
    }
    case WM_GETDLGCODE:
        if (st.kind == ControlKind::RadioButton)
            return DLGC_BUTTON | DLGC_RADIOBUTTON;
        return DLGC_BUTTON | ((st.style & BS_TYPEMASK) == BS_DEFPUSHBUTTON ? DLGC_DEFPUSHBUTTON : DLGC_UNDEFPUSHBUTTON);
    default:
        handled = false;
        return 0;
    }
}

LRESULT StaticProc(wxWindow* w, WindowState& st, UINT msg, WPARAM wParam, LPARAM lParam, bool& handled) {
    handled = true;
    switch (msg) {
    case STM_SETIMAGE:
    case STM_SETICON: {
        LRESULT old = reinterpret_cast<LRESULT>(st.image);
        st.image = reinterpret_cast<void*>(lParam);
        auto* sb = wxDynamicCast(w, wxStaticBitmap);
        if (sb) {
            if (msg == STM_SETICON || wParam == IMAGE_ICON) {
                wxIcon* icon = reinterpret_cast<wxIcon*>(lParam);
                if (icon && icon->IsOk())
                    sb->SetIcon(*icon);
            } else {
                wxBitmap* bmp = BitmapFromHandle(reinterpret_cast<HBITMAP>(lParam));
                if (bmp && bmp->IsOk()) {
                    sb->SetBitmap(*bmp);
                    if (!(st.style & SS_REALSIZECONTROL) && !(st.style & SS_CENTERIMAGE))
                        sb->SetSize(bmp->GetSize());
                }
            }
        } else {
            w->Refresh();
        }
        return old;
    }
    case STM_GETIMAGE:
    case STM_GETICON:
        return reinterpret_cast<LRESULT>(st.image);
    case WM_GETDLGCODE:
        return DLGC_STATIC;
    default:
        handled = false;
        return 0;
    }
}

LRESULT RangeProc(wxWindow* w, WindowState& st, UINT msg, WPARAM wParam, LPARAM lParam, bool& handled) {
    handled = true;
    RangeExtra& r = Extra<RangeExtra>(st);
    auto* gauge = wxDynamicCast(w, wxGauge);
    auto* slider = wxDynamicCast(w, wxSlider);
    auto applyGauge = [&] {
        if (!gauge)
            return;
        int range = std::max(1, r.maxValue - r.minValue);
        gauge->SetRange(range);
        gauge->SetValue(std::max(0, std::min(range, r.pos - r.minValue)));
    };
    if (st.kind == ControlKind::Progress) {
        switch (msg) {
        case PBM_SETRANGE: {
            LRESULT old = MAKELRESULT(r.minValue, r.maxValue);
            r.minValue = LOWORD(lParam);
            r.maxValue = HIWORD(lParam);
            applyGauge();
            return old;
        }
        case PBM_SETRANGE32: {
            LRESULT old = MAKELRESULT(r.minValue, r.maxValue);
            r.minValue = static_cast<int>(wParam);
            r.maxValue = static_cast<int>(lParam);
            applyGauge();
            return old;
        }
        case PBM_GETRANGE: {
            auto* range = reinterpret_cast<int*>(lParam);
            if (range) {
                range[0] = r.minValue;
                range[1] = r.maxValue;
            }
            return wParam ? r.minValue : r.maxValue;
        }
        case PBM_SETPOS: {
            int old = r.pos;
            r.pos = static_cast<int>(wParam);
            applyGauge();
            return old;
        }
        case PBM_DELTAPOS: {
            int old = r.pos;
            r.pos += static_cast<int>(wParam);
            applyGauge();
            return old;
        }
        case PBM_SETSTEP: {
            int old = r.step;
            r.step = static_cast<int>(wParam);
            return old;
        }
        case PBM_STEPIT: {
            int old = r.pos;
            r.pos += r.step;
            if (r.pos > r.maxValue)
                r.pos = r.minValue + (r.pos - r.maxValue);
            applyGauge();
            return old;
        }
        case PBM_GETPOS:
            return r.pos;
        case PBM_SETBARCOLOR:
        case PBM_SETBKCOLOR:
            return CLR_DEFAULT;
        case PBM_SETMARQUEE:
            if (gauge && wParam)
                gauge->Pulse();
            return TRUE;
        default:
            break;
        }
    }
    if (st.kind == ControlKind::Slider) {
        switch (msg) {
        case TBM_GETPOS:
            return slider ? slider->GetValue() : r.pos;
        case TBM_SETPOS:
            r.pos = static_cast<int>(lParam);
            if (slider)
                slider->SetValue(r.pos);
            return 0;
        case TBM_SETRANGE:
            r.minValue = LOWORD(lParam);
            r.maxValue = HIWORD(lParam);
            if (slider)
                slider->SetRange(r.minValue, std::max(r.minValue, r.maxValue));
            return 0;
        case TBM_SETRANGEMIN:
            r.minValue = static_cast<int>(lParam);
            if (slider)
                slider->SetRange(r.minValue, std::max(r.minValue, r.maxValue));
            return 0;
        case TBM_SETRANGEMAX:
            r.maxValue = static_cast<int>(lParam);
            if (slider)
                slider->SetRange(r.minValue, std::max(r.minValue, r.maxValue));
            return 0;
        case TBM_GETRANGEMIN:
            return slider ? slider->GetMin() : r.minValue;
        case TBM_GETRANGEMAX:
            return slider ? slider->GetMax() : r.maxValue;
        case TBM_SETTICFREQ:
            if (slider)
                slider->SetTickFreq(std::max(1, static_cast<int>(wParam)));
            return 0;
        case TBM_SETPAGESIZE: {
            int old = slider ? slider->GetPageSize() : 0;
            if (slider)
                slider->SetPageSize(static_cast<int>(lParam));
            return old;
        }
        case TBM_GETPAGESIZE:
            return slider ? slider->GetPageSize() : 0;
        case TBM_SETLINESIZE: {
            int old = slider ? slider->GetLineSize() : 0;
            if (slider)
                slider->SetLineSize(static_cast<int>(lParam));
            return old;
        }
        case TBM_GETLINESIZE:
            return slider ? slider->GetLineSize() : 1;
        case TBM_SETTIC:
            if (slider)
                slider->SetTick(static_cast<int>(lParam));
            return TRUE;
        case TBM_CLEARTICS:
            if (slider)
                slider->ClearTicks();
            return 0;
        case TBM_GETNUMTICS:
            return 2;
        case TBM_SETSEL:
            r.selStart = static_cast<short>(LOWORD(lParam));
            r.selEnd = static_cast<short>(HIWORD(lParam));
            return 0;
        case TBM_SETSELSTART:
            r.selStart = static_cast<int>(lParam);
            return 0;
        case TBM_SETSELEND:
            r.selEnd = static_cast<int>(lParam);
            return 0;
        case TBM_CLEARSEL:
            r.selStart = r.selEnd = 0;
            return 0;
        case TBM_GETSELSTART:
            return r.selStart;
        case TBM_GETSELEND:
            return r.selEnd;
        default:
            break;
        }
    }
    if (st.kind == ControlKind::Spin) {
        switch (msg) {
        case UDM_SETRANGE:
            r.maxValue = static_cast<short>(LOWORD(lParam));
            r.minValue = static_cast<short>(HIWORD(lParam));
            return 0;
        case UDM_SETRANGE32:
            r.minValue = static_cast<int>(wParam);
            r.maxValue = static_cast<int>(lParam);
            return 0;
        case UDM_GETRANGE:
            return MAKELRESULT(r.maxValue, r.minValue);
        case UDM_GETRANGE32:
            if (wParam)
                *reinterpret_cast<int*>(wParam) = r.minValue;
            if (lParam)
                *reinterpret_cast<int*>(lParam) = r.maxValue;
            return 0;
        case UDM_SETPOS:
        case UDM_SETPOS32: {
            int old = r.pos;
            r.pos = msg == UDM_SETPOS ? static_cast<short>(LOWORD(lParam)) : static_cast<int>(lParam);
            UpdateSpinBuddy(w, st);
            return old;
        }
        case UDM_GETPOS:
            return MAKELRESULT(ReadSpinBuddy(st), 0);
        case UDM_GETPOS32:
            if (lParam)
                *reinterpret_cast<BOOL*>(lParam) = FALSE;
            return ReadSpinBuddy(st);
        case UDM_SETBUDDY: {
            CWnd* old = st.buddy;
            st.buddy = CWnd::FromHandle(reinterpret_cast<HWND>(wParam));
            return reinterpret_cast<LRESULT>(old ? old->m_hWnd : nullptr);
        }
        case UDM_GETBUDDY:
            return reinterpret_cast<LRESULT>(st.buddy ? st.buddy->m_hWnd : nullptr);
        case UDM_SETBASE:
        case UDM_SETACCEL:
            return TRUE;
        case UDM_GETBASE:
            return 10;
        default:
            break;
        }
    }
    if (st.kind == ControlKind::ScrollBar) {
        switch (msg) {
        case SBM_SETPOS:
            if (auto* sb = wxDynamicCast(w, wxScrollBar)) {
                int old = sb->GetThumbPosition();
                sb->SetThumbPosition(static_cast<int>(wParam) - r.minValue);
                return old + r.minValue;
            }
            return 0;
        case SBM_GETPOS:
            if (auto* sb = wxDynamicCast(w, wxScrollBar))
                return sb->GetThumbPosition() + r.minValue;
            return 0;
        case SBM_SETRANGE:
        case SBM_SETRANGEREDRAW:
            if (auto* sb = wxDynamicCast(w, wxScrollBar)) {
                r.minValue = static_cast<int>(wParam);
                r.maxValue = static_cast<int>(lParam);
                int thumb = std::max(1, r.page);
                int pageSize = r.page > 0 ? r.page : std::max(1, (r.maxValue - r.minValue) / 10);
                sb->SetScrollbar(sb->GetThumbPosition(), thumb, std::max(thumb, r.maxValue - r.minValue + 1), pageSize);
            }
            return 0;
        case SBM_SETSCROLLINFO: {
            auto* si = reinterpret_cast<SCROLLINFO*>(lParam);
            auto* sb = wxDynamicCast(w, wxScrollBar);
            if (!si || !sb)
                return 0;
            int pos = sb->GetThumbPosition() + r.minValue;
            if (si->fMask & SIF_RANGE) {
                r.minValue = si->nMin;
                r.maxValue = si->nMax;
            }
            if (si->fMask & SIF_PAGE)
                r.page = static_cast<int>(si->nPage);
            if (si->fMask & SIF_POS)
                pos = si->nPos;
            int thumb = std::max(1, r.page);
            int range = std::max(thumb, r.maxValue - r.minValue + 1);
            pos = std::max(0, std::min(pos - r.minValue, range - thumb));
            sb->SetScrollbar(pos, thumb, range, r.page > 0 ? r.page : std::max(1, range / 10), wParam != 0);
            return pos + r.minValue;
        }
        case SBM_GETSCROLLINFO: {
            auto* si = reinterpret_cast<SCROLLINFO*>(lParam);
            auto* sb = wxDynamicCast(w, wxScrollBar);
            if (!si || !sb)
                return FALSE;
            if (si->fMask & SIF_RANGE) {
                si->nMin = r.minValue;
                si->nMax = r.maxValue;
            }
            if (si->fMask & SIF_PAGE)
                si->nPage = static_cast<UINT>(std::max(0, r.page));
            if (si->fMask & SIF_POS)
                si->nPos = sb->GetThumbPosition() + r.minValue;
            if (si->fMask & SIF_TRACKPOS)
                si->nTrackPos = sb->GetThumbPosition() + r.minValue;
            return TRUE;
        }
        case SBM_GETRANGE:
            if (wParam)
                *reinterpret_cast<int*>(wParam) = r.minValue;
            if (lParam)
                *reinterpret_cast<int*>(lParam) = r.maxValue;
            return 0;
        default:
            break;
        }
    }
    handled = false;
    return 0;
}

LRESULT TabProc(wxWindow* w, WindowState& st, UINT msg, WPARAM wParam, LPARAM lParam, bool& handled) {
    handled = true;
    TabExtra& tx = Extra<TabExtra>(st);
    (void)wParam;
    (void)lParam;
    switch (msg) {
    case WM_GETDLGCODE:
        return DLGC_WANTARROWS;
    default:
        handled = false;
        break;
    }
    (void)w;
    (void)tx;
    return 0;
}

} // namespace

LRESULT ControlWindowProc(wxWindow* w, WindowState& st, UINT msg, WPARAM wParam, LPARAM lParam, bool& handled) {
    switch (st.kind) {
    case ControlKind::Edit:
    case ControlKind::RichEdit:
        if (auto* t = AsText(w))
            return EditProc(t, st, msg, wParam, lParam, handled);
        break;
    case ControlKind::ComboBox:
    case ControlKind::ComboList:
        return ComboProc(w, st, msg, wParam, lParam, handled);
    case ControlKind::ListBox:
        if (auto* lb = wxDynamicCast(w, wxListBox))
            return ListBoxProc(lb, st, msg, wParam, lParam, handled);
        break;
    case ControlKind::PushButton:
    case ControlKind::CheckBox:
    case ControlKind::RadioButton:
    case ControlKind::OwnerDrawButton:
    case ControlKind::GroupBox:
        return ButtonProc(w, st, msg, wParam, lParam, handled);
    case ControlKind::Static:
    case ControlKind::StaticBitmap:
    case ControlKind::StaticFrame:
        return StaticProc(w, st, msg, wParam, lParam, handled);
    case ControlKind::Progress:
    case ControlKind::Slider:
    case ControlKind::Spin:
    case ControlKind::ScrollBar:
        return RangeProc(w, st, msg, wParam, lParam, handled);
    case ControlKind::Tab:
        return TabProc(w, st, msg, wParam, lParam, handled);
    case ControlKind::ListView:
        return ListViewProc(w, st, msg, wParam, lParam, handled);
    case ControlKind::Scintilla:
        if (msg >= 2000 || msg == WM_GETTEXT || msg == WM_SETTEXT || msg == WM_GETTEXTLENGTH)
            return ScintillaWindowProc(w, msg, wParam, lParam, handled);
        break;
    default:
        break;
    }
    handled = false;
    return 0;
}

// ---------------------------------------------------------------------------------------------
// WM_CTLCOLOR emulation

void ApplyCtlColor(wxWindow* control) {
    WindowState* st = GetState(control);
    wxWindow* parent = LogicalParent(control);
    if (!st || !parent || !IsManagedWindow(parent) || !PermanentWnd(parent))
        return;
    UINT msg = WM_CTLCOLORSTATIC;
    switch (st->kind) {
    case ControlKind::Edit:
    case ControlKind::RichEdit:
    case ControlKind::ComboBox:
        msg = (st->style & ES_READONLY) || !control->IsEnabled() ? WM_CTLCOLORSTATIC : WM_CTLCOLOREDIT;
        break;
    case ControlKind::ListBox:
    case ControlKind::ComboList:
        msg = WM_CTLCOLORLISTBOX;
        break;
    case ControlKind::PushButton:
    case ControlKind::OwnerDrawButton:
        msg = WM_CTLCOLORBTN;
        break;
    case ControlKind::Static:
    case ControlKind::StaticBitmap:
    case ControlKind::CheckBox:
    case ControlKind::RadioButton:
    case ControlKind::GroupBox:
        msg = WM_CTLCOLORSTATIC;
        break;
    default:
        return;
    }
    CWnd* parentWnd = PermanentWnd(parent);
    if (!HasMessageHandler(parentWnd, WM_CTLCOLOR) && !HasMessageHandler(st->permanent, WM_CTLCOLOR + WM_REFLECT_BASE))
        return;
    wxBitmap scratch(1, 1);
    wxMemoryDC mdc(scratch);
    CDC* cdc = WrapDC(&mdc, control);
    COLORREF defaultText = 0xFFFFFFFF;
    cdc->SetTextColor(defaultText);
    LRESULT r = DispatchMessageTo(ToHwnd(parent), msg, reinterpret_cast<WPARAM>(cdc->m_hDC),
                                  reinterpret_cast<LPARAM>(ToHwnd(control)));
    COLORREF text = cdc->GetTextColor();
    COLORREF bk = cdc->GetBkColor();
    int bkMode = cdc->GetBkMode();
    UnwrapDC(cdc);
    if (text != defaultText)
        control->SetForegroundColour(ToWxColour(text));
    if (r) {
        wxBrush brush = BrushFromHandle(reinterpret_cast<HBRUSH>(r));
        if (brush.IsOk() && brush.GetStyle() != wxBRUSHSTYLE_TRANSPARENT)
            control->SetBackgroundColour(brush.GetColour());
        else if (bkMode == OPAQUE)
            control->SetBackgroundColour(ToWxColour(bk));
    }
    control->Refresh();
}

} // namespace mfcwx
