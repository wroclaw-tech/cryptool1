#include "globalmem.h"
#include "windows_impl.h"

#include <wx/artprov.h>
#include <wx/clipbrd.h>
#include <wx/colordlg.h>
#include <wx/dataobj.h>
#include <wx/display.h>
#include <wx/evtloop.h>
#include <wx/filename.h>
#include <wx/filedlg.h>
#include <wx/utils.h>

#include <map>
#include <mutex>

namespace mfcwx {

wxWindow* LogicalParent(wxWindow* w) {
    if (!w)
        return nullptr;
    WindowState* st = GetState(w);
    if (st && st->owner)
        return ToWx(st->owner);
    for (wxWindow* p = w->GetParent(); p; p = p->GetParent())
        if (IsManagedWindow(p))
            return p;
    return nullptr;
}

void DestroyWxWindow(wxWindow* w) {
    WindowState* st = GetState(w);
    if (st) {
        st->destroyNotified = true;
        CWnd* p = st->permanent;
        st->permanent = nullptr;
        if (p) {
            p->m_hWnd = nullptr;
            p->PostNcDestroy();
        }
    }
    if (w->IsTopLevel())
        w->Hide();
    w->Destroy();
}

namespace {

const wxCursor* g_requestedCursor = nullptr;
bool g_cursorRequested = false;
const wxCursor* g_currentCursor = nullptr;

wxWindow* ManagedAncestor(wxWindow* w) {
    while (w && !IsManagedWindow(w))
        w = w->GetParent();
    return w;
}

std::vector<wxWindow*> ManagedChildren(wxWindow* parent) {
    std::vector<wxWindow*> out;
    if (!parent)
        return out;
    for (wxWindow* c : parent->GetChildren())
        if (IsManagedWindow(c))
            out.push_back(c);
    return out;
}

wxWindow* FindChildById(wxWindow* parent, int id) {
    if (!parent)
        return nullptr;
    for (wxWindow* c : parent->GetChildren()) {
        WindowState* st = GetState(c);
        if (st && st->winId == id)
            return c;
    }
    for (wxWindow* c : parent->GetChildren()) {
        if (IsManagedWindow(c) && !c->IsTopLevel())
            continue;
        if (c->IsTopLevel())
            continue;
        if (wxWindow* r = FindChildById(c, id))
            return r;
    }
    return nullptr;
}

void MarkDefault(HWND hWnd, UINT msg) {
    CurrentMessage* cm = GetCurrentMessageSlot();
    if (cm && cm->event && cm->msg.hwnd == hWnd && cm->msg.message == msg)
        cm->defaultCalled = true;
}

bool IsDialogWindow(wxWindow* w) {
    WindowState* st = GetState(w);
    return st && st->kind == ControlKind::Dialog;
}

} // namespace

void ResetCursorRequest() {
    g_requestedCursor = nullptr;
    g_cursorRequested = true;
}

const wxCursor* RequestedCursor() {
    g_cursorRequested = false;
    return g_requestedCursor;
}

LRESULT DefaultWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    wxWindow* w = ToWx(hWnd);
    WindowState* st = GetState(w);
    if (!st)
        return 0;
    bool handled = false;
    LRESULT r = ControlWindowProc(w, *st, msg, wParam, lParam, handled);
    if (handled)
        return r;
    switch (msg) {
    case WM_SETTEXT:
        SetControlText(w, ToWx(reinterpret_cast<const char*>(lParam)));
        return TRUE;
    case WM_GETTEXT: {
        if (!wParam || !lParam)
            return 0;
        std::string a = FromWx(GetControlText(w));
        size_t n = std::min(a.size(), static_cast<size_t>(wParam) - 1);
        memcpy(reinterpret_cast<char*>(lParam), a.data(), n);
        reinterpret_cast<char*>(lParam)[n] = 0;
        return static_cast<LRESULT>(n);
    }
    case WM_GETTEXTLENGTH:
        return static_cast<LRESULT>(FromWx(GetControlText(w)).size());
    case WM_SETFONT: {
        st->font = reinterpret_cast<HFONT>(wParam);
        wxFont f = st->font ? FontFromHandle(st->font) : wxFont();
        if (f.IsOk()) {
            w->SetFont(f);
            if (st->kind == ControlKind::Static)
                SetControlText(w, st->text);
        }
        if (lParam)
            w->Refresh();
        return 0;
    }
    case WM_GETFONT:
        return reinterpret_cast<LRESULT>(st->font ? st->font : DefaultGuiFont());
    case WM_SETREDRAW:
        if (wParam) {
            if (w->IsFrozen())
                w->Thaw();
        } else if (!w->IsFrozen()) {
            w->Freeze();
        }
        st->redraw = wParam != 0;
        return 0;
    case WM_CLOSE:
        if (IsDialogWindow(w)) {
            wxWindow* cancel = FindChildById(w, IDCANCEL);
            DispatchMessageTo(hWnd, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), reinterpret_cast<LPARAM>(ToHwnd(cancel)));
        } else if (st->permanent) {
            st->permanent->DestroyWindow();
        } else {
            DestroyWxWindow(w);
        }
        return 0;
    case WM_SETICON:
        if (auto* tlw = wxDynamicCast(w, wxTopLevelWindow)) {
            auto* icon = reinterpret_cast<wxIcon*>(lParam);
            if (icon && icon->IsOk())
                tlw->SetIcon(*icon);
        }
        return 0;
    case WM_GETICON:
        return 0;
    case WM_HELP: {
        wxWindow* parent = LogicalParent(w);
        if (parent && !w->IsTopLevel())
            return DispatchMessageTo(ToHwnd(parent), WM_HELP, wParam, lParam);
        return TRUE;
    }
    case WM_SYSCOMMAND:
        switch (wParam & 0xFFF0) {
        case SC_CLOSE:
            DispatchMessageTo(hWnd, WM_CLOSE, 0, 0);
            break;
        case SC_MINIMIZE:
            if (auto* tlw = wxDynamicCast(w, wxTopLevelWindow))
                tlw->Iconize();
            break;
        case SC_MAXIMIZE:
            if (auto* tlw = wxDynamicCast(w, wxTopLevelWindow))
                tlw->Maximize();
            break;
        case SC_RESTORE:
            if (auto* tlw = wxDynamicCast(w, wxTopLevelWindow))
                tlw->Restore();
            break;
        default:
            break;
        }
        return 0;
    case WM_ERASEBKGND:
        MarkDefault(hWnd, msg);
        return 1;
    case DM_GETDEFID:
        return st->defaultButtonId ? MAKELRESULT(st->defaultButtonId, DC_HASDEFID) : 0;
    case DM_SETDEFID: {
        st->defaultButtonId = static_cast<int>(wParam);
        if (auto* b = wxDynamicCast(FindChildById(w, st->defaultButtonId), wxButton))
            b->SetDefault();
        return TRUE;
    }
    case WM_NEXTDLGCTL:
        if (lParam)
            ::SetFocus(reinterpret_cast<HWND>(wParam));
        else
            w->Navigate(wParam ? wxNavigationKeyEvent::IsBackward : wxNavigationKeyEvent::IsForward);
        return 0;
    case WM_CTLCOLORMSGBOX:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSCROLLBAR:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOR:
        return 0;
    case WM_ACTIVATE:
    case WM_SETCURSOR:
    case WM_MOUSEWHEEL:
    default:
        MarkDefault(hWnd, msg);
        return 0;
    }
}

} // namespace mfcwx

using namespace mfcwx;

// ---------------------------------------------------------------------------------------------
// Messages

LRESULT SendMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    if (!hWnd)
        return 0;
    return OnMain([&]() -> LRESULT { return DispatchMessageTo(hWnd, Msg, wParam, lParam); });
}

LRESULT SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) { return SendMessage(hWnd, Msg, wParam, lParam); }

BOOL PostMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    if (!hWnd)
        return Msg == WM_QUIT ? (PostToMainThread([] { if (wxTheApp) wxTheApp->ExitMainLoop(); }), TRUE) : FALSE;
    PostToMainThread([=] {
        if (IsManagedWindow(ToWx(hWnd)))
            DispatchMessageTo(hWnd, Msg, wParam, lParam);
    });
    return TRUE;
}

BOOL PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) { return PostMessage(hWnd, Msg, wParam, lParam); }

BOOL SendNotifyMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    if (IsMainThread()) {
        SendMessage(hWnd, Msg, wParam, lParam);
        return TRUE;
    }
    return PostMessage(hWnd, Msg, wParam, lParam);
}

BOOL PostThreadMessage(DWORD, UINT Msg, WPARAM, LPARAM) {
    if (Msg == WM_QUIT)
        PostToMainThread([] { if (wxTheApp) wxTheApp->ExitMainLoop(); });
    return TRUE;
}

void PostQuitMessage(int) {
    PostToMainThread([] {
        if (wxTheApp)
            wxTheApp->ExitMainLoop();
    });
}

UINT RegisterWindowMessage(const char* lpString) {
    static std::mutex m;
    static std::map<std::string, UINT> ids;
    std::lock_guard<std::mutex> lock(m);
    std::string key = lpString ? lpString : "";
    auto it = ids.find(key);
    if (it != ids.end())
        return it->second;
    UINT id = 0xC000 + static_cast<UINT>(ids.size());
    ids[key] = id;
    return id;
}

UINT RegisterWindowMessageA(const char* lpString) { return RegisterWindowMessage(lpString); }

BOOL PeekMessage(LPMSG lpMsg, HWND, UINT, UINT, UINT) {
    if (lpMsg)
        memset(lpMsg, 0, sizeof *lpMsg);
    if (IsMainThread()) {
        wxEventLoopBase* loop = wxEventLoopBase::GetActive();
        if (loop && !loop->IsYielding())
            loop->YieldFor(wxEVT_CATEGORY_ALL);
    }
    return FALSE;
}

BOOL GetMessage(LPMSG lpMsg, HWND hWnd, UINT a, UINT b) {
    PeekMessage(lpMsg, hWnd, a, b, PM_REMOVE);
    wxMilliSleep(10);
    return TRUE;
}

BOOL TranslateMessage(const MSG*) { return FALSE; }

LRESULT DispatchMessage(const MSG* lpMsg) {
    if (!lpMsg || !lpMsg->hwnd)
        return 0;
    return SendMessage(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam);
}

BOOL WaitMessage() {
    PeekMessage(nullptr, nullptr, 0, 0, PM_NOREMOVE);
    return TRUE;
}

DWORD MsgWaitForMultipleObjects(DWORD nCount, const HANDLE* pHandles, BOOL fWaitAll, DWORD dwMilliseconds, DWORD) {
    DWORD start = GetTickCount();
    for (;;) {
        DWORD r = WaitForMultipleObjects(nCount, pHandles, fWaitAll, 0);
        if (r != WAIT_TIMEOUT)
            return r;
        if (IsMainThread())
            return WAIT_OBJECT_0 + nCount;
        if (dwMilliseconds != INFINITE && GetTickCount() - start >= dwMilliseconds)
            return WAIT_TIMEOUT;
        wxMilliSleep(5);
    }
}

LRESULT DefWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    return OnMain([&]() -> LRESULT { return DefaultWindowProc(hWnd, Msg, wParam, lParam); });
}

LRESULT CallWindowProc(WNDPROC lpPrevWndFunc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    return lpPrevWndFunc ? lpPrevWndFunc(hWnd, Msg, wParam, lParam) : DefWindowProc(hWnd, Msg, wParam, lParam);
}

// ---------------------------------------------------------------------------------------------
// Window state

BOOL IsWindow(HWND hWnd) {
    return hWnd && OnMain([&]() -> BOOL { return IsManagedWindow(ToWx(hWnd)); });
}

BOOL IsWindowVisible(HWND hWnd) {
    return IsWindow(hWnd) && OnMain([&]() -> BOOL { return ToWx(hWnd)->IsShown(); });
}

BOOL IsWindowEnabled(HWND hWnd) {
    return IsWindow(hWnd) && OnMain([&]() -> BOOL { return ToWx(hWnd)->IsThisEnabled(); });
}

BOOL EnableWindow(HWND hWnd, BOOL bEnable) {
    if (!IsWindow(hWnd))
        return FALSE;
    return OnMain([&]() -> BOOL {
        wxWindow* w = ToWx(hWnd);
        BOOL wasDisabled = !w->IsThisEnabled();
        w->Enable(bEnable != FALSE);
        WindowState* st = GetState(w);
        if (st) {
            st->style = bEnable ? (st->style & ~WS_DISABLED) : (st->style | WS_DISABLED);
            if (st->kind == ControlKind::OwnerDrawButton || st->kind == ControlKind::Generic)
                w->Refresh();
        }
        return wasDisabled;
    });
}

BOOL ShowWindow(HWND hWnd, int nCmdShow) {
    if (!IsWindow(hWnd))
        return FALSE;
    return OnMain([&]() -> BOOL {
        wxWindow* w = ToWx(hWnd);
        BOOL wasVisible = w->IsShown();
        auto* tlw = wxDynamicCast(w, wxTopLevelWindow);
        switch (nCmdShow) {
        case SW_HIDE:
            w->Hide();
            break;
        case SW_MINIMIZE:
        case SW_SHOWMINIMIZED:
        case SW_SHOWMINNOACTIVE:
        case SW_FORCEMINIMIZE:
            if (tlw)
                tlw->Iconize();
            break;
        case SW_MAXIMIZE:
            if (tlw)
                tlw->Maximize();
            w->Show();
            break;
        case SW_RESTORE:
            if (tlw)
                tlw->Restore();
            w->Show();
            break;
        default:
            w->Show();
            if (tlw && (nCmdShow == SW_SHOW || nCmdShow == SW_SHOWNORMAL))
                tlw->Raise();
            break;
        }
        WindowState* st = GetState(w);
        if (st)
            st->style = w->IsShown() ? (st->style | WS_VISIBLE) : (st->style & ~WS_VISIBLE);
        return wasVisible;
    });
}

BOOL IsIconic(HWND hWnd) {
    return IsWindow(hWnd) && OnMain([&]() -> BOOL {
        auto* tlw = wxDynamicCast(ToWx(hWnd), wxTopLevelWindow);
        return tlw && tlw->IsIconized();
    });
}

BOOL IsZoomed(HWND hWnd) {
    return IsWindow(hWnd) && OnMain([&]() -> BOOL {
        auto* tlw = wxDynamicCast(ToWx(hWnd), wxTopLevelWindow);
        return tlw && tlw->IsMaximized();
    });
}

BOOL IsChild(HWND hWndParent, HWND hWnd) {
    if (!hWndParent || !hWnd)
        return FALSE;
    return OnMain([&]() -> BOOL {
        for (wxWindow* w = ToWx(hWnd); w && !w->IsTopLevel(); w = w->GetParent())
            if (w->GetParent() == ToWx(hWndParent))
                return TRUE;
        return FALSE;
    });
}

HWND GetParent(HWND hWnd) {
    if (!IsWindow(hWnd))
        return nullptr;
    return OnMain([&]() -> HWND { return ToHwnd(LogicalParent(ToWx(hWnd))); });
}

HWND SetParent(HWND hWndChild, HWND hWndNewParent) {
    HWND old = GetParent(hWndChild);
    if (IsWindow(hWndChild) && IsWindow(hWndNewParent))
        OnMain([&] { ToWx(hWndChild)->Reparent(ToWx(hWndNewParent)); });
    return old;
}

HWND GetWindow(HWND hWnd, UINT uCmd) {
    if (!IsWindow(hWnd))
        return nullptr;
    return OnMain([&]() -> HWND {
        wxWindow* w = ToWx(hWnd);
        if (uCmd == GW_CHILD) {
            auto children = ManagedChildren(w);
            return children.empty() ? nullptr : ToHwnd(children.front());
        }
        if (uCmd == GW_OWNER)
            return w->IsTopLevel() ? ToHwnd(ManagedAncestor(w->GetParent())) : nullptr;
        auto siblings = ManagedChildren(w->GetParent());
        auto it = std::find(siblings.begin(), siblings.end(), w);
        if (it == siblings.end())
            return nullptr;
        switch (uCmd) {
        case GW_HWNDFIRST:
            return ToHwnd(siblings.front());
        case GW_HWNDLAST:
            return ToHwnd(siblings.back());
        case GW_HWNDNEXT:
            return ++it == siblings.end() ? nullptr : ToHwnd(*it);
        case GW_HWNDPREV:
            return it == siblings.begin() ? nullptr : ToHwnd(*--it);
        default:
            return nullptr;
        }
    });
}

HWND GetNextWindow(HWND hWnd, UINT wCmd) { return GetWindow(hWnd, wCmd); }
HWND GetTopWindow(HWND hWnd) { return GetWindow(hWnd, GW_CHILD); }
HWND GetDesktopWindow() { return nullptr; }

HWND GetActiveWindow() {
    return OnMain([]() -> HWND { return ToHwnd(ManagedAncestor(wxGetActiveWindow())); });
}

HWND SetActiveWindow(HWND hWnd) {
    HWND old = GetActiveWindow();
    if (IsWindow(hWnd))
        OnMain([&] { wxGetTopLevelParent(ToWx(hWnd))->Raise(); });
    return old;
}

HWND GetForegroundWindow() { return GetActiveWindow(); }

BOOL SetForegroundWindow(HWND hWnd) {
    SetActiveWindow(hWnd);
    return TRUE;
}

HWND GetFocus() {
    return OnMain([]() -> HWND { return ToHwnd(ManagedAncestor(wxWindow::FindFocus())); });
}

HWND SetFocus(HWND hWnd) {
    HWND old = GetFocus();
    if (IsWindow(hWnd))
        OnMain([&] { ToWx(hWnd)->SetFocus(); });
    return old;
}

HWND GetCapture() {
    return OnMain([]() -> HWND { return ToHwnd(ManagedAncestor(wxWindow::GetCapture())); });
}

HWND SetCapture(HWND hWnd) {
    HWND old = GetCapture();
    if (IsWindow(hWnd))
        OnMain([&] {
            if (!ToWx(hWnd)->HasCapture())
                ToWx(hWnd)->CaptureMouse();
        });
    return old;
}

BOOL ReleaseCapture() {
    OnMain([] {
        if (wxWindow* w = wxWindow::GetCapture())
            w->ReleaseMouse();
    });
    return TRUE;
}

HWND FindWindow(const char*, const char* lpWindowName) {
    if (!lpWindowName)
        return nullptr;
    return OnMain([&]() -> HWND {
        wxString name = ToWx(lpWindowName);
        for (wxWindow* w : wxTopLevelWindows)
            if (IsManagedWindow(w) && w->GetLabel() == name)
                return ToHwnd(w);
        return nullptr;
    });
}

HWND FindWindowA(const char* lpClassName, const char* lpWindowName) { return FindWindow(lpClassName, lpWindowName); }

HWND WindowFromPoint(POINT Point) {
    return OnMain([&]() -> HWND { return ToHwnd(ManagedAncestor(wxFindWindowAtPoint(wxPoint(Point.x, Point.y)))); });
}

HWND ChildWindowFromPoint(HWND hWndParent, POINT Point) {
    if (!IsWindow(hWndParent))
        return nullptr;
    return OnMain([&]() -> HWND {
        wxWindow* parent = ToWx(hWndParent);
        for (wxWindow* c : ManagedChildren(parent))
            if (c->GetRect().Contains(Point.x, Point.y))
                return ToHwnd(c);
        return wxRect(parent->GetClientSize()).Contains(Point.x, Point.y) ? hWndParent : nullptr;
    });
}

BOOL DestroyWindow(HWND hWnd) {
    if (!IsWindow(hWnd))
        return FALSE;
    return OnMain([&]() -> BOOL {
        CWnd* p = PermanentWnd(ToWx(hWnd));
        if (p)
            return p->DestroyWindow();
        DestroyWxWindow(ToWx(hWnd));
        return TRUE;
    });
}

BOOL CloseWindow(HWND hWnd) { return ShowWindow(hWnd, SW_MINIMIZE); }

BOOL BringWindowToTop(HWND hWnd) {
    if (!IsWindow(hWnd))
        return FALSE;
    OnMain([&] { ToWx(hWnd)->Raise(); });
    return TRUE;
}

BOOL SetWindowText(HWND hWnd, const char* lpString) {
    return IsWindow(hWnd) && SendMessage(hWnd, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(lpString ? lpString : ""));
}

BOOL SetWindowTextA(HWND hWnd, const char* lpString) { return SetWindowText(hWnd, lpString); }

int GetWindowText(HWND hWnd, char* lpString, int nMaxCount) {
    if (!lpString || nMaxCount <= 0)
        return 0;
    lpString[0] = 0;
    if (!IsWindow(hWnd))
        return 0;
    return static_cast<int>(SendMessage(hWnd, WM_GETTEXT, static_cast<WPARAM>(nMaxCount), reinterpret_cast<LPARAM>(lpString)));
}

int GetWindowTextA(HWND hWnd, char* lpString, int nMaxCount) { return GetWindowText(hWnd, lpString, nMaxCount); }

int GetWindowTextLength(HWND hWnd) {
    return IsWindow(hWnd) ? static_cast<int>(SendMessage(hWnd, WM_GETTEXTLENGTH, 0, 0)) : 0;
}

int GetClassName(HWND hWnd, char* lpClassName, int nMaxCount) {
    if (!lpClassName || nMaxCount <= 0)
        return 0;
    const char* name = "";
    if (IsWindow(hWnd)) {
        switch (GetState(ToWx(hWnd))->kind) {
        case ControlKind::PushButton: case ControlKind::CheckBox: case ControlKind::RadioButton:
        case ControlKind::OwnerDrawButton: case ControlKind::GroupBox: name = "Button"; break;
        case ControlKind::Edit: name = "Edit"; break;
        case ControlKind::RichEdit: name = "RichEdit20A"; break;
        case ControlKind::Static: case ControlKind::StaticBitmap: case ControlKind::StaticFrame: name = "Static"; break;
        case ControlKind::ComboBox: case ControlKind::ComboList: name = "ComboBox"; break;
        case ControlKind::ListBox: name = "ListBox"; break;
        case ControlKind::ScrollBar: name = "ScrollBar"; break;
        case ControlKind::Progress: name = "msctls_progress32"; break;
        case ControlKind::Slider: name = "msctls_trackbar32"; break;
        case ControlKind::Spin: name = "msctls_updown32"; break;
        case ControlKind::ListView: name = "SysListView32"; break;
        case ControlKind::TreeView: name = "SysTreeView32"; break;
        case ControlKind::Tab: name = "SysTabControl32"; break;
        case ControlKind::Scintilla: name = "Scintilla"; break;
        case ControlKind::Dialog: name = "#32770"; break;
        default: name = "AfxWnd"; break;
        }
    }
    strncpy(lpClassName, name, static_cast<size_t>(nMaxCount) - 1);
    lpClassName[nMaxCount - 1] = 0;
    return static_cast<int>(strlen(lpClassName));
}

LONG_PTR GetWindowLongPtr(HWND hWnd, int nIndex) {
    if (!IsWindow(hWnd))
        return 0;
    return OnMain([&]() -> LONG_PTR {
        wxWindow* w = ToWx(hWnd);
        WindowState* st = GetState(w);
        switch (nIndex) {
        case GWL_STYLE:
            return static_cast<LONG_PTR>(CurrentStyle(w));
        case GWL_EXSTYLE:
            return static_cast<LONG_PTR>(st->exStyle);
        case GWL_ID:
            return st->winId;
        case GWL_USERDATA:
            return st->userData;
        case GWL_HWNDPARENT:
            return reinterpret_cast<LONG_PTR>(ToHwnd(LogicalParent(w)));
        default:
            return 0;
        }
    });
}

LONG_PTR SetWindowLongPtr(HWND hWnd, int nIndex, LONG_PTR dwNewLong) {
    if (!IsWindow(hWnd))
        return 0;
    return OnMain([&]() -> LONG_PTR {
        wxWindow* w = ToWx(hWnd);
        WindowState* st = GetState(w);
        LONG_PTR old = GetWindowLongPtr(hWnd, nIndex);
        switch (nIndex) {
        case GWL_STYLE:
            ApplyStyle(w, CurrentStyle(w), static_cast<DWORD>(dwNewLong));
            break;
        case GWL_EXSTYLE:
            st->exStyle = static_cast<DWORD>(dwNewLong);
            break;
        case GWL_ID:
            st->winId = static_cast<int>(dwNewLong);
            w->SetId(ToWxId(st->winId));
            break;
        case GWL_USERDATA:
            st->userData = dwNewLong;
            break;
        default:
            break;
        }
        return old;
    });
}

LONG GetWindowLong(HWND hWnd, int nIndex) { return static_cast<LONG>(GetWindowLongPtr(hWnd, nIndex)); }
LONG SetWindowLong(HWND hWnd, int nIndex, LONG dwNewLong) { return static_cast<LONG>(SetWindowLongPtr(hWnd, nIndex, dwNewLong)); }

// ---------------------------------------------------------------------------------------------
// Geometry and painting

namespace {

void ToRect(const wxRect& r, LPRECT out) {
    out->left = r.x;
    out->top = r.y;
    out->right = r.x + r.width;
    out->bottom = r.y + r.height;
}

} // namespace

BOOL GetWindowRect(HWND hWnd, LPRECT lpRect) {
    if (!IsWindow(hWnd) || !lpRect)
        return FALSE;
    OnMain([&] { ToRect(ToWx(hWnd)->GetScreenRect(), lpRect); });
    return TRUE;
}

BOOL GetClientRect(HWND hWnd, LPRECT lpRect) {
    if (!IsWindow(hWnd) || !lpRect)
        return FALSE;
    OnMain([&] { ToRect(wxRect(ToWx(hWnd)->GetClientSize()), lpRect); });
    return TRUE;
}

BOOL ClientToScreen(HWND hWnd, LPPOINT lpPoint) {
    if (!IsWindow(hWnd) || !lpPoint)
        return FALSE;
    OnMain([&] {
        wxPoint p = ToWx(hWnd)->ClientToScreen(wxPoint(lpPoint->x, lpPoint->y));
        lpPoint->x = p.x;
        lpPoint->y = p.y;
    });
    return TRUE;
}

BOOL ScreenToClient(HWND hWnd, LPPOINT lpPoint) {
    if (!IsWindow(hWnd) || !lpPoint)
        return FALSE;
    OnMain([&] {
        wxPoint p = ToWx(hWnd)->ScreenToClient(wxPoint(lpPoint->x, lpPoint->y));
        lpPoint->x = p.x;
        lpPoint->y = p.y;
    });
    return TRUE;
}

int MapWindowPoints(HWND hWndFrom, HWND hWndTo, LPPOINT lpPoints, UINT cPoints) {
    int dx = 0, dy = 0;
    POINT origin = {0, 0};
    if (hWndFrom) {
        ClientToScreen(hWndFrom, &origin);
        dx += origin.x;
        dy += origin.y;
    }
    if (hWndTo) {
        POINT o = {0, 0};
        ClientToScreen(hWndTo, &o);
        dx -= o.x;
        dy -= o.y;
    }
    for (UINT i = 0; i < cPoints; ++i) {
        lpPoints[i].x += dx;
        lpPoints[i].y += dy;
    }
    return MAKELONG(static_cast<WORD>(dx), static_cast<WORD>(dy));
}

BOOL MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint) {
    if (!IsWindow(hWnd))
        return FALSE;
    OnMain([&] {
        wxWindow* w = ToWx(hWnd);
        w->SetSize(X, Y, nWidth, nHeight, wxSIZE_ALLOW_MINUS_ONE);
        if (bRepaint)
            w->Refresh();
    });
    return TRUE;
}

BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags) {
    if (!IsWindow(hWnd))
        return FALSE;
    OnMain([&] {
        wxWindow* w = ToWx(hWnd);
        wxRect r = w->GetRect();
        if (!(uFlags & SWP_NOMOVE))
            r.SetPosition(wxPoint(X, Y));
        if (!(uFlags & SWP_NOSIZE))
            r.SetSize(wxSize(cx, cy));
        if (!(uFlags & (SWP_NOMOVE | SWP_NOSIZE)) || (uFlags & (SWP_NOMOVE | SWP_NOSIZE)) != (SWP_NOMOVE | SWP_NOSIZE))
            w->SetSize(r, wxSIZE_ALLOW_MINUS_ONE);
        if (!(uFlags & SWP_NOZORDER)) {
            if (hWndInsertAfter == HWND_TOPMOST || hWndInsertAfter == HWND_NOTOPMOST) {
                if (auto* tlw = wxDynamicCast(w, wxTopLevelWindow)) {
                    long style = tlw->GetWindowStyleFlag();
                    tlw->SetWindowStyleFlag(hWndInsertAfter == HWND_TOPMOST ? (style | wxSTAY_ON_TOP) : (style & ~wxSTAY_ON_TOP));
                }
            } else if (hWndInsertAfter == HWND_BOTTOM) {
                w->Lower();
            } else if (hWndInsertAfter == HWND_TOP) {
                w->Raise();
            }
        }
        if (uFlags & SWP_SHOWWINDOW)
            w->Show();
        if (uFlags & SWP_HIDEWINDOW)
            w->Hide();
        if (uFlags & SWP_FRAMECHANGED)
            w->Refresh();
    });
    return TRUE;
}

BOOL InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase) {
    if (!IsWindow(hWnd))
        return FALSE;
    OnMain([&] {
        wxWindow* w = ToWx(hWnd);
        if (lpRect) {
            wxRect r(lpRect->left, lpRect->top, lpRect->right - lpRect->left, lpRect->bottom - lpRect->top);
            w->RefreshRect(r, bErase != FALSE);
        } else {
            w->Refresh(bErase != FALSE);
        }
        if (!w->IsTopLevel())
            ApplyCtlColor(w);
    });
    return TRUE;
}

BOOL ValidateRect(HWND, const RECT*) { return TRUE; }

BOOL UpdateWindow(HWND hWnd) {
    if (!IsWindow(hWnd))
        return FALSE;
    OnMain([&] { ToWx(hWnd)->Update(); });
    return TRUE;
}

BOOL RedrawWindow(HWND hWnd, const RECT* lprcUpdate, HRGN, UINT) {
    if (!IsWindow(hWnd))
        return FALSE;
    InvalidateRect(hWnd, lprcUpdate, TRUE);
    return UpdateWindow(hWnd);
}

UINT_PTR SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, TIMERPROC lpTimerFunc) {
    if (!IsWindow(hWnd))
        return 0;
    return OnMain([&]() -> UINT_PTR { return StartTimer(ToWx(hWnd), nIDEvent, uElapse, lpTimerFunc); });
}

BOOL KillTimer(HWND hWnd, UINT_PTR uIDEvent) {
    if (!hWnd)
        return FALSE;
    return OnMain([&]() -> BOOL { return IsManagedWindow(ToWx(hWnd)) && StopTimer(ToWx(hWnd), uIDEvent); });
}

// ---------------------------------------------------------------------------------------------
// Dialog items

HWND GetDlgItem(HWND hDlg, int nIDDlgItem) {
    if (!IsWindow(hDlg))
        return nullptr;
    return OnMain([&]() -> HWND { return ToHwnd(FindChildById(ToWx(hDlg), nIDDlgItem)); });
}

int GetDlgCtrlID(HWND hWnd) { return static_cast<int>(GetWindowLongPtr(hWnd, GWL_ID)); }

UINT GetDlgItemInt(HWND hDlg, int nIDDlgItem, BOOL* lpTranslated, BOOL bSigned) {
    char buf[64];
    if (lpTranslated)
        *lpTranslated = FALSE;
    if (!GetDlgItem(hDlg, nIDDlgItem))
        return 0;
    GetDlgItemText(hDlg, nIDDlgItem, buf, sizeof buf);
    char* end = nullptr;
    const char* p = buf;
    while (*p == ' ')
        ++p;
    long long v = bSigned ? strtoll(p, &end, 10) : static_cast<long long>(strtoull(p, &end, 10));
    if (end == p || (!bSigned && *p == '-'))
        return 0;
    while (end && *end == ' ')
        ++end;
    if (end && *end)
        return 0;
    if (lpTranslated)
        *lpTranslated = TRUE;
    return static_cast<UINT>(v);
}

BOOL SetDlgItemInt(HWND hDlg, int nIDDlgItem, UINT uValue, BOOL bSigned) {
    char buf[32];
    if (bSigned)
        snprintf(buf, sizeof buf, "%d", static_cast<int>(uValue));
    else
        snprintf(buf, sizeof buf, "%u", uValue);
    return SetDlgItemText(hDlg, nIDDlgItem, buf);
}

UINT GetDlgItemText(HWND hDlg, int nIDDlgItem, char* lpString, int cchMax) {
    if (lpString && cchMax > 0)
        lpString[0] = 0;
    HWND h = GetDlgItem(hDlg, nIDDlgItem);
    return h ? static_cast<UINT>(GetWindowText(h, lpString, cchMax)) : 0;
}

UINT GetDlgItemTextA(HWND hDlg, int nIDDlgItem, char* lpString, int cchMax) {
    return GetDlgItemText(hDlg, nIDDlgItem, lpString, cchMax);
}

BOOL SetDlgItemText(HWND hDlg, int nIDDlgItem, const char* lpString) {
    HWND h = GetDlgItem(hDlg, nIDDlgItem);
    return h ? SetWindowText(h, lpString) : FALSE;
}

BOOL SetDlgItemTextA(HWND hDlg, int nIDDlgItem, const char* lpString) { return SetDlgItemText(hDlg, nIDDlgItem, lpString); }

BOOL CheckDlgButton(HWND hDlg, int nIDButton, UINT uCheck) {
    HWND h = GetDlgItem(hDlg, nIDButton);
    if (!h)
        return FALSE;
    SendMessage(h, BM_SETCHECK, uCheck, 0);
    return TRUE;
}

UINT IsDlgButtonChecked(HWND hDlg, int nIDButton) {
    HWND h = GetDlgItem(hDlg, nIDButton);
    return h ? static_cast<UINT>(SendMessage(h, BM_GETCHECK, 0, 0)) : 0;
}

BOOL CheckRadioButton(HWND hDlg, int nIDFirstButton, int nIDLastButton, int nIDCheckButton) {
    for (int id = nIDFirstButton; id <= nIDLastButton; ++id) {
        HWND h = GetDlgItem(hDlg, id);
        if (h)
            SendMessage(h, BM_SETCHECK, id == nIDCheckButton ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    return TRUE;
}

LRESULT SendDlgItemMessage(HWND hDlg, int nIDDlgItem, UINT Msg, WPARAM wParam, LPARAM lParam) {
    HWND h = GetDlgItem(hDlg, nIDDlgItem);
    return h ? SendMessage(h, Msg, wParam, lParam) : 0;
}

BOOL EndDialog(HWND hDlg, INT_PTR nResult) {
    CWnd* p = CWnd::FromHandlePermanent(hDlg);
    if (p && p->IsKindOf(RUNTIME_CLASS(CDialog))) {
        static_cast<CDialog*>(p)->EndDialog(static_cast<int>(nResult));
        return TRUE;
    }
    return FALSE;
}

namespace {

HWND NextTabItem(HWND hDlg, HWND hCtl, BOOL bPrevious, bool groupOnly) {
    if (!IsWindow(hDlg))
        return nullptr;
    return OnMain([&]() -> HWND {
        auto children = ManagedChildren(ToWx(hDlg));
        if (children.empty())
            return nullptr;
        auto eligible = [&](wxWindow* c) {
            WindowState* st = GetState(c);
            return c->IsShown() && c->IsEnabled() && st && (groupOnly || (st->style & WS_TABSTOP));
        };
        size_t n = children.size();
        size_t start = 0;
        for (size_t i = 0; i < n; ++i)
            if (ToHwnd(children[i]) == hCtl)
                start = i;
        for (size_t k = 1; k <= n; ++k) {
            size_t i = bPrevious ? (start + n - k) % n : (start + k) % n;
            if (groupOnly) {
                WindowState* st = GetState(children[i]);
                if (!bPrevious && st && (st->style & WS_GROUP))
                    break;
            }
            if (eligible(children[i]))
                return ToHwnd(children[i]);
        }
        return hCtl;
    });
}

} // namespace

HWND GetNextDlgTabItem(HWND hDlg, HWND hCtl, BOOL bPrevious) { return NextTabItem(hDlg, hCtl, bPrevious, false); }
HWND GetNextDlgGroupItem(HWND hDlg, HWND hCtl, BOOL bPrevious) { return NextTabItem(hDlg, hCtl, bPrevious, true); }

BOOL MapDialogRect(HWND hDlg, LPRECT lpRect) {
    if (!IsWindow(hDlg) || !lpRect)
        return FALSE;
    OnMain([&] {
        wxRect r = DialogUnitsToPixels(ToWx(hDlg), lpRect->left, lpRect->top, lpRect->right - lpRect->left,
                                       lpRect->bottom - lpRect->top);
        ToRect(r, lpRect);
    });
    return TRUE;
}

LONG GetDialogBaseUnits() {
    return OnMain([]() -> LONG {
        wxRect r = DialogUnitsToPixels(nullptr, 0, 0, 4, 8);
        return MAKELONG(r.width, r.height);
    });
}

int MessageBox(HWND hWnd, const char* lpText, const char* lpCaption, UINT uType) {
    return OnMain([&]() -> int { return ShowMessageBox(hWnd ? ToWx(hWnd) : nullptr, lpText, lpCaption, uType); });
}

int MessageBoxA(HWND hWnd, const char* lpText, const char* lpCaption, UINT uType) {
    return MessageBox(hWnd, lpText, lpCaption, uType);
}

BOOL WinHelp(HWND, const char*, UINT uCommand, ULONG_PTR dwData) {
    if (CWinApp* app = AfxGetApp())
        app->WinHelp(dwData, uCommand);
    return TRUE;
}

BOOL WinHelpA(HWND hWndMain, const char* lpszHelp, UINT uCommand, ULONG_PTR dwData) {
    return WinHelp(hWndMain, lpszHelp, uCommand, dwData);
}

// ---------------------------------------------------------------------------------------------
// System information, keyboard, cursors, icons

int GetSystemMetrics(int nIndex) {
    return OnMain([&]() -> int {
        wxDisplay display(static_cast<unsigned>(0));
        wxRect screen = display.GetGeometry();
        wxRect work = display.GetClientArea();
        switch (nIndex) {
        case SM_CXSCREEN: case SM_CXVIRTUALSCREEN: return screen.width;
        case SM_CYSCREEN: case SM_CYVIRTUALSCREEN: return screen.height;
        case SM_CXFULLSCREEN: case SM_CXMAXIMIZED: return work.width;
        case SM_CYFULLSCREEN: case SM_CYMAXIMIZED: return work.height;
        case SM_CXVSCROLL: return wxSystemSettings::GetMetric(wxSYS_VSCROLL_X);
        case SM_CYHSCROLL: return wxSystemSettings::GetMetric(wxSYS_HSCROLL_Y);
        case SM_CYVSCROLL: case SM_CXHSCROLL: return 16;
        case SM_CYCAPTION: return std::max(20, wxSystemSettings::GetMetric(wxSYS_CAPTION_Y));
        case SM_CYMENU: return std::max(20, wxSystemSettings::GetMetric(wxSYS_MENU_Y));
        case SM_CXBORDER: case SM_CYBORDER: return 1;
        case SM_CXEDGE: case SM_CYEDGE: return 2;
        case SM_CXDLGFRAME: case SM_CYDLGFRAME: case SM_CXFRAME: case SM_CYFRAME: return 4;
        case SM_CXICON: case SM_CYICON: case SM_CXCURSOR: case SM_CYCURSOR: return 32;
        case SM_CXSMICON: case SM_CYSMICON: return 16;
        case SM_CXDRAG: case SM_CYDRAG: return 4;
        case SM_CXMIN: return 112;
        case SM_CYMIN: return 27;
        default: return 0;
        }
    });
}

BOOL SystemParametersInfo(UINT uiAction, UINT, PVOID pvParam, UINT) {
    if (uiAction == SPI_GETWORKAREA && pvParam) {
        OnMain([&] { ToRect(wxDisplay(static_cast<unsigned>(0)).GetClientArea(), static_cast<LPRECT>(pvParam)); });
        return TRUE;
    }
    return FALSE;
}

BOOL SystemParametersInfoA(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni) {
    return SystemParametersInfo(uiAction, uiParam, pvParam, fWinIni);
}

SHORT GetKeyState(int nVirtKey) {
    return OnMain([&]() -> SHORT {
        wxMouseState ms = wxGetMouseState();
        bool down = false;
        switch (nVirtKey) {
        case VK_LBUTTON: down = ms.LeftIsDown(); break;
        case VK_RBUTTON: down = ms.RightIsDown(); break;
        case VK_MBUTTON: down = ms.MiddleIsDown(); break;
        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: down = ms.ShiftDown(); break;
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: down = ms.ControlDown(); break;
        case VK_MENU: case VK_LMENU: case VK_RMENU: down = ms.AltDown(); break;
        default: {
            int key = WxKeyFromVirtual(static_cast<UINT>(nVirtKey));
            if (key == WXK_CAPITAL || key == WXK_NUMLOCK || key == WXK_SCROLL)
                return wxGetKeyState(static_cast<wxKeyCode>(key)) ? 1 : 0;
            down = key && wxGetKeyState(static_cast<wxKeyCode>(key));
            break;
        }
        }
        return down ? static_cast<SHORT>(0x8000) : 0;
    });
}

SHORT GetAsyncKeyState(int vKey) { return GetKeyState(vKey); }

BOOL GetKeyboardState(BYTE* lpKeyState) {
    if (!lpKeyState)
        return FALSE;
    for (int i = 0; i < 256; ++i)
        lpKeyState[i] = (i == VK_SHIFT || i == VK_CONTROL || i == VK_MENU || i == VK_LBUTTON || i == VK_RBUTTON)
                            ? static_cast<BYTE>((GetKeyState(i) & 0x8000) ? 0x80 : 0)
                            : 0;
    return TRUE;
}

BOOL GetCursorPos(LPPOINT lpPoint) {
    if (!lpPoint)
        return FALSE;
    OnMain([&] {
        wxPoint p = wxGetMousePosition();
        lpPoint->x = p.x;
        lpPoint->y = p.y;
    });
    return TRUE;
}

BOOL SetCursorPos(int, int) { return FALSE; }

namespace {

wxCursor* StockCursor(int id) {
    static std::map<int, wxCursor*> cache;
    auto it = cache.find(id);
    if (it != cache.end())
        return it->second;
    wxStockCursor stock = wxCURSOR_ARROW;
    switch (id) {
    case 32513: stock = wxCURSOR_IBEAM; break;
    case 32514: stock = wxCURSOR_WAIT; break;
    case 32515: stock = wxCURSOR_CROSS; break;
    case 32516: stock = wxCURSOR_POINT_RIGHT; break;
    case 32640: case 32646: stock = wxCURSOR_SIZING; break;
    case 32642: stock = wxCURSOR_SIZENWSE; break;
    case 32643: stock = wxCURSOR_SIZENESW; break;
    case 32644: stock = wxCURSOR_SIZEWE; break;
    case 32645: stock = wxCURSOR_SIZENS; break;
    case 32648: stock = wxCURSOR_NO_ENTRY; break;
    case 32649: stock = wxCURSOR_HAND; break;
    case 32650: stock = wxCURSOR_ARROWWAIT; break;
    case 32651: stock = wxCURSOR_QUESTION_ARROW; break;
    default: break;
    }
    auto* c = new wxCursor(stock);
    cache[id] = c;
    return c;
}

wxCursor* ResourceCursor(const ResRef& ref) {
    static std::map<std::string, wxCursor*> cache;
    std::string key = ref.name.empty() ? std::to_string(ref.id) : ref.name;
    auto it = cache.find(key);
    if (it != cache.end())
        return it->second;
    wxCursor c = LoadCursorResource(ref);
    wxCursor* p = c.IsOk() ? new wxCursor(c) : nullptr;
    cache[key] = p;
    return p;
}

wxIcon* ResourceIcon(const ResRef& ref) {
    static std::map<std::string, wxIcon*> cache;
    std::string key = ref.name.empty() ? std::to_string(ref.id) : ref.name;
    auto it = cache.find(key);
    if (it != cache.end())
        return it->second;
    wxIcon icon;
    if (ref.name.empty() && ref.id >= 32512 && ref.id <= 32517) {
        wxArtID art = wxART_INFORMATION;
        if (ref.id == 32513)
            art = wxART_ERROR;
        else if (ref.id == 32514)
            art = wxART_QUESTION;
        else if (ref.id == 32515)
            art = wxART_WARNING;
        icon = wxArtProvider::GetIcon(art, wxART_MESSAGE_BOX);
    } else {
        icon = LoadIconResource(ref);
    }
    wxIcon* p = icon.IsOk() ? new wxIcon(icon) : nullptr;
    cache[key] = p;
    return p;
}

} // namespace

HCURSOR SetCursor(HCURSOR hCursor) {
    HCURSOR old = reinterpret_cast<HCURSOR>(const_cast<wxCursor*>(g_currentCursor));
    auto* c = reinterpret_cast<wxCursor*>(hCursor);
    g_currentCursor = c;
    if (g_cursorRequested) {
        g_requestedCursor = c;
        return old;
    }
    OnMain([&] { wxSetCursor(c ? *c : wxNullCursor); });
    return old;
}

HCURSOR GetCursor() { return reinterpret_cast<HCURSOR>(const_cast<wxCursor*>(g_currentCursor)); }
int ShowCursor(BOOL bShow) { return bShow ? 0 : -1; }

HCURSOR LoadCursor(HINSTANCE hInstance, const char* lpCursorName) {
    ResRef ref = ResRef::From(lpCursorName);
    return OnMain([&]() -> HCURSOR {
        if (!hInstance && ref.name.empty())
            return reinterpret_cast<HCURSOR>(StockCursor(ref.id));
        if (wxCursor* c = ResourceCursor(ref))
            return reinterpret_cast<HCURSOR>(c);
        return ref.name.empty() && ref.id >= 32512 ? reinterpret_cast<HCURSOR>(StockCursor(ref.id)) : nullptr;
    });
}

HCURSOR LoadCursorA(HINSTANCE hInstance, const char* lpCursorName) { return LoadCursor(hInstance, lpCursorName); }

HCURSOR LoadCursorFromFile(const char* lpFileName) {
    return OnMain([&]() -> HCURSOR {
        wxImage image;
        wxLogNull noLog;
        if (!image.LoadFile(NativePathWx(lpFileName)))
            return nullptr;
        return reinterpret_cast<HCURSOR>(new wxCursor(image));
    });
}

HICON LoadIcon(HINSTANCE, const char* lpIconName) {
    ResRef ref = ResRef::From(lpIconName);
    return OnMain([&]() -> HICON { return reinterpret_cast<HICON>(ResourceIcon(ref)); });
}

HICON LoadIconA(HINSTANCE hInstance, const char* lpIconName) { return LoadIcon(hInstance, lpIconName); }
BOOL DestroyIcon(HICON) { return TRUE; }
BOOL DestroyCursor(HCURSOR) { return TRUE; }

HANDLE LoadImage(HINSTANCE hInst, const char* name, UINT type, int cx, int cy, UINT fuLoad) {
    return OnMain([&]() -> HANDLE {
        if (fuLoad & LR_LOADFROMFILE) {
            wxImage image;
            wxLogNull noLog;
            if (!image.LoadFile(NativePathWx(name)))
                return nullptr;
            if (cx > 0 && cy > 0)
                image.Rescale(cx, cy, wxIMAGE_QUALITY_HIGH);
            if (type == IMAGE_BITMAP)
                return CreateBitmapHandle(wxBitmap(image));
            if (type == IMAGE_CURSOR)
                return new wxCursor(image);
            wxIcon icon;
            icon.CopyFromBitmap(wxBitmap(image));
            return new wxIcon(icon);
        }
        ResRef ref = ResRef::From(name);
        if (type == IMAGE_BITMAP) {
            wxBitmap bmp = LoadBitmapResource(ref);
            if (!bmp.IsOk())
                return nullptr;
            if (cx > 0 && cy > 0 && (bmp.GetWidth() != cx || bmp.GetHeight() != cy))
                bmp = wxBitmap(bmp.ConvertToImage().Rescale(cx, cy, wxIMAGE_QUALITY_HIGH));
            return CreateBitmapHandle(bmp);
        }
        if (type == IMAGE_CURSOR)
            return LoadCursor(hInst, name);
        return LoadIcon(hInst, name);
    });
}

HANDLE LoadImageA(HINSTANCE hInst, const char* name, UINT type, int cx, int cy, UINT fuLoad) {
    return LoadImage(hInst, name, type, cx, cy, fuLoad);
}

HBITMAP LoadBitmap(HINSTANCE, const char* lpBitmapName) {
    ResRef ref = ResRef::From(lpBitmapName);
    return OnMain([&]() -> HBITMAP {
        wxBitmap bmp = LoadBitmapResource(ref);
        return bmp.IsOk() ? CreateBitmapHandle(bmp) : nullptr;
    });
}

HBITMAP LoadBitmapA(HINSTANCE hInstance, const char* lpBitmapName) { return LoadBitmap(hInstance, lpBitmapName); }

HACCEL LoadAccelerators(HINSTANCE, const char* lpTableName) {
    const rc::AccelTable* t = FindAccelTable(ResRef::From(lpTableName));
    return reinterpret_cast<HACCEL>(const_cast<rc::AccelTable*>(t));
}

int TranslateAccelerator(HWND, HACCEL, LPMSG) { return 0; }

// ---------------------------------------------------------------------------------------------
// Clipboard

namespace {

struct ClipboardState {
    bool open = false;
    bool emptied = false;
    std::vector<HGLOBAL> owned;
    std::map<std::string, UINT> formats;
};

ClipboardState& Clip() {
    static ClipboardState s;
    return s;
}

std::string FormatName(UINT format) {
    for (auto& f : Clip().formats)
        if (f.second == format)
            return f.first;
    return "CrypTool.Format." + std::to_string(format);
}

} // namespace

BOOL OpenClipboard(HWND) {
    return OnMain([]() -> BOOL {
        if (Clip().open)
            return FALSE;
        if (!wxTheClipboard->Open())
            return FALSE;
        Clip().open = true;
        Clip().emptied = false;
        return TRUE;
    });
}

BOOL CloseClipboard() {
    return OnMain([]() -> BOOL {
        if (!Clip().open)
            return FALSE;
        wxTheClipboard->Flush();
        wxTheClipboard->Close();
        Clip().open = false;
        for (HGLOBAL h : Clip().owned)
            GlobalFree(h);
        Clip().owned.clear();
        return TRUE;
    });
}

BOOL EmptyClipboard() {
    return OnMain([]() -> BOOL {
        if (!Clip().open)
            return FALSE;
        wxTheClipboard->Clear();
        Clip().emptied = true;
        return TRUE;
    });
}

HANDLE SetClipboardData(UINT uFormat, HANDLE hMem) {
    return OnMain([&]() -> HANDLE {
        if (!Clip().open || !hMem)
            return nullptr;
        const char* data = static_cast<const char*>(GlobalData(hMem));
        size_t size = GlobalDataSize(hMem);
        if (!data)
            return nullptr;
        if (uFormat == CF_TEXT || uFormat == CF_OEMTEXT) {
            size_t len = strnlen(data, size);
            wxString text = ToWx(data, static_cast<int>(len));
            text.Replace("\r\n", "\n");
            wxTheClipboard->AddData(new wxTextDataObject(text));
        } else {
            auto* obj = new wxCustomDataObject(wxDataFormat(wxString::FromUTF8(FormatName(uFormat))));
            obj->SetData(size, data);
            wxTheClipboard->AddData(obj);
        }
        Clip().owned.push_back(hMem);
        return hMem;
    });
}

HANDLE GetClipboardData(UINT uFormat) {
    return OnMain([&]() -> HANDLE {
        if (!Clip().open)
            return nullptr;
        if (uFormat == CF_TEXT || uFormat == CF_OEMTEXT || uFormat == CF_UNICODETEXT) {
            if (!wxTheClipboard->IsSupported(wxDF_UNICODETEXT) && !wxTheClipboard->IsSupported(wxDF_TEXT))
                return nullptr;
            wxTextDataObject obj;
            if (!wxTheClipboard->GetData(obj))
                return nullptr;
            wxString text = obj.GetText();
            text.Replace("\r\n", "\n");
            text.Replace("\n", "\r\n");
            std::string a = FromWx(text);
            HGLOBAL h = GlobalFromData(a.c_str(), a.size() + 1);
            Clip().owned.push_back(h);
            return h;
        }
        wxDataFormat fmt(wxString::FromUTF8(FormatName(uFormat)));
        if (!wxTheClipboard->IsSupported(fmt))
            return nullptr;
        wxCustomDataObject obj(fmt);
        if (!wxTheClipboard->GetData(obj))
            return nullptr;
        HGLOBAL h = GlobalFromData(obj.GetData(), obj.GetSize());
        Clip().owned.push_back(h);
        return h;
    });
}

BOOL IsClipboardFormatAvailable(UINT format) {
    return OnMain([&]() -> BOOL {
        bool opened = false;
        if (!Clip().open) {
            if (!wxTheClipboard->Open())
                return FALSE;
            opened = true;
        }
        bool r;
        if (format == CF_TEXT || format == CF_OEMTEXT || format == CF_UNICODETEXT)
            r = wxTheClipboard->IsSupported(wxDF_UNICODETEXT) || wxTheClipboard->IsSupported(wxDF_TEXT);
        else
            r = wxTheClipboard->IsSupported(wxDataFormat(wxString::FromUTF8(FormatName(format))));
        if (opened)
            wxTheClipboard->Close();
        return r;
    });
}

UINT RegisterClipboardFormat(const char* lpszFormat) {
    std::string name = lpszFormat ? lpszFormat : "";
    auto it = Clip().formats.find(name);
    if (it != Clip().formats.end())
        return it->second;
    UINT id = 0xC100 + static_cast<UINT>(Clip().formats.size());
    Clip().formats[name] = id;
    return id;
}

UINT RegisterClipboardFormatA(const char* lpszFormat) { return RegisterClipboardFormat(lpszFormat); }

// ---------------------------------------------------------------------------------------------
// Drag and drop of files

namespace {

struct DropList {
    std::vector<std::string> files;
};

} // namespace

UINT DragQueryFile(HDROP hDrop, UINT iFile, char* lpszFile, UINT cch) {
    auto* list = reinterpret_cast<DropList*>(hDrop);
    if (!list)
        return 0;
    if (iFile == 0xFFFFFFFF)
        return static_cast<UINT>(list->files.size());
    if (iFile >= list->files.size())
        return 0;
    const std::string& f = list->files[iFile];
    if (!lpszFile)
        return static_cast<UINT>(f.size());
    size_t n = std::min<size_t>(f.size(), cch ? cch - 1 : 0);
    memcpy(lpszFile, f.data(), n);
    if (cch)
        lpszFile[n] = 0;
    return static_cast<UINT>(n);
}

UINT DragQueryFileA(HDROP hDrop, UINT iFile, char* lpszFile, UINT cch) { return DragQueryFile(hDrop, iFile, lpszFile, cch); }

void DragFinish(HDROP hDrop) { delete reinterpret_cast<DropList*>(hDrop); }

void DragAcceptFiles(HWND hWnd, BOOL fAccept) {
    if (!IsWindow(hWnd))
        return;
    OnMain([&] {
        wxWindow* w = ToWx(hWnd);
        w->DragAcceptFiles(fAccept != FALSE);
        WindowState* st = GetState(w);
        if (!fAccept || !st || st->dropBound)
            return;
        st->dropBound = true;
        w->Bind(wxEVT_DROP_FILES, [w](wxDropFilesEvent& e) {
            auto* list = new DropList;
            for (int i = 0; i < e.GetNumberOfFiles(); ++i)
                list->files.push_back(AppPath(e.GetFiles()[i].utf8_str()));
            DispatchMessageTo(ToHwnd(w), WM_DROPFILES, reinterpret_cast<WPARAM>(list), 0);
        });
    });
}

// ---------------------------------------------------------------------------------------------
// Window classes, carets, locale, common dialogs

namespace {

std::map<std::string, WNDCLASS>& Classes() {
    static std::map<std::string, WNDCLASS> classes;
    return classes;
}

} // namespace

LONG GetClassLong(HWND, int) { return 0; }

LONG SetClassLong(HWND hWnd, int nIndex, LONG dwNewLong) {
    return static_cast<LONG>(SetClassLongPtr(hWnd, nIndex, dwNewLong));
}

ULONG_PTR SetClassLongPtr(HWND hWnd, int nIndex, LONG_PTR dwNewLong) {
    if (nIndex == GCL_HCURSOR && IsWindow(hWnd)) {
        auto* c = reinterpret_cast<wxCursor*>(dwNewLong);
        OnMain([&] { ToWx(hWnd)->SetCursor(c ? *c : wxNullCursor); });
    }
    return 0;
}

BOOL GetClassInfo(HINSTANCE, const char* lpClassName, WNDCLASS* lpWndClass) {
    if (!lpWndClass)
        return FALSE;
    auto it = Classes().find(lpClassName && !IS_INTRESOURCE(lpClassName) ? lpClassName : "");
    if (it == Classes().end())
        return FALSE;
    *lpWndClass = it->second;
    return TRUE;
}

ATOM RegisterClass(const WNDCLASS* lpWndClass) {
    if (!lpWndClass || !lpWndClass->lpszClassName)
        return 0;
    Classes()[lpWndClass->lpszClassName] = *lpWndClass;
    return static_cast<ATOM>(Classes().size());
}

BOOL UnregisterClass(const char* lpClassName, HINSTANCE) {
    return lpClassName && Classes().erase(lpClassName) ? TRUE : FALSE;
}

HWND CreateWindowEx(DWORD dwExStyle, const char* lpClassName, const char* lpWindowName, DWORD dwStyle, int X, int Y,
                    int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE, LPVOID lpParam) {
    return OnMain([&]() -> HWND {
        CREATESTRUCT cs;
        memset(&cs, 0, sizeof cs);
        cs.dwExStyle = dwExStyle;
        cs.lpszClass = lpClassName;
        cs.lpszName = lpWindowName;
        cs.style = static_cast<LONG>(dwStyle);
        cs.x = X;
        cs.y = Y;
        cs.cx = nWidth;
        cs.cy = nHeight;
        cs.hwndParent = hWndParent;
        cs.hMenu = hMenu;
        cs.lpCreateParams = lpParam;
        wxWindow* w = CreateWindowForClass(nullptr, cs);
        if (w && (dwStyle & WS_VISIBLE))
            w->Show();
        return ToHwnd(w);
    });
}

HWND CreateWindow(const char* lpClassName, const char* lpWindowName, DWORD dwStyle, int X, int Y, int nWidth,
                  int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {
    return CreateWindowEx(0, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance,
                          lpParam);
}

BOOL DestroyCaret() { return TRUE; }
BOOL CreateCaret(HWND, HBITMAP, int, int) { return TRUE; }
BOOL ShowCaret(HWND) { return TRUE; }
BOOL HideCaret(HWND) { return TRUE; }
BOOL SetCaretPos(int, int) { return TRUE; }

int GetLocaleInfo(LCID, DWORD LCType, char* lpLCData, int cchData) {
    std::string value;
    switch (LCType) {
    case LOCALE_IMEASURE: value = "0"; break;
    case LOCALE_SDECIMAL: value = "."; break;
    case LOCALE_STHOUSAND: value = ","; break;
    case LOCALE_IDEFAULTANSICODEPAGE: value = std::to_string(GetAnsiCodePage()); break;
    case LOCALE_SABBREVLANGNAME: value = GetResourceLanguage(); break;
    default: value = ""; break;
    }
    if (!lpLCData || cchData == 0)
        return static_cast<int>(value.size() + 1);
    strncpy(lpLCData, value.c_str(), static_cast<size_t>(cchData) - 1);
    lpLCData[cchData - 1] = 0;
    return static_cast<int>(strlen(lpLCData) + 1);
}

int GetLocaleInfoA(LCID Locale, DWORD LCType, char* lpLCData, int cchData) {
    return GetLocaleInfo(Locale, LCType, lpLCData, cchData);
}

namespace {

// "Text (*.txt)\0*.txt\0All\0*.*\0\0" -> "Text (*.txt)|*.txt|All|*.*"
wxString FilterFromDoubleNul(const char* f) {
    wxString out;
    if (!f)
        return wxString("*");
    while (*f) {
        if (!out.empty())
            out += '|';
        out += ToWx(f);
        f += strlen(f) + 1;
    }
    return out;
}

BOOL FileDialog(OPENFILENAME* ofn, bool open) {
    if (!ofn)
        return FALSE;
    return OnMain([&]() -> BOOL {
        long style = open ? wxFD_OPEN : wxFD_SAVE;
        if (open && (ofn->Flags & OFN_FILEMUSTEXIST))
            style |= wxFD_FILE_MUST_EXIST;
        if (!open && (ofn->Flags & OFN_OVERWRITEPROMPT))
            style |= wxFD_OVERWRITE_PROMPT;
        if (ofn->Flags & OFN_ALLOWMULTISELECT)
            style |= wxFD_MULTIPLE;
        wxString initial = ofn->lpstrFile ? ToWx(ofn->lpstrFile) : wxString();
        wxString dir = ofn->lpstrInitialDir ? NativePathWx(ofn->lpstrInitialDir) : wxString();
        wxWindow* parent = ofn->hwndOwner ? ToWx(ofn->hwndOwner) : MainWxWindow();
        wxFileDialog dlg(parent, ofn->lpstrTitle ? ToWx(ofn->lpstrTitle) : wxString(), dir,
                         wxFileName(NativePathWx(ofn->lpstrFile ? ofn->lpstrFile : "")).GetFullName(),
                         FilterFromDoubleNul(ofn->lpstrFilter), style);
        if (ofn->nFilterIndex > 0)
            dlg.SetFilterIndex(static_cast<int>(ofn->nFilterIndex) - 1);
        if (dlg.ShowModal() != wxID_OK)
            return FALSE;
        std::string path = AppPath(dlg.GetPath().utf8_str());
        if (!open && ofn->lpstrDefExt && *ofn->lpstrDefExt && path.find('.', path.rfind('\\') + 1) == std::string::npos)
            path += std::string(".") + ofn->lpstrDefExt;
        if (ofn->lpstrFile && ofn->nMaxFile) {
            strncpy(ofn->lpstrFile, path.c_str(), ofn->nMaxFile - 1);
            ofn->lpstrFile[ofn->nMaxFile - 1] = 0;
        }
        size_t slash = path.rfind('\\');
        ofn->nFileOffset = static_cast<WORD>(slash == std::string::npos ? 0 : slash + 1);
        size_t dot = path.rfind('.');
        ofn->nFileExtension = static_cast<WORD>(dot == std::string::npos || dot < ofn->nFileOffset ? path.size() : dot + 1);
        if (ofn->lpstrFileTitle && ofn->nMaxFileTitle) {
            strncpy(ofn->lpstrFileTitle, path.c_str() + ofn->nFileOffset, ofn->nMaxFileTitle - 1);
            ofn->lpstrFileTitle[ofn->nMaxFileTitle - 1] = 0;
        }
        ofn->nFilterIndex = static_cast<DWORD>(dlg.GetFilterIndex() + 1);
        (void)initial;
        return TRUE;
    });
}

} // namespace

BOOL GetOpenFileName(OPENFILENAME* lpofn) { return FileDialog(lpofn, true); }
BOOL GetSaveFileName(OPENFILENAME* lpofn) { return FileDialog(lpofn, false); }
BOOL GetOpenFileNameA(OPENFILENAME* lpofn) { return FileDialog(lpofn, true); }
BOOL GetSaveFileNameA(OPENFILENAME* lpofn) { return FileDialog(lpofn, false); }

BOOL ChooseColor(CHOOSECOLOR* lpcc) {
    if (!lpcc)
        return FALSE;
    return OnMain([&]() -> BOOL {
        wxColourData data;
        data.SetColour(ToWxColour(lpcc->rgbResult));
        wxColourDialog dlg(lpcc->hwndOwner ? ToWx(lpcc->hwndOwner) : MainWxWindow(), &data);
        if (dlg.ShowModal() != wxID_OK)
            return FALSE;
        lpcc->rgbResult = FromWxColour(dlg.GetColourData().GetColour());
        return TRUE;
    });
}

BOOL FlashWindow(HWND hWnd, BOOL) {
    if (IsWindow(hWnd))
        OnMain([&] {
            if (auto* tlw = wxDynamicCast(wxGetTopLevelParent(ToWx(hWnd)), wxTopLevelWindow))
                tlw->RequestUserAttention();
        });
    return TRUE;
}
