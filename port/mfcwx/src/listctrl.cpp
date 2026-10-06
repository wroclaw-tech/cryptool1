#include "controls_internal.h"

#include "afxcmn.h"

#include <wx/imaglist.h>
#include <wx/listctrl.h>
#include <wx/renderer.h>

#include <algorithm>

namespace mfcwx {

namespace {

struct ListViewExtra {
    DWORD exStyle = 0;
    int selectionMark = -1;
    COLORREF textBkColor = CLR_DEFAULT;
    CImageList* imageLists[3] = {nullptr, nullptr, nullptr};
    bool suppressActivate = false;
};

ListViewExtra& ListExtra(wxListCtrl* lc) { return Extra<ListViewExtra>(EnsureState(lc)); }

UINT StateImage(int index) { return static_cast<UINT>(index) << 12; }

wxWindow* ListMainWindow(wxListCtrl* lc) {
#if !defined(__WXMSW__) && !defined(__WXQT__)
    for (wxWindow* c : lc->GetChildren())
        if (static_cast<void*>(c) == static_cast<void*>(lc->m_mainWin))
            return c;
#endif
    return lc;
}

wxPoint ToListPoint(wxListCtrl* lc, wxWindow* from, const wxPoint& p) {
    return from == lc ? p : lc->ScreenToClient(from->ClientToScreen(p));
}

wxString ItemText(const char* p) {
    if (!p || p == LPSTR_TEXTCALLBACK)
        return wxString();
    return ToWx(p);
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

bool ValidItem(wxListCtrl* lc, long item) { return item >= 0 && item < lc->GetItemCount(); }

bool ValidColumn(wxListCtrl* lc, int col) { return col >= 0 && (col == 0 || col < lc->GetColumnCount()); }

UINT ItemState(wxListCtrl* lc, long item, UINT mask) {
    long wxMask = 0;
    if (mask & LVIS_SELECTED)
        wxMask |= wxLIST_STATE_SELECTED;
    if (mask & LVIS_FOCUSED)
        wxMask |= wxLIST_STATE_FOCUSED;
    long s = wxMask ? lc->GetItemState(item, wxMask) : 0;
    UINT state = 0;
    if (s & wxLIST_STATE_SELECTED)
        state |= LVIS_SELECTED;
    if (s & wxLIST_STATE_FOCUSED)
        state |= LVIS_FOCUSED;
    if ((mask & LVIS_STATEIMAGEMASK) && lc->HasCheckBoxes())
        state |= StateImage(lc->IsItemChecked(item) ? 2 : 1);
    return state & mask;
}

void SetItemStateOne(wxListCtrl* lc, long item, UINT state, UINT mask) {
    long wxMask = 0;
    long wxState = 0;
    if (mask & LVIS_SELECTED) {
        wxMask |= wxLIST_STATE_SELECTED;
        if (state & LVIS_SELECTED)
            wxState |= wxLIST_STATE_SELECTED;
    }
    if (mask & LVIS_FOCUSED) {
        wxMask |= wxLIST_STATE_FOCUSED;
        if (state & LVIS_FOCUSED)
            wxState |= wxLIST_STATE_FOCUSED;
    }
    if (wxMask)
        lc->SetItemState(item, wxState, wxMask);
    if ((mask & LVIS_STATEIMAGEMASK) && lc->HasCheckBoxes())
        lc->CheckItem(item, ((state & LVIS_STATEIMAGEMASK) >> 12) == 2);
}

bool SetItemStates(wxListCtrl* lc, long item, UINT state, UINT mask) {
    if (item == -1) {
        for (long i = 0; i < lc->GetItemCount(); ++i)
            SetItemStateOne(lc, i, state, mask);
        return true;
    }
    if (!ValidItem(lc, item))
        return false;
    SetItemStateOne(lc, item, state, mask);
    return true;
}

wxListColumnFormat AlignFromFormat(int fmt) {
    switch (fmt & LVCFMT_JUSTIFYMASK) {
    case LVCFMT_RIGHT: return wxLIST_FORMAT_RIGHT;
    case LVCFMT_CENTER: return wxLIST_FORMAT_CENTRE;
    default: return wxLIST_FORMAT_LEFT;
    }
}

int FormatFromAlign(wxListColumnFormat align) {
    switch (align) {
    case wxLIST_FORMAT_RIGHT: return LVCFMT_RIGHT;
    case wxLIST_FORMAT_CENTRE: return LVCFMT_CENTER;
    default: return LVCFMT_LEFT;
    }
}

int RectCode(int code) {
    switch (code) {
    case LVIR_ICON: return wxLIST_RECT_ICON;
    case LVIR_LABEL: return wxLIST_RECT_LABEL;
    default: return wxLIST_RECT_BOUNDS;
    }
}

bool ItemRect(wxListCtrl* lc, long item, int subItem, int code, RECT* out) {
    if (!out || !ValidItem(lc, item))
        return false;
    wxRect r;
    bool ok = subItem < 0 ? lc->GetItemRect(item, r, RectCode(code))
                          : subItem < lc->GetColumnCount() && lc->GetSubItemRect(item, subItem, r, RectCode(code));
    if (!ok)
        return false;
    out->left = r.x;
    out->top = r.y;
    out->right = r.x + r.width;
    out->bottom = r.y + r.height;
    return true;
}

struct ListHit {
    long item = -1;
    int subItem = -1;
    UINT flags = LVHT_NOWHERE;
};

ListHit HitTestList(wxListCtrl* lc, const wxPoint& pt) {
    ListHit hit;
    long count = lc->GetItemCount();
    long first = 0;
    long last = count - 1;
    bool report = lc->InReportView();
    if (report) {
        first = std::max(0L, lc->GetTopItem());
        last = std::min(count - 1, first + lc->GetCountPerPage() + 1);
    }
    for (long i = first; i <= last; ++i) {
        wxRect r;
        if (!lc->GetItemRect(i, r, wxLIST_RECT_BOUNDS) || !r.Contains(pt))
            continue;
        hit.item = i;
        hit.subItem = 0;
        hit.flags = LVHT_ONITEMLABEL;
        if (report) {
            for (int c = 0; c < lc->GetColumnCount(); ++c) {
                wxRect sr;
                if (lc->GetSubItemRect(i, c, sr) && pt.x >= sr.x && pt.x < sr.x + sr.width) {
                    hit.subItem = c;
                    break;
                }
            }
        }
        wxRect icon;
        if (lc->HasCheckBoxes() && hit.subItem == 0 &&
            pt.x < r.x + wxRendererNative::Get().GetCheckBoxSize(lc).x + 4)
            hit.flags = LVHT_ONITEMSTATEICON;
        else if (lc->GetItemRect(i, icon, wxLIST_RECT_ICON) && icon.width > 0 && icon.Contains(pt))
            hit.flags = LVHT_ONITEMICON;
        return hit;
    }
    return hit;
}

void ApplyExtendedStyle(wxListCtrl* lc, DWORD oldStyle, DWORD newStyle) {
    if ((oldStyle ^ newStyle) & LVS_EX_CHECKBOXES)
        lc->EnableCheckBoxes((newStyle & LVS_EX_CHECKBOXES) != 0);
    if ((oldStyle ^ newStyle) & LVS_EX_GRIDLINES) {
        bool on = (newStyle & LVS_EX_GRIDLINES) != 0;
        lc->SetSingleStyle(wxLC_HRULES, on);
        lc->SetSingleStyle(wxLC_VRULES, on);
        lc->Refresh();
    }
}

LRESULT ListNotify(wxListCtrl* lc, UINT code, long item, int subItem, UINT newState = 0, UINT oldState = 0,
                   UINT changed = 0, const wxPoint& pt = wxPoint(0, 0)) {
    NMITEMACTIVATE nm;
    memset(&nm, 0, sizeof nm);
    nm.hdr.code = code;
    nm.iItem = static_cast<int>(item);
    nm.iSubItem = subItem;
    nm.uNewState = newState;
    nm.uOldState = oldState;
    nm.uChanged = changed;
    nm.ptAction.x = pt.x;
    nm.ptAction.y = pt.y;
    if (ValidItem(lc, item))
        nm.lParam = static_cast<LPARAM>(lc->GetItemData(item));
    return NotifyParentNM(lc, &nm.hdr);
}

void MouseNotify(wxListCtrl* lc, wxWindow* from, UINT code, const wxMouseEvent& e) {
    wxPoint pt = ToListPoint(lc, from, e.GetPosition());
    ListHit hit = HitTestList(lc, pt);
    ListNotify(lc, code, hit.item, hit.item >= 0 ? hit.subItem : -1, 0, 0, 0, pt);
}

} // namespace

void BindListViewEvents(wxWindow* w) {
    auto* lc = wxDynamicCast(w, wxListCtrl);
    if (!lc)
        return;
    lc->Bind(wxEVT_LIST_ITEM_SELECTED, [lc](wxListEvent& e) {
        if (WindowState* st = GetState(lc))
            Extra<ListViewExtra>(*st).selectionMark = static_cast<int>(e.GetIndex());
        ListNotify(lc, LVN_ITEMCHANGED, e.GetIndex(), 0, LVIS_SELECTED | LVIS_FOCUSED, 0, LVIF_STATE);
    });
    lc->Bind(wxEVT_LIST_ITEM_DESELECTED,
             [lc](wxListEvent& e) { ListNotify(lc, LVN_ITEMCHANGED, e.GetIndex(), 0, 0, LVIS_SELECTED, LVIF_STATE); });
    lc->Bind(wxEVT_LIST_ITEM_CHECKED, [lc](wxListEvent& e) {
        ListNotify(lc, LVN_ITEMCHANGED, e.GetIndex(), 0, StateImage(2), StateImage(1), LVIF_STATE);
    });
    lc->Bind(wxEVT_LIST_ITEM_UNCHECKED, [lc](wxListEvent& e) {
        ListNotify(lc, LVN_ITEMCHANGED, e.GetIndex(), 0, StateImage(1), StateImage(2), LVIF_STATE);
    });
    lc->Bind(wxEVT_LIST_COL_CLICK,
             [lc](wxListEvent& e) { ListNotify(lc, LVN_COLUMNCLICK, -1, std::max(0, e.GetColumn())); });
    lc->Bind(wxEVT_LIST_KEY_DOWN, [lc](wxListEvent& e) {
        NMLVKEYDOWN nm;
        memset(&nm, 0, sizeof nm);
        nm.hdr.code = LVN_KEYDOWN;
        nm.wVKey = static_cast<WORD>(VirtualKeyFromWx(e.GetKeyCode()));
        NotifyParentNM(lc, &nm.hdr);
        e.Skip();
    });
    lc->Bind(wxEVT_LIST_ITEM_ACTIVATED, [lc](wxListEvent&) {
        WindowState* st = GetState(lc);
        if (!st)
            return;
        ListViewExtra& ex = Extra<ListViewExtra>(*st);
        if (ex.suppressActivate) {
            ex.suppressActivate = false;
            return;
        }
        NMHDR hdr;
        hdr.code = NM_RETURN;
        NotifyParentNM(lc, &hdr);
    });

    wxWindow* main = ListMainWindow(lc);
    main->Bind(wxEVT_LEFT_UP, [lc, main](wxMouseEvent& e) {
        MouseNotify(lc, main, NM_CLICK, e);
        e.Skip();
    });
    main->Bind(wxEVT_LEFT_DCLICK, [lc, main](wxMouseEvent& e) {
        if (WindowState* st = GetState(lc)) {
            Extra<ListViewExtra>(*st).suppressActivate = true;
            lc->CallAfter([lc] {
                if (WindowState* s = GetState(lc))
                    Extra<ListViewExtra>(*s).suppressActivate = false;
            });
        }
        MouseNotify(lc, main, NM_DBLCLK, e);
        e.Skip();
    });
    main->Bind(wxEVT_RIGHT_UP, [lc, main](wxMouseEvent& e) {
        MouseNotify(lc, main, NM_RCLICK, e);
        e.Skip();
    });
    if (main != lc) {
        main->Bind(wxEVT_SET_FOCUS, [lc](wxFocusEvent& e) {
            if (WindowState* st = GetState(lc))
                NotifyFocusChange(lc, *st, true);
            e.Skip();
        });
        main->Bind(wxEVT_KILL_FOCUS, [lc](wxFocusEvent& e) {
            if (WindowState* st = GetState(lc))
                NotifyFocusChange(lc, *st, false);
            e.Skip();
        });
    }
}

LRESULT ListViewProc(wxWindow* w, WindowState& st, UINT msg, WPARAM wParam, LPARAM lParam, bool& handled) {
    handled = true;
    auto* lc = wxDynamicCast(w, wxListCtrl);
    if (!lc) {
        handled = false;
        return 0;
    }
    ListViewExtra& ex = Extra<ListViewExtra>(st);
    switch (msg) {
    case LVM_GETITEMCOUNT:
        return lc->GetItemCount();
    case LVM_GETTOPINDEX:
        return lc->InReportView() || lc->HasFlag(wxLC_LIST) ? std::max(0L, lc->GetTopItem()) : 0;
    case LVM_GETITEMTEXT: {
        auto* item = reinterpret_cast<LVITEM*>(lParam);
        if (!item)
            return 0;
        long index = static_cast<long>(wParam);
        wxString text;
        if (ValidItem(lc, index) && ValidColumn(lc, item->iSubItem))
            text = lc->GetItemText(index, item->iSubItem);
        return CopyText(text, item->pszText, item->cchTextMax);
    }
    case LVM_GETITEMRECT: {
        auto* r = reinterpret_cast<RECT*>(lParam);
        return r && ItemRect(lc, static_cast<long>(wParam), -1, r->left, r);
    }
    case LVM_GETSUBITEMRECT: {
        auto* r = reinterpret_cast<RECT*>(lParam);
        if (!r)
            return FALSE;
        int sub = r->top;
        int code = r->left;
        if (sub == 0 && code == LVIR_BOUNDS)
            sub = -1;
        return ItemRect(lc, static_cast<long>(wParam), sub, code, r);
    }
    case LVM_SUBITEMHITTEST: {
        auto* info = reinterpret_cast<LVHITTESTINFO*>(lParam);
        if (!info)
            return -1;
        ListHit hit = HitTestList(lc, wxPoint(info->pt.x, info->pt.y));
        info->flags = hit.flags;
        info->iItem = static_cast<int>(hit.item);
        info->iSubItem = hit.item >= 0 ? hit.subItem : -1;
        return hit.item;
    }
    case LVM_SETEXTENDEDLISTVIEWSTYLE: {
        DWORD mask = wParam ? static_cast<DWORD>(wParam) : 0xFFFFFFFFu;
        DWORD old = ex.exStyle;
        ex.exStyle = (old & ~mask) | (static_cast<DWORD>(lParam) & mask);
        ApplyExtendedStyle(lc, old, ex.exStyle);
        return static_cast<LRESULT>(old);
    }
    case LVM_GETEXTENDEDLISTVIEWSTYLE:
        return static_cast<LRESULT>(ex.exStyle);
    case WM_GETDLGCODE:
        return DLGC_WANTARROWS | DLGC_WANTCHARS;
    default:
        handled = false;
        return 0;
    }
}

} // namespace mfcwx

using namespace mfcwx;

namespace {

wxListCtrl* ListOf(const CWnd* w) { return w->m_hWnd ? wxDynamicCast(w->GetWx(), wxListCtrl) : nullptr; }

wxImageList* WxImageList(const CImageList* il) {
    return il ? reinterpret_cast<wxImageList*>(il->m_hImageList) : nullptr;
}

struct SortContext {
    int (*compare)(LPARAM, LPARAM, LPARAM);
    LPARAM data;
};

int wxCALLBACK SortTrampoline(wxIntPtr item1, wxIntPtr item2, wxIntPtr sortData) {
    auto* ctx = reinterpret_cast<SortContext*>(sortData);
    return ctx->compare(static_cast<LPARAM>(item1), static_cast<LPARAM>(item2), ctx->data);
}

int AddBitmap(wxImageList* il, const wxBitmap& bmp, COLORREF crMask) {
    if (!il || !bmp.IsOk())
        return -1;
    int cx = 0;
    int cy = 0;
    il->GetSize(0, cx, cy);
    if (cx <= 0 || cy <= 0)
        return -1;
    int first = -1;
    int count = std::max(1, bmp.GetWidth() / cx);
    for (int i = 0; i < count; ++i) {
        wxBitmap part = bmp.GetWidth() > cx ? bmp.GetSubBitmap(wxRect(i * cx, 0, cx, std::min(cy, bmp.GetHeight())))
                                            : bmp;
        if (part.GetWidth() != cx || part.GetHeight() != cy)
            part = wxBitmap(part.ConvertToImage().Size(wxSize(cx, cy), wxPoint(0, 0)));
        int index = crMask == CLR_NONE ? il->Add(part) : il->Add(part, ToWxColour(crMask));
        if (first < 0)
            first = index;
    }
    return first;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// CImageList

IMPLEMENT_DYNCREATE(CImageList, CObject)

CImageList::CImageList() : m_hImageList(nullptr) {}

CImageList::~CImageList() { DeleteImageList(); }

BOOL CImageList::Create(int cx, int cy, UINT nFlags, int nInitial, int) {
    return OnMain([&]() -> BOOL {
        DeleteImageList();
        m_hImageList = reinterpret_cast<HIMAGELIST>(new wxImageList(cx, cy, (nFlags & ILC_MASK) != 0, std::max(1, nInitial)));
        return TRUE;
    });
}

BOOL CImageList::Create(UINT nBitmapID, int cx, int, COLORREF crMask) {
    return OnMain([&]() -> BOOL {
        wxBitmap bmp = LoadBitmapResource(ResRef::FromId(nBitmapID));
        if (!bmp.IsOk() || cx <= 0)
            return FALSE;
        DeleteImageList();
        auto* il = new wxImageList(cx, bmp.GetHeight(), crMask != CLR_NONE, std::max(1, bmp.GetWidth() / cx));
        m_hImageList = reinterpret_cast<HIMAGELIST>(il);
        AddBitmap(il, bmp, crMask);
        return TRUE;
    });
}

BOOL CImageList::DeleteImageList() {
    if (!m_hImageList)
        return FALSE;
    wxImageList* il = WxImageList(this);
    m_hImageList = nullptr;
    OnMain([&] { delete il; });
    return TRUE;
}

int CImageList::Add(CBitmap* pbmImage, CBitmap* pbmMask) {
    return OnMain([&]() -> int {
        wxBitmap* bmp = pbmImage ? BitmapFromHandle(static_cast<HBITMAP>(pbmImage->m_hObject)) : nullptr;
        if (!bmp || !bmp->IsOk())
            return -1;
        wxBitmap* mask = pbmMask ? BitmapFromHandle(static_cast<HBITMAP>(pbmMask->m_hObject)) : nullptr;
        if (!mask || !mask->IsOk())
            return AddBitmap(WxImageList(this), *bmp, CLR_NONE);
        wxImage img = bmp->ConvertToImage();
        wxImage m = mask->ConvertToImage();
        if (!img.HasAlpha())
            img.InitAlpha();
        int w = std::min(img.GetWidth(), m.GetWidth());
        int h = std::min(img.GetHeight(), m.GetHeight());
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                if (m.GetRed(x, y) > 127)
                    img.SetAlpha(x, y, 0);
        return AddBitmap(WxImageList(this), wxBitmap(img), CLR_NONE);
    });
}

int CImageList::Add(CBitmap* pbmImage, COLORREF crMask) {
    return OnMain([&]() -> int {
        wxBitmap* bmp = pbmImage ? BitmapFromHandle(static_cast<HBITMAP>(pbmImage->m_hObject)) : nullptr;
        return bmp ? AddBitmap(WxImageList(this), *bmp, crMask) : -1;
    });
}

int CImageList::Add(HICON hIcon) {
    return OnMain([&]() -> int {
        auto* icon = reinterpret_cast<wxIcon*>(hIcon);
        wxImageList* il = WxImageList(this);
        if (!il || !icon || !icon->IsOk())
            return -1;
        return il->Add(*icon);
    });
}

int CImageList::GetImageCount() const {
    return OnMain([&]() -> int {
        wxImageList* il = WxImageList(this);
        return il ? il->GetImageCount() : 0;
    });
}

BOOL CImageList::Draw(CDC* pDC, int nImage, POINT pt, UINT nStyle) {
    return OnMain([&]() -> BOOL {
        wxImageList* il = WxImageList(this);
        wxDC* dc = pDC ? pDC->GetWx() : nullptr;
        if (!il || !dc || nImage < 0 || nImage >= il->GetImageCount())
            return FALSE;
        int flags = (nStyle & ILD_TRANSPARENT) ? wxIMAGELIST_DRAW_TRANSPARENT : wxIMAGELIST_DRAW_NORMAL;
        return il->Draw(nImage, *dc, pt.x, pt.y, flags, true);
    });
}

// ---------------------------------------------------------------------------------------------
// CHeaderCtrl

IMPLEMENT_DYNAMIC(CHeaderCtrl, CWnd)

int CHeaderCtrl::GetItemCount() const {
    return OnMain([&]() -> int {
        wxListCtrl* lc = ListOf(this);
        return lc ? lc->GetColumnCount() : 0;
    });
}

// ---------------------------------------------------------------------------------------------
// CListCtrl

IMPLEMENT_DYNAMIC(CListCtrl, CWnd)

BOOL CListCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID) {
    return CWnd::Create("SysListView32", nullptr, dwStyle, rect, pParentWnd, nID);
}

COLORREF CListCtrl::GetBkColor() const {
    return OnMain([&]() -> COLORREF {
        wxListCtrl* lc = ListOf(this);
        return lc ? FromWxColour(lc->GetBackgroundColour()) : CLR_NONE;
    });
}

BOOL CListCtrl::SetBkColor(COLORREF cr) {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        if (!lc)
            return FALSE;
        if (cr == CLR_NONE && lc->GetParent())
            lc->SetBackgroundColour(lc->GetParent()->GetBackgroundColour());
        else if (cr != CLR_NONE)
            lc->SetBackgroundColour(ToWxColour(cr));
        lc->Refresh();
        return TRUE;
    });
}

COLORREF CListCtrl::GetTextColor() const {
    return OnMain([&]() -> COLORREF {
        wxListCtrl* lc = ListOf(this);
        return lc ? FromWxColour(lc->GetTextColour()) : 0;
    });
}

BOOL CListCtrl::SetTextColor(COLORREF cr) {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        if (!lc)
            return FALSE;
        lc->SetTextColour(ToWxColour(cr));
        lc->Refresh();
        return TRUE;
    });
}

COLORREF CListCtrl::GetTextBkColor() const {
    return OnMain([&]() -> COLORREF {
        wxListCtrl* lc = ListOf(this);
        if (!lc)
            return CLR_NONE;
        COLORREF c = ListExtra(lc).textBkColor;
        return c == CLR_DEFAULT ? FromWxColour(lc->GetBackgroundColour()) : c;
    });
}

BOOL CListCtrl::SetTextBkColor(COLORREF cr) {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        if (!lc)
            return FALSE;
        ListExtra(lc).textBkColor = cr;
        return TRUE;
    });
}

CImageList* CListCtrl::GetImageList(int nImageList) const {
    return OnMain([&]() -> CImageList* {
        wxListCtrl* lc = ListOf(this);
        if (!lc || nImageList < 0 || nImageList > 2)
            return nullptr;
        return ListExtra(lc).imageLists[nImageList];
    });
}

CImageList* CListCtrl::SetImageList(CImageList* pImageList, int nImageListType) {
    return OnMain([&]() -> CImageList* {
        wxListCtrl* lc = ListOf(this);
        if (!lc || nImageListType < 0 || nImageListType > 2)
            return nullptr;
        ListViewExtra& ex = ListExtra(lc);
        CImageList* old = ex.imageLists[nImageListType];
        ex.imageLists[nImageListType] = pImageList;
        int which = nImageListType == LVSIL_NORMAL ? wxIMAGE_LIST_NORMAL
                                                   : nImageListType == LVSIL_SMALL ? wxIMAGE_LIST_SMALL : wxIMAGE_LIST_STATE;
        lc->SetImageList(WxImageList(pImageList), which);
        return old;
    });
}

int CListCtrl::GetItemCount() const { return static_cast<int>(::SendMessage(m_hWnd, LVM_GETITEMCOUNT, 0, 0)); }

BOOL CListCtrl::GetItem(LVITEM* pItem) const {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        if (!lc || !pItem || !ValidItem(lc, pItem->iItem) || !ValidColumn(lc, pItem->iSubItem))
            return FALSE;
        long item = pItem->iItem;
        if (pItem->mask & LVIF_TEXT)
            CopyText(lc->GetItemText(item, pItem->iSubItem), pItem->pszText, pItem->cchTextMax);
        if (pItem->mask & LVIF_IMAGE) {
            wxListItem li;
            li.SetId(item);
            li.SetColumn(pItem->iSubItem);
            li.SetMask(wxLIST_MASK_IMAGE);
            pItem->iImage = lc->GetItem(li) ? li.GetImage() : -1;
        }
        if (pItem->mask & LVIF_PARAM)
            pItem->lParam = static_cast<LPARAM>(lc->GetItemData(item));
        if (pItem->mask & LVIF_STATE)
            pItem->state = ItemState(lc, item, pItem->stateMask);
        if (pItem->mask & LVIF_INDENT)
            pItem->iIndent = 0;
        return TRUE;
    });
}

BOOL CListCtrl::SetItem(const LVITEM* pItem) {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        if (!lc || !pItem || !ValidItem(lc, pItem->iItem) || !ValidColumn(lc, pItem->iSubItem))
            return FALSE;
        long item = pItem->iItem;
        int col = pItem->iSubItem;
        if (pItem->mask & LVIF_TEXT)
            lc->SetItem(item, col, ItemText(pItem->pszText));
        if (pItem->mask & LVIF_IMAGE)
            lc->SetItemColumnImage(item, col, pItem->iImage);
        if (col == 0 && (pItem->mask & LVIF_PARAM))
            lc->SetItemPtrData(item, static_cast<wxUIntPtr>(pItem->lParam));
        if (col == 0 && (pItem->mask & LVIF_STATE))
            SetItemStateOne(lc, item, pItem->state, pItem->stateMask);
        return TRUE;
    });
}

BOOL CListCtrl::SetItem(int nItem, int nSubItem, UINT nMask, const char* lpszItem, int nImage, UINT nState,
                        UINT nStateMask, LPARAM lParam) {
    LVITEM lvi;
    memset(&lvi, 0, sizeof lvi);
    lvi.mask = nMask;
    lvi.iItem = nItem;
    lvi.iSubItem = nSubItem;
    lvi.state = nState;
    lvi.stateMask = nStateMask;
    lvi.pszText = const_cast<char*>(lpszItem);
    lvi.iImage = nImage;
    lvi.lParam = lParam;
    return SetItem(&lvi);
}

int CListCtrl::GetNextItem(int nItem, int nFlags) const {
    return OnMain([&]() -> int {
        wxListCtrl* lc = ListOf(this);
        if (!lc)
            return -1;
        int count = lc->GetItemCount();
        UINT want = static_cast<UINT>(nFlags) & (LVNI_SELECTED | LVNI_FOCUSED | LVNI_CUT | LVNI_DROPHILITED);
        if (want & (LVNI_CUT | LVNI_DROPHILITED))
            return -1;
        auto matches = [&](int i) { return (ItemState(lc, i, want) & want) == want; };
        if (nFlags & (LVNI_ABOVE | LVNI_TOLEFT)) {
            for (int i = std::min(nItem, count) - 1; i >= 0; --i)
                if (matches(i))
                    return i;
            return -1;
        }
        if (want == LVNI_SELECTED)
            return static_cast<int>(lc->GetNextItem(nItem, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED));
        for (int i = nItem < 0 ? 0 : nItem + 1; i < count; ++i)
            if (matches(i))
                return i;
        return -1;
    });
}

POSITION CListCtrl::GetFirstSelectedItemPosition() const {
    return reinterpret_cast<POSITION>(static_cast<intptr_t>(1 + GetNextItem(-1, LVNI_SELECTED)));
}

int CListCtrl::GetNextSelectedItem(POSITION& pos) const {
    int old = static_cast<int>(reinterpret_cast<intptr_t>(pos)) - 1;
    pos = reinterpret_cast<POSITION>(static_cast<intptr_t>(1 + GetNextItem(old, LVNI_SELECTED)));
    return old;
}

BOOL CListCtrl::GetItemRect(int nItem, LPRECT lpRect, UINT nCode) const {
    if (!lpRect)
        return FALSE;
    lpRect->left = static_cast<LONG>(nCode);
    return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_GETITEMRECT, static_cast<WPARAM>(nItem),
                                           reinterpret_cast<LPARAM>(lpRect)));
}

BOOL CListCtrl::GetSubItemRect(int iItem, int iSubItem, int nArea, CRect& ref) {
    RECT* r = &ref;
    r->top = iSubItem;
    r->left = nArea;
    return static_cast<BOOL>(::SendMessage(m_hWnd, LVM_GETSUBITEMRECT, static_cast<WPARAM>(iItem),
                                           reinterpret_cast<LPARAM>(r)));
}

BOOL CListCtrl::GetItemPosition(int nItem, LPPOINT lpPoint) const {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        wxPoint p;
        if (!lc || !lpPoint || !ValidItem(lc, nItem) || !lc->GetItemPosition(nItem, p))
            return FALSE;
        lpPoint->x = p.x;
        lpPoint->y = p.y;
        return TRUE;
    });
}

int CListCtrl::GetStringWidth(const char* lpsz) const {
    return OnMain([&]() -> int {
        wxListCtrl* lc = ListOf(this);
        return lc && lpsz ? lc->GetTextExtent(ToWx(lpsz)).x : 0;
    });
}

BOOL CListCtrl::GetColumn(int nCol, LVCOLUMN* pColumn) const {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        if (!lc || !pColumn || nCol < 0 || nCol >= lc->GetColumnCount())
            return FALSE;
        wxListItem li;
        li.SetMask(wxLIST_MASK_TEXT | wxLIST_MASK_WIDTH | wxLIST_MASK_FORMAT | wxLIST_MASK_IMAGE);
        if (!lc->GetColumn(nCol, li))
            return FALSE;
        if (pColumn->mask & LVCF_TEXT)
            CopyText(li.GetText(), pColumn->pszText, pColumn->cchTextMax);
        if (pColumn->mask & LVCF_WIDTH)
            pColumn->cx = li.GetWidth();
        if (pColumn->mask & LVCF_FMT)
            pColumn->fmt = FormatFromAlign(li.GetAlign());
        if (pColumn->mask & LVCF_IMAGE)
            pColumn->iImage = li.GetImage();
        if (pColumn->mask & LVCF_SUBITEM)
            pColumn->iSubItem = nCol;
        if (pColumn->mask & LVCF_ORDER)
            pColumn->iOrder = nCol;
        return TRUE;
    });
}

BOOL CListCtrl::SetColumn(int nCol, const LVCOLUMN* pColumn) {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        if (!lc || !pColumn || nCol < 0 || nCol >= lc->GetColumnCount())
            return FALSE;
        wxListItem li;
        if (pColumn->mask & LVCF_TEXT)
            li.SetText(ItemText(pColumn->pszText));
        if (pColumn->mask & LVCF_WIDTH)
            li.SetWidth(pColumn->cx);
        if (pColumn->mask & LVCF_FMT)
            li.SetAlign(AlignFromFormat(pColumn->fmt));
        if (pColumn->mask & LVCF_IMAGE)
            li.SetImage(pColumn->iImage);
        return lc->SetColumn(nCol, li);
    });
}

int CListCtrl::GetColumnWidth(int nCol) const {
    return OnMain([&]() -> int {
        wxListCtrl* lc = ListOf(this);
        return lc && nCol >= 0 && nCol < lc->GetColumnCount() ? lc->GetColumnWidth(nCol) : 0;
    });
}

BOOL CListCtrl::SetColumnWidth(int nCol, int cx) {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        if (!lc || nCol < 0 || nCol >= lc->GetColumnCount())
            return FALSE;
        int width = cx == LVSCW_AUTOSIZE ? wxLIST_AUTOSIZE : cx == LVSCW_AUTOSIZE_USEHEADER ? wxLIST_AUTOSIZE_USEHEADER : cx;
        return lc->SetColumnWidth(nCol, width);
    });
}

BOOL CListCtrl::GetViewRect(LPRECT lpRect) const {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        if (!lc || !lpRect)
            return FALSE;
        wxRect r = lc->GetViewRect();
        lpRect->left = r.x;
        lpRect->top = r.y;
        lpRect->right = r.x + r.width;
        lpRect->bottom = r.y + r.height;
        return TRUE;
    });
}

int CListCtrl::GetTopIndex() const { return static_cast<int>(::SendMessage(m_hWnd, LVM_GETTOPINDEX, 0, 0)); }

int CListCtrl::GetCountPerPage() const {
    return OnMain([&]() -> int {
        wxListCtrl* lc = ListOf(this);
        return lc ? lc->GetCountPerPage() : 0;
    });
}

BOOL CListCtrl::GetOrigin(LPPOINT lpPoint) const {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        if (!lc || !lpPoint)
            return FALSE;
        lpPoint->x = 0;
        lpPoint->y = 0;
        return !(lc->InReportView() || lc->HasFlag(wxLC_LIST));
    });
}

BOOL CListCtrl::SetItemState(int nItem, LVITEM* pItem) {
    if (!pItem)
        return FALSE;
    return SetItemState(nItem, pItem->state, pItem->stateMask);
}

BOOL CListCtrl::SetItemState(int nItem, UINT nState, UINT nMask) {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        return lc && SetItemStates(lc, nItem, nState, nMask);
    });
}

UINT CListCtrl::GetItemState(int nItem, UINT nMask) const {
    return OnMain([&]() -> UINT {
        wxListCtrl* lc = ListOf(this);
        return lc && ValidItem(lc, nItem) ? ItemState(lc, nItem, nMask) : 0;
    });
}

CString CListCtrl::GetItemText(int nItem, int nSubItem) const {
    return OnMain([&]() -> CString {
        wxListCtrl* lc = ListOf(this);
        if (!lc || !ValidItem(lc, nItem) || !ValidColumn(lc, nSubItem))
            return CString();
        return CStr(lc->GetItemText(nItem, nSubItem));
    });
}

int CListCtrl::GetItemText(int nItem, int nSubItem, char* lpszText, int nLen) const {
    LVITEM lvi;
    memset(&lvi, 0, sizeof lvi);
    lvi.iSubItem = nSubItem;
    lvi.cchTextMax = nLen;
    lvi.pszText = lpszText;
    return static_cast<int>(::SendMessage(m_hWnd, LVM_GETITEMTEXT, static_cast<WPARAM>(nItem),
                                          reinterpret_cast<LPARAM>(&lvi)));
}

BOOL CListCtrl::SetItemText(int nItem, int nSubItem, const char* lpszText) {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        if (!lc || !ValidItem(lc, nItem) || !ValidColumn(lc, nSubItem))
            return FALSE;
        return lc->SetItem(nItem, nSubItem, ItemText(lpszText));
    });
}

void CListCtrl::SetItemCount(int nItems) {
    OnMain([&] {
        wxListCtrl* lc = ListOf(this);
        if (lc && lc->IsVirtual())
            lc->SetItemCount(nItems);
    });
}

BOOL CListCtrl::SetItemData(int nItem, DWORD_PTR dwData) {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        return lc && ValidItem(lc, nItem) && lc->SetItemPtrData(nItem, static_cast<wxUIntPtr>(dwData));
    });
}

DWORD_PTR CListCtrl::GetItemData(int nItem) const {
    return OnMain([&]() -> DWORD_PTR {
        wxListCtrl* lc = ListOf(this);
        return lc && ValidItem(lc, nItem) ? static_cast<DWORD_PTR>(lc->GetItemData(nItem)) : 0;
    });
}

UINT CListCtrl::GetSelectedCount() const {
    return OnMain([&]() -> UINT {
        wxListCtrl* lc = ListOf(this);
        return lc ? static_cast<UINT>(lc->GetSelectedItemCount()) : 0;
    });
}

int CListCtrl::GetSelectionMark() const {
    return OnMain([&]() -> int {
        wxListCtrl* lc = ListOf(this);
        if (!lc)
            return -1;
        int mark = ListExtra(lc).selectionMark;
        return ValidItem(lc, mark) ? mark : -1;
    });
}

int CListCtrl::SetSelectionMark(int iIndex) {
    return OnMain([&]() -> int {
        wxListCtrl* lc = ListOf(this);
        if (!lc)
            return -1;
        ListViewExtra& ex = ListExtra(lc);
        int old = ex.selectionMark;
        ex.selectionMark = iIndex;
        return old;
    });
}

DWORD CListCtrl::GetExtendedStyle() const {
    return static_cast<DWORD>(::SendMessage(m_hWnd, LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0));
}

DWORD CListCtrl::SetExtendedStyle(DWORD dwNewStyle) {
    return static_cast<DWORD>(::SendMessage(m_hWnd, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, static_cast<LPARAM>(dwNewStyle)));
}

CHeaderCtrl* CListCtrl::GetHeaderCtrl() const {
    return m_hWnd ? static_cast<CHeaderCtrl*>(CWnd::FromHandle(m_hWnd)) : nullptr;
}

int CListCtrl::InsertItem(const LVITEM* pItem) {
    return OnMain([&]() -> int {
        wxListCtrl* lc = ListOf(this);
        if (!lc || !pItem || pItem->iSubItem != 0 || pItem->iItem < 0)
            return -1;
        long index = std::min<long>(pItem->iItem, lc->GetItemCount());
        wxListItem li;
        li.SetId(index);
        li.SetText((pItem->mask & LVIF_TEXT) ? ItemText(pItem->pszText) : wxString());
        if (pItem->mask & LVIF_IMAGE)
            li.SetImage(pItem->iImage);
        long r = lc->InsertItem(li);
        if (r < 0)
            return -1;
        if (pItem->mask & LVIF_PARAM)
            lc->SetItemPtrData(r, static_cast<wxUIntPtr>(pItem->lParam));
        if (pItem->mask & LVIF_STATE)
            SetItemStateOne(lc, r, pItem->state, pItem->stateMask);
        return static_cast<int>(r);
    });
}

int CListCtrl::InsertItem(int nItem, const char* lpszItem) {
    return InsertItem(LVIF_TEXT, nItem, lpszItem, 0, 0, 0, 0);
}

int CListCtrl::InsertItem(int nItem, const char* lpszItem, int nImage) {
    return InsertItem(LVIF_TEXT | LVIF_IMAGE, nItem, lpszItem, 0, 0, nImage, 0);
}

int CListCtrl::InsertItem(UINT nMask, int nItem, const char* lpszItem, UINT nState, UINT nStateMask, int nImage,
                          LPARAM lParam) {
    LVITEM lvi;
    memset(&lvi, 0, sizeof lvi);
    lvi.mask = nMask;
    lvi.iItem = nItem;
    lvi.state = nState;
    lvi.stateMask = nStateMask;
    lvi.pszText = const_cast<char*>(lpszItem);
    lvi.iImage = nImage;
    lvi.lParam = lParam;
    return InsertItem(&lvi);
}

BOOL CListCtrl::DeleteItem(int nItem) {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        if (!lc || !ValidItem(lc, nItem))
            return FALSE;
        ListViewExtra& ex = ListExtra(lc);
        if (ex.selectionMark == nItem)
            ex.selectionMark = -1;
        else if (ex.selectionMark > nItem)
            --ex.selectionMark;
        return lc->DeleteItem(nItem);
    });
}

BOOL CListCtrl::DeleteAllItems() {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        if (!lc)
            return FALSE;
        ListExtra(lc).selectionMark = -1;
        return lc->DeleteAllItems();
    });
}

int CListCtrl::FindItem(LVFINDINFO* pFindInfo, int nStart) const {
    return OnMain([&]() -> int {
        wxListCtrl* lc = ListOf(this);
        if (!lc || !pFindInfo)
            return -1;
        int count = lc->GetItemCount();
        wxString needle = (pFindInfo->flags & (LVFI_STRING | LVFI_PARTIAL)) && pFindInfo->psz ? ToWx(pFindInfo->psz)
                                                                                                : wxString();
        auto matches = [&](int i) {
            if (pFindInfo->flags & LVFI_PARAM)
                return static_cast<LPARAM>(lc->GetItemData(i)) == pFindInfo->lParam;
            if (pFindInfo->flags & (LVFI_STRING | LVFI_PARTIAL)) {
                wxString text = lc->GetItemText(i);
                if (pFindInfo->flags & LVFI_PARTIAL)
                    return text.Lower().StartsWith(needle.Lower());
                return text.CmpNoCase(needle) == 0;
            }
            return false;
        };
        for (int i = nStart + 1; i < count; ++i)
            if (i >= 0 && matches(i))
                return i;
        if (pFindInfo->flags & LVFI_WRAP)
            for (int i = 0; i <= std::min(nStart, count - 1); ++i)
                if (matches(i))
                    return i;
        return -1;
    });
}

int CListCtrl::HitTest(LVHITTESTINFO* pHitTestInfo) const {
    return OnMain([&]() -> int {
        wxListCtrl* lc = ListOf(this);
        if (!lc || !pHitTestInfo)
            return -1;
        ListHit hit = HitTestList(lc, wxPoint(pHitTestInfo->pt.x, pHitTestInfo->pt.y));
        pHitTestInfo->flags = hit.flags;
        pHitTestInfo->iItem = static_cast<int>(hit.item);
        return static_cast<int>(hit.item);
    });
}

int CListCtrl::HitTest(CPoint pt, UINT* pFlags) const {
    LVHITTESTINFO info;
    memset(&info, 0, sizeof info);
    info.pt = pt;
    int item = HitTest(&info);
    if (pFlags)
        *pFlags = info.flags;
    return item;
}

int CListCtrl::SubItemHitTest(LVHITTESTINFO* pInfo) {
    return static_cast<int>(::SendMessage(m_hWnd, LVM_SUBITEMHITTEST, 0, reinterpret_cast<LPARAM>(pInfo)));
}

BOOL CListCtrl::EnsureVisible(int nItem, BOOL) {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        return lc && ValidItem(lc, nItem) && lc->EnsureVisible(nItem);
    });
}

BOOL CListCtrl::Scroll(CSize size) {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        return lc && lc->ScrollList(size.cx, size.cy);
    });
}

BOOL CListCtrl::RedrawItems(int nFirst, int nLast) {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        if (!lc)
            return FALSE;
        int count = lc->GetItemCount();
        nFirst = std::max(0, nFirst);
        nLast = std::min(count - 1, nLast);
        if (nFirst <= nLast)
            lc->RefreshItems(nFirst, nLast);
        return TRUE;
    });
}

int CListCtrl::InsertColumn(int nCol, const LVCOLUMN* pColumn) {
    return OnMain([&]() -> int {
        wxListCtrl* lc = ListOf(this);
        if (!lc || !pColumn || nCol < 0)
            return -1;
        long col = std::min(nCol, lc->GetColumnCount());
        wxString heading = (pColumn->mask & LVCF_TEXT) ? ItemText(pColumn->pszText) : wxString();
        wxListColumnFormat fmt = (pColumn->mask & LVCF_FMT) ? AlignFromFormat(pColumn->fmt) : wxLIST_FORMAT_LEFT;
        int width = (pColumn->mask & LVCF_WIDTH) ? pColumn->cx : wxLIST_DEFAULT_COL_WIDTH;
        return static_cast<int>(lc->InsertColumn(col, heading, fmt, width));
    });
}

int CListCtrl::InsertColumn(int nCol, const char* lpszColumnHeading, int nFormat, int nWidth, int nSubItem) {
    LVCOLUMN column;
    memset(&column, 0, sizeof column);
    column.mask = LVCF_TEXT | LVCF_FMT;
    column.pszText = const_cast<char*>(lpszColumnHeading);
    column.fmt = nFormat;
    if (nWidth != -1) {
        column.mask |= LVCF_WIDTH;
        column.cx = nWidth;
    }
    if (nSubItem != -1) {
        column.mask |= LVCF_SUBITEM;
        column.iSubItem = nSubItem;
    }
    return InsertColumn(nCol, &column);
}

BOOL CListCtrl::DeleteColumn(int nCol) {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        return lc && nCol >= 0 && nCol < lc->GetColumnCount() && lc->DeleteColumn(nCol);
    });
}

BOOL CListCtrl::Update(int nItem) {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        if (!lc || !ValidItem(lc, nItem))
            return FALSE;
        lc->RefreshItem(nItem);
        return TRUE;
    });
}

BOOL CListCtrl::SortItems(int (*pfnCompare)(LPARAM, LPARAM, LPARAM), DWORD_PTR dwData) {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        if (!lc || !pfnCompare)
            return FALSE;
        SortContext ctx{pfnCompare, static_cast<LPARAM>(dwData)};
        return lc->SortItems(&SortTrampoline, reinterpret_cast<wxIntPtr>(&ctx));
    });
}

BOOL CListCtrl::GetCheck(int nItem) const {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        return lc && ValidItem(lc, nItem) && lc->HasCheckBoxes() && lc->IsItemChecked(nItem);
    });
}

BOOL CListCtrl::SetCheck(int nItem, BOOL fCheck) {
    return OnMain([&]() -> BOOL {
        wxListCtrl* lc = ListOf(this);
        if (!lc || !ValidItem(lc, nItem) || !lc->HasCheckBoxes())
            return FALSE;
        lc->CheckItem(nItem, fCheck != FALSE);
        return TRUE;
    });
}

void CListCtrl::DrawItem(LPDRAWITEMSTRUCT) {}
