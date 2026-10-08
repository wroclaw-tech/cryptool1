#include "windows_impl.h"

#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace mfcwx {

struct MenuHandle {
    wxMenuBar* bar = nullptr;
    wxMenu* menu = nullptr;
};

namespace {

std::unordered_map<const void*, MenuHandle*>& Handles() {
    static auto* m = new std::unordered_map<const void*, MenuHandle*>();
    return *m;
}

std::unordered_map<MenuHandle*, CMenu*>& TempMenus() {
    static auto* m = new std::unordered_map<MenuHandle*, CMenu*>();
    return *m;
}

MenuHandle* FromH(HMENU h) { return reinterpret_cast<MenuHandle*>(h); }

wxString AccelText(const rc::Accel& a) {
    wxString s;
    if (a.flags & FCONTROL)
        s += "Ctrl+";
    if (a.flags & FALT)
        s += "Alt+";
    if (a.flags & FSHIFT)
        s += "Shift+";
    if (a.flags & FVIRTKEY) {
        int key = WxKeyFromVirtual(static_cast<UINT>(a.key));
        if (key >= WXK_F1 && key <= WXK_F24)
            s += wxString::Format("F%d", key - WXK_F1 + 1);
        else if (key == WXK_DELETE)
            s += "Del";
        else if (key == WXK_INSERT)
            s += "Ins";
        else if (key == WXK_BACK)
            s += "Back";
        else if (key == WXK_RETURN)
            s += "Enter";
        else if (key == WXK_ESCAPE)
            s += "Esc";
        else if (key == WXK_TAB)
            s += "Tab";
        else if (key == WXK_SPACE)
            s += "Space";
        else if (key > 32 && key < 127)
            s += static_cast<wxChar>(key);
        else
            return wxString();
    } else if (a.key > 0 && a.key < 32) {
        s = "Ctrl+" + wxString(static_cast<wxChar>('A' + a.key - 1));
    } else {
        s += static_cast<wxChar>(a.key);
    }
    return s;
}

const rc::AccelTable*& CurrentAccelTable() {
    static const rc::AccelTable* t = nullptr;
    return t;
}

wxString LabelFor(const rc::MenuItem& item) {
    wxString label = MenuTextToWx(item.text);
    if (const rc::AccelTable* t = CurrentAccelTable()) {
        for (int i = 0; i < t->itemCount; ++i)
            if (t->items[i].id == item.id) {
                wxString accel = AccelText(t->items[i]);
                if (!accel.empty())
                    return label + "\t" + accel;
            }
    }
    return label;
}

void AppendItems(wxMenu* menu, const rc::Menu& res, int& index, int depth) {
    while (index < res.itemCount && res.items[index].depth >= depth) {
        const rc::MenuItem& item = res.items[index];
        if (item.depth > depth) {
            ++index;
            continue;
        }
        ++index;
        if (item.flags & MF_POPUP) {
            auto* sub = new wxMenu;
            AppendItems(sub, res, index, depth + 1);
            menu->AppendSubMenu(sub, MenuTextToWx(item.text));
        } else if ((item.flags & MF_SEPARATOR) || item.id == 0) {
            menu->AppendSeparator();
        } else {
            wxMenuItem* mi = menu->Append(MenuWxId(item.id), LabelFor(item));
            if (item.flags & MF_CHECKED) {
                mi->SetCheckable(true);
                mi->Check(true);
            }
            if (item.flags & (MF_GRAYED | MF_DISABLED))
                mi->Enable(false);
        }
    }
}

void AppendItemsTopLevel(wxMenu* menu, const rc::Menu& res, int& index) { AppendItems(menu, res, index, 0); }

wxMenu* CloneMenu(wxMenu* src) {
    auto* copy = new wxMenu;
    for (wxMenuItem* item : src->GetMenuItems()) {
        if (item->IsSeparator()) {
            copy->AppendSeparator();
        } else if (item->IsSubMenu()) {
            copy->AppendSubMenu(CloneMenu(item->GetSubMenu()), item->GetItemLabel());
        } else {
            wxMenuItem* mi = copy->Append(item->GetId(), item->GetItemLabel(), wxString(),
                                          item->IsCheckable() ? wxITEM_CHECK : wxITEM_NORMAL);
            if (item->IsCheckable())
                mi->Check(item->IsChecked());
            mi->Enable(item->IsEnabled());
        }
    }
    return copy;
}

} // namespace

namespace {

constexpr int kCompactBase = wxID_HIGHEST + 1;

struct CompactIds {
    std::mutex mutex;
    std::unordered_map<int, int> toWx;
    std::vector<int> toWin;
};

CompactIds& Compact() {
    static CompactIds ids;
    return ids;
}

} // namespace

int ToWxId(int winId) {
    if (winId == -1 || winId == 0xFFFF)
        return wxID_ANY;
    if (winId + kIdOffset < 0x7fff)
        return winId + kIdOffset;
    CompactIds& c = Compact();
    std::lock_guard<std::mutex> lock(c.mutex);
    auto it = c.toWx.find(winId);
    if (it != c.toWx.end())
        return it->second;
    int compact = kCompactBase + static_cast<int>(c.toWin.size());
    c.toWx.emplace(winId, compact);
    c.toWin.push_back(winId);
    return compact;
}

int FromWxId(int wxId) {
    if (wxId == wxID_ANY)
        return -1;
    if (wxId >= kIdOffset)
        return wxId - kIdOffset;
    CompactIds& c = Compact();
    std::lock_guard<std::mutex> lock(c.mutex);
    if (wxId >= kCompactBase && wxId < kCompactBase + static_cast<int>(c.toWin.size()))
        return c.toWin[static_cast<size_t>(wxId - kCompactBase)];
    return wxId;
}

bool IsCommandWxId(int wxId) {
    if (wxId >= kIdOffset - 1)
        return true;
    CompactIds& c = Compact();
    std::lock_guard<std::mutex> lock(c.mutex);
    return wxId >= kCompactBase && wxId < kCompactBase + static_cast<int>(c.toWin.size());
}

int MenuWxId(int winId) {
    if (winId == ID_APP_ABOUT)
        return wxID_ABOUT;
    if (winId == ID_APP_EXIT)
        return wxID_EXIT;
    return ToWxId(winId);
}

int MenuWinId(int wxId) {
    if (wxId == wxID_ABOUT)
        return ID_APP_ABOUT;
    if (wxId == wxID_EXIT)
        return ID_APP_EXIT;
    return FromWxId(wxId);
}

void SetMenuAccelerators(const rc::AccelTable* table) { CurrentAccelTable() = table; }

wxMenuBar* BuildMenuBar(const rc::Menu& res) {
    auto* bar = new wxMenuBar;
    int index = 0;
    while (index < res.itemCount) {
        const rc::MenuItem& item = res.items[index++];
        if (item.depth != 0)
            continue;
        auto* menu = new wxMenu;
        AppendItems(menu, res, index, 1);
        bar->Append(menu, MenuTextToWx(item.text));
    }
    return bar;
}

wxMenu* BuildPopupMenu(const rc::Menu& res, int popupIndex) {
    int index = 0;
    int seen = 0;
    while (index < res.itemCount) {
        const rc::MenuItem& item = res.items[index++];
        if (item.depth != 0)
            continue;
        if (seen++ == popupIndex) {
            auto* menu = new wxMenu;
            AppendItems(menu, res, index, 1);
            return menu;
        }
    }
    return nullptr;
}

HMENU HandleFromWxMenu(wxMenu* menu) {
    if (!menu)
        return nullptr;
    auto& h = Handles()[menu];
    if (!h) {
        h = new MenuHandle;
        h->menu = menu;
    }
    return reinterpret_cast<HMENU>(h);
}

HMENU HandleFromWxMenuBar(wxMenuBar* bar) {
    if (!bar)
        return nullptr;
    auto& h = Handles()[bar];
    if (!h) {
        h = new MenuHandle;
        h->bar = bar;
    }
    return reinterpret_cast<HMENU>(h);
}

wxMenu* WxMenuFromHandle(HMENU h) { return h ? FromH(h)->menu : nullptr; }
wxMenuBar* WxMenuBarFromHandle(HMENU h) { return h ? FromH(h)->bar : nullptr; }

namespace {

// Finds a menu item by command id or position in a menu or menu bar.
struct ItemRef {
    wxMenu* parent = nullptr;
    wxMenuItem* item = nullptr;
    wxMenuBar* bar = nullptr;
    int barIndex = -1;
};

ItemRef FindItem(HMENU h, UINT pos, UINT flags) {
    ItemRef r;
    MenuHandle* mh = FromH(h);
    if (!mh)
        return r;
    if (flags & MF_BYPOSITION) {
        if (mh->bar) {
            if (pos < mh->bar->GetMenuCount()) {
                r.bar = mh->bar;
                r.barIndex = static_cast<int>(pos);
            }
        } else if (mh->menu && pos < mh->menu->GetMenuItemCount()) {
            r.parent = mh->menu;
            r.item = mh->menu->FindItemByPosition(pos);
        }
        return r;
    }
    int id = MenuWxId(static_cast<int>(pos));
    wxMenu* owner = nullptr;
    wxMenuItem* item = mh->bar ? mh->bar->FindItem(id, &owner) : (mh->menu ? mh->menu->FindItem(id, &owner) : nullptr);
    r.item = item;
    r.parent = owner;
    return r;
}

} // namespace

} // namespace mfcwx

using namespace mfcwx;

// ---------------------------------------------------------------------------------------------
// CMenu

IMPLEMENT_DYNCREATE(CMenu, CObject)

CMenu::CMenu() : m_hMenu(nullptr) {}

CMenu::~CMenu() {}

CMenu* CMenu::FromHandle(HMENU hMenu) {
    if (!hMenu)
        return nullptr;
    auto& m = TempMenus()[FromH(hMenu)];
    if (!m) {
        m = new CMenu;
        m->m_hMenu = hMenu;
    }
    return m;
}

BOOL CMenu::Attach(HMENU hMenu) {
    m_hMenu = hMenu;
    return hMenu != nullptr;
}

HMENU CMenu::Detach() {
    HMENU h = m_hMenu;
    m_hMenu = nullptr;
    return h;
}

BOOL CMenu::CreateMenu() { return Attach(::CreateMenu()); }
BOOL CMenu::CreatePopupMenu() { return Attach(::CreatePopupMenu()); }
BOOL CMenu::LoadMenu(const char* lpszResourceName) { return Attach(::LoadMenu(nullptr, lpszResourceName)); }
BOOL CMenu::LoadMenu(UINT nIDResource) { return Attach(::LoadMenu(nullptr, MAKEINTRESOURCE(nIDResource))); }

BOOL CMenu::DestroyMenu() {
    if (!m_hMenu)
        return FALSE;
    BOOL r = ::DestroyMenu(m_hMenu);
    m_hMenu = nullptr;
    return r;
}

BOOL CMenu::DeleteMenu(UINT nPosition, UINT nFlags) { return ::DeleteMenu(m_hMenu, nPosition, nFlags); }
BOOL CMenu::RemoveMenu(UINT nPosition, UINT nFlags) { return ::RemoveMenu(m_hMenu, nPosition, nFlags); }
BOOL CMenu::AppendMenu(UINT nFlags, UINT_PTR nIDNewItem, const char* lpszNewItem) {
    return ::AppendMenu(m_hMenu, nFlags, nIDNewItem, lpszNewItem);
}
BOOL CMenu::InsertMenu(UINT nPosition, UINT nFlags, UINT_PTR nIDNewItem, const char* lpszNewItem) {
    return ::InsertMenu(m_hMenu, nPosition, nFlags, nIDNewItem, lpszNewItem);
}
BOOL CMenu::ModifyMenu(UINT nPosition, UINT nFlags, UINT_PTR nIDNewItem, const char* lpszNewItem) {
    return ::ModifyMenu(m_hMenu, nPosition, nFlags, nIDNewItem, lpszNewItem);
}
UINT CMenu::CheckMenuItem(UINT nIDCheckItem, UINT nCheck) { return ::CheckMenuItem(m_hMenu, nIDCheckItem, nCheck); }
BOOL CMenu::CheckMenuRadioItem(UINT nIDFirst, UINT nIDLast, UINT nIDItem, UINT nFlags) {
    for (UINT id = nIDFirst; id <= nIDLast; ++id)
        ::CheckMenuItem(m_hMenu, id, (nFlags & MF_BYPOSITION) | (id == nIDItem ? MF_CHECKED : MF_UNCHECKED));
    return TRUE;
}
UINT CMenu::EnableMenuItem(UINT nIDEnableItem, UINT nEnable) { return ::EnableMenuItem(m_hMenu, nIDEnableItem, nEnable); }
UINT CMenu::GetMenuItemCount() const { return static_cast<UINT>(::GetMenuItemCount(m_hMenu)); }
UINT CMenu::GetMenuItemID(int nPos) const { return ::GetMenuItemID(m_hMenu, nPos); }
UINT CMenu::GetMenuState(UINT nID, UINT nFlags) const { return ::GetMenuState(m_hMenu, nID, nFlags); }
int CMenu::GetMenuString(UINT nIDItem, char* lpString, int nMaxCount, UINT nFlags) const {
    return ::GetMenuString(m_hMenu, nIDItem, lpString, nMaxCount, nFlags);
}
int CMenu::GetMenuString(UINT nIDItem, CString& rString, UINT nFlags) const {
    char buf[512];
    int n = ::GetMenuString(m_hMenu, nIDItem, buf, sizeof buf, nFlags);
    rString = buf;
    return n;
}
CMenu* CMenu::GetSubMenu(int nPos) const { return FromHandle(::GetSubMenu(m_hMenu, nPos)); }
BOOL CMenu::TrackPopupMenu(UINT nFlags, int x, int y, CWnd* pWnd, LPCRECT lpRect) {
    return ::TrackPopupMenu(m_hMenu, nFlags, x, y, 0, pWnd ? pWnd->m_hWnd : nullptr, lpRect);
}
wxMenu* CMenu::GetWxMenu() const { return WxMenuFromHandle(m_hMenu); }
wxMenuBar* CMenu::GetWxMenuBar() const { return WxMenuBarFromHandle(m_hMenu); }

// ---------------------------------------------------------------------------------------------
// Win32 menu functions

HMENU LoadMenu(HINSTANCE, const char* lpMenuName) {
    const rc::Menu* res = FindMenu(ResRef::From(lpMenuName));
    if (!res)
        return nullptr;
    return OnMain([&]() -> HMENU {
        bool hasTopLevelItems = false;
        for (int i = 0; i < res->itemCount; ++i)
            if (res->items[i].depth == 0 && !(res->items[i].flags & MF_POPUP))
                hasTopLevelItems = true;
        if (hasTopLevelItems) {
            auto* menu = new wxMenu;
            int index = 0;
            AppendItemsTopLevel(menu, *res, index);
            return HandleFromWxMenu(menu);
        }
        return HandleFromWxMenuBar(BuildMenuBar(*res));
    });
}

HMENU GetMenu(HWND hWnd) {
    if (!IsWindow(hWnd))
        return nullptr;
    return OnMain([&]() -> HMENU {
        auto* frame = wxDynamicCast(wxGetTopLevelParent(ToWx(hWnd)), wxFrame);
        return frame ? HandleFromWxMenuBar(frame->GetMenuBar()) : nullptr;
    });
}

BOOL SetMenu(HWND hWnd, HMENU hMenu) {
    if (!IsWindow(hWnd))
        return FALSE;
    return OnMain([&]() -> BOOL {
        auto* frame = wxDynamicCast(ToWx(hWnd), wxFrame);
        if (!frame)
            return FALSE;
        wxMenuBar* bar = WxMenuBarFromHandle(hMenu);
        if (bar && frame->GetMenuBar() != bar)
            frame->SetMenuBar(bar);
        return TRUE;
    });
}

HMENU GetSubMenu(HMENU hMenu, int nPos) {
    MenuHandle* mh = FromH(hMenu);
    if (!mh || nPos < 0)
        return nullptr;
    if (mh->bar)
        return nPos < static_cast<int>(mh->bar->GetMenuCount()) ? HandleFromWxMenu(mh->bar->GetMenu(static_cast<size_t>(nPos))) : nullptr;
    if (mh->menu && nPos < static_cast<int>(mh->menu->GetMenuItemCount())) {
        wxMenuItem* item = mh->menu->FindItemByPosition(static_cast<size_t>(nPos));
        return item && item->GetSubMenu() ? HandleFromWxMenu(item->GetSubMenu()) : nullptr;
    }
    return nullptr;
}

HMENU GetSystemMenu(HWND, BOOL) { return nullptr; }

HMENU CreateMenu() {
    return OnMain([]() -> HMENU { return HandleFromWxMenuBar(new wxMenuBar); });
}

HMENU CreatePopupMenu() {
    return OnMain([]() -> HMENU { return HandleFromWxMenu(new wxMenu); });
}

BOOL DestroyMenu(HMENU hMenu) {
    MenuHandle* mh = FromH(hMenu);
    if (!mh)
        return FALSE;
    OnMain([&] {
        auto tmp = TempMenus().find(mh);
        if (tmp != TempMenus().end()) {
            tmp->second->m_hMenu = nullptr;
            delete tmp->second;
            TempMenus().erase(tmp);
        }
        if (mh->menu && !mh->menu->GetParent() && !mh->menu->GetMenuBar())
            delete mh->menu;
        if (mh->bar && !mh->bar->GetFrame())
            delete mh->bar;
        Handles().erase(mh->menu ? static_cast<const void*>(mh->menu) : static_cast<const void*>(mh->bar));
        delete mh;
    });
    return TRUE;
}

namespace {

BOOL InsertItem(HMENU hMenu, int position, UINT uFlags, UINT_PTR uIDNewItem, const char* lpNewItem) {
    MenuHandle* mh = FromH(hMenu);
    if (!mh)
        return FALSE;
    return OnMain([&]() -> BOOL {
        wxString text = ToWx(lpNewItem ? lpNewItem : "");
        if (mh->bar) {
            wxMenu* sub = (uFlags & MF_POPUP) ? WxMenuFromHandle(reinterpret_cast<HMENU>(uIDNewItem)) : new wxMenu;
            if (position < 0 || position >= static_cast<int>(mh->bar->GetMenuCount()))
                mh->bar->Append(sub, text);
            else
                mh->bar->Insert(static_cast<size_t>(position), sub, text);
            return TRUE;
        }
        wxMenu* menu = mh->menu;
        if (!menu)
            return FALSE;
        size_t pos = position < 0 || position > static_cast<int>(menu->GetMenuItemCount()) ? menu->GetMenuItemCount()
                                                                                             : static_cast<size_t>(position);
        if (uFlags & MF_SEPARATOR) {
            menu->InsertSeparator(pos);
        } else if (uFlags & MF_POPUP) {
            menu->Insert(pos, wxID_ANY, text, WxMenuFromHandle(reinterpret_cast<HMENU>(uIDNewItem)));
        } else {
            wxMenuItem* mi = menu->Insert(pos, MenuWxId(static_cast<int>(uIDNewItem)), text);
            if (uFlags & MF_CHECKED) {
                mi->SetCheckable(true);
                mi->Check(true);
            }
            if (uFlags & (MF_GRAYED | MF_DISABLED))
                mi->Enable(false);
        }
        return TRUE;
    });
}

} // namespace

BOOL AppendMenu(HMENU hMenu, UINT uFlags, UINT_PTR uIDNewItem, const char* lpNewItem) {
    return InsertItem(hMenu, -1, uFlags, uIDNewItem, lpNewItem);
}

BOOL AppendMenuA(HMENU hMenu, UINT uFlags, UINT_PTR uIDNewItem, const char* lpNewItem) {
    return AppendMenu(hMenu, uFlags, uIDNewItem, lpNewItem);
}

BOOL InsertMenu(HMENU hMenu, UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, const char* lpNewItem) {
    int position = -1;
    if (uFlags & MF_BYPOSITION) {
        position = static_cast<int>(uPosition);
    } else {
        MenuHandle* mh = FromH(hMenu);
        if (mh && mh->menu) {
            int id = MenuWxId(static_cast<int>(uPosition));
            const wxMenuItemList& items = mh->menu->GetMenuItems();
            int i = 0;
            for (auto* item : items) {
                if (item->GetId() == id) {
                    position = i;
                    break;
                }
                ++i;
            }
        }
    }
    return InsertItem(hMenu, position, uFlags, uIDNewItem, lpNewItem);
}

BOOL DeleteMenu(HMENU hMenu, UINT uPosition, UINT uFlags) {
    return OnMain([&]() -> BOOL {
        ItemRef r = FindItem(hMenu, uPosition, uFlags);
        if (r.bar) {
            delete r.bar->Remove(static_cast<size_t>(r.barIndex));
            return TRUE;
        }
        if (r.parent && r.item) {
            r.parent->Destroy(r.item);
            return TRUE;
        }
        return FALSE;
    });
}

BOOL RemoveMenu(HMENU hMenu, UINT uPosition, UINT uFlags) {
    return OnMain([&]() -> BOOL {
        ItemRef r = FindItem(hMenu, uPosition, uFlags);
        if (r.bar) {
            r.bar->Remove(static_cast<size_t>(r.barIndex));
            return TRUE;
        }
        if (r.parent && r.item) {
            r.parent->Remove(r.item);
            return TRUE;
        }
        return FALSE;
    });
}

BOOL ModifyMenu(HMENU hMnu, UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, const char* lpNewItem) {
    return OnMain([&]() -> BOOL {
        ItemRef r = FindItem(hMnu, uPosition, uFlags);
        if (r.bar) {
            if (lpNewItem)
                r.bar->SetMenuLabel(static_cast<size_t>(r.barIndex), ToWx(lpNewItem));
            return TRUE;
        }
        if (!r.item)
            return FALSE;
        if (lpNewItem && !(uFlags & MF_SEPARATOR))
            r.item->SetItemLabel(ToWx(lpNewItem));
        if (!(uFlags & MF_POPUP) && uIDNewItem && static_cast<int>(uIDNewItem) != MenuWinId(r.item->GetId())) {
            size_t pos = 0;
            r.parent->FindChildItem(r.item->GetId(), &pos);
            wxString label = r.item->GetItemLabel();
            r.parent->Destroy(r.item);
            r.parent->Insert(pos, MenuWxId(static_cast<int>(uIDNewItem)), label);
        }
        return TRUE;
    });
}

DWORD CheckMenuItem(HMENU hMenu, UINT uIDCheckItem, UINT uCheck) {
    return OnMain([&]() -> DWORD {
        ItemRef r = FindItem(hMenu, uIDCheckItem, uCheck);
        if (!r.item)
            return 0xFFFFFFFF;
        DWORD old = r.item->IsCheckable() && r.item->IsChecked() ? MF_CHECKED : MF_UNCHECKED;
        if (!r.item->IsCheckable())
            r.item->SetCheckable(true);
        r.item->Check((uCheck & MF_CHECKED) != 0);
        return old;
    });
}

BOOL EnableMenuItem(HMENU hMenu, UINT uIDEnableItem, UINT uEnable) {
    return OnMain([&]() -> BOOL {
        ItemRef r = FindItem(hMenu, uIDEnableItem, uEnable);
        bool enable = (uEnable & (MF_GRAYED | MF_DISABLED)) == 0;
        if (r.bar) {
            BOOL old = r.bar->IsEnabledTop(static_cast<size_t>(r.barIndex)) ? MF_ENABLED : MF_GRAYED;
            r.bar->EnableTop(static_cast<size_t>(r.barIndex), enable);
            return old;
        }
        if (!r.item)
            return static_cast<BOOL>(0xFFFFFFFF);
        BOOL old = r.item->IsEnabled() ? MF_ENABLED : MF_GRAYED;
        r.item->Enable(enable);
        return old;
    });
}

int GetMenuItemCount(HMENU hMenu) {
    MenuHandle* mh = FromH(hMenu);
    if (!mh)
        return -1;
    return OnMain([&]() -> int {
        return mh->bar ? static_cast<int>(mh->bar->GetMenuCount()) : static_cast<int>(mh->menu->GetMenuItemCount());
    });
}

UINT GetMenuItemID(HMENU hMenu, int nPos) {
    return OnMain([&]() -> UINT {
        ItemRef r = FindItem(hMenu, static_cast<UINT>(nPos), MF_BYPOSITION);
        if (!r.item)
            return static_cast<UINT>(-1);
        if (r.item->IsSubMenu())
            return static_cast<UINT>(-1);
        if (r.item->IsSeparator())
            return 0;
        return static_cast<UINT>(MenuWinId(r.item->GetId()));
    });
}

UINT GetMenuState(HMENU hMenu, UINT uId, UINT uFlags) {
    return OnMain([&]() -> UINT {
        ItemRef r = FindItem(hMenu, uId, uFlags);
        if (r.bar)
            return r.bar->IsEnabledTop(static_cast<size_t>(r.barIndex)) ? MF_POPUP : (MF_POPUP | MF_GRAYED);
        if (!r.item)
            return static_cast<UINT>(-1);
        UINT state = 0;
        if (r.item->IsSeparator())
            state |= MF_SEPARATOR;
        if (r.item->IsSubMenu())
            state |= MF_POPUP | (static_cast<UINT>(r.item->GetSubMenu()->GetMenuItemCount()) << 8);
        if (!r.item->IsEnabled())
            state |= MF_GRAYED;
        if (r.item->IsCheckable() && r.item->IsChecked())
            state |= MF_CHECKED;
        return state;
    });
}

int GetMenuString(HMENU hMenu, UINT uIDItem, char* lpString, int cchMax, UINT flags) {
    return OnMain([&]() -> int {
        ItemRef r = FindItem(hMenu, uIDItem, flags);
        wxString label;
        if (r.bar)
            label = r.bar->GetMenuLabel(static_cast<size_t>(r.barIndex));
        else if (r.item)
            label = r.item->GetItemLabel();
        std::string a = FromWx(label);
        if (lpString && cchMax > 0) {
            strncpy(lpString, a.c_str(), static_cast<size_t>(cchMax) - 1);
            lpString[cchMax - 1] = 0;
        }
        return static_cast<int>(a.size());
    });
}

BOOL TrackPopupMenu(HMENU hMenu, UINT uFlags, int x, int y, int, HWND hWnd, const RECT*) {
    wxMenu* menu = WxMenuFromHandle(hMenu);
    if (!menu || !IsWindow(hWnd))
        return FALSE;
    return OnMain([&]() -> BOOL {
        wxWindow* w = ToWx(hWnd);
        wxPoint p = w->ScreenToClient(wxPoint(x, y));
        std::unique_ptr<wxMenu> copy(CloneMenu(menu));
        int id = w->GetPopupMenuSelectionFromUser(*copy, p);
        if (id == wxID_NONE)
            return 0;
        int cmd = MenuWinId(id);
        if (uFlags & TPM_RETURNCMD)
            return cmd;
        DispatchMessageTo(hWnd, WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
        return TRUE;
    });
}

BOOL DrawMenuBar(HWND hWnd) {
    if (IsWindow(hWnd))
        OnMain([&] {
            if (auto* frame = wxDynamicCast(wxGetTopLevelParent(ToWx(hWnd)), wxFrame))
                if (frame->GetMenuBar())
                    frame->GetMenuBar()->Refresh();
        });
    return TRUE;
}
