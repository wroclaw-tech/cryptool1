#pragma once
#define __AFXCMN_H__

// MFC common controls. Like the basic controls these classes carry no data and operate on m_hWnd.

#include "afxwin.h"

class CImageList : public CObject {
    DECLARE_DYNCREATE(CImageList)
public:
    CImageList();
    ~CImageList() override;
    HIMAGELIST m_hImageList;
    operator HIMAGELIST() const { return AfxIsNullThis(this) ? nullptr : m_hImageList; }
    BOOL Create(int cx, int cy, UINT nFlags, int nInitial, int nGrow);
    BOOL Create(UINT nBitmapID, int cx, int nGrow, COLORREF crMask);
    BOOL DeleteImageList();
    int Add(CBitmap* pbmImage, CBitmap* pbmMask);
    int Add(CBitmap* pbmImage, COLORREF crMask);
    int Add(HICON hIcon);
    int GetImageCount() const;
    BOOL Draw(CDC* pDC, int nImage, POINT pt, UINT nStyle);
};
#define ILC_MASK 0x0001
#define ILC_COLOR 0x0000
#define ILC_COLOR4 0x0004
#define ILC_COLOR8 0x0008
#define ILC_COLOR16 0x0010
#define ILC_COLOR24 0x0018
#define ILC_COLOR32 0x0020
#define ILD_NORMAL 0x0000
#define ILD_TRANSPARENT 0x0001

class CProgressCtrl : public CWnd {
    DECLARE_DYNAMIC(CProgressCtrl)
public:
    CProgressCtrl() = default;
    BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
    void SetRange(short nLower, short nUpper);
    void SetRange32(int nLower, int nUpper);
    void GetRange(int& nLower, int& nUpper) const;
    int GetPos() const;
    int SetPos(int nPos);
    int OffsetPos(int nPos);
    int SetStep(int nStep);
    int StepIt();
    COLORREF SetBkColor(COLORREF clrNew);
    COLORREF SetBarColor(COLORREF clrBar);
    BOOL SetMarquee(BOOL fMarqueeMode, int nInterval);
};

class CSliderCtrl : public CWnd {
    DECLARE_DYNAMIC(CSliderCtrl)
public:
    CSliderCtrl() = default;
    BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
    int GetLineSize() const;
    int SetLineSize(int nSize);
    int GetPageSize() const;
    int SetPageSize(int nSize);
    int GetRangeMax() const;
    int GetRangeMin() const;
    void GetRange(int& nMin, int& nMax) const;
    void SetRangeMin(int nMin, BOOL bRedraw = FALSE);
    void SetRangeMax(int nMax, BOOL bRedraw = FALSE);
    void SetRange(int nMin, int nMax, BOOL bRedraw = FALSE);
    void GetSelection(int& nMin, int& nMax) const;
    void SetSelection(int nMin, int nMax);
    void GetChannelRect(LPRECT lprc) const;
    void GetThumbRect(LPRECT lprc) const;
    int GetPos() const;
    void SetPos(int nPos);
    UINT GetNumTics() const;
    int GetTic(int nTic) const;
    BOOL SetTic(int nTic);
    void SetTicFreq(int nFreq);
    void ClearSel(BOOL bRedraw = FALSE);
    void ClearTics(BOOL bRedraw = FALSE);
};

struct UDACCEL {
    UINT nSec;
    UINT nInc;
};

class CSpinButtonCtrl : public CWnd {
    DECLARE_DYNAMIC(CSpinButtonCtrl)
public:
    CSpinButtonCtrl() = default;
    BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
    BOOL SetAccel(int nAccel, UDACCEL* pAccel);
    UINT GetAccel(int nAccel, UDACCEL* pAccel) const;
    int SetBase(int nBase);
    UINT GetBase() const;
    CWnd* SetBuddy(CWnd* pWndBuddy);
    CWnd* GetBuddy() const;
    int SetPos(int nPos);
    int GetPos() const;
    int SetPos32(int nPos) { return SetPos(nPos); }
    int GetPos32(BOOL* lpbError = nullptr) const;
    void SetRange(short nLower, short nUpper);
    void SetRange32(int nLower, int nUpper);
    DWORD GetRange() const;
    void GetRange(int& lower, int& upper) const;
    void GetRange32(int& lower, int& upper) const { GetRange(lower, upper); }
};

class CHeaderCtrl : public CWnd {
    DECLARE_DYNAMIC(CHeaderCtrl)
public:
    CHeaderCtrl() = default;
    int GetItemCount() const;
};

class CListCtrl : public CWnd {
    DECLARE_DYNAMIC(CListCtrl)
public:
    CListCtrl() = default;
    BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
    COLORREF GetBkColor() const;
    BOOL SetBkColor(COLORREF cr);
    COLORREF GetTextColor() const;
    BOOL SetTextColor(COLORREF cr);
    COLORREF GetTextBkColor() const;
    BOOL SetTextBkColor(COLORREF cr);
    CImageList* GetImageList(int nImageList) const;
    CImageList* SetImageList(CImageList* pImageList, int nImageListType);
    int GetItemCount() const;
    BOOL GetItem(LVITEM* pItem) const;
    BOOL SetItem(const LVITEM* pItem);
    BOOL SetItem(int nItem, int nSubItem, UINT nMask, const char* lpszItem, int nImage, UINT nState, UINT nStateMask,
                 LPARAM lParam);
    UINT GetCallbackMask() const { return 0; }
    BOOL SetCallbackMask(UINT) { return TRUE; }
    int GetNextItem(int nItem, int nFlags) const;
    POSITION GetFirstSelectedItemPosition() const;
    int GetNextSelectedItem(POSITION& pos) const;
    BOOL GetItemRect(int nItem, LPRECT lpRect, UINT nCode) const;
    BOOL GetSubItemRect(int iItem, int iSubItem, int nArea, CRect& ref);
    BOOL SetItemPosition(int, POINT) { return TRUE; }
    BOOL GetItemPosition(int nItem, LPPOINT lpPoint) const;
    int GetStringWidth(const char* lpsz) const;
    CEdit* GetEditControl() const { return nullptr; }
    BOOL GetColumn(int nCol, LVCOLUMN* pColumn) const;
    BOOL SetColumn(int nCol, const LVCOLUMN* pColumn);
    int GetColumnWidth(int nCol) const;
    BOOL SetColumnWidth(int nCol, int cx);
    BOOL GetViewRect(LPRECT lpRect) const;
    int GetTopIndex() const;
    int GetCountPerPage() const;
    BOOL GetOrigin(LPPOINT lpPoint) const;
    BOOL SetItemState(int nItem, LVITEM* pItem);
    BOOL SetItemState(int nItem, UINT nState, UINT nMask);
    UINT GetItemState(int nItem, UINT nMask) const;
    CString GetItemText(int nItem, int nSubItem) const;
    int GetItemText(int nItem, int nSubItem, char* lpszText, int nLen) const;
    BOOL SetItemText(int nItem, int nSubItem, const char* lpszText);
    void SetItemCount(int nItems);
    BOOL SetItemData(int nItem, DWORD_PTR dwData);
    DWORD_PTR GetItemData(int nItem) const;
    UINT GetSelectedCount() const;
    int GetSelectionMark() const;
    int SetSelectionMark(int iIndex);
    DWORD GetExtendedStyle() const;
    DWORD SetExtendedStyle(DWORD dwNewStyle);
    CHeaderCtrl* GetHeaderCtrl() const;
    int GetHotItem() const { return -1; }
    int InsertItem(const LVITEM* pItem);
    int InsertItem(int nItem, const char* lpszItem);
    int InsertItem(int nItem, const char* lpszItem, int nImage);
    int InsertItem(UINT nMask, int nItem, const char* lpszItem, UINT nState, UINT nStateMask, int nImage,
                   LPARAM lParam);
    BOOL DeleteItem(int nItem);
    BOOL DeleteAllItems();
    int FindItem(LVFINDINFO* pFindInfo, int nStart = -1) const;
    int HitTest(LVHITTESTINFO* pHitTestInfo) const;
    int HitTest(CPoint pt, UINT* pFlags = nullptr) const;
    int SubItemHitTest(LVHITTESTINFO* pInfo);
    BOOL EnsureVisible(int nItem, BOOL bPartialOK);
    BOOL Scroll(CSize size);
    BOOL RedrawItems(int nFirst, int nLast);
    BOOL Arrange(UINT) { return TRUE; }
    CEdit* EditLabel(int) { return nullptr; }
    int InsertColumn(int nCol, const LVCOLUMN* pColumn);
    int InsertColumn(int nCol, const char* lpszColumnHeading, int nFormat = LVCFMT_LEFT, int nWidth = -1,
                     int nSubItem = -1);
    BOOL DeleteColumn(int nCol);
    BOOL Update(int nItem);
    BOOL SortItems(int (*pfnCompare)(LPARAM, LPARAM, LPARAM), DWORD_PTR dwData);
    BOOL GetCheck(int nItem) const;
    BOOL SetCheck(int nItem, BOOL fCheck = TRUE);
    virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
};

class CTreeCtrl : public CWnd {
    DECLARE_DYNAMIC(CTreeCtrl)
public:
    CTreeCtrl() = default;
};

class CTabCtrl : public CWnd {
    DECLARE_DYNAMIC(CTabCtrl)
public:
    CTabCtrl() = default;
    BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
    CImageList* GetImageList() const { return nullptr; }
    CImageList* SetImageList(CImageList*) { return nullptr; }
    int GetItemCount() const;
    BOOL GetItem(int nItem, TCITEM* pTabCtrlItem) const;
    BOOL SetItem(int nItem, TCITEM* pTabCtrlItem);
    BOOL GetItemRect(int nItem, LPRECT lpRect) const;
    int GetCurSel() const;
    int SetCurSel(int nItem);
    void SetCurFocus(int nItem);
    CSize SetItemSize(CSize size);
    void SetPadding(CSize) {}
    int GetRowCount() const { return 1; }
    int GetCurFocus() const { return GetCurSel(); }
    LONG InsertItem(int nItem, TCITEM* pTabCtrlItem);
    LONG InsertItem(int nItem, const char* lpszItem);
    LONG InsertItem(int nItem, const char* lpszItem, int nImage);
    BOOL DeleteItem(int nItem);
    BOOL DeleteAllItems();
    void AdjustRect(BOOL bLarger, LPRECT lpRect);
    DWORD GetExtendedStyle() { return 0; }
    DWORD SetExtendedStyle(DWORD, DWORD = 0) { return 0; }
    int HitTest(void*) const { return -1; }
};

#define LPSTR_TEXTCALLBACK ((LPSTR)-1L)

class CToolTipCtrl : public CWnd {
    DECLARE_DYNAMIC(CToolTipCtrl)
public:
    CToolTipCtrl() = default;
    BOOL Create(CWnd* pParentWnd, DWORD dwStyle = 0);
    BOOL AddTool(CWnd* pWnd, UINT nIDText, LPCRECT lpRectTool = nullptr, UINT_PTR nIDTool = 0);
    BOOL AddTool(CWnd* pWnd, const char* lpszText = LPSTR_TEXTCALLBACK, LPCRECT lpRectTool = nullptr,
                 UINT_PTR nIDTool = 0);
    void DelTool(CWnd* pWnd, UINT_PTR nIDTool = 0);
    void UpdateTipText(const char* lpszText, CWnd* pWnd, UINT_PTR nIDTool = 0);
    void UpdateTipText(UINT nIDText, CWnd* pWnd, UINT_PTR nIDTool = 0);
    void RelayEvent(LPMSG lpMsg);
    void Activate(BOOL bActivate);
    void SetDelayTime(UINT nDelay);
    void SetDelayTime(DWORD dwDuration, int iTime);
    int SetMaxTipWidth(int iWidth);
    void SetTipBkColor(COLORREF) {}
    void SetTipTextColor(COLORREF) {}
    int GetToolCount() const;
    void Pop() {}
    void Update() {}
};
#define TTDT_AUTOMATIC 0
#define TTDT_RESHOW 1
#define TTDT_AUTOPOP 2
#define TTDT_INITIAL 3
#define TTF_IDISHWND 0x0001
#define TTF_CENTERTIP 0x0002
#define TTF_SUBCLASS 0x0010

class CRichEditCtrl : public CWnd {
    DECLARE_DYNAMIC(CRichEditCtrl)
public:
    CRichEditCtrl() = default;
    BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
    BOOL CanUndo() const;
    BOOL CanRedo() const;
    int GetLineCount() const;
    BOOL GetModify() const;
    void SetModify(BOOL bModified = TRUE);
    void GetRect(LPRECT lpRect) const;
    CPoint GetCharPos(long lChar) const;
    void SetOptions(WORD, DWORD) {}
    int GetLine(int nIndex, char* lpszBuffer) const;
    int GetLine(int nIndex, char* lpszBuffer, int nMaxLength) const;
    BOOL CanPaste(UINT nFormat = 0) const;
    void GetSel(long& nStartChar, long& nEndChar) const;
    void GetSel(CHARRANGE& cr) const;
    void LimitText(long nChars = 0);
    long LineFromChar(long nIndex) const;
    void SetSel(long nStartChar, long nEndChar);
    void SetSel(CHARRANGE& cr);
    DWORD GetDefaultCharFormat(CHARFORMAT& cf) const;
    DWORD GetSelectionCharFormat(CHARFORMAT& cf) const;
    long GetEventMask() const { return 0; }
    long GetLimitText() const;
    DWORD GetParaFormat(PARAFORMAT& pf) const;
    long GetSelText(char* lpBuf) const;
    CString GetSelText() const;
    WORD GetSelectionType() const;
    COLORREF SetBackgroundColor(BOOL bSysColor, COLORREF cr);
    BOOL SetDefaultCharFormat(CHARFORMAT& cf);
    BOOL SetSelectionCharFormat(CHARFORMAT& cf);
    BOOL SetWordCharFormat(CHARFORMAT& cf);
    DWORD SetEventMask(DWORD dwEventMask) { return dwEventMask; }
    BOOL SetParaFormat(PARAFORMAT& pf);
    BOOL SetReadOnly(BOOL bReadOnly = TRUE);
    void SetTargetDevice(HDC, long) {}
    long GetTextLength() const;
    long GetTextLengthEx(DWORD, UINT = 1200) const { return GetTextLength(); }
    BOOL SetAutoURLDetect(BOOL = TRUE) { return TRUE; }
    void EmptyUndoBuffer();
    int LineIndex(int nLine = -1) const;
    int LineLength(int nLine = -1) const;
    void LineScroll(int nLines, int nChars = 0);
    void ReplaceSel(const char* lpszNewText, BOOL bCanUndo = FALSE);
    long StreamIn(int nFormat, EDITSTREAM& es);
    long StreamOut(int nFormat, EDITSTREAM& es);
    long FindText(DWORD dwFlags, FINDTEXTEX* pFindText) const;
    BOOL Undo();
    BOOL Redo();
    void Clear();
    void Copy();
    void Cut();
    void Paste();
    int GetFirstVisibleLine() const;
    void HideSelection(BOOL, BOOL) {}
};
