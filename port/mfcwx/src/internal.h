#pragma once

// Internal contracts shared by the mfcwx implementation files. Not visible to application code.

#include "bridge.h"
#include "mfcwx/rcdata.h"

#include <wx/dc.h>
#include <wx/timer.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace mfcwx {

// ---------------------------------------------------------------------------------------------
// Resources (resources.cpp)

// Registers the generated resource module of the application (g_rcModule_<Name>).
void RegisterResourceModule(const rc::Module* module);
// Selects the UI language ("de", "en", ...); also sets the ANSI code page.
void SetResourceLanguage(const char* code);
const char* GetResourceLanguage();
std::vector<std::string> AvailableResourceLanguages();

// A resource reference as passed to Win32 APIs: MAKEINTRESOURCE(id) or a name.
struct ResRef {
    int id = 0;
    std::string name;
    static ResRef From(const char* lpszName);
    static ResRef FromId(UINT id) { ResRef r; r.id = static_cast<int>(id); return r; }
    bool Matches(int otherId, const char* otherName) const;
};

const rc::Dialog* FindDialog(const ResRef& ref);
const rc::Menu* FindMenu(const ResRef& ref);
const rc::AccelTable* FindAccelTable(const ResRef& ref);
const rc::Toolbar* FindToolbar(const ResRef& ref);
// Path of a file resource (BITMAP, ICON, CURSOR, GIF, ...) on disk, empty if unknown.
std::string FindFileResource(const char* type, const ResRef& ref);
std::vector<const rc::DlgInitEntry*> FindDlgInit(int dialogId);
// Returns the resource string in the ANSI code page.
bool LoadResourceString(UINT id, std::string& out);
const rc::VersionInfo* GetVersionInfo();
// Directory holding the application's data files (res/, help, pse, examples, ...).
std::string GetDataDirectory();
void SetDataDirectory(const std::string& dir);

wxBitmap LoadBitmapResource(const ResRef& ref);
wxIcon LoadIconResource(const ResRef& ref);
wxCursor LoadCursorResource(const ResRef& ref);
// Converts an .rc menu text ("&File\tCtrl+O") to wx syntax ("&File\tCtrl+O" with wx accelerator names).
wxString MenuTextToWx(const char* utf8Text);

// ---------------------------------------------------------------------------------------------
// Windows (wnd.cpp)

// Win32 control ids map to wx ids with an offset so they never collide with wx's stock ids.
constexpr int kIdOffset = 20000;
inline int ToWxId(int winId) { return winId == -1 || winId == 0xFFFF ? wxID_ANY : winId + kIdOffset; }
inline int FromWxId(int wxId) { return wxId >= kIdOffset ? wxId - kIdOffset : (wxId == wxID_ANY ? -1 : wxId); }

enum class ControlKind {
    Generic,      // custom-drawn wxWindow (CWnd::Create, owner-drawn controls, views)
    Static,
    StaticBitmap,
    StaticFrame,  // SS_*RECT / SS_*FRAME / SS_ETCHED*
    GroupBox,
    PushButton,
    CheckBox,
    RadioButton,
    OwnerDrawButton,
    Edit,
    RichEdit,
    ComboBox,     // editable (CBS_DROPDOWN / CBS_SIMPLE)
    ComboList,    // CBS_DROPDOWNLIST
    ListBox,
    ScrollBar,
    Progress,
    Slider,
    Spin,
    ListView,
    TreeView,
    Tab,
    Scintilla,
    Dialog,
    Frame,
    MDIChild,
    ToolBar,
    StatusBar,
    GLCanvas,
};

struct WindowState {
    wxWindow* window = nullptr;
    CWnd* permanent = nullptr;     // attached CWnd (subclassed), may be null
    ControlKind kind = ControlKind::Generic;
    int winId = 0;                 // Win32 control id
    DWORD style = 0;               // Win32 style bits
    DWORD exStyle = 0;
    LONG_PTR userData = 0;         // GWL_USERDATA
    std::map<UINT_PTR, std::unique_ptr<wxTimer>> timers;
    std::map<UINT_PTR, TIMERPROC> timerProcs;
    HFONT font = nullptr;
    bool redraw = true;
    bool destroying = false;
    bool destroyNotified = false;
    bool hooked = false;
    bool dropBound = false;
    bool settingText = false;      // suppresses change notifications for programmatic updates
    int defaultButtonId = 0;       // dialogs: DM_SETDEFID
    int checkState = 0;            // BM_SETCHECK state of buttons
    void* image = nullptr;         // STM_SETIMAGE / STM_SETICON handle
    wxString text;                 // caption of custom-drawn windows
    std::function<void(wxPaintEvent&)> customPaint;
    std::vector<std::pair<DWORD_PTR, wxString>> itemData; // unused by most kinds
    CWnd* buddy = nullptr;         // spin buttons
    HWND owner = nullptr;
    void* extra = nullptr;         // kind-specific data owned by the control implementation
    std::function<void()> deleteExtra;
    ~WindowState() {
        if (deleteExtra)
            deleteExtra();
    }
};

WindowState* GetState(wxWindow* window);       // null if the window is not managed
WindowState& EnsureState(wxWindow* window);
inline wxWindow* ToWx(HWND hWnd) { return reinterpret_cast<wxWindow*>(hWnd); }
inline HWND ToHwnd(wxWindow* w) { return reinterpret_cast<HWND>(w); }

// Binds the event bridge to a window created by mfcwx (idempotent).
void HookWindow(wxWindow* window, ControlKind kind, int winId, DWORD style, DWORD exStyle);

// Message dispatch. All functions must be called on the main thread except SendMessageAnyThread.
LRESULT DispatchMessageTo(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT DefaultWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
bool IsMainThread();
// Runs fn on the main thread and waits for it (Windows SendMessage semantics across threads).
void RunOnMainThread(const std::function<void()>& fn);
void PostToMainThread(std::function<void()> fn);

// Converts a window rectangle in dialog units to pixels for the given dialog window.
wxRect DialogUnitsToPixels(wxWindow* dialog, int x, int y, int cx, int cy);

// Creates the wx controls of a dialog template inside parent; returns false on failure.
bool CreateDialogControls(wxWindow* parent, const rc::Dialog& tmpl);
// Creates a single control from a template entry (also used by CWnd::Create for standard classes).
wxWindow* CreateControl(wxWindow* parent, const char* cls, const char* text, int winId, const wxRect& rect,
                        DWORD style, DWORD exStyle);

// Current message being processed (for CWnd::Default / GetCurrentMessage).
struct CurrentMessage {
    MSG msg;
    wxEvent* event;      // originating wx event, null for synthetic messages
    bool defaultCalled;  // CWnd::Default() requested the toolkit's default processing
};
CurrentMessage* GetCurrentMessageSlot();

// ---------------------------------------------------------------------------------------------
// GDI (gdi.cpp)

struct GdiObjectImpl;
// HGDIOBJ values are CGdiObject* for application-created objects and stock objects.
wxPen PenFromHandle(HPEN h);
wxBrush BrushFromHandle(HBRUSH h);
wxFont FontFromHandle(HFONT h);
wxBitmap* BitmapFromHandle(HBITMAP h);
HFONT DefaultGuiFont();
wxColour ToWxColour(COLORREF c);
COLORREF FromWxColour(const wxColour& c);
COLORREF SysColor(int index);

// Wraps a wx DC that is owned elsewhere (e.g. a wxPaintDC in an event handler) as CDC.
CDC* WrapDC(wxDC* dc, wxWindow* window);
void UnwrapDC(CDC* cdc);

// ---------------------------------------------------------------------------------------------
// Menus (menu.cpp)

// HMENU values are CMenu-independent menu handles (MenuHandle*).
struct MenuHandle;
wxMenuBar* BuildMenuBar(const rc::Menu& menu);
wxMenu* BuildPopupMenu(const rc::Menu& menu, int index);
HMENU HandleFromWxMenu(wxMenu* menu);
HMENU HandleFromWxMenuBar(wxMenuBar* bar);
wxMenu* WxMenuFromHandle(HMENU h);
wxMenuBar* WxMenuBarFromHandle(HMENU h);

// ---------------------------------------------------------------------------------------------
// Application (app.cpp)

CWinApp*& AppInstance();
wxWindow* MainWxWindow();
// Shows a message box; nType is the MB_* combination. Returns IDOK, IDCANCEL, ...
int ShowMessageBox(wxWindow* parent, const char* text, const char* caption, UINT nType);

} // namespace mfcwx
