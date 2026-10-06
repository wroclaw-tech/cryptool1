#include "windows_impl.h"

#include "afxdlgs.h"

#include <wx/colordlg.h>
#include <wx/fdrepdlg.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/fontdlg.h>
#include <wx/notebook.h>

#include <cfloat>

namespace mfcwx {

// ---------------------------------------------------------------------------------------------
// Dialog units

namespace {

wxFont DialogFontFor(const rc::Dialog* tmpl) {
    wxFont f = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
#ifdef __WXOSX__
    f.SetPointSize(11);
#endif
    if (tmpl && tmpl->fontName && tmpl->fontWeight >= FW_BOLD)
        f.SetWeight(wxFONTWEIGHT_BOLD);
    return f;
}

wxSize BaseUnits(const wxFont& font) {
    wxBitmap bmp(1, 1);
    wxMemoryDC dc(bmp);
    dc.SetFont(font);
    wxCoord w = 0, h = 0;
    dc.GetTextExtent("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", &w, &h);
    int baseX = static_cast<int>((w / 26 + 1) / 2);
    return wxSize(std::max(4, baseX), std::max(8, static_cast<int>(h)));
}

} // namespace

wxRect DialogUnitsToPixels(wxWindow* dialog, int x, int y, int cx, int cy) {
    static wxSize defaultUnits = BaseUnits(DialogFontFor(nullptr));
    wxSize base = defaultUnits;
    if (dialog) {
        WindowState* st = GetState(dialog);
        if (st && st->baseUnits.x > 0)
            base = st->baseUnits;
    }
    return wxRect(MulDiv(x, base.x, 4), MulDiv(y, base.y, 8), MulDiv(cx, base.x, 4), MulDiv(cy, base.y, 8));
}

bool CreateDialogControls(wxWindow* parent, const rc::Dialog& tmpl) {
    WindowState& ps = EnsureState(parent);
    wxWindow* previous = nullptr;
    for (int i = 0; i < tmpl.controlCount; ++i) {
        const rc::Control& c = tmpl.controls[i];
        wxRect r = DialogUnitsToPixels(parent, c.x, c.y, c.cx, c.cy);
        wxWindow* w = CreateControl(parent, c.cls, c.text, c.id, r, c.style, c.exStyle);
        if (!w)
            continue;
        w->SetFont(parent->GetFont());
        WindowState* st = GetState(w);
        if (st && st->kind == ControlKind::PushButton && (c.style & BS_TYPEMASK) == BS_DEFPUSHBUTTON) {
            if (auto* b = wxDynamicCast(w, wxButton))
                b->SetDefault();
            ps.defaultButtonId = c.id;
        }
        if (st && st->kind == ControlKind::Spin && previous && (c.style & UDS_AUTOBUDDY)) {
            st->buddy = WrapperFor(previous);
            if (c.style & (UDS_ALIGNRIGHT | UDS_ALIGNLEFT)) {
                wxRect br = previous->GetRect();
                int sw = std::max(16, w->GetBestSize().x);
                if (c.style & UDS_ALIGNRIGHT) {
                    previous->SetSize(br.x, br.y, std::max(10, br.width - sw), br.height);
                    w->SetSize(br.x + std::max(10, br.width - sw), br.y, sw, br.height);
                } else {
                    previous->SetSize(br.x + sw, br.y, std::max(10, br.width - sw), br.height);
                    w->SetSize(br.x, br.y, sw, br.height);
                }
            }
        }
        previous = w;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
// Keyboard handling of dialogs (IsDialogMessage)

bool PreTranslateKey(wxWindow* topLevel, wxKeyEvent& event, UINT message) {
    wxWindow* focus = wxWindow::FindFocus();
    while (focus && !IsManagedWindow(focus))
        focus = focus->GetParent();
    MSG msg;
    memset(&msg, 0, sizeof msg);
    msg.hwnd = ToHwnd(focus ? focus : topLevel);
    msg.message = message;
    msg.wParam = VirtualKeyFromWx(event.GetKeyCode());
    msg.lParam = KeyLParam(event, false);
    if (!msg.wParam)
        return false;
    for (wxWindow* w = focus ? focus : topLevel; w; w = w->GetParent()) {
        CWnd* p = PermanentWnd(w);
        if (p && p->PreTranslateMessage(&msg))
            return true;
        if (w == topLevel)
            break;
    }
    CWinApp* app = AfxGetApp();
    CWnd* main = app ? app->m_pMainWnd : nullptr;
    if (main && main->m_hWnd && main->GetWx() != topLevel && main->PreTranslateMessage(&msg))
        return true;
    return false;
}

bool DialogKeyHook(wxWindow* dialog, wxKeyEvent& event) {
    if (PreTranslateKey(dialog, event, event.AltDown() ? WM_SYSKEYDOWN : WM_KEYDOWN))
        return true;
    int code = event.GetKeyCode();
    wxWindow* focus = wxWindow::FindFocus();
    WindowState* fs = nullptr;
    for (wxWindow* f = focus; f && !fs; f = f->GetParent())
        fs = GetState(f);
    if (code == WXK_ESCAPE) {
        wxWindow* cancel = nullptr;
        for (wxWindow* c : dialog->GetChildren()) {
            WindowState* st = GetState(c);
            if (st && st->winId == IDCANCEL)
                cancel = c;
        }
        if (cancel && !cancel->IsEnabled())
            return true;
        DispatchMessageTo(ToHwnd(dialog), WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), reinterpret_cast<LPARAM>(ToHwnd(cancel)));
        return true;
    }
    if (code == WXK_RETURN || code == WXK_NUMPAD_ENTER) {
        if (fs && (fs->kind == ControlKind::Edit || fs->kind == ControlKind::RichEdit) && (fs->style & ES_MULTILINE) &&
            (fs->style & ES_WANTRETURN))
            return false;
        if (fs && (fs->kind == ControlKind::ListView || fs->kind == ControlKind::Scintilla || fs->kind == ControlKind::Generic))
            return false;
        int id = IDOK;
        wxWindow* target = nullptr;
        if (fs && (fs->kind == ControlKind::PushButton || fs->kind == ControlKind::OwnerDrawButton)) {
            id = fs->winId;
            target = fs->window;
        } else {
            WindowState* ds = GetState(dialog);
            if (ds && ds->defaultButtonId)
                id = ds->defaultButtonId;
            for (wxWindow* c : dialog->GetChildren()) {
                WindowState* st = GetState(c);
                if (st && st->winId == id)
                    target = c;
            }
        }
        if (target && !target->IsEnabled())
            return true;
        DispatchMessageTo(ToHwnd(dialog), WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), reinterpret_cast<LPARAM>(ToHwnd(target)));
        return true;
    }
    if (code == WXK_F1) {
        HELPINFO hi;
        memset(&hi, 0, sizeof hi);
        hi.cbSize = sizeof hi;
        hi.iContextType = HELPINFO_WINDOW;
        hi.iCtrlId = fs ? fs->winId : 0;
        hi.hItemHandle = fs ? fs->window : nullptr;
        wxPoint p = wxGetMousePosition();
        hi.MousePos.x = p.x;
        hi.MousePos.y = p.y;
        HWND target = fs ? ToHwnd(fs->window) : ToHwnd(dialog);
        DispatchMessageTo(target, WM_HELP, 0, reinterpret_cast<LPARAM>(&hi));
        return true;
    }
    return false;
}

BOOL ApplyDlgInit(CWnd* pWnd, int dialogId) {
    if (!pWnd || !pWnd->m_hWnd || !dialogId)
        return TRUE;
    for (const rc::DlgInitEntry* e : FindDlgInit(dialogId)) {
        HWND h = ::GetDlgItem(pWnd->m_hWnd, e->controlId);
        WindowState* st = h ? GetState(ToWx(h)) : nullptr;
        if (!st)
            continue;
        std::string text = Utf8ToAnsi(e->text);
        UINT msg = st->kind == ControlKind::ListBox ? LB_ADDSTRING : CB_ADDSTRING;
        DispatchMessageTo(h, msg, 0, reinterpret_cast<LPARAM>(text.c_str()));
    }
    return TRUE;
}

namespace {

long DialogWxStyle(DWORD style) {
    long f = 0;
    if (style & WS_CAPTION)
        f |= wxCAPTION;
    if (style & WS_SYSMENU)
        f |= wxSYSTEM_MENU | wxCLOSE_BOX;
    if (style & WS_THICKFRAME)
        f |= wxRESIZE_BORDER;
    if (style & WS_MINIMIZEBOX)
        f |= wxMINIMIZE_BOX;
    if (style & WS_MAXIMIZEBOX)
        f |= wxMAXIMIZE_BOX;
    if (!(style & WS_CAPTION))
        f |= wxBORDER_SIMPLE;
    return f;
}

class MfcDialog : public wxDialog {
public:
    MfcDialog(wxWindow* parent, const wxString& title, long style) : wxDialog() {
        SetExtraStyle(GetExtraStyle() | wxWS_EX_BLOCK_EVENTS);
        Create(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, style);
        Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& e) {
            if (IsManagedWindow(this)) {
                e.Veto();
                DispatchMessageTo(ToHwnd(this), WM_CLOSE, 0, 0);
            } else {
                e.Skip();
            }
        });
        Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& e) {
            if (!DialogKeyHook(this, e))
                e.Skip();
        });
    }
};

class MfcPanel : public wxPanel {
public:
    MfcPanel(wxWindow* parent, const wxPoint& pos, const wxSize& size)
        : wxPanel(parent, wxID_ANY, pos, size, wxTAB_TRAVERSAL | wxBORDER_NONE) {}
};

} // namespace

// Creates the window of a dialog from its template; returns null on failure.
wxWindow* CreateDialogWindow(CDialog* dlg, const rc::Dialog& tmpl, wxWindow* parent) {
    wxFont font = DialogFontFor(&tmpl);
    wxSize base = BaseUnits(font);
    wxRect r(MulDiv(tmpl.x, base.x, 4), MulDiv(tmpl.y, base.y, 8), MulDiv(tmpl.cx, base.x, 4),
             MulDiv(tmpl.cy, base.y, 8));
    wxWindow* w = nullptr;
    ControlKind kind = ControlKind::Dialog;
    if ((tmpl.style & WS_CHILD) && parent) {
        w = new MfcPanel(parent, r.GetPosition(), r.GetSize());
        kind = ControlKind::Generic;
    } else {
        auto* d = new MfcDialog(parent, wxString::FromUTF8(tmpl.caption ? tmpl.caption : ""), DialogWxStyle(tmpl.style));
        d->SetClientSize(r.GetSize());
        w = d;
    }
    w->SetFont(font);
    HookWindow(w, kind, static_cast<int>(tmpl.id), tmpl.style, tmpl.exStyle);
    WindowState& st = EnsureState(w);
    st.baseUnits = base;
    st.text = wxString::FromUTF8(tmpl.caption ? tmpl.caption : "");
    if (dlg)
        dlg->AttachWx(w);
    CreateDialogControls(w, tmpl);
    if (w->IsTopLevel()) {
        if (tmpl.style & (DS_CENTER | DS_CENTERMOUSE) || true)
            w->CentreOnParent();
    }
    return w;
}

} // namespace mfcwx

using namespace mfcwx;

// ---------------------------------------------------------------------------------------------
// CDialog

IMPLEMENT_DYNAMIC(CDialog, CWnd)

BEGIN_MESSAGE_MAP(CDialog, CWnd)
    ON_COMMAND(IDOK, &CDialog::OnOK)
    ON_COMMAND(IDCANCEL, &CDialog::OnCancel)
    ON_COMMAND(ID_HELP, &CDialog::OnHelp)
    ON_MESSAGE(WM_INITDIALOG, &CDialog::HandleInitDialog)
END_MESSAGE_MAP()

CDialog::CDialog()
    : m_nIDHelp(0), m_lpszTemplateName(nullptr), m_nIDTemplate(0), m_pParentWnd(nullptr), m_bModalRunning(FALSE),
      m_nModalResult(-1) {}

CDialog::CDialog(const char* lpszTemplateName, CWnd* pParentWnd) : CDialog() {
    if (IS_INTRESOURCE(lpszTemplateName))
        m_nIDTemplate = static_cast<UINT>(reinterpret_cast<uintptr_t>(lpszTemplateName));
    else
        m_lpszTemplateName = lpszTemplateName;
    m_pParentWnd = pParentWnd;
    m_nIDHelp = m_nIDTemplate;
}

CDialog::CDialog(UINT nIDTemplate, CWnd* pParentWnd) : CDialog() {
    m_nIDTemplate = nIDTemplate;
    m_pParentWnd = pParentWnd;
    m_nIDHelp = nIDTemplate;
}

CDialog::~CDialog() {
    if (m_hWnd)
        DestroyWindow();
}

BOOL CDialog::CreateFromTemplate(BOOL bModal) {
    ResRef ref = m_lpszTemplateName ? ResRef::From(m_lpszTemplateName) : ResRef::FromId(m_nIDTemplate);
    const rc::Dialog* tmpl = FindDialog(ref);
    if (!tmpl) {
        wxLogDebug("mfcwx: dialog template %d not found", static_cast<int>(m_nIDTemplate));
        return FALSE;
    }
    CWnd* parent = m_pParentWnd;
    if (!parent && bModal)
        parent = AfxGetMainWnd();
    wxWindow* parentWx = parent && parent->m_hWnd ? parent->GetWx() : nullptr;
    if (parentWx && bModal)
        parentWx = wxGetTopLevelParent(parentWx);
    wxWindow* w = CreateDialogWindow(this, *tmpl, parentWx);
    if (!w)
        return FALSE;
    ApplyDlgInit(this, tmpl->id);
    m_nModalResult = -1;
    m_bModalRunning = bModal;
    if (!SendMessage(WM_INITDIALOG, 0, 0) && m_hWnd) {
    }
    if (!m_hWnd)
        return FALSE;
    for (wxWindow* c : w->GetChildren())
        ApplyCtlColor(c);
    if (!bModal && (tmpl->style & WS_VISIBLE))
        w->Show();
    return TRUE;
}

LRESULT CDialog::HandleInitDialog(WPARAM, LPARAM) {
    BOOL focusDefault = OnInitDialog();
    if (focusDefault && m_hWnd) {
        HWND first = ::GetNextDlgTabItem(m_hWnd, nullptr, FALSE);
        if (first)
            ::SetFocus(first);
    }
    return focusDefault;
}

BOOL CDialog::OnInitDialog() {
    UpdateData(FALSE);
    return TRUE;
}

INT_PTR CDialog::DoModal() {
    return OnMain([&]() -> INT_PTR {
        if (!CreateFromTemplate(TRUE))
            return -1;
        wxWindow* w = GetWx();
        auto* dlg = wxDynamicCast(w, wxDialog);
        if (dlg && m_bModalRunning && m_nModalResult == -1) {
            wxWindow* active = wxGetActiveWindow();
            dlg->ShowModal();
            if (active && active != dlg && !active->IsBeingDeleted())
                active->Raise();
        }
        m_bModalRunning = FALSE;
        int result = m_nModalResult;
        if (m_hWnd)
            DestroyWindow();
        return result;
    });
}

void CDialog::EndDialog(int nResult) {
    OnMain([&] {
        m_nModalResult = nResult;
        if (!m_hWnd)
            return;
        auto* dlg = wxDynamicCast(GetWx(), wxDialog);
        if (m_bModalRunning) {
            if (dlg && dlg->IsModal())
                dlg->EndModal(nResult);
        } else if (dlg) {
            dlg->Hide();
        }
    });
}

BOOL CDialog::Create(UINT nIDTemplate, CWnd* pParentWnd) {
    m_nIDTemplate = nIDTemplate;
    m_lpszTemplateName = nullptr;
    m_pParentWnd = pParentWnd;
    if (!m_nIDHelp)
        m_nIDHelp = nIDTemplate;
    return OnMain([&]() -> BOOL { return CreateFromTemplate(FALSE); });
}

BOOL CDialog::Create(const char* lpszTemplateName, CWnd* pParentWnd) {
    if (IS_INTRESOURCE(lpszTemplateName))
        return Create(static_cast<UINT>(reinterpret_cast<uintptr_t>(lpszTemplateName)), pParentWnd);
    m_lpszTemplateName = lpszTemplateName;
    m_pParentWnd = pParentWnd;
    return OnMain([&]() -> BOOL { return CreateFromTemplate(FALSE); });
}

BOOL CDialog::CreateIndirect(const void*, CWnd*, void*) { return FALSE; }
BOOL CDialog::InitModalIndirect(const void*, CWnd*, void*) { return FALSE; }

BOOL CDialog::DestroyWindow() {
    if (m_hWnd && m_bModalRunning) {
        EndDialog(IDCANCEL);
        m_bModalRunning = FALSE;
    }
    return CWnd::DestroyWindow();
}

void CDialog::OnOK() {
    if (!UpdateData(TRUE))
        return;
    EndDialog(IDOK);
}

void CDialog::OnCancel() { EndDialog(IDCANCEL); }

void CDialog::OnHelp() {
    CWinApp* app = AfxGetApp();
    if (app && m_nIDHelp)
        app->WinHelpInternal(0x20000 + m_nIDHelp, HELP_CONTEXT);
}

void CDialog::MapDialogRect(LPRECT lpRect) const { ::MapDialogRect(m_hWnd, lpRect); }

void CDialog::SetDefID(UINT nID) { SendMessage(DM_SETDEFID, nID, 0); }

DWORD CDialog::GetDefID() const { return static_cast<DWORD>(const_cast<CDialog*>(this)->SendMessage(DM_GETDEFID)); }

void CDialog::NextDlgCtrl() const { const_cast<CDialog*>(this)->SendMessage(WM_NEXTDLGCTL, 0, 0); }
void CDialog::PrevDlgCtrl() const { const_cast<CDialog*>(this)->SendMessage(WM_NEXTDLGCTL, 1, 0); }

void CDialog::GotoDlgCtrl(CWnd* pWndCtrl) {
    if (pWndCtrl && pWndCtrl->m_hWnd)
        SendMessage(WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(pWndCtrl->m_hWnd), 1);
}

// ---------------------------------------------------------------------------------------------
// Data exchange

CDataExchange::CDataExchange(CWnd* pDlgWnd, BOOL bSaveAndValidate)
    : m_bSaveAndValidate(bSaveAndValidate), m_pDlgWnd(pDlgWnd), m_hWndLastControl(nullptr), m_bEditLastControl(FALSE) {}

HWND CDataExchange::PrepareCtrl(int nIDC) {
    HWND h = ::GetDlgItem(m_pDlgWnd->m_hWnd, nIDC);
    if (!h)
        wxLogDebug("mfcwx: DDX control %d not found", nIDC);
    m_hWndLastControl = h;
    m_bEditLastControl = FALSE;
    return h;
}

HWND CDataExchange::PrepareEditCtrl(int nIDC) {
    HWND h = PrepareCtrl(nIDC);
    m_bEditLastControl = TRUE;
    return h;
}

void CDataExchange::Fail() {
    if (m_bSaveAndValidate && m_hWndLastControl) {
        ::SetFocus(m_hWndLastControl);
        if (m_bEditLastControl)
            ::SendMessage(m_hWndLastControl, EM_SETSEL, 0, -1);
    }
    throw new CUserException();
}

namespace {

CString PromptText(UINT id, const char* fallback) {
    CString s;
    if (!s.LoadString(id))
        s = fallback;
    return s;
}

void FailWith(CDataExchange* pDX, UINT id, const char* fallback, const char* a1 = nullptr, const char* a2 = nullptr) {
    CString text = PromptText(id, fallback);
    if (a1) {
        text.Replace("%1", a1);
        if (a2)
            text.Replace("%2", a2);
    }
    AfxMessageBox(text, MB_ICONEXCLAMATION);
    pDX->Fail();
}

void SetTextIfChanged(HWND h, const char* text) {
    char buf[256];
    int len = ::GetWindowTextLength(h);
    if (len < static_cast<int>(sizeof buf)) {
        ::GetWindowText(h, buf, sizeof buf);
        if (strcmp(buf, text) == 0)
            return;
    }
    ::SetWindowText(h, text);
}

CString WindowText(HWND h) {
    int len = ::GetWindowTextLength(h);
    CString s;
    ::GetWindowText(h, s.GetBuffer(len + 1), len + 1);
    s.ReleaseBuffer();
    return s;
}

template <class T>
void TextSigned(CDataExchange* pDX, int nIDC, T& value) {
    HWND h = pDX->PrepareEditCtrl(nIDC);
    if (!h)
        return;
    if (pDX->m_bSaveAndValidate) {
        CString s = WindowText(h);
        s.Trim();
        char* end = nullptr;
        long long v = strtoll(s, &end, 10);
        if (s.IsEmpty() || (end && *end))
            FailWith(pDX, AFX_IDP_PARSE_INT, "Please enter an integer.");
        value = static_cast<T>(v);
    } else {
        char buf[32];
        snprintf(buf, sizeof buf, "%lld", static_cast<long long>(value));
        SetTextIfChanged(h, buf);
    }
}

template <class T>
void TextUnsigned(CDataExchange* pDX, int nIDC, T& value) {
    HWND h = pDX->PrepareEditCtrl(nIDC);
    if (!h)
        return;
    if (pDX->m_bSaveAndValidate) {
        CString s = WindowText(h);
        s.Trim();
        char* end = nullptr;
        unsigned long long v = strtoull(s, &end, 10);
        if (s.IsEmpty() || (end && *end) || s[0] == '-')
            FailWith(pDX, AFX_IDP_PARSE_UINT, "Please enter a positive integer.");
        value = static_cast<T>(v);
    } else {
        char buf[32];
        snprintf(buf, sizeof buf, "%llu", static_cast<unsigned long long>(value));
        SetTextIfChanged(h, buf);
    }
}

template <class T>
void TextFloat(CDataExchange* pDX, int nIDC, T& value, int digits) {
    HWND h = pDX->PrepareEditCtrl(nIDC);
    if (!h)
        return;
    if (pDX->m_bSaveAndValidate) {
        CString s = WindowText(h);
        s.Trim();
        char* end = nullptr;
        double v = strtod(s, &end);
        if (s.IsEmpty() || (end && *end))
            FailWith(pDX, AFX_IDP_PARSE_REAL, "Please enter a number.");
        value = static_cast<T>(v);
    } else {
        char buf[64];
        snprintf(buf, sizeof buf, "%.*g", digits, static_cast<double>(value));
        SetTextIfChanged(h, buf);
    }
}

template <class T>
void MinMax(CDataExchange* pDX, T value, T minVal, T maxVal, UINT id, const char* fmt) {
    if (!pDX->m_bSaveAndValidate || (value >= minVal && value <= maxVal))
        return;
    char a[64], b[64];
    snprintf(a, sizeof a, fmt, minVal);
    snprintf(b, sizeof b, fmt, maxVal);
    FailWith(pDX, id, "Please enter a value between %1 and %2.", a, b);
}

std::vector<HWND> RadioGroup(CDataExchange* pDX, int nIDC) {
    std::vector<HWND> group;
    HWND first = pDX->PrepareCtrl(nIDC);
    if (!first)
        return group;
    for (HWND h = first; h; h = ::GetWindow(h, GW_HWNDNEXT)) {
        if (h != first && (::GetWindowLongPtr(h, GWL_STYLE) & WS_GROUP))
            break;
        WindowState* st = GetState(ToWx(h));
        if (st && st->kind == ControlKind::RadioButton)
            group.push_back(h);
    }
    return group;
}

} // namespace

void AFXAPI DDX_Control(CDataExchange* pDX, int nIDC, CWnd& rControl) {
    if (rControl.m_hWnd)
        return;
    HWND h = pDX->PrepareCtrl(nIDC);
    if (h)
        rControl.SubclassWindow(h);
}

void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, BYTE& value) {
    int v = value;
    TextSigned(pDX, nIDC, v);
    if (pDX->m_bSaveAndValidate) {
        if (v < 0 || v > 255)
            FailWith(pDX, AFX_IDP_PARSE_BYTE, "Please enter an integer between 0 and 255.");
        value = static_cast<BYTE>(v);
    }
}
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, short& value) { TextSigned(pDX, nIDC, value); }
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, int& value) { TextSigned(pDX, nIDC, value); }
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, UINT& value) { TextUnsigned(pDX, nIDC, value); }
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, long& value) { TextSigned(pDX, nIDC, value); }
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, unsigned long& value) { TextUnsigned(pDX, nIDC, value); }
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, LONGLONG& value) { TextSigned(pDX, nIDC, value); }
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, ULONGLONG& value) { TextUnsigned(pDX, nIDC, value); }
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, float& value) { TextFloat(pDX, nIDC, value, FLT_DIG); }
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, double& value) { TextFloat(pDX, nIDC, value, DBL_DIG); }

void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, CString& value) {
    HWND h = pDX->PrepareEditCtrl(nIDC);
    if (!h)
        return;
    if (pDX->m_bSaveAndValidate)
        value = WindowText(h);
    else
        SetTextIfChanged(h, value);
}

void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, char* value, int nMaxLen) {
    HWND h = pDX->PrepareEditCtrl(nIDC);
    if (!h)
        return;
    if (pDX->m_bSaveAndValidate)
        ::GetWindowText(h, value, nMaxLen);
    else
        SetTextIfChanged(h, value);
}

void AFXAPI DDX_Check(CDataExchange* pDX, int nIDC, int& value) {
    HWND h = pDX->PrepareCtrl(nIDC);
    if (!h)
        return;
    if (pDX->m_bSaveAndValidate)
        value = static_cast<int>(::SendMessage(h, BM_GETCHECK, 0, 0));
    else
        ::SendMessage(h, BM_SETCHECK, static_cast<WPARAM>(value), 0);
}

void AFXAPI DDX_Check(CDataExchange* pDX, int nIDC, bool& value) {
    int v = value ? 1 : 0;
    DDX_Check(pDX, nIDC, v);
    value = v != 0;
}

void AFXAPI DDX_Radio(CDataExchange* pDX, int nIDC, int& value) {
    std::vector<HWND> group = RadioGroup(pDX, nIDC);
    if (pDX->m_bSaveAndValidate) {
        value = -1;
        for (size_t i = 0; i < group.size(); ++i)
            if (::SendMessage(group[i], BM_GETCHECK, 0, 0) == BST_CHECKED)
                value = static_cast<int>(i);
    } else {
        for (size_t i = 0; i < group.size(); ++i)
            ::SendMessage(group[i], BM_SETCHECK, static_cast<int>(i) == value ? BST_CHECKED : BST_UNCHECKED, 0);
    }
}

void AFXAPI DDX_LBString(CDataExchange* pDX, int nIDC, CString& value) {
    HWND h = pDX->PrepareCtrl(nIDC);
    if (!h)
        return;
    if (pDX->m_bSaveAndValidate) {
        int sel = static_cast<int>(::SendMessage(h, LB_GETCURSEL, 0, 0));
        if (sel == LB_ERR) {
            value.Empty();
            return;
        }
        int len = static_cast<int>(::SendMessage(h, LB_GETTEXTLEN, static_cast<WPARAM>(sel), 0));
        ::SendMessage(h, LB_GETTEXT, static_cast<WPARAM>(sel), reinterpret_cast<LPARAM>(value.GetBuffer(len + 1)));
        value.ReleaseBuffer();
    } else if (::SendMessage(h, LB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(value.GetString())) == LB_ERR) {
        ::SendMessage(h, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
    }
}

void AFXAPI DDX_LBStringExact(CDataExchange* pDX, int nIDC, CString& value) {
    HWND h = pDX->PrepareCtrl(nIDC);
    if (!h)
        return;
    if (pDX->m_bSaveAndValidate) {
        DDX_LBString(pDX, nIDC, value);
        return;
    }
    LRESULT i = ::SendMessage(h, LB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(value.GetString()));
    ::SendMessage(h, LB_SETCURSEL, static_cast<WPARAM>(i == LB_ERR ? -1 : i), 0);
}

void AFXAPI DDX_CBString(CDataExchange* pDX, int nIDC, CString& value) {
    HWND h = pDX->PrepareCtrl(nIDC);
    if (!h)
        return;
    WindowState* st = GetState(ToWx(h));
    if (pDX->m_bSaveAndValidate) {
        if (st && st->kind == ControlKind::ComboList) {
            int sel = static_cast<int>(::SendMessage(h, CB_GETCURSEL, 0, 0));
            if (sel == CB_ERR) {
                value.Empty();
                return;
            }
            int len = static_cast<int>(::SendMessage(h, CB_GETLBTEXTLEN, static_cast<WPARAM>(sel), 0));
            ::SendMessage(h, CB_GETLBTEXT, static_cast<WPARAM>(sel), reinterpret_cast<LPARAM>(value.GetBuffer(len + 1)));
            value.ReleaseBuffer();
        } else {
            value = WindowText(h);
        }
    } else {
        if (::SendMessage(h, CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(value.GetString())) == CB_ERR)
            ::SetWindowText(h, value);
    }
}

void AFXAPI DDX_CBStringExact(CDataExchange* pDX, int nIDC, CString& value) {
    HWND h = pDX->PrepareCtrl(nIDC);
    if (!h)
        return;
    if (pDX->m_bSaveAndValidate) {
        DDX_CBString(pDX, nIDC, value);
        return;
    }
    LRESULT i = ::SendMessage(h, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(value.GetString()));
    if (i == CB_ERR)
        ::SetWindowText(h, value);
    else
        ::SendMessage(h, CB_SETCURSEL, static_cast<WPARAM>(i), 0);
}

void AFXAPI DDX_LBIndex(CDataExchange* pDX, int nIDC, int& index) {
    HWND h = pDX->PrepareCtrl(nIDC);
    if (!h)
        return;
    if (pDX->m_bSaveAndValidate)
        index = static_cast<int>(::SendMessage(h, LB_GETCURSEL, 0, 0));
    else
        ::SendMessage(h, LB_SETCURSEL, static_cast<WPARAM>(index), 0);
}

void AFXAPI DDX_CBIndex(CDataExchange* pDX, int nIDC, int& index) {
    HWND h = pDX->PrepareCtrl(nIDC);
    if (!h)
        return;
    if (pDX->m_bSaveAndValidate)
        index = static_cast<int>(::SendMessage(h, CB_GETCURSEL, 0, 0));
    else
        ::SendMessage(h, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
}

void AFXAPI DDX_Scroll(CDataExchange* pDX, int nIDC, int& value) {
    HWND h = pDX->PrepareCtrl(nIDC);
    if (!h)
        return;
    if (pDX->m_bSaveAndValidate)
        value = static_cast<int>(::SendMessage(h, SBM_GETPOS, 0, 0));
    else
        ::SendMessage(h, SBM_SETPOS, static_cast<WPARAM>(value), TRUE);
}

void AFXAPI DDX_Slider(CDataExchange* pDX, int nIDC, int& value) {
    HWND h = pDX->PrepareCtrl(nIDC);
    if (!h)
        return;
    if (pDX->m_bSaveAndValidate)
        value = static_cast<int>(::SendMessage(h, TBM_GETPOS, 0, 0));
    else
        ::SendMessage(h, TBM_SETPOS, TRUE, value);
}

void AFXAPI DDV_MaxChars(CDataExchange* pDX, const CString& value, int nChars) {
    if (pDX->m_bSaveAndValidate) {
        if (value.GetLength() > nChars) {
            char n[32];
            snprintf(n, sizeof n, "%d", nChars);
            FailWith(pDX, AFX_IDP_PARSE_STRING_SIZE, "Please enter no more than %1 characters.", n);
        }
    } else if (pDX->m_hWndLastControl && pDX->m_bEditLastControl) {
        WindowState* st = GetState(ToWx(pDX->m_hWndLastControl));
        if (st && st->kind == ControlKind::ComboBox)
            ::SendMessage(pDX->m_hWndLastControl, CB_LIMITTEXT, static_cast<WPARAM>(nChars), 0);
        else
            ::SendMessage(pDX->m_hWndLastControl, EM_LIMITTEXT, static_cast<WPARAM>(nChars), 0);
    }
}

void AFXAPI DDV_MinMaxByte(CDataExchange* pDX, BYTE value, BYTE minVal, BYTE maxVal) {
    MinMax<int>(pDX, value, minVal, maxVal, AFX_IDP_PARSE_INT_RANGE, "%d");
}
void AFXAPI DDV_MinMaxShort(CDataExchange* pDX, short value, short minVal, short maxVal) {
    MinMax<int>(pDX, value, minVal, maxVal, AFX_IDP_PARSE_INT_RANGE, "%d");
}
void AFXAPI DDV_MinMaxInt(CDataExchange* pDX, int value, int minVal, int maxVal) {
    MinMax<int>(pDX, value, minVal, maxVal, AFX_IDP_PARSE_INT_RANGE, "%d");
}
void AFXAPI DDV_MinMaxLong(CDataExchange* pDX, long value, long minVal, long maxVal) {
    MinMax<long>(pDX, value, minVal, maxVal, AFX_IDP_PARSE_INT_RANGE, "%ld");
}
void AFXAPI DDV_MinMaxUInt(CDataExchange* pDX, UINT value, UINT minVal, UINT maxVal) {
    MinMax<UINT>(pDX, value, minVal, maxVal, AFX_IDP_PARSE_INT_RANGE, "%u");
}
void AFXAPI DDV_MinMaxDWord(CDataExchange* pDX, DWORD value, DWORD minVal, DWORD maxVal) {
    MinMax<DWORD>(pDX, value, minVal, maxVal, AFX_IDP_PARSE_INT_RANGE, "%u");
}
void AFXAPI DDV_MinMaxLongLong(CDataExchange* pDX, LONGLONG value, LONGLONG minVal, LONGLONG maxVal) {
    MinMax<LONGLONG>(pDX, value, minVal, maxVal, AFX_IDP_PARSE_INT_RANGE, "%lld");
}
void AFXAPI DDV_MinMaxULongLong(CDataExchange* pDX, ULONGLONG value, ULONGLONG minVal, ULONGLONG maxVal) {
    MinMax<ULONGLONG>(pDX, value, minVal, maxVal, AFX_IDP_PARSE_INT_RANGE, "%llu");
}
void AFXAPI DDV_MinMaxFloat(CDataExchange* pDX, float const& value, float minVal, float maxVal) {
    MinMax<double>(pDX, value, minVal, maxVal, AFX_IDP_PARSE_REAL_RANGE, "%g");
}
void AFXAPI DDV_MinMaxDouble(CDataExchange* pDX, double const& value, double minVal, double maxVal) {
    MinMax<double>(pDX, value, minVal, maxVal, AFX_IDP_PARSE_REAL_RANGE, "%g");
}
void AFXAPI DDV_MinMaxSlider(CDataExchange* pDX, DWORD value, DWORD minVal, DWORD maxVal) {
    MinMax<DWORD>(pDX, value, minVal, maxVal, AFX_IDP_PARSE_INT_RANGE, "%u");
}

// ---------------------------------------------------------------------------------------------
// Common dialogs

IMPLEMENT_DYNAMIC(CCommonDialog, CDialog)
IMPLEMENT_DYNAMIC(CFileDialog, CCommonDialog)
IMPLEMENT_DYNAMIC(CColorDialog, CCommonDialog)
IMPLEMENT_DYNAMIC(CFontDialog, CCommonDialog)
IMPLEMENT_DYNAMIC(CPageSetupDialog, CCommonDialog)
IMPLEMENT_DYNAMIC(CPrintDialog, CCommonDialog)
IMPLEMENT_DYNAMIC(CFindReplaceDialog, CCommonDialog)
IMPLEMENT_DYNAMIC(CPropertyPage, CDialog)
IMPLEMENT_DYNAMIC(CPropertySheet, CWnd)

namespace {

wxWindow* OwnerWindow(CWnd* pParent) {
    if (pParent && pParent->m_hWnd)
        return wxGetTopLevelParent(pParent->GetWx());
    return MainWxWindow();
}

// "Text (*.txt)|*.txt|All files (*.*)|*.*||" -> wx wildcard
wxString MfcFilterToWx(const CString& filter) {
    wxString f = ToWx(filter);
    while (f.EndsWith("|"))
        f.RemoveLast();
    return f.empty() ? wxString("*") : f;
}

} // namespace

CFileDialog::CFileDialog(BOOL bOpenFileDialog, const char* lpszDefExt, const char* lpszFileName, DWORD dwFlags,
                         const char* lpszFilter, CWnd* pParentWnd, DWORD, BOOL)
    : CCommonDialog(pParentWnd), m_bOpenFileDialog(bOpenFileDialog) {
    memset(&m_ofn, 0, sizeof m_ofn);
    m_ofn.lStructSize = sizeof m_ofn;
    m_ofn.Flags = dwFlags;
    m_szFileName[0] = 0;
    m_szFileTitle[0] = 0;
    if (lpszFileName) {
        strncpy(m_szFileName, lpszFileName, sizeof m_szFileName - 1);
        m_szFileName[sizeof m_szFileName - 1] = 0;
    }
    m_ofn.lpstrFile = m_szFileName;
    m_ofn.nMaxFile = sizeof m_szFileName;
    m_ofn.lpstrFileTitle = m_szFileTitle;
    m_ofn.nMaxFileTitle = sizeof m_szFileTitle;
    m_ofn.nFilterIndex = 1;
    if (lpszDefExt)
        m_strDefExt = lpszDefExt;
    if (lpszFilter)
        m_strFilter = lpszFilter;
}

INT_PTR CFileDialog::DoModal() {
    return OnMain([&]() -> INT_PTR {
        long style = m_bOpenFileDialog ? wxFD_OPEN : wxFD_SAVE;
        if (m_bOpenFileDialog && (m_ofn.Flags & OFN_FILEMUSTEXIST))
            style |= wxFD_FILE_MUST_EXIST;
        if (!m_bOpenFileDialog && (m_ofn.Flags & OFN_OVERWRITEPROMPT))
            style |= wxFD_OVERWRITE_PROMPT;
        if (m_ofn.Flags & OFN_ALLOWMULTISELECT)
            style |= wxFD_MULTIPLE;
        wxString initialDir;
        if (m_ofn.lpstrInitialDir && *m_ofn.lpstrInitialDir)
            initialDir = NativePathWx(m_ofn.lpstrInitialDir);
        else if (!m_strInitialDir.IsEmpty())
            initialDir = NativePathWx(m_strInitialDir);
        wxFileName current(NativePathWx(m_ofn.lpstrFile ? m_ofn.lpstrFile : ""));
        if (initialDir.empty() && current.IsAbsolute())
            initialDir = current.GetPath();
        wxString title = m_ofn.lpstrTitle ? ToWx(m_ofn.lpstrTitle) : ToWx(m_strTitle);
        wxFileDialog dlg(OwnerWindow(m_pParentWnd), title, initialDir, current.GetFullName(), MfcFilterToWx(m_strFilter),
                         style);
        if (m_ofn.nFilterIndex > 0)
            dlg.SetFilterIndex(static_cast<int>(m_ofn.nFilterIndex) - 1);
        if (dlg.ShowModal() != wxID_OK)
            return IDCANCEL;
        m_paths.clear();
        wxArrayString paths;
        if (style & wxFD_MULTIPLE)
            dlg.GetPaths(paths);
        else
            paths.Add(dlg.GetPath());
        for (const wxString& p : paths) {
            std::string a = AppPath(p.utf8_str());
            CString path(a.c_str());
            if (!m_bOpenFileDialog && !m_strDefExt.IsEmpty()) {
                int slash = path.ReverseFind('\\');
                if (path.Find('.', slash + 1) < 0)
                    path += "." + m_strDefExt;
            }
            m_paths.push_back(path);
        }
        strncpy(m_szFileName, m_paths.front(), sizeof m_szFileName - 1);
        m_szFileName[sizeof m_szFileName - 1] = 0;
        CString title2 = GetFileName();
        strncpy(m_szFileTitle, title2, sizeof m_szFileTitle - 1);
        m_szFileTitle[sizeof m_szFileTitle - 1] = 0;
        m_ofn.nFilterIndex = static_cast<DWORD>(dlg.GetFilterIndex() + 1);
        return IDOK;
    });
}

CString CFileDialog::GetPathName() const { return m_paths.empty() ? CString(m_szFileName) : m_paths.front(); }

CString CFileDialog::GetFileName() const {
    CString p = GetPathName();
    int slash = p.ReverseFind('\\');
    return p.Mid(slash + 1);
}

CString CFileDialog::GetFileExt() const {
    CString n = GetFileName();
    int dot = n.ReverseFind('.');
    return dot < 0 ? CString() : n.Mid(dot + 1);
}

CString CFileDialog::GetFileTitle() const {
    CString n = GetFileName();
    int dot = n.ReverseFind('.');
    return dot < 0 ? n : n.Left(dot);
}

CString CFileDialog::GetFolderPath() const {
    CString p = GetPathName();
    int slash = p.ReverseFind('\\');
    return slash < 0 ? CString() : p.Left(slash);
}

POSITION CFileDialog::GetStartPosition() const {
    return m_paths.empty() ? nullptr : reinterpret_cast<POSITION>(static_cast<uintptr_t>(1));
}

CString CFileDialog::GetNextPathName(POSITION& pos) const {
    size_t i = static_cast<size_t>(reinterpret_cast<uintptr_t>(pos)) - 1;
    CString r = i < m_paths.size() ? m_paths[i] : CString();
    pos = i + 1 < m_paths.size() ? reinterpret_cast<POSITION>(static_cast<uintptr_t>(i + 2)) : nullptr;
    return r;
}

CColorDialog::CColorDialog(COLORREF clrInit, DWORD dwFlags, CWnd* pParentWnd) : CCommonDialog(pParentWnd) {
    memset(&m_cc, 0, sizeof m_cc);
    m_cc.lStructSize = sizeof m_cc;
    m_cc.rgbResult = clrInit;
    m_cc.Flags = dwFlags;
}

INT_PTR CColorDialog::DoModal() {
    return OnMain([&]() -> INT_PTR {
        wxColourData data;
        data.SetColour(ToWxColour(m_cc.rgbResult));
        wxColourDialog dlg(OwnerWindow(m_pParentWnd), &data);
        if (dlg.ShowModal() != wxID_OK)
            return IDCANCEL;
        m_cc.rgbResult = FromWxColour(dlg.GetColourData().GetColour());
        return IDOK;
    });
}

CFontDialog::CFontDialog(LPLOGFONT lplfInitial, DWORD dwFlags, CDC*, CWnd* pParentWnd) : CCommonDialog(pParentWnd) {
    memset(&m_cf, 0, sizeof m_cf);
    memset(&m_lf, 0, sizeof m_lf);
    m_cf.lStructSize = sizeof m_cf;
    m_cf.Flags = dwFlags;
    m_cf.lpLogFont = &m_lf;
    if (lplfInitial)
        m_lf = *lplfInitial;
}

INT_PTR CFontDialog::DoModal() {
    return OnMain([&]() -> INT_PTR {
        wxFontData data;
        if (m_lf.lfFaceName[0]) {
            CFont initial;
            initial.CreateFontIndirect(&m_lf);
            data.SetInitialFont(FontFromHandle(reinterpret_cast<HFONT>(initial.m_hObject)));
        }
        data.SetColour(ToWxColour(m_cf.rgbColors));
        wxFontDialog dlg(OwnerWindow(m_pParentWnd), data);
        if (dlg.ShowModal() != wxID_OK)
            return IDCANCEL;
        wxFont f = dlg.GetFontData().GetChosenFont();
        std::string face = mfcwx::FromWx(f.GetFaceName());
        memset(m_lf.lfFaceName, 0, sizeof m_lf.lfFaceName);
        strncpy(m_lf.lfFaceName, face.c_str(), sizeof m_lf.lfFaceName - 1);
        m_lf.lfHeight = -MulDiv(f.GetPointSize(), 96, 72);
        m_lf.lfWeight = f.GetWeight() >= wxFONTWEIGHT_BOLD ? FW_BOLD : FW_NORMAL;
        m_lf.lfItalic = f.GetStyle() == wxFONTSTYLE_ITALIC;
        m_lf.lfUnderline = f.GetUnderlined();
        m_cf.iPointSize = f.GetPointSize() * 10;
        m_cf.rgbColors = FromWxColour(dlg.GetFontData().GetColour());
        if (m_cf.lpLogFont && m_cf.lpLogFont != &m_lf)
            *m_cf.lpLogFont = m_lf;
        return IDOK;
    });
}

void CFontDialog::GetCurrentFont(LPLOGFONT lplf) {
    if (lplf)
        *lplf = m_lf;
}

CString CFontDialog::GetFaceName() const { return CString(m_lf.lfFaceName); }
int CFontDialog::GetSize() const { return m_cf.iPointSize; }

CPageSetupDialog::CPageSetupDialog(DWORD dwFlags, CWnd* pParentWnd) : CCommonDialog(pParentWnd) {
    memset(&m_psd, 0, sizeof m_psd);
    m_psd.Flags = dwFlags;
}

INT_PTR CPageSetupDialog::DoModal() { return IDCANCEL; }
CSize CPageSetupDialog::GetPaperSize() const { return CSize(21000, 29700); }
void CPageSetupDialog::GetMargins(LPRECT lpRectMargins, LPRECT lpRectMinMargins) const {
    if (lpRectMargins)
        *lpRectMargins = m_psd.rtMargin;
    if (lpRectMinMargins)
        *lpRectMinMargins = m_psd.rtMinMargin;
}

CPrintDialog::CPrintDialog(BOOL, DWORD dwFlags, CWnd* pParentWnd) : CCommonDialog(pParentWnd) {
    memset(&m_pd, 0, sizeof m_pd);
    m_pd.lStructSize = sizeof m_pd;
    m_pd.Flags = dwFlags;
}

INT_PTR CPrintDialog::DoModal() { return IDCANCEL; }

namespace {

std::map<wxFindReplaceDialog*, CFindReplaceDialog*>& FindDialogs() {
    static std::map<wxFindReplaceDialog*, CFindReplaceDialog*> m;
    return m;
}

} // namespace

CFindReplaceDialog::CFindReplaceDialog() : CCommonDialog(nullptr), m_flags(FR_DOWN) {}

BOOL CFindReplaceDialog::Create(BOOL bFindDialogOnly, const char* lpszFindWhat, const char* lpszReplaceWith,
                                DWORD dwFlags, CWnd* pParentWnd) {
    return OnMain([&]() -> BOOL {
        m_find = lpszFindWhat ? lpszFindWhat : "";
        m_replace = lpszReplaceWith ? lpszReplaceWith : "";
        m_flags = dwFlags;
        m_pParentWnd = pParentWnd;
        auto* data = new wxFindReplaceData(static_cast<wxUint32>((dwFlags & FR_DOWN ? wxFR_DOWN : 0) |
                                                                 (dwFlags & FR_MATCHCASE ? wxFR_MATCHCASE : 0) |
                                                                 (dwFlags & FR_WHOLEWORD ? wxFR_WHOLEWORD : 0)));
        data->SetFindString(ToWx(m_find));
        data->SetReplaceString(ToWx(m_replace));
        auto* dlg = new wxFindReplaceDialog(OwnerWindow(pParentWnd), data, wxString(),
                                            bFindDialogOnly ? 0 : wxFR_REPLACEDIALOG);
        HookWindow(dlg, ControlKind::Dialog, 0, WS_POPUP | WS_CAPTION, 0);
        AttachWx(dlg);
        FindDialogs()[dlg] = this;
        static const UINT findMsg = RegisterWindowMessage(FINDMSGSTRING);
        CWnd* owner = pParentWnd;
        auto notify = [this, dlg, data, owner](DWORD extra) {
            m_find = CStr(data->GetFindString());
            m_replace = CStr(data->GetReplaceString());
            int f = data->GetFlags();
            m_flags = extra | (f & wxFR_DOWN ? FR_DOWN : 0) | (f & wxFR_MATCHCASE ? FR_MATCHCASE : 0) |
                      (f & wxFR_WHOLEWORD ? FR_WHOLEWORD : 0);
            if (owner && owner->m_hWnd)
                owner->SendMessage(findMsg, 0, reinterpret_cast<LPARAM>(this));
            (void)dlg;
        };
        dlg->Bind(wxEVT_FIND, [notify](wxFindDialogEvent&) { notify(FR_FINDNEXT); });
        dlg->Bind(wxEVT_FIND_NEXT, [notify](wxFindDialogEvent&) { notify(FR_FINDNEXT); });
        dlg->Bind(wxEVT_FIND_REPLACE, [notify](wxFindDialogEvent&) { notify(FR_REPLACE); });
        dlg->Bind(wxEVT_FIND_REPLACE_ALL, [notify](wxFindDialogEvent&) { notify(FR_REPLACEALL); });
        dlg->Bind(wxEVT_FIND_CLOSE, [this, notify, dlg, data](wxFindDialogEvent&) {
            notify(FR_DIALOGTERM);
            FindDialogs().erase(dlg);
            DestroyWindow();
            delete data;
        });
        dlg->Show();
        return TRUE;
    });
}

CFindReplaceDialog* CFindReplaceDialog::GetNotifier(LPARAM lParam) { return reinterpret_cast<CFindReplaceDialog*>(lParam); }

CPropertyPage::CPropertyPage() : CDialog() {}
CPropertyPage::CPropertyPage(UINT nIDTemplate, UINT nIDCaption, DWORD) : CDialog(nIDTemplate) {
    if (nIDCaption)
        m_strCaption.LoadString(nIDCaption);
}

CPropertySheet::CPropertySheet() : m_pParentWnd(nullptr), m_active(0) {}
CPropertySheet::CPropertySheet(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
    : m_pParentWnd(pParentWnd), m_active(static_cast<int>(iSelectPage)) {
    m_strCaption.LoadString(nIDCaption);
}
CPropertySheet::CPropertySheet(const char* pszCaption, CWnd* pParentWnd, UINT iSelectPage)
    : m_strCaption(pszCaption), m_pParentWnd(pParentWnd), m_active(static_cast<int>(iSelectPage)) {}

void CPropertySheet::AddPage(CPropertyPage* pPage) { m_pages.push_back(pPage); }

void CPropertySheet::RemovePage(CPropertyPage* pPage) {
    m_pages.erase(std::remove(m_pages.begin(), m_pages.end(), pPage), m_pages.end());
}

BOOL CPropertySheet::SetActivePage(int nPage) {
    m_active = nPage;
    return TRUE;
}

INT_PTR CPropertySheet::DoModal() {
    return OnMain([&]() -> INT_PTR {
        wxDialog dlg(OwnerWindow(m_pParentWnd), wxID_ANY, ToWx(m_strCaption), wxDefaultPosition, wxDefaultSize,
                     wxDEFAULT_DIALOG_STYLE);
        dlg.SetFont(DialogFontFor(nullptr));
        auto* sizer = new wxBoxSizer(wxVERTICAL);
        auto* book = new wxNotebook(&dlg, wxID_ANY);
        sizer->Add(book, 1, wxEXPAND | wxALL, 8);
        HookWindow(book, ControlKind::Generic, 0, WS_CHILD | WS_VISIBLE, 0);
        for (CPropertyPage* page : m_pages) {
            const rc::Dialog* tmpl = FindDialog(ResRef::FromId(page->m_nIDTemplate));
            if (!tmpl)
                continue;
            rc::Dialog child = *tmpl;
            child.style = (child.style & ~WS_POPUP) | WS_CHILD;
            wxWindow* w = CreateDialogWindow(page, child, book);
            CString caption = page->m_strCaption.IsEmpty() ? CString(child.caption ? Utf8ToAnsi(child.caption).c_str() : "")
                                                           : page->m_strCaption;
            book->AddPage(w, ToWx(caption));
            page->SendMessage(WM_INITDIALOG, 0, 0);
        }
        if (m_active >= 0 && m_active < static_cast<int>(book->GetPageCount()))
            book->SetSelection(static_cast<size_t>(m_active));
        sizer->Add(dlg.CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 8);
        dlg.SetSizerAndFit(sizer);
        dlg.CentreOnParent();
        int r = dlg.ShowModal();
        for (CPropertyPage* page : m_pages) {
            if (r == wxID_OK && page->m_hWnd) {
                page->OnKillActive();
                page->OnApply();
            }
            if (page->m_hWnd)
                page->CWnd::DestroyWindow();
        }
        return r == wxID_OK ? IDOK : IDCANCEL;
    });
}
