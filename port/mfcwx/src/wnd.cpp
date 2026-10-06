#include "internal.h"
#include "windows_impl.h"

#include <wx/display.h>
#include <wx/evtloop.h>
#include <wx/tooltip.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace mfcwx {

// ---------------------------------------------------------------------------------------------
// Threads

namespace {

const std::thread::id g_mainThread = std::this_thread::get_id();

} // namespace

bool IsMainThread() { return std::this_thread::get_id() == g_mainThread; }

void RunOnMainThread(const std::function<void()>& fn) {
    if (IsMainThread() || !wxTheApp) {
        fn();
        return;
    }
    std::mutex m;
    std::condition_variable cv;
    bool done = false;
    std::exception_ptr error;
    wxTheApp->CallAfter([&] {
        try {
            fn();
        } catch (...) {
            error = std::current_exception();
        }
        std::lock_guard<std::mutex> lock(m);
        done = true;
        cv.notify_one();
    });
    std::unique_lock<std::mutex> lock(m);
    cv.wait(lock, [&] { return done; });
    if (error)
        std::rethrow_exception(error);
}

void PostToMainThread(std::function<void()> fn) {
    if (wxTheApp)
        wxTheApp->CallAfter(std::move(fn));
}

// ---------------------------------------------------------------------------------------------
// Window registry

namespace {

std::unordered_map<wxWindow*, std::unique_ptr<WindowState>>& States() {
    static auto* states = new std::unordered_map<wxWindow*, std::unique_ptr<WindowState>>();
    return *states;
}

std::unordered_map<wxWindow*, CWnd*>& TempWrappers() {
    static auto* temps = new std::unordered_map<wxWindow*, CWnd*>();
    return *temps;
}

// Deleted together with the wx window; performs the final MFC-side cleanup.
class WindowTracker : public wxClientData {
public:
    explicit WindowTracker(wxWindow* window) : m_window(window) {}
    ~WindowTracker() override { OnWindowGone(m_window); }

private:
    wxWindow* m_window;
};

std::vector<CurrentMessage>& MessageStack() {
    static std::vector<CurrentMessage> stack;
    return stack;
}

} // namespace

WindowState* GetState(wxWindow* window) {
    if (!window)
        return nullptr;
    auto it = States().find(window);
    return it == States().end() ? nullptr : it->second.get();
}

WindowState& EnsureState(wxWindow* window) {
    auto& slot = States()[window];
    if (!slot) {
        slot.reset(new WindowState);
        slot->window = window;
        window->SetClientObject(new WindowTracker(window));
    }
    return *slot;
}

bool IsManagedWindow(wxWindow* window) { return window && States().count(window) != 0; }

void OnWindowGone(wxWindow* window) {
    auto it = States().find(window);
    if (it == States().end())
        return;
    std::unique_ptr<WindowState> state = std::move(it->second);
    States().erase(it);
    for (auto& t : state->timers)
        t.second->Stop();
    auto tmp = TempWrappers().find(window);
    if (tmp != TempWrappers().end()) {
        CWnd* wrapper = tmp->second;
        TempWrappers().erase(tmp);
        wrapper->m_hWnd = nullptr;
        delete wrapper;
    }
    CWnd* pWnd = state->permanent;
    if (pWnd && pWnd->m_hWnd == ToHwnd(window)) {
        if (!state->destroyNotified) {
            state->destroyNotified = true;
            pWnd->SendMessage(WM_DESTROY);
        }
        pWnd->m_hWnd = nullptr;
        pWnd->PostNcDestroy();
    }
}

CurrentMessage* GetCurrentMessageSlot() {
    auto& stack = MessageStack();
    return stack.empty() ? nullptr : &stack.back();
}

MessageScope::MessageScope(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, wxEvent* event) {
    CurrentMessage cm;
    cm.msg.hwnd = hWnd;
    cm.msg.message = message;
    cm.msg.wParam = wParam;
    cm.msg.lParam = lParam;
    cm.msg.time = static_cast<DWORD>(wxGetLocalTimeMillis().GetLo());
    wxPoint p = wxGetMousePosition();
    cm.msg.pt.x = p.x;
    cm.msg.pt.y = p.y;
    cm.event = event;
    cm.defaultCalled = false;
    MessageStack().push_back(cm);
}

MessageScope::~MessageScope() { MessageStack().pop_back(); }

bool MessageScope::DefaultCalled() const { return MessageStack().back().defaultCalled; }

CWnd* PermanentWnd(wxWindow* window) {
    WindowState* s = GetState(window);
    return s ? s->permanent : nullptr;
}

CWnd* WrapperFor(wxWindow* window) {
    if (!window)
        return nullptr;
    if (CWnd* p = PermanentWnd(window))
        return p;
    auto it = TempWrappers().find(window);
    if (it != TempWrappers().end())
        return it->second;
    EnsureState(window);
    CWnd* wrapper = new CWnd;
    wrapper->m_hWnd = ToHwnd(window);
    TempWrappers()[window] = wrapper;
    return wrapper;
}

bool HasMessageHandler(CWnd* pWnd, UINT message) {
    return pWnd && pWnd->FindMessageEntry(message, 0, 0) != nullptr;
}

// ---------------------------------------------------------------------------------------------
// Message dispatch

LRESULT DispatchMessageTo(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    wxWindow* window = ToWx(hWnd);
    if (!window || !IsManagedWindow(window))
        return 0;
    CWnd* pWnd = PermanentWnd(window);
    MessageScope scope(hWnd, msg, wParam, lParam, nullptr);
    if (pWnd)
        return pWnd->WindowProc(msg, wParam, lParam);
    return DefaultWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT SendFromEvent(wxWindow* window, UINT msg, WPARAM wParam, LPARAM lParam, wxEvent& event, bool* defaultCalled) {
    CWnd* pWnd = PermanentWnd(window);
    MessageScope scope(ToHwnd(window), msg, wParam, lParam, &event);
    LRESULT r = pWnd ? pWnd->WindowProc(msg, wParam, lParam) : DefaultWindowProc(ToHwnd(window), msg, wParam, lParam);
    if (defaultCalled)
        *defaultCalled = scope.DefaultCalled();
    return r;
}

// ---------------------------------------------------------------------------------------------
// Keyboard translation

UINT VirtualKeyFromWx(int code) {
    if (code >= 'a' && code <= 'z')
        return static_cast<UINT>(code - 'a' + 'A');
    if ((code >= 'A' && code <= 'Z') || (code >= '0' && code <= '9'))
        return static_cast<UINT>(code);
    switch (code) {
    case WXK_BACK: return VK_BACK;
    case WXK_TAB: return VK_TAB;
    case WXK_RETURN: case WXK_NUMPAD_ENTER: return VK_RETURN;
    case WXK_ESCAPE: return VK_ESCAPE;
    case WXK_SPACE: return VK_SPACE;
    case WXK_DELETE: case WXK_NUMPAD_DELETE: return VK_DELETE;
    case WXK_SHIFT: return VK_SHIFT;
    case WXK_ALT: return VK_MENU;
    case WXK_CONTROL: return VK_CONTROL;
    case WXK_PAUSE: return VK_PAUSE;
    case WXK_CAPITAL: return VK_CAPITAL;
    case WXK_END: case WXK_NUMPAD_END: return VK_END;
    case WXK_HOME: case WXK_NUMPAD_HOME: return VK_HOME;
    case WXK_LEFT: case WXK_NUMPAD_LEFT: return VK_LEFT;
    case WXK_UP: case WXK_NUMPAD_UP: return VK_UP;
    case WXK_RIGHT: case WXK_NUMPAD_RIGHT: return VK_RIGHT;
    case WXK_DOWN: case WXK_NUMPAD_DOWN: return VK_DOWN;
    case WXK_PAGEUP: case WXK_NUMPAD_PAGEUP: return VK_PRIOR;
    case WXK_PAGEDOWN: case WXK_NUMPAD_PAGEDOWN: return VK_NEXT;
    case WXK_INSERT: case WXK_NUMPAD_INSERT: return VK_INSERT;
    case WXK_HELP: return VK_HELP;
    case WXK_NUMPAD0: case WXK_NUMPAD1: case WXK_NUMPAD2: case WXK_NUMPAD3: case WXK_NUMPAD4:
    case WXK_NUMPAD5: case WXK_NUMPAD6: case WXK_NUMPAD7: case WXK_NUMPAD8: case WXK_NUMPAD9:
        return static_cast<UINT>(VK_NUMPAD0 + (code - WXK_NUMPAD0));
    case WXK_MULTIPLY: case WXK_NUMPAD_MULTIPLY: return VK_MULTIPLY;
    case WXK_ADD: case WXK_NUMPAD_ADD: return VK_ADD;
    case WXK_SUBTRACT: case WXK_NUMPAD_SUBTRACT: return VK_SUBTRACT;
    case WXK_DECIMAL: case WXK_NUMPAD_DECIMAL: return VK_DECIMAL;
    case WXK_DIVIDE: case WXK_NUMPAD_DIVIDE: return VK_DIVIDE;
    case WXK_NUMLOCK: return VK_NUMLOCK;
    case WXK_SCROLL: return VK_SCROLL;
    case ',': return VK_OEM_COMMA;
    case '.': return VK_OEM_PERIOD;
    case '-': return VK_OEM_MINUS;
    case '+': case '=': return VK_OEM_PLUS;
    default: break;
    }
    if (code >= WXK_F1 && code <= WXK_F24)
        return static_cast<UINT>(VK_F1 + (code - WXK_F1));
    return 0;
}

int WxKeyFromVirtual(UINT vk) {
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9'))
        return static_cast<int>(vk);
    switch (vk) {
    case VK_BACK: return WXK_BACK;
    case VK_TAB: return WXK_TAB;
    case VK_RETURN: return WXK_RETURN;
    case VK_ESCAPE: return WXK_ESCAPE;
    case VK_SPACE: return WXK_SPACE;
    case VK_DELETE: return WXK_DELETE;
    case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: return WXK_SHIFT;
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return WXK_CONTROL;
    case VK_MENU: case VK_LMENU: case VK_RMENU: return WXK_ALT;
    case VK_CAPITAL: return WXK_CAPITAL;
    case VK_END: return WXK_END;
    case VK_HOME: return WXK_HOME;
    case VK_LEFT: return WXK_LEFT;
    case VK_UP: return WXK_UP;
    case VK_RIGHT: return WXK_RIGHT;
    case VK_DOWN: return WXK_DOWN;
    case VK_PRIOR: return WXK_PAGEUP;
    case VK_NEXT: return WXK_PAGEDOWN;
    case VK_INSERT: return WXK_INSERT;
    case VK_NUMLOCK: return WXK_NUMLOCK;
    case VK_SCROLL: return WXK_SCROLL;
    case VK_LBUTTON: return WXK_LBUTTON;
    case VK_RBUTTON: return WXK_RBUTTON;
    case VK_MBUTTON: return WXK_MBUTTON;
    default: break;
    }
    if (vk >= VK_F1 && vk <= VK_F1 + 23)
        return WXK_F1 + static_cast<int>(vk - VK_F1);
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
        return WXK_NUMPAD0 + static_cast<int>(vk - VK_NUMPAD0);
    return 0;
}

LPARAM KeyLParam(const wxKeyEvent& e, bool up) {
    LPARAM lp = 1;
    if (e.AltDown())
        lp |= 1L << 29;
    if (up)
        lp |= (1L << 30) | (1L << 31);
    return lp;
}

WPARAM MouseFlags(const wxMouseEvent& e) {
    WPARAM f = 0;
    if (e.LeftIsDown())
        f |= MK_LBUTTON;
    if (e.RightIsDown())
        f |= MK_RBUTTON;
    if (e.MiddleIsDown())
        f |= MK_MBUTTON;
    if (e.ShiftDown())
        f |= MK_SHIFT;
    if (e.ControlDown())
        f |= MK_CONTROL;
    return f;
}

bool TranslateCharToAnsi(const wxKeyEvent& e, UINT& ch) {
    wxChar uc = e.GetUnicodeKey();
    int code = e.GetKeyCode();
    if (uc == WXK_NONE) {
        if (code > 0 && code < 32) {
            ch = static_cast<UINT>(code);
            return true;
        }
        return false;
    }
    if (uc == 127)
        return false;
    if (uc < 32) {
        ch = static_cast<UINT>(uc);
        return true;
    }
    char a;
    UnicodeToAnsi(static_cast<wchar_t>(uc), a);
    ch = static_cast<unsigned char>(a);
    return true;
}

// ---------------------------------------------------------------------------------------------
// Event bridge: generic window events

namespace {

void OnKeyDown(wxKeyEvent& e) {
    wxWindow* w = static_cast<wxWindow*>(e.GetEventObject());
    CWnd* pWnd = PermanentWnd(w);
    UINT msg = e.AltDown() && !e.ControlDown() ? WM_SYSKEYDOWN : WM_KEYDOWN;
    UINT vk = VirtualKeyFromWx(e.GetKeyCode());
    if (!vk || !HasMessageHandler(pWnd, msg)) {
        e.Skip();
        return;
    }
    bool def = false;
    SendFromEvent(w, msg, vk, KeyLParam(e, false), e, &def);
    if (def)
        e.Skip();
}

void OnKeyUp(wxKeyEvent& e) {
    wxWindow* w = static_cast<wxWindow*>(e.GetEventObject());
    CWnd* pWnd = PermanentWnd(w);
    UINT msg = e.AltDown() && !e.ControlDown() ? WM_SYSKEYUP : WM_KEYUP;
    UINT vk = VirtualKeyFromWx(e.GetKeyCode());
    if (!vk || !HasMessageHandler(pWnd, msg)) {
        e.Skip();
        return;
    }
    bool def = false;
    SendFromEvent(w, msg, vk, KeyLParam(e, true), e, &def);
    if (def)
        e.Skip();
}

void OnChar(wxKeyEvent& e) {
    wxWindow* w = static_cast<wxWindow*>(e.GetEventObject());
    WindowState* st = GetState(w);
    CWnd* pWnd = st ? st->permanent : nullptr;
    UINT ch = 0;
    if (!TranslateCharToAnsi(e, ch)) {
        e.Skip();
        return;
    }
    if (st && FilterEditChar(w, *st, ch, e))
        return;
    if (!HasMessageHandler(pWnd, WM_CHAR)) {
        e.Skip();
        return;
    }
    bool def = false;
    SendFromEvent(w, WM_CHAR, ch, KeyLParam(e, false), e, &def);
}

void SendMouse(wxMouseEvent& e, UINT msg) {
    wxWindow* w = static_cast<wxWindow*>(e.GetEventObject());
    CWnd* pWnd = PermanentWnd(w);
    if (!HasMessageHandler(pWnd, msg)) {
        e.Skip();
        return;
    }
    bool def = false;
    SendFromEvent(w, msg, MouseFlags(e), MAKELPARAM(e.GetX(), e.GetY()), e, &def);
    // Native controls need the default handling to keep working (focus, selection, ...).
    e.Skip();
}

void OnMouseWheel(wxMouseEvent& e) {
    wxWindow* w = static_cast<wxWindow*>(e.GetEventObject());
    CWnd* pWnd = PermanentWnd(w);
    if (!HasMessageHandler(pWnd, WM_MOUSEWHEEL) || e.GetWheelAxis() != wxMOUSE_WHEEL_VERTICAL) {
        e.Skip();
        return;
    }
    int delta = e.GetWheelRotation() * WHEEL_DELTA / std::max(1, e.GetWheelDelta());
    wxPoint screen = w->ClientToScreen(e.GetPosition());
    bool def = false;
    SendFromEvent(w, WM_MOUSEWHEEL, MAKEWPARAM(MouseFlags(e), static_cast<WORD>(static_cast<short>(delta))),
                  MAKELPARAM(screen.x, screen.y), e, &def);
    if (def)
        e.Skip();
}

void OnPaint(wxPaintEvent& e) {
    wxWindow* w = static_cast<wxWindow*>(e.GetEventObject());
    CWnd* pWnd = PermanentWnd(w);
    WindowState* st = GetState(w);
    if (st && st->customPaint) {
        st->customPaint(e);
        return;
    }
    if (!HasMessageHandler(pWnd, WM_PAINT)) {
        if (HasClientDCOverlay(w)) {
            wxPaintDC dc(w);
            PaintClientDCOverlay(w, dc);
        }
        e.Skip();
        return;
    }
    bool def = false;
    wxPaintDC dc(w);
    CDC* c = WrapDC(&dc, w);
    SendFromEvent(w, WM_PAINT, 0, 0, e, &def);
    PaintClientDCOverlay(w, dc);
    UnwrapDC(c);
    if (def)
        e.Skip();
}

void OnEraseBackground(wxEraseEvent& e) {
    wxWindow* w = static_cast<wxWindow*>(e.GetEventObject());
    CWnd* pWnd = PermanentWnd(w);
    if (!HasMessageHandler(pWnd, WM_ERASEBKGND) || !e.GetDC()) {
        e.Skip();
        return;
    }
    CDC* dc = WrapDC(e.GetDC(), w);
    bool def = false;
    LRESULT r = SendFromEvent(w, WM_ERASEBKGND, reinterpret_cast<WPARAM>(dc->m_hDC), 0, e, &def);
    UnwrapDC(dc);
    if (def || !r)
        e.Skip();
}

void OnSize(wxSizeEvent& e) {
    wxWindow* w = static_cast<wxWindow*>(e.GetEventObject());
    e.Skip();
    ClearClientDCOverlay(w);
    CWnd* pWnd = PermanentWnd(w);
    if (!HasMessageHandler(pWnd, WM_SIZE))
        return;
    wxSize sz = w->GetClientSize();
    WPARAM type = SIZE_RESTORED;
    if (auto* tlw = wxDynamicCast(w, wxTopLevelWindow)) {
        if (tlw->IsIconized())
            type = SIZE_MINIMIZED;
        else if (tlw->IsMaximized())
            type = SIZE_MAXIMIZED;
    }
    SendFromEvent(w, WM_SIZE, type, MAKELPARAM(sz.x, sz.y), e, nullptr);
}

void OnMove(wxMoveEvent& e) {
    wxWindow* w = static_cast<wxWindow*>(e.GetEventObject());
    e.Skip();
    CWnd* pWnd = PermanentWnd(w);
    if (!HasMessageHandler(pWnd, WM_MOVE))
        return;
    wxPoint p = w->GetPosition();
    SendFromEvent(w, WM_MOVE, 0, MAKELPARAM(p.x, p.y), e, nullptr);
}

void OnFocus(wxFocusEvent& e, bool set) {
    wxWindow* w = static_cast<wxWindow*>(e.GetEventObject());
    e.Skip();
    WindowState* st = GetState(w);
    if (!st)
        return;
    HWND other = ToHwnd(e.GetWindow());
    CWnd* pWnd = st->permanent;
    if (HasMessageHandler(pWnd, set ? WM_SETFOCUS : WM_KILLFOCUS))
        SendFromEvent(w, set ? WM_SETFOCUS : WM_KILLFOCUS, reinterpret_cast<WPARAM>(other), 0, e, nullptr);
    NotifyFocusChange(w, *st, set);
}

void OnContextMenu(wxContextMenuEvent& e) {
    wxWindow* w = static_cast<wxWindow*>(e.GetEventObject());
    for (wxWindow* t = w; t; t = t->GetParent()) {
        CWnd* pWnd = PermanentWnd(t);
        if (HasMessageHandler(pWnd, WM_CONTEXTMENU)) {
            wxPoint p = e.GetPosition();
            LPARAM lp = p == wxDefaultPosition ? static_cast<LPARAM>(-1) : MAKELPARAM(p.x, p.y);
            bool def = false;
            SendFromEvent(t, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(ToHwnd(w)), lp, e, &def);
            if (def)
                e.Skip();
            return;
        }
        if (t->IsTopLevel())
            break;
    }
    e.Skip();
}

void OnSetCursor(wxSetCursorEvent& e) {
    wxWindow* w = static_cast<wxWindow*>(e.GetEventObject());
    CWnd* pWnd = PermanentWnd(w);
    if (!HasMessageHandler(pWnd, WM_SETCURSOR)) {
        e.Skip();
        return;
    }
    ResetCursorRequest();
    bool def = false;
    LRESULT r = SendFromEvent(w, WM_SETCURSOR, reinterpret_cast<WPARAM>(ToHwnd(w)),
                              MAKELPARAM(HTCLIENT, WM_MOUSEMOVE), e, &def);
    const wxCursor* c = RequestedCursor();
    if (r && c)
        e.SetCursor(*c);
    else
        e.Skip();
}

void OnShow(wxShowEvent& e) {
    wxWindow* w = static_cast<wxWindow*>(e.GetEventObject());
    e.Skip();
    CWnd* pWnd = PermanentWnd(w);
    if (HasMessageHandler(pWnd, WM_SHOWWINDOW))
        SendFromEvent(w, WM_SHOWWINDOW, e.IsShown() ? TRUE : FALSE, 0, e, nullptr);
}

void OnScrollWin(wxScrollWinEvent& e) {
    wxWindow* w = static_cast<wxWindow*>(e.GetEventObject());
    CWnd* pWnd = PermanentWnd(w);
    UINT msg = e.GetOrientation() == wxVERTICAL ? WM_VSCROLL : WM_HSCROLL;
    if (!HasMessageHandler(pWnd, msg)) {
        e.Skip();
        return;
    }
    wxEventType t = e.GetEventType();
    UINT code = SB_ENDSCROLL;
    if (t == wxEVT_SCROLLWIN_TOP)
        code = SB_TOP;
    else if (t == wxEVT_SCROLLWIN_BOTTOM)
        code = SB_BOTTOM;
    else if (t == wxEVT_SCROLLWIN_LINEUP)
        code = SB_LINEUP;
    else if (t == wxEVT_SCROLLWIN_LINEDOWN)
        code = SB_LINEDOWN;
    else if (t == wxEVT_SCROLLWIN_PAGEUP)
        code = SB_PAGEUP;
    else if (t == wxEVT_SCROLLWIN_PAGEDOWN)
        code = SB_PAGEDOWN;
    else if (t == wxEVT_SCROLLWIN_THUMBTRACK)
        code = SB_THUMBTRACK;
    else if (t == wxEVT_SCROLLWIN_THUMBRELEASE)
        code = SB_THUMBPOSITION;
    WPARAM wp = MAKEWPARAM(code, static_cast<WORD>(e.GetPosition()));
    SendFromEvent(w, msg, wp, 0, e, nullptr);
    if (code == SB_THUMBPOSITION)
        SendFromEvent(w, msg, MAKEWPARAM(SB_ENDSCROLL, 0), 0, e, nullptr);
}

void OnDestroyEvent(wxWindowDestroyEvent& e) {
    e.Skip();
    wxWindow* w = static_cast<wxWindow*>(e.GetEventObject());
    if (w != e.GetWindow())
        return;
    WindowState* st = GetState(w);
    if (!st || st->destroyNotified || !st->permanent)
        return;
    st->destroyNotified = true;
    st->permanent->SendMessage(WM_DESTROY);
}

} // namespace

void HookWindow(wxWindow* window, ControlKind kind, int winId, DWORD style, DWORD exStyle) {
    WindowState& st = EnsureState(window);
    st.kind = kind;
    st.winId = winId;
    st.style = style;
    st.exStyle = exStyle;
    if (st.hooked)
        return;
    st.hooked = true;
    window->Bind(wxEVT_KEY_DOWN, &OnKeyDown);
    window->Bind(wxEVT_KEY_UP, &OnKeyUp);
    window->Bind(wxEVT_CHAR, &OnChar);
    window->Bind(wxEVT_LEFT_DOWN, [](wxMouseEvent& e) { SendMouse(e, WM_LBUTTONDOWN); });
    window->Bind(wxEVT_LEFT_UP, [](wxMouseEvent& e) { SendMouse(e, WM_LBUTTONUP); });
    window->Bind(wxEVT_LEFT_DCLICK, [](wxMouseEvent& e) { SendMouse(e, WM_LBUTTONDBLCLK); });
    window->Bind(wxEVT_RIGHT_DOWN, [](wxMouseEvent& e) { SendMouse(e, WM_RBUTTONDOWN); });
    window->Bind(wxEVT_RIGHT_UP, [](wxMouseEvent& e) { SendMouse(e, WM_RBUTTONUP); });
    window->Bind(wxEVT_RIGHT_DCLICK, [](wxMouseEvent& e) { SendMouse(e, WM_RBUTTONDBLCLK); });
    window->Bind(wxEVT_MIDDLE_DOWN, [](wxMouseEvent& e) { SendMouse(e, WM_MBUTTONDOWN); });
    window->Bind(wxEVT_MIDDLE_UP, [](wxMouseEvent& e) { SendMouse(e, WM_MBUTTONUP); });
    window->Bind(wxEVT_MOTION, [](wxMouseEvent& e) { SendMouse(e, WM_MOUSEMOVE); });
    window->Bind(wxEVT_MOUSEWHEEL, &OnMouseWheel);
    window->Bind(wxEVT_PAINT, &OnPaint);
    window->Bind(wxEVT_ERASE_BACKGROUND, &OnEraseBackground);
    window->Bind(wxEVT_SIZE, &OnSize);
    window->Bind(wxEVT_MOVE, &OnMove);
    window->Bind(wxEVT_SET_FOCUS, [](wxFocusEvent& e) { OnFocus(e, true); });
    window->Bind(wxEVT_KILL_FOCUS, [](wxFocusEvent& e) { OnFocus(e, false); });
    window->Bind(wxEVT_CONTEXT_MENU, &OnContextMenu);
    window->Bind(wxEVT_SET_CURSOR, &OnSetCursor);
    window->Bind(wxEVT_SHOW, &OnShow);
    window->Bind(wxEVT_DESTROY, &OnDestroyEvent);
    for (const wxEventTypeTag<wxScrollWinEvent>& t : {wxEVT_SCROLLWIN_TOP, wxEVT_SCROLLWIN_BOTTOM, wxEVT_SCROLLWIN_LINEUP, wxEVT_SCROLLWIN_LINEDOWN,
                          wxEVT_SCROLLWIN_PAGEUP, wxEVT_SCROLLWIN_PAGEDOWN, wxEVT_SCROLLWIN_THUMBTRACK,
                          wxEVT_SCROLLWIN_THUMBRELEASE})
        window->Bind(t, &OnScrollWin);
    BindControlEvents(window, st);
}

// ---------------------------------------------------------------------------------------------
// Timers

namespace {

class TimerOwner : public wxTimer {
public:
    TimerOwner(wxWindow* window, UINT_PTR id) : m_window(window), m_id(id) {}
    void Notify() override {
        wxWindow* w = m_window;
        UINT_PTR id = m_id;
        WindowState* st = GetState(w);
        if (!st)
            return;
        auto proc = st->timerProcs.find(id);
        if (proc != st->timerProcs.end() && proc->second) {
            proc->second(ToHwnd(w), WM_TIMER, id, GetTickCount());
            return;
        }
        DispatchMessageTo(ToHwnd(w), WM_TIMER, id, 0);
    }

private:
    wxWindow* m_window;
    UINT_PTR m_id;
};

} // namespace

UINT_PTR StartTimer(wxWindow* window, UINT_PTR id, UINT elapse, TIMERPROC proc) {
    WindowState& st = EnsureState(window);
    auto it = st.timers.find(id);
    if (it != st.timers.end())
        it->second->Stop();
    std::unique_ptr<wxTimer> timer(new TimerOwner(window, id));
    timer->Start(static_cast<int>(std::max<UINT>(elapse, USER_TIMER_MINIMUM)));
    st.timers[id] = std::move(timer);
    st.timerProcs[id] = proc;
    return id ? id : 1;
}

bool StopTimer(wxWindow* window, UINT_PTR id) {
    WindowState* st = GetState(window);
    if (!st)
        return false;
    auto it = st->timers.find(id);
    if (it == st->timers.end())
        return false;
    it->second->Stop();
    // The timer may be the one currently notifying; delete it later.
    wxTimer* timer = it->second.release();
    st->timers.erase(it);
    st->timerProcs.erase(id);
    if (wxTheApp)
        wxTheApp->CallAfter([timer] { delete timer; });
    return true;
}

} // namespace mfcwx

using namespace mfcwx;

// ---------------------------------------------------------------------------------------------
// CWnd

CRuntimeClass CWnd::classCWnd = {"CWnd", sizeof(CWnd), 0xFFFF, &CWnd::CreateObject, &CCmdTarget::GetThisClass, nullptr};
static AFX_CLASSINIT _init_CWnd(&CWnd::classCWnd);
CRuntimeClass* CWnd::GetThisClass() { return &CWnd::classCWnd; }
CRuntimeClass* CWnd::GetRuntimeClass() const { return &CWnd::classCWnd; }
CObject* CWnd::CreateObject() { return new CWnd; }

const AFX_MSGMAP* CWnd::GetMessageMap() const { return GetThisMessageMap(); }
const AFX_MSGMAP* CWnd::GetThisMessageMap() {
    static const AFX_MSGMAP_ENTRY entries[] = {{0, 0, 0, 0, AfxSig_end, nullptr}};
    static const AFX_MSGMAP map = {&CCmdTarget::GetThisMessageMap, &entries[0]};
    return &map;
}

const CWnd wndTop;
const CWnd wndBottom;
const CWnd wndTopMost;
const CWnd wndNoTopMost;

CWnd::CWnd() : m_hWnd(nullptr) {}

CWnd::~CWnd() {
    if (m_hWnd && IsMainThread()) {
        wxWindow* w = GetWx();
        WindowState* st = GetState(w);
        if (st && st->permanent == this) {
            st->permanent = nullptr;
            if (!st->destroyNotified && !w->IsBeingDeleted()) {
                st->destroyNotified = true;
                m_hWnd = nullptr;
                w->Destroy();
            }
        }
        m_hWnd = nullptr;
    }
}

CWnd* CWnd::FromWx(wxWindow* window) { return WrapperFor(window); }

CWnd* CWnd::FromHandle(HWND hWnd) {
    if (!hWnd)
        return nullptr;
    return OnMain([&] { return WrapperFor(ToWx(hWnd)); });
}

CWnd* CWnd::FromHandlePermanent(HWND hWnd) {
    if (!hWnd)
        return nullptr;
    return PermanentWnd(ToWx(hWnd));
}

void CWnd::DeleteTempMap() {}

void CWnd::AttachWx(wxWindow* window) {
    WindowState& st = EnsureState(window);
    auto tmp = TempWrappers().find(window);
    if (tmp != TempWrappers().end()) {
        tmp->second->m_hWnd = nullptr;
        delete tmp->second;
        TempWrappers().erase(tmp);
    }
    st.permanent = this;
    m_hWnd = ToHwnd(window);
}

BOOL CWnd::Attach(HWND hWndNew) {
    if (!hWndNew)
        return FALSE;
    OnMain([&] { AttachWx(ToWx(hWndNew)); });
    return TRUE;
}

HWND CWnd::Detach() {
    HWND h = m_hWnd;
    if (h) {
        WindowState* st = GetState(GetWx());
        if (st && st->permanent == this)
            st->permanent = nullptr;
    }
    m_hWnd = nullptr;
    return h;
}

BOOL CWnd::SubclassWindow(HWND hWnd) {
    if (!hWnd)
        return FALSE;
    return OnMain([&] {
        wxWindow* w = ToWx(hWnd);
        w = PrepareForSubclass(w, this);
        AttachWx(w);
        PreSubclassWindow();
        return TRUE;
    });
}

BOOL CWnd::SubclassDlgItem(UINT nID, CWnd* pParent) {
    if (!pParent)
        return FALSE;
    HWND h = nullptr;
    pParent->GetDlgItem(static_cast<int>(nID), &h);
    return h ? SubclassWindow(h) : FALSE;
}

HWND CWnd::UnsubclassWindow() { return Detach(); }

BOOL CWnd::PreCreateWindow(CREATESTRUCT&) { return TRUE; }

BOOL CWnd::Create(const char* lpszClassName, const char* lpszWindowName, DWORD dwStyle, const RECT& rect,
                  CWnd* pParentWnd, UINT nID, CCreateContext* pContext) {
    return CreateEx(0, lpszClassName, lpszWindowName, dwStyle | WS_CHILD, rect.left, rect.top,
                    rect.right - rect.left, rect.bottom - rect.top, pParentWnd ? pParentWnd->m_hWnd : nullptr,
                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(nID)), pContext);
}

BOOL CWnd::CreateEx(DWORD dwExStyle, const char* lpszClassName, const char* lpszWindowName, DWORD dwStyle,
                    const RECT& rect, CWnd* pParentWnd, UINT nID, LPVOID lpParam) {
    return CreateEx(dwExStyle, lpszClassName, lpszWindowName, dwStyle, rect.left, rect.top, rect.right - rect.left,
                    rect.bottom - rect.top, pParentWnd ? pParentWnd->m_hWnd : nullptr,
                    reinterpret_cast<HMENU>(static_cast<UINT_PTR>(nID)), lpParam);
}

BOOL CWnd::CreateEx(DWORD dwExStyle, const char* lpszClassName, const char* lpszWindowName, DWORD dwStyle, int x,
                    int y, int nWidth, int nHeight, HWND hWndParent, HMENU nIDorHMenu, LPVOID lpParam) {
    return OnMain([&]() -> BOOL {
        CREATESTRUCT cs;
        memset(&cs, 0, sizeof cs);
        cs.dwExStyle = dwExStyle;
        cs.lpszClass = lpszClassName;
        cs.lpszName = lpszWindowName;
        cs.style = static_cast<LONG>(dwStyle);
        cs.x = x;
        cs.y = y;
        cs.cx = nWidth;
        cs.cy = nHeight;
        cs.hwndParent = hWndParent;
        cs.hMenu = nIDorHMenu;
        cs.lpCreateParams = lpParam;
        if (!PreCreateWindow(cs))
            return FALSE;
        wxWindow* window = CreateWindowForClass(this, cs);
        if (!window)
            return FALSE;
        AttachWx(window);
        PreSubclassWindow();
        if (SendMessage(WM_CREATE, 0, reinterpret_cast<LPARAM>(&cs)) == -1) {
            DestroyWindow();
            return FALSE;
        }
        if (m_hWnd) {
            wxSize sz = window->GetClientSize();
            SendMessage(WM_SIZE, SIZE_RESTORED, MAKELPARAM(sz.x, sz.y));
            if (cs.style & WS_VISIBLE)
                window->Show();
        }
        return m_hWnd != nullptr;
    });
}

BOOL CWnd::DestroyWindow() {
    if (!m_hWnd)
        return FALSE;
    return OnMain([&]() -> BOOL {
        wxWindow* w = GetWx();
        WindowState* st = GetState(w);
        if (st && !st->destroyNotified) {
            st->destroyNotified = true;
            SendMessage(WM_DESTROY);
        }
        if (!m_hWnd)
            return TRUE;
        DestroyWxWindow(w);
        return TRUE;
    });
}

LRESULT CWnd::SendMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    return ::SendMessage(m_hWnd, message, wParam, lParam);
}

BOOL CWnd::PostMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    return ::PostMessage(m_hWnd, message, wParam, lParam);
}

BOOL CWnd::SendNotifyMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    return ::SendNotifyMessage(m_hWnd, message, wParam, lParam);
}

void CWnd::SendMessageToDescendants(UINT message, WPARAM wParam, LPARAM lParam, BOOL bDeep, BOOL bOnlyPerm) {
    OnMain([&] {
        std::function<void(wxWindow*)> walk = [&](wxWindow* parent) {
            for (wxWindow* child : parent->GetChildren()) {
                if (!IsManagedWindow(child))
                    continue;
                if (!bOnlyPerm || PermanentWnd(child))
                    DispatchMessageTo(ToHwnd(child), message, wParam, lParam);
                if (bDeep && IsManagedWindow(child))
                    walk(child);
            }
        };
        if (m_hWnd)
            walk(GetWx());
    });
}

LRESULT CWnd::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    LRESULT result = 0;
    if (!OnWndMsg(message, wParam, lParam, &result))
        result = DefWindowProc(message, wParam, lParam);
    return result;
}

LRESULT CWnd::DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    return DefaultWindowProc(m_hWnd, message, wParam, lParam);
}

LRESULT CWnd::Default() {
    CurrentMessage* cm = GetCurrentMessageSlot();
    if (!cm)
        return 0;
    return DefWindowProc(cm->msg.message, cm->msg.wParam, cm->msg.lParam);
}

const MSG* CWnd::GetCurrentMessage() {
    CurrentMessage* cm = GetCurrentMessageSlot();
    static MSG empty;
    return cm ? &cm->msg : &empty;
}

struct AFX_NOTIFY {
    LRESULT* pResult;
    NMHDR* pNMHDR;
};

BOOL CWnd::OnCommand(WPARAM wParam, LPARAM lParam) {
    UINT nID = LOWORD(wParam);
    int nCode = HIWORD(wParam);
    HWND hWndCtrl = reinterpret_cast<HWND>(lParam);
    if (hWndCtrl) {
        if (ReflectChildNotify(WM_COMMAND, wParam, lParam, nullptr))
            return TRUE;
    }
    if (nID == 0)
        return FALSE;
    return OnCmdMsg(nID, nCode, nullptr, nullptr);
}

BOOL CWnd::OnNotify(WPARAM, LPARAM lParam, LRESULT* pResult) {
    NMHDR* pNMHDR = reinterpret_cast<NMHDR*>(lParam);
    HWND hWndCtrl = pNMHDR->hwndFrom;
    UINT nID = static_cast<UINT>(pNMHDR->idFrom);
    int nCode = static_cast<int>(pNMHDR->code);
    if (hWndCtrl && ReflectChildNotify(WM_NOTIFY, 0, lParam, pResult))
        return TRUE;
    AFX_NOTIFY notify;
    notify.pResult = pResult;
    notify.pNMHDR = pNMHDR;
    return OnCmdMsg(nID, MAKELONG(nCode, WM_NOTIFY), &notify, nullptr);
}

BOOL CWnd::ReflectChildNotify(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult) {
    HWND hChild = nullptr;
    switch (message) {
    case WM_COMMAND:
        hChild = reinterpret_cast<HWND>(lParam);
        break;
    case WM_NOTIFY:
        hChild = reinterpret_cast<NMHDR*>(lParam)->hwndFrom;
        break;
    case WM_DRAWITEM:
        hChild = reinterpret_cast<LPDRAWITEMSTRUCT>(lParam)->hwndItem;
        break;
    case WM_HSCROLL:
    case WM_VSCROLL:
    case WM_CTLCOLOR:
        hChild = reinterpret_cast<HWND>(lParam);
        break;
    default:
        break;
    }
    CWnd* child = hChild ? FromHandlePermanent(hChild) : nullptr;
    if (!child || child == this)
        return FALSE;
    return child->OnChildNotify(message, wParam, lParam, pResult);
}

BOOL CWnd::OnChildNotify(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult) {
    UINT reflected = message + WM_REFLECT_BASE;
    UINT code = 0;
    if (message == WM_COMMAND)
        code = HIWORD(wParam);
    else if (message == WM_NOTIFY)
        code = reinterpret_cast<NMHDR*>(lParam)->code;
    const AFX_MSGMAP_ENTRY* e = nullptr;
    for (const AFX_MSGMAP* map = GetMessageMap(); map && !e; map = map->pfnGetBaseMap ? map->pfnGetBaseMap() : nullptr) {
        for (const AFX_MSGMAP_ENTRY* it = map->lpEntries; it->nSig != AfxSig_end; ++it)
            if (it->nMessage == reflected && (message != WM_COMMAND && message != WM_NOTIFY ? true : it->nCode == code)) {
                e = it;
                break;
            }
        if (!map->pfnGetBaseMap)
            break;
    }
    if (!e)
        return FALSE;
    switch (e->nSig) {
    case AfxSig_vv_reflect:
        (this->*e->pfn)();
        return TRUE;
    case AfxSig_bv_reflect:
        return (this->*reinterpret_cast<BOOL (CCmdTarget::*)()>(e->pfn))();
    case AfxSig_vNMHDRpl:
        (this->*reinterpret_cast<void (CCmdTarget::*)(NMHDR*, LRESULT*)>(e->pfn))(
            reinterpret_cast<NMHDR*>(lParam), pResult);
        return TRUE;
    case AfxSig_bNMHDRpl:
        return (this->*reinterpret_cast<BOOL (CCmdTarget::*)(NMHDR*, LRESULT*)>(e->pfn))(
            reinterpret_cast<NMHDR*>(lParam), pResult);
    case AfxSig_hDw: {
        HBRUSH h = (static_cast<CWnd*>(this)->*reinterpret_cast<HBRUSH (CWnd::*)(CDC*, UINT)>(e->pfn))(
            CDC::FromHandle(reinterpret_cast<HDC>(wParam)), static_cast<UINT>(pResult ? *pResult : 0));
        if (pResult)
            *pResult = reinterpret_cast<LRESULT>(h);
        return h != nullptr;
    }
    case AfxSig_vOWNER:
        (static_cast<CWnd*>(this)->*reinterpret_cast<void (CWnd::*)(LPDRAWITEMSTRUCT)>(e->pfn))(
            reinterpret_cast<LPDRAWITEMSTRUCT>(lParam));
        return TRUE;
    case AfxSig_vwwW:
        (static_cast<CWnd*>(this)->*reinterpret_cast<void (CWnd::*)(UINT, UINT)>(e->pfn))(LOWORD(wParam),
                                                                                         HIWORD(wParam));
        return TRUE;
    default:
        return FALSE;
    }
}

BOOL CWnd::OnWndMsg(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult) {
    LRESULT lResult = 0;
    if (message == WM_COMMAND) {
        if (OnCommand(wParam, lParam)) {
            if (pResult)
                *pResult = 1;
            return TRUE;
        }
        return FALSE;
    }
    if (message == WM_NOTIFY) {
        if (OnNotify(wParam, lParam, &lResult)) {
            if (pResult)
                *pResult = lResult;
            return TRUE;
        }
        return FALSE;
    }
    UINT lookup = message;
    if (message >= WM_CTLCOLORMSGBOX && message <= WM_CTLCOLORSTATIC)
        lookup = WM_CTLCOLOR;
    if (message == WM_DRAWITEM || message == WM_HSCROLL || message == WM_VSCROLL ||
        (message >= WM_CTLCOLORMSGBOX && message <= WM_CTLCOLORSTATIC)) {
        LRESULT reflectResult = message >= WM_CTLCOLORMSGBOX ? static_cast<LRESULT>(message - WM_CTLCOLORMSGBOX) : 0;
        if (ReflectChildNotify(lookup, wParam, lParam, &reflectResult)) {
            if (pResult)
                *pResult = reflectResult;
            return TRUE;
        }
    }
    const AFX_MSGMAP_ENTRY* e = FindMessageEntry(lookup, 0, 0);
    if (!e)
        return FALSE;
    union {
        AFX_PMSG pfn;
        void (CWnd::*vv)();
        BOOL (CWnd::*bv)();
        LRESULT (CWnd::*lwl)(WPARAM, LPARAM);
        int (CWnd::*is)(LPCREATESTRUCT);
        BOOL (CWnd::*bD)(CDC*);
        void (CWnd::*vwii)(UINT, int, int);
        void (CWnd::*vwwx)(UINT, UINT, UINT);
        void (CWnd::*vwp)(UINT, CPoint);
        BOOL (CWnd::*bwsp)(UINT, short, CPoint);
        void (CWnd::*vwwW)(UINT, UINT, CScrollBar*);
        HBRUSH (CWnd::*hDWw)(CDC*, CWnd*, UINT);
        void (CWnd::*vOWNER)(int, LPDRAWITEMSTRUCT);
        void (CWnd::*vMEASURE)(int, LPMEASUREITEMSTRUCT);
        void (CWnd::*vW)(CWnd*);
        void (CWnd::*vWp)(CWnd*, CPoint);
        BOOL (CWnd::*bWww)(CWnd*, UINT, UINT);
        void (CWnd::*vbw)(BOOL, UINT);
        void (CWnd::*vwl)(UINT, LPARAM);
        void (CWnd::*vw_ptr)(UINT_PTR);
        void (CWnd::*vw_uint)(UINT);
        HCURSOR (CWnd::*hv)();
        BOOL (CWnd::*bHELPINFO)(HELPINFO*);
        void (CWnd::*vPOS)(MINMAXINFO*);
        void (CWnd::*vwWb)(UINT, CWnd*, BOOL);
        UINT (CWnd::*wv)();
        void (CWnd::*vHDROP)(HDROP);
        void (CWnd::*vMwb)(CMenu*, UINT, BOOL);
        void (CWnd::*vii)(int, int);
        void (CWnd::*vbWW)(BOOL, CWnd*, CWnd*);
        BOOL (CWnd::*bb)(BOOL);
        void (CWnd::*vb)(BOOL);
        void (CWnd::*vwwh)(UINT, UINT, HMENU);
        void (CWnd::*vRECT)(UINT, LPRECT);
    } mmf;
    mmf.pfn = e->pfn;
    UINT_PTR sig = e->nMessage == 0xC000 ? AfxSig_lwl : e->nSig;
    switch (sig) {
    case AfxSig_vv:
        (this->*mmf.vv)();
        break;
    case AfxSig_bv:
        lResult = (this->*mmf.bv)();
        break;
    case AfxSig_lwl:
        lResult = (this->*mmf.lwl)(wParam, lParam);
        break;
    case AfxSig_is:
        lResult = (this->*mmf.is)(reinterpret_cast<LPCREATESTRUCT>(lParam));
        break;
    case AfxSig_bD:
        lResult = (this->*mmf.bD)(CDC::FromHandle(reinterpret_cast<HDC>(wParam)));
        break;
    case AfxSig_vwii:
        (this->*mmf.vwii)(static_cast<UINT>(wParam), LOWORD(lParam), HIWORD(lParam));
        break;
    case AfxSig_vwwx:
        (this->*mmf.vwwx)(static_cast<UINT>(wParam), LOWORD(lParam), HIWORD(lParam));
        break;
    case AfxSig_vwp:
        (this->*mmf.vwp)(static_cast<UINT>(wParam), CPoint(static_cast<short>(LOWORD(lParam)),
                                                           static_cast<short>(HIWORD(lParam))));
        break;
    case AfxSig_bwsp: {
        CPoint pt(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)));
        lResult = (this->*mmf.bwsp)(LOWORD(wParam), static_cast<short>(HIWORD(wParam)), pt);
        break;
    }
    case AfxSig_vwwW:
        (this->*mmf.vwwW)(LOWORD(wParam), HIWORD(wParam),
                          reinterpret_cast<CScrollBar*>(CWnd::FromHandle(reinterpret_cast<HWND>(lParam))));
        break;
    case AfxSig_hDWw: {
        UINT ctlType = message == WM_CTLCOLOR ? static_cast<UINT>(HIWORD(lParam)) : message - WM_CTLCOLORMSGBOX;
        lResult = reinterpret_cast<LRESULT>((this->*mmf.hDWw)(CDC::FromHandle(reinterpret_cast<HDC>(wParam)),
                                                              CWnd::FromHandle(reinterpret_cast<HWND>(lParam)),
                                                              ctlType));
        break;
    }
    case AfxSig_vOWNER:
        (this->*mmf.vOWNER)(static_cast<int>(wParam), reinterpret_cast<LPDRAWITEMSTRUCT>(lParam));
        lResult = TRUE;
        break;
    case AfxSig_vMEASURE:
        (this->*mmf.vMEASURE)(static_cast<int>(wParam), reinterpret_cast<LPMEASUREITEMSTRUCT>(lParam));
        lResult = TRUE;
        break;
    case AfxSig_vW:
        (this->*mmf.vW)(CWnd::FromHandle(reinterpret_cast<HWND>(wParam)));
        break;
    case AfxSig_vWp:
        (this->*mmf.vWp)(CWnd::FromHandle(reinterpret_cast<HWND>(wParam)),
                         CPoint(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))));
        break;
    case AfxSig_bWww:
        lResult = (this->*mmf.bWww)(CWnd::FromHandle(reinterpret_cast<HWND>(wParam)), LOWORD(lParam), HIWORD(lParam));
        break;
    case AfxSig_vbw:
        (this->*mmf.vbw)(static_cast<BOOL>(wParam), static_cast<UINT>(lParam));
        break;
    case AfxSig_vwl:
        (this->*mmf.vwl)(static_cast<UINT>(wParam), lParam);
        break;
    case AfxSig_vw_ptr:
        (this->*mmf.vw_ptr)(static_cast<UINT_PTR>(wParam));
        break;
    case AfxSig_vw_uint:
        (this->*mmf.vw_uint)(static_cast<UINT>(wParam));
        break;
    case AfxSig_hv:
        lResult = reinterpret_cast<LRESULT>((this->*mmf.hv)());
        break;
    case AfxSig_bHELPINFO:
        lResult = (this->*mmf.bHELPINFO)(reinterpret_cast<HELPINFO*>(lParam));
        break;
    case AfxSig_vPOS:
        (this->*mmf.vPOS)(reinterpret_cast<MINMAXINFO*>(lParam));
        break;
    case AfxSig_vwWb:
        (this->*mmf.vwWb)(LOWORD(wParam), CWnd::FromHandle(reinterpret_cast<HWND>(lParam)), HIWORD(wParam));
        break;
    case AfxSig_wv:
        lResult = (this->*mmf.wv)();
        break;
    case AfxSig_vHDROP:
        (this->*mmf.vHDROP)(reinterpret_cast<HDROP>(wParam));
        break;
    case AfxSig_vMwb:
        (this->*mmf.vMwb)(CMenu::FromHandle(reinterpret_cast<HMENU>(wParam)), LOWORD(lParam), HIWORD(lParam));
        break;
    case AfxSig_vii:
        (this->*mmf.vii)(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)));
        break;
    case AfxSig_vbWW:
        (this->*mmf.vbWW)(reinterpret_cast<HWND>(lParam) == m_hWnd, CWnd::FromHandle(reinterpret_cast<HWND>(lParam)),
                          CWnd::FromHandle(reinterpret_cast<HWND>(wParam)));
        break;
    case AfxSig_bb:
        lResult = (this->*mmf.bb)(static_cast<BOOL>(wParam));
        break;
    case AfxSig_vb:
        (this->*mmf.vb)(static_cast<BOOL>(wParam));
        break;
    case AfxSig_vwwh:
        (this->*mmf.vwwh)(LOWORD(wParam), HIWORD(wParam), reinterpret_cast<HMENU>(lParam));
        break;
    case AfxSig_vRECT:
        (this->*mmf.vRECT)(static_cast<UINT>(wParam), reinterpret_cast<LPRECT>(lParam));
        lResult = TRUE;
        break;
    default:
        return FALSE;
    }
    if (pResult)
        *pResult = lResult;
    return TRUE;
}

BOOL CWnd::PreTranslateMessage(MSG*) { return FALSE; }

// Default handlers: forward to the window's default processing like MFC's CWnd::Default().
int CWnd::OnCreate(LPCREATESTRUCT) { return static_cast<int>(Default()); }
void CWnd::OnDestroy() { Default(); }
void CWnd::OnNcDestroy() { Default(); }
void CWnd::OnClose() { Default(); }
void CWnd::OnPaint() { Default(); }
BOOL CWnd::OnEraseBkgnd(CDC*) { return static_cast<BOOL>(Default()); }
void CWnd::OnSize(UINT, int, int) { Default(); }
void CWnd::OnMove(int, int) { Default(); }
void CWnd::OnTimer(UINT_PTR) { Default(); }
void CWnd::OnKeyDown(UINT, UINT, UINT) { Default(); }
void CWnd::OnKeyUp(UINT, UINT, UINT) { Default(); }
void CWnd::OnChar(UINT, UINT, UINT) { Default(); }
void CWnd::OnSysKeyDown(UINT, UINT, UINT) { Default(); }
void CWnd::OnSysKeyUp(UINT, UINT, UINT) { Default(); }
void CWnd::OnSysChar(UINT, UINT, UINT) { Default(); }
void CWnd::OnMouseMove(UINT, CPoint) { Default(); }
void CWnd::OnLButtonDown(UINT, CPoint) { Default(); }
void CWnd::OnLButtonUp(UINT, CPoint) { Default(); }
void CWnd::OnLButtonDblClk(UINT, CPoint) { Default(); }
void CWnd::OnRButtonDown(UINT, CPoint) { Default(); }
void CWnd::OnRButtonUp(UINT, CPoint) { Default(); }
void CWnd::OnRButtonDblClk(UINT, CPoint) { Default(); }
void CWnd::OnMButtonDown(UINT, CPoint) { Default(); }
void CWnd::OnMButtonUp(UINT, CPoint) { Default(); }
void CWnd::OnNcLButtonDown(UINT, CPoint) { Default(); }
BOOL CWnd::OnMouseWheel(UINT, short, CPoint) { return static_cast<BOOL>(Default()); }
void CWnd::OnHScroll(UINT, UINT, CScrollBar*) { Default(); }
void CWnd::OnVScroll(UINT, UINT, CScrollBar*) { Default(); }
HBRUSH CWnd::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
    LRESULT r = 0;
    if (pWnd && pWnd != this && pWnd->m_hWnd) {
        CWnd* child = FromHandlePermanent(pWnd->m_hWnd);
        r = nCtlColor;
        if (child && child->OnChildNotify(WM_CTLCOLOR, reinterpret_cast<WPARAM>(pDC->m_hDC),
                                          reinterpret_cast<LPARAM>(pWnd->m_hWnd), &r))
            return reinterpret_cast<HBRUSH>(r);
    }
    return nullptr;
}
void CWnd::OnDrawItem(int, LPDRAWITEMSTRUCT lpDrawItemStruct) {
    LRESULT r = 0;
    if (lpDrawItemStruct && lpDrawItemStruct->CtlType != ODT_MENU)
        ReflectChildNotify(WM_DRAWITEM, 0, reinterpret_cast<LPARAM>(lpDrawItemStruct), &r);
}
void CWnd::OnMeasureItem(int, LPMEASUREITEMSTRUCT) {}
void CWnd::OnSetFocus(CWnd*) { Default(); }
void CWnd::OnKillFocus(CWnd*) { Default(); }
void CWnd::OnContextMenu(CWnd*, CPoint) { Default(); }
BOOL CWnd::OnSetCursor(CWnd*, UINT, UINT) { return static_cast<BOOL>(Default()); }
void CWnd::OnShowWindow(BOOL, UINT) { Default(); }
void CWnd::OnSysCommand(UINT, LPARAM) { Default(); }
HCURSOR CWnd::OnQueryDragIcon() { return nullptr; }
BOOL CWnd::OnHelpInfo(HELPINFO* pHelpInfo) { return static_cast<BOOL>(ShowContextHelp(this, pHelpInfo)); }
void CWnd::OnGetMinMaxInfo(MINMAXINFO*) {}
void CWnd::OnActivate(UINT, CWnd*, BOOL) { Default(); }
UINT CWnd::OnGetDlgCode() { return static_cast<UINT>(Default()); }
void CWnd::OnDropFiles(HDROP) {}
void CWnd::OnInitMenuPopup(CMenu*, UINT, BOOL) {}
void CWnd::OnMDIActivate(BOOL, CWnd*, CWnd*) {}
void CWnd::OnEnable(BOOL) {}
BOOL CWnd::OnNcActivate(BOOL) { return TRUE; }
void CWnd::OnMenuSelect(UINT, UINT, HMENU) {}
void CWnd::OnSizing(UINT, LPRECT) {}

// ---------------------------------------------------------------------------------------------
// CWnd state and geometry

DWORD CWnd::GetStyle() const {
    return OnMain([&]() -> DWORD { return m_hWnd ? CurrentStyle(GetWx()) : 0; });
}

DWORD CWnd::GetExStyle() const {
    return OnMain([&]() -> DWORD {
        WindowState* st = m_hWnd ? GetState(GetWx()) : nullptr;
        return st ? st->exStyle : 0;
    });
}

BOOL CWnd::ModifyStyle(DWORD dwRemove, DWORD dwAdd, UINT) {
    return OnMain([&]() -> BOOL {
        if (!m_hWnd)
            return FALSE;
        DWORD old = CurrentStyle(GetWx());
        DWORD now = (old & ~dwRemove) | dwAdd;
        if (now == old)
            return FALSE;
        ApplyStyle(GetWx(), old, now);
        return TRUE;
    });
}

BOOL CWnd::ModifyStyleEx(DWORD dwRemove, DWORD dwAdd, UINT) {
    return OnMain([&]() -> BOOL {
        WindowState* st = m_hWnd ? GetState(GetWx()) : nullptr;
        if (!st)
            return FALSE;
        st->exStyle = (st->exStyle & ~dwRemove) | dwAdd;
        return TRUE;
    });
}

int CWnd::GetDlgCtrlID() const {
    return OnMain([&]() -> int {
        WindowState* st = m_hWnd ? GetState(GetWx()) : nullptr;
        return st ? st->winId : 0;
    });
}

int CWnd::SetDlgCtrlID(int nID) {
    return OnMain([&]() -> int {
        WindowState* st = m_hWnd ? GetState(GetWx()) : nullptr;
        if (!st)
            return 0;
        int old = st->winId;
        st->winId = nID;
        GetWx()->SetId(ToWxId(nID));
        return old;
    });
}

CWnd* CWnd::GetParent() const {
    return OnMain([&]() -> CWnd* {
        if (!m_hWnd)
            return nullptr;
        wxWindow* w = GetWx();
        WindowState* st = GetState(w);
        if (st && st->owner)
            return FromHandle(st->owner);
        wxWindow* p = LogicalParent(w);
        return p ? WrapperFor(p) : nullptr;
    });
}

CWnd* CWnd::SetParent(CWnd* pWndNewParent) {
    return OnMain([&]() -> CWnd* {
        CWnd* old = GetParent();
        if (m_hWnd && pWndNewParent && pWndNewParent->m_hWnd)
            GetWx()->Reparent(pWndNewParent->GetWx());
        return old;
    });
}

CWnd* CWnd::GetOwner() const { return GetParent(); }

void CWnd::SetOwner(CWnd* pOwnerWnd) {
    OnMain([&] {
        if (m_hWnd)
            EnsureState(GetWx()).owner = pOwnerWnd ? pOwnerWnd->m_hWnd : nullptr;
    });
}

CWnd* CWnd::GetParentOwner() const { return GetParent(); }

CFrameWnd* CWnd::GetParentFrame() const {
    return OnMain([&]() -> CFrameWnd* {
        if (!m_hWnd)
            return nullptr;
        for (wxWindow* w = LogicalParent(GetWx()); w; w = LogicalParent(w)) {
            CWnd* p = PermanentWnd(w);
            if (p && p->IsKindOf(RUNTIME_CLASS(CFrameWnd)))
                return static_cast<CFrameWnd*>(p);
        }
        return nullptr;
    });
}

CFrameWnd* CWnd::GetTopLevelFrame() const {
    return OnMain([&]() -> CFrameWnd* {
        CFrameWnd* frame = IsKindOf(RUNTIME_CLASS(CFrameWnd)) ? static_cast<CFrameWnd*>(const_cast<CWnd*>(this))
                                                               : GetParentFrame();
        while (frame) {
            CFrameWnd* parent = frame->GetParentFrame();
            if (!parent)
                break;
            frame = parent;
        }
        return frame;
    });
}

CWnd* CWnd::GetTopLevelParent() const {
    return OnMain([&]() -> CWnd* {
        if (!m_hWnd)
            return nullptr;
        wxWindow* w = GetWx();
        while (w && !w->IsTopLevel() && w->GetParent())
            w = w->GetParent();
        return WrapperFor(w);
    });
}

CWnd* CWnd::GetTopLevelOwner() const { return GetTopLevelParent(); }

CWnd* CWnd::GetTopWindow() const { return GetWindow(GW_CHILD); }

CWnd* CWnd::GetWindow(UINT nCmd) const { return FromHandle(::GetWindow(m_hWnd, nCmd)); }

CWnd* CWnd::GetNextWindow(UINT nFlag) const { return FromHandle(::GetWindow(m_hWnd, nFlag)); }

CWnd* CWnd::GetLastActivePopup() const { return const_cast<CWnd*>(this); }

CWnd* CWnd::GetDescendantWindow(int nID, BOOL) const {
    return OnMain([&]() -> CWnd* {
        if (!m_hWnd)
            return nullptr;
        std::function<wxWindow*(wxWindow*)> find = [&](wxWindow* parent) -> wxWindow* {
            for (wxWindow* child : parent->GetChildren()) {
                WindowState* st = GetState(child);
                if (st && st->winId == nID)
                    return child;
                if (wxWindow* r = find(child))
                    return r;
            }
            return nullptr;
        };
        wxWindow* w = find(GetWx());
        return w ? WrapperFor(w) : nullptr;
    });
}

CWnd* CWnd::ChildWindowFromPoint(POINT point) const { return FromHandle(::ChildWindowFromPoint(m_hWnd, point)); }
CWnd* CWnd::WindowFromPoint(POINT point) { return FromHandle(::WindowFromPoint(point)); }
CWnd* CWnd::GetActiveWindow() { return FromHandle(::GetActiveWindow()); }
CWnd* CWnd::SetActiveWindow() { return FromHandle(::SetActiveWindow(m_hWnd)); }
CWnd* CWnd::GetForegroundWindow() { return FromHandle(::GetForegroundWindow()); }
BOOL CWnd::SetForegroundWindow() { return ::SetForegroundWindow(m_hWnd); }
CWnd* CWnd::GetFocus() { return FromHandle(::GetFocus()); }
CWnd* CWnd::SetFocus() { return FromHandle(::SetFocus(m_hWnd)); }
CWnd* CWnd::GetDesktopWindow() { return nullptr; }
CWnd* CWnd::GetCapture() { return FromHandle(::GetCapture()); }
CWnd* CWnd::SetCapture() { return FromHandle(::SetCapture(m_hWnd)); }
CWnd* CWnd::FindWindow(const char* lpszClassName, const char* lpszWindowName) {
    return FromHandle(::FindWindow(lpszClassName, lpszWindowName));
}
BOOL CWnd::IsChild(const CWnd* pWnd) const { return pWnd && ::IsChild(m_hWnd, pWnd->m_hWnd); }
BOOL CWnd::IsWindowEnabled() const { return ::IsWindowEnabled(m_hWnd); }
BOOL CWnd::EnableWindow(BOOL bEnable) { return ::EnableWindow(m_hWnd, bEnable); }
BOOL CWnd::IsWindowVisible() const { return ::IsWindowVisible(m_hWnd); }
BOOL CWnd::ShowWindow(int nCmdShow) { return ::ShowWindow(m_hWnd, nCmdShow); }
BOOL CWnd::IsIconic() const { return ::IsIconic(m_hWnd); }
BOOL CWnd::IsZoomed() const { return ::IsZoomed(m_hWnd); }
void CWnd::DragAcceptFiles(BOOL bAccept) { ::DragAcceptFiles(m_hWnd, bAccept); }

void CWnd::SetWindowText(const char* lpszString) { ::SetWindowText(m_hWnd, lpszString); }

int CWnd::GetWindowText(char* lpszStringBuf, int nMaxCount) const {
    return ::GetWindowText(m_hWnd, lpszStringBuf, nMaxCount);
}

void CWnd::GetWindowText(CString& rString) const {
    int n = GetWindowTextLength();
    char* buf = rString.GetBuffer(n + 1);
    ::GetWindowText(m_hWnd, buf, n + 1);
    rString.ReleaseBuffer();
}

int CWnd::GetWindowTextLength() const { return ::GetWindowTextLength(m_hWnd); }

void CWnd::SetFont(CFont* pFont, BOOL bRedraw) {
    SendMessage(WM_SETFONT, reinterpret_cast<WPARAM>(pFont ? pFont->m_hObject : nullptr), bRedraw);
}

CFont* CWnd::GetFont() const {
    return CFont::FromHandle(reinterpret_cast<HFONT>(const_cast<CWnd*>(this)->SendMessage(WM_GETFONT)));
}

void CWnd::GetWindowRect(LPRECT lpRect) const { ::GetWindowRect(m_hWnd, lpRect); }
void CWnd::GetClientRect(LPRECT lpRect) const { ::GetClientRect(m_hWnd, lpRect); }
void CWnd::ClientToScreen(LPPOINT lpPoint) const { ::ClientToScreen(m_hWnd, lpPoint); }
void CWnd::ClientToScreen(LPRECT lpRect) const {
    ::ClientToScreen(m_hWnd, reinterpret_cast<LPPOINT>(lpRect));
    ::ClientToScreen(m_hWnd, reinterpret_cast<LPPOINT>(lpRect) + 1);
}
void CWnd::ScreenToClient(LPPOINT lpPoint) const { ::ScreenToClient(m_hWnd, lpPoint); }
void CWnd::ScreenToClient(LPRECT lpRect) const {
    ::ScreenToClient(m_hWnd, reinterpret_cast<LPPOINT>(lpRect));
    ::ScreenToClient(m_hWnd, reinterpret_cast<LPPOINT>(lpRect) + 1);
}
void CWnd::MapWindowPoints(CWnd* pwndTo, LPPOINT lpPoint, UINT nCount) const {
    ::MapWindowPoints(m_hWnd, pwndTo ? pwndTo->m_hWnd : nullptr, lpPoint, nCount);
}
void CWnd::MapWindowPoints(CWnd* pwndTo, LPRECT lpRect) const {
    ::MapWindowPoints(m_hWnd, pwndTo ? pwndTo->m_hWnd : nullptr, reinterpret_cast<LPPOINT>(lpRect), 2);
}
void CWnd::MoveWindow(int x, int y, int nWidth, int nHeight, BOOL bRepaint) {
    ::MoveWindow(m_hWnd, x, y, nWidth, nHeight, bRepaint);
}
void CWnd::MoveWindow(LPCRECT lpRect, BOOL bRepaint) {
    ::MoveWindow(m_hWnd, lpRect->left, lpRect->top, lpRect->right - lpRect->left, lpRect->bottom - lpRect->top,
                 bRepaint);
}
BOOL CWnd::SetWindowPos(const CWnd* pWndInsertAfter, int x, int y, int cx, int cy, UINT nFlags) {
    HWND after = nullptr;
    if (pWndInsertAfter == &wndTopMost)
        after = HWND_TOPMOST;
    else if (pWndInsertAfter == &wndNoTopMost)
        after = HWND_NOTOPMOST;
    else if (pWndInsertAfter == &wndBottom)
        after = HWND_BOTTOM;
    else if (pWndInsertAfter && pWndInsertAfter != &wndTop)
        after = pWndInsertAfter->m_hWnd;
    return ::SetWindowPos(m_hWnd, after, x, y, cx, cy, nFlags);
}

BOOL CWnd::GetWindowPlacement(WINDOWPLACEMENT* lpwndpl) const {
    return OnMain([&]() -> BOOL {
        if (!m_hWnd || !lpwndpl)
            return FALSE;
        wxWindow* w = GetWx();
        wxRect r = w->GetRect();
        lpwndpl->flags = 0;
        lpwndpl->showCmd = SW_SHOWNORMAL;
        if (auto* tlw = wxDynamicCast(w, wxTopLevelWindow)) {
            if (tlw->IsMaximized())
                lpwndpl->showCmd = SW_SHOWMAXIMIZED;
            else if (tlw->IsIconized())
                lpwndpl->showCmd = SW_SHOWMINIMIZED;
        }
        lpwndpl->ptMinPosition.x = lpwndpl->ptMinPosition.y = -1;
        lpwndpl->ptMaxPosition.x = lpwndpl->ptMaxPosition.y = -1;
        lpwndpl->rcNormalPosition.left = r.x;
        lpwndpl->rcNormalPosition.top = r.y;
        lpwndpl->rcNormalPosition.right = r.x + r.width;
        lpwndpl->rcNormalPosition.bottom = r.y + r.height;
        return TRUE;
    });
}

BOOL CWnd::SetWindowPlacement(const WINDOWPLACEMENT* lpwndpl) {
    return OnMain([&]() -> BOOL {
        if (!m_hWnd || !lpwndpl)
            return FALSE;
        wxWindow* w = GetWx();
        const RECT& r = lpwndpl->rcNormalPosition;
        wxRect rect(r.left, r.top, r.right - r.left, r.bottom - r.top);
        if (w->IsTopLevel()) {
            int display = wxDisplay::GetFromPoint(rect.GetTopLeft());
            if (display == wxNOT_FOUND)
                rect.SetPosition(wxPoint(50, 50));
        }
        w->SetSize(rect);
        if (auto* tlw = wxDynamicCast(w, wxTopLevelWindow)) {
            if (lpwndpl->showCmd == SW_SHOWMAXIMIZED || lpwndpl->showCmd == SW_MAXIMIZE)
                tlw->Maximize();
        }
        return TRUE;
    });
}

void CWnd::CenterWindow(CWnd* pAlternateOwner) {
    OnMain([&] {
        if (!m_hWnd)
            return;
        wxWindow* w = GetWx();
        if (pAlternateOwner && pAlternateOwner->m_hWnd && w->IsTopLevel()) {
            wxRect owner = pAlternateOwner->GetWx()->GetScreenRect();
            wxSize sz = w->GetSize();
            w->Move(owner.x + (owner.width - sz.x) / 2, owner.y + (owner.height - sz.y) / 2);
        } else {
            w->CentreOnParent();
        }
    });
}

void CWnd::BringWindowToTop() { ::BringWindowToTop(m_hWnd); }

void CWnd::CalcWindowRect(LPRECT lpClientRect, UINT) {
    OnMain([&] {
        if (!m_hWnd || !lpClientRect)
            return;
        wxWindow* w = GetWx();
        wxSize outer = w->GetSize();
        wxSize inner = w->GetClientSize();
        lpClientRect->right += outer.x - inner.x;
        lpClientRect->bottom += outer.y - inner.y;
    });
}

void CWnd::Invalidate(BOOL bErase) { ::InvalidateRect(m_hWnd, nullptr, bErase); }
void CWnd::InvalidateRect(LPCRECT lpRect, BOOL bErase) { ::InvalidateRect(m_hWnd, lpRect, bErase); }
void CWnd::InvalidateRgn(CRgn*, BOOL bErase) { ::InvalidateRect(m_hWnd, nullptr, bErase); }
void CWnd::UpdateWindow() { ::UpdateWindow(m_hWnd); }
BOOL CWnd::RedrawWindow(LPCRECT lpRectUpdate, CRgn*, UINT flags) {
    return ::RedrawWindow(m_hWnd, lpRectUpdate, nullptr, flags);
}
void CWnd::SetRedraw(BOOL bRedraw) { SendMessage(WM_SETREDRAW, bRedraw, 0); }
BOOL CWnd::GetUpdateRect(LPRECT lpRect, BOOL) {
    return OnMain([&]() -> BOOL {
        if (!m_hWnd)
            return FALSE;
        wxRect r = GetWx()->GetUpdateRegion().GetBox();
        if (lpRect) {
            lpRect->left = r.x;
            lpRect->top = r.y;
            lpRect->right = r.x + r.width;
            lpRect->bottom = r.y + r.height;
        }
        return !r.IsEmpty();
    });
}
void CWnd::ScrollWindow(int, int, LPCRECT, LPCRECT) { Invalidate(); }
CPoint CWnd::GetCaretPos() { return CPoint(0, 0); }

// scroll bars of generic windows
int CWnd::GetScrollPos(int nBar) const {
    return OnMain([&]() -> int {
        if (!m_hWnd)
            return 0;
        if (nBar == SB_CTL)
            return static_cast<int>(const_cast<CWnd*>(this)->SendMessage(SBM_GETPOS));
        return GetWx()->GetScrollPos(nBar == SB_VERT ? wxVERTICAL : wxHORIZONTAL);
    });
}

int CWnd::SetScrollPos(int nBar, int nPos, BOOL bRedraw) {
    return OnMain([&]() -> int {
        if (!m_hWnd)
            return 0;
        if (nBar == SB_CTL)
            return static_cast<int>(SendMessage(SBM_SETPOS, static_cast<WPARAM>(nPos), bRedraw));
        int orient = nBar == SB_VERT ? wxVERTICAL : wxHORIZONTAL;
        int old = GetWx()->GetScrollPos(orient);
        GetWx()->SetScrollPos(orient, nPos, bRedraw != FALSE);
        return old;
    });
}

void CWnd::GetScrollRange(int nBar, LPINT lpMinPos, LPINT lpMaxPos) const {
    OnMain([&] {
        if (!m_hWnd)
            return;
        int orient = nBar == SB_VERT ? wxVERTICAL : wxHORIZONTAL;
        if (lpMinPos)
            *lpMinPos = 0;
        if (lpMaxPos)
            *lpMaxPos = std::max(0, GetWx()->GetScrollRange(orient) - 1);
    });
}

void CWnd::SetScrollRange(int nBar, int nMinPos, int nMaxPos, BOOL bRedraw) {
    OnMain([&] {
        if (!m_hWnd || nBar == SB_CTL)
            return;
        int orient = nBar == SB_VERT ? wxVERTICAL : wxHORIZONTAL;
        int range = std::max(0, nMaxPos - nMinPos + 1);
        GetWx()->SetScrollbar(orient, std::min(GetWx()->GetScrollPos(orient), range), 1,
                              nMaxPos > nMinPos ? range : 0, bRedraw != FALSE);
    });
}

BOOL CWnd::SetScrollInfo(int nBar, LPSCROLLINFO lpScrollInfo, BOOL bRedraw) {
    return OnMain([&]() -> BOOL {
        if (!m_hWnd || !lpScrollInfo)
            return FALSE;
        int orient = nBar == SB_VERT ? wxVERTICAL : wxHORIZONTAL;
        wxWindow* w = GetWx();
        int pos = w->GetScrollPos(orient);
        int thumb = w->GetScrollThumb(orient);
        int range = w->GetScrollRange(orient);
        if (lpScrollInfo->fMask & SIF_RANGE)
            range = lpScrollInfo->nMax - lpScrollInfo->nMin + 1;
        if (lpScrollInfo->fMask & SIF_PAGE)
            thumb = static_cast<int>(lpScrollInfo->nPage);
        if (lpScrollInfo->fMask & SIF_POS)
            pos = lpScrollInfo->nPos - ((lpScrollInfo->fMask & SIF_RANGE) ? lpScrollInfo->nMin : 0);
        if (thumb >= range && !(lpScrollInfo->fMask & SIF_DISABLENOSCROLL))
            range = 0;
        w->SetScrollbar(orient, std::max(0, pos), std::max(1, thumb), std::max(0, range), bRedraw != FALSE);
        return TRUE;
    });
}

BOOL CWnd::GetScrollInfo(int nBar, LPSCROLLINFO lpScrollInfo, UINT nMask) {
    return OnMain([&]() -> BOOL {
        if (!m_hWnd || !lpScrollInfo)
            return FALSE;
        int orient = nBar == SB_VERT ? wxVERTICAL : wxHORIZONTAL;
        wxWindow* w = GetWx();
        lpScrollInfo->fMask = nMask;
        lpScrollInfo->nMin = 0;
        lpScrollInfo->nMax = std::max(0, w->GetScrollRange(orient) - 1);
        lpScrollInfo->nPage = static_cast<UINT>(w->GetScrollThumb(orient));
        lpScrollInfo->nPos = w->GetScrollPos(orient);
        lpScrollInfo->nTrackPos = lpScrollInfo->nPos;
        return TRUE;
    });
}

int CWnd::GetScrollLimit(int nBar) {
    SCROLLINFO si;
    GetScrollInfo(nBar, &si, SIF_ALL);
    return std::max(0, si.nMax - static_cast<int>(si.nPage) + 1);
}

void CWnd::ShowScrollBar(UINT nBar, BOOL bShow) {
    OnMain([&] {
        if (!m_hWnd || bShow)
            return;
        if (nBar == SB_HORZ || nBar == SB_BOTH)
            GetWx()->SetScrollbar(wxHORIZONTAL, 0, 0, 0);
        if (nBar == SB_VERT || nBar == SB_BOTH)
            GetWx()->SetScrollbar(wxVERTICAL, 0, 0, 0);
    });
}

BOOL CWnd::EnableScrollBar(int, UINT) { return TRUE; }
CScrollBar* CWnd::GetScrollBarCtrl(int) const { return nullptr; }

UINT_PTR CWnd::SetTimer(UINT_PTR nIDEvent, UINT nElapse, void (*lpfnTimer)(HWND, UINT, UINT_PTR, DWORD)) {
    return ::SetTimer(m_hWnd, nIDEvent, nElapse, lpfnTimer);
}

BOOL CWnd::KillTimer(UINT_PTR nIDEvent) { return ::KillTimer(m_hWnd, nIDEvent); }

CWnd* CWnd::GetDlgItem(int nID) const { return FromHandle(::GetDlgItem(m_hWnd, nID)); }
void CWnd::GetDlgItem(int nID, HWND* phWnd) const {
    if (phWnd)
        *phWnd = ::GetDlgItem(m_hWnd, nID);
}
UINT CWnd::GetDlgItemInt(int nID, BOOL* lpTrans, BOOL bSigned) const {
    return ::GetDlgItemInt(m_hWnd, nID, lpTrans, bSigned);
}
void CWnd::SetDlgItemInt(int nID, UINT nValue, BOOL bSigned) { ::SetDlgItemInt(m_hWnd, nID, nValue, bSigned); }
int CWnd::GetDlgItemText(int nID, char* lpStr, int nMaxCount) const {
    return static_cast<int>(::GetDlgItemText(m_hWnd, nID, lpStr, nMaxCount));
}
int CWnd::GetDlgItemText(int nID, CString& rString) const {
    CWnd* item = GetDlgItem(nID);
    if (!item) {
        rString.Empty();
        return 0;
    }
    item->GetWindowText(rString);
    return rString.GetLength();
}
void CWnd::SetDlgItemText(int nID, const char* lpszString) { ::SetDlgItemText(m_hWnd, nID, lpszString); }
UINT CWnd::IsDlgButtonChecked(int nIDButton) const { return ::IsDlgButtonChecked(m_hWnd, nIDButton); }
void CWnd::CheckDlgButton(int nIDButton, UINT nCheck) { ::CheckDlgButton(m_hWnd, nIDButton, nCheck); }
void CWnd::CheckRadioButton(int nIDFirstButton, int nIDLastButton, int nIDCheckButton) {
    ::CheckRadioButton(m_hWnd, nIDFirstButton, nIDLastButton, nIDCheckButton);
}
int CWnd::GetCheckedRadioButton(int nIDFirstButton, int nIDLastButton) {
    for (int id = nIDFirstButton; id <= nIDLastButton; ++id)
        if (IsDlgButtonChecked(id))
            return id;
    return 0;
}
LRESULT CWnd::SendDlgItemMessage(int nID, UINT message, WPARAM wParam, LPARAM lParam) {
    return ::SendDlgItemMessage(m_hWnd, nID, message, wParam, lParam);
}
CWnd* CWnd::GetNextDlgGroupItem(CWnd* pWndCtl, BOOL bPrevious) const {
    return FromHandle(::GetNextDlgGroupItem(m_hWnd, pWndCtl ? pWndCtl->m_hWnd : nullptr, bPrevious));
}
CWnd* CWnd::GetNextDlgTabItem(CWnd* pWndCtl, BOOL bPrevious) const {
    return FromHandle(::GetNextDlgTabItem(m_hWnd, pWndCtl ? pWndCtl->m_hWnd : nullptr, bPrevious));
}

BOOL CWnd::UpdateData(BOOL bSaveAndValidate) {
    return OnMain([&]() -> BOOL {
        CDataExchange dx(this, bSaveAndValidate);
        try {
            DoDataExchange(&dx);
        } catch (CUserException* e) {
            e->Delete();
            return FALSE;
        }
        return TRUE;
    });
}

void CWnd::DoDataExchange(CDataExchange*) {}

BOOL CWnd::ExecuteDlgInit(UINT nIDTemplate) { return ApplyDlgInit(this, static_cast<int>(nIDTemplate)); }
BOOL CWnd::ExecuteDlgInit(const char* lpszResourceName) {
    ResRef ref = ResRef::From(lpszResourceName);
    return ApplyDlgInit(this, ref.id);
}

CMenu* CWnd::GetMenu() const { return CMenu::FromHandle(::GetMenu(m_hWnd)); }
BOOL CWnd::SetMenu(CMenu* pMenu) { return ::SetMenu(m_hWnd, pMenu ? pMenu->m_hMenu : nullptr); }
void CWnd::DrawMenuBar() { ::DrawMenuBar(m_hWnd); }
CMenu* CWnd::GetSystemMenu(BOOL bRevert) const { return CMenu::FromHandle(::GetSystemMenu(m_hWnd, bRevert)); }

void CWnd::WinHelp(DWORD_PTR dwData, UINT nCmd) {
    CWinApp* app = AfxGetApp();
    if (app)
        app->WinHelp(dwData, nCmd);
}

void CWnd::HtmlHelp(DWORD_PTR dwData, UINT nCmd) {
    CWinApp* app = AfxGetApp();
    if (app)
        app->HtmlHelp(dwData, nCmd);
}

HICON CWnd::SetIcon(HICON hIcon, BOOL bBigIcon) {
    return reinterpret_cast<HICON>(SendMessage(WM_SETICON, bBigIcon, reinterpret_cast<LPARAM>(hIcon)));
}

HICON CWnd::GetIcon(BOOL bBigIcon) const {
    return reinterpret_cast<HICON>(const_cast<CWnd*>(this)->SendMessage(WM_GETICON, bBigIcon, 0));
}

BOOL CWnd::EnableToolTips(BOOL) { return TRUE; }

int CWnd::MessageBox(const char* lpszText, const char* lpszCaption, UINT nType) {
    return ::MessageBox(m_hWnd, lpszText, lpszCaption, nType);
}

BOOL CWnd::OpenClipboard() { return ::OpenClipboard(m_hWnd); }

BOOL CWnd::LockWindowUpdate() {
    return OnMain([&]() -> BOOL {
        if (m_hWnd)
            GetWx()->Freeze();
        return TRUE;
    });
}

void CWnd::UnlockWindowUpdate() {
    OnMain([&] {
        if (m_hWnd && GetWx()->IsFrozen())
            GetWx()->Thaw();
    });
}

LONG_PTR CWnd::GetWindowLongPtr(int nIndex) const { return ::GetWindowLongPtr(m_hWnd, nIndex); }
LONG_PTR CWnd::SetWindowLongPtr(int nIndex, LONG_PTR dwNewLong) { return ::SetWindowLongPtr(m_hWnd, nIndex, dwNewLong); }
