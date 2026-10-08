#pragma once

// Internal helpers shared by wnd.cpp, controls.cpp, dialog.cpp, winapi.cpp, frames and doc/view.

#include "internal.h"

#include <type_traits>

namespace mfcwx {

template <class R, class F>
R OnMainImpl(F&& f, std::false_type) {
    R result{};
    RunOnMainThread([&] { result = f(); });
    return result;
}

template <class R, class F>
void OnMainImpl(F&& f, std::true_type) {
    RunOnMainThread([&] { f(); });
}

// Runs f on the main thread (synchronously) and returns its result.
template <class F>
auto OnMain(F&& f) -> decltype(f()) {
    using R = decltype(f());
    if (IsMainThread())
        return f();
    return OnMainImpl<R>(std::forward<F>(f), std::is_void<R>());
}


class MessageScope {
public:
    MessageScope(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, wxEvent* event);
    ~MessageScope();
    bool DefaultCalled() const;
    MessageScope(const MessageScope&) = delete;
    MessageScope& operator=(const MessageScope&) = delete;
};

bool IsManagedWindow(wxWindow* window);
void OnWindowGone(wxWindow* window);
CWnd* PermanentWnd(wxWindow* window);
CWnd* WrapperFor(wxWindow* window);
bool HasMessageHandler(CWnd* pWnd, UINT message);
LRESULT SendFromEvent(wxWindow* window, UINT msg, WPARAM wParam, LPARAM lParam, wxEvent& event, bool* defaultCalled);

UINT VirtualKeyFromWx(int code);
int WxKeyFromVirtual(UINT vk);
LPARAM KeyLParam(const wxKeyEvent& e, bool up);
WPARAM MouseFlags(const wxMouseEvent& e);
bool TranslateCharToAnsi(const wxKeyEvent& e, UINT& ch);

UINT_PTR StartTimer(wxWindow* window, UINT_PTR id, UINT elapse, TIMERPROC proc);
bool StopTimer(wxWindow* window, UINT_PTR id);

// controls.cpp
void BindControlEvents(wxWindow* window, WindowState& state);
// Applies ES_NUMBER / ES_UPPERCASE / ES_LOWERCASE and read-only rules; true if the key was consumed.
bool FilterEditChar(wxWindow* window, WindowState& state, UINT& ch, wxKeyEvent& event);
void NotifyFocusChange(wxWindow* window, WindowState& state, bool gained);
// Replaces a native control by a custom-drawn window if pWnd paints it itself; returns the window to attach.
wxWindow* PrepareForSubclass(wxWindow* window, CWnd* pWnd);
wxWindow* CreateWindowForClass(CWnd* pWnd, CREATESTRUCT& cs);
DWORD CurrentStyle(wxWindow* window);
void ApplyStyle(wxWindow* window, DWORD oldStyle, DWORD newStyle);
LRESULT ControlWindowProc(wxWindow* window, WindowState& state, UINT msg, WPARAM wParam, LPARAM lParam, bool& handled);
// Sends WM_CTLCOLOR* for a control and applies the resulting colours.
void ApplyCtlColor(wxWindow* control);
wxString GetControlText(wxWindow* window);
void SetControlText(wxWindow* window, const wxString& text);

// scintilla.cpp: wxStyledTextCtrl behind the Scintilla message API (ANSI text, ANSI positions)
void BindScintillaEvents(wxWindow* window);
LRESULT ScintillaWindowProc(wxWindow* window, UINT msg, WPARAM wParam, LPARAM lParam, bool& handled);

// winapi.cpp: exact ANSI bytes copied by a Scintilla window; GetClipboardData(CF_TEXT) returns them (zero-padded to minSize).
void RememberClipboardText(std::string ansiBytes, size_t minSize);

// gdi.cpp: an HBITMAP owning a copy of bmp
HBITMAP CreateBitmapHandle(const wxBitmap& bmp);

// wnd.cpp / winapi.cpp
void DestroyWxWindow(wxWindow* window);
// The parent in the Win32 sense (skips internal wx containers).
wxWindow* LogicalParent(wxWindow* window);
void ResetCursorRequest();
const wxCursor* RequestedCursor();
// Sends a WM_COMMAND notification for a control to its parent.
void NotifyParent(wxWindow* control, UINT code);
// Sends WM_NOTIFY with the given header (hwndFrom/idFrom are filled in).
LRESULT NotifyParentNM(wxWindow* control, NMHDR* hdr);

// menu.cpp: command ids of menu items (ID_APP_ABOUT/ID_APP_EXIT map to the wx stock ids)
int MenuWxId(int winId);
int MenuWinId(int wxId);
// Accelerator table whose keys are shown in menus built afterwards.
void SetMenuAccelerators(const rc::AccelTable* table);

// dialog.cpp
wxWindow* CreateDialogWindow(CDialog* dlg, const rc::Dialog& tmpl, wxWindow* parent);
BOOL ApplyDlgInit(CWnd* pWnd, int dialogId);
// Handles Enter/Escape/F1 like the Windows dialog manager; true if consumed.
bool DialogKeyHook(wxWindow* dialog, wxKeyEvent& event);

// app.cpp
BOOL ShowContextHelp(CWnd* pWnd, HELPINFO* pHelpInfo);
// Runs the application's PreTranslateMessage chain for a key event; true if consumed.
bool PreTranslateKey(wxWindow* topLevel, wxKeyEvent& event, UINT message);

} // namespace mfcwx
