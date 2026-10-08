#pragma once

// MFC GUI subset on top of wxWidgets.
#define __AFXWIN_H__

#include "afx.h"
#include "afxres.h"
#include "mfcwx/msgmap.h"

#include <set>

class wxWindow;
class wxDC;
class wxMenu;
class wxMenuBar;
class wxBitmap;
class wxFrame;

class CCmdTarget;
class CWnd;
class CDC;
class CMenu;
class CCmdUI;
class CDocument;
class CView;
class CFrameWnd;
class CMDIFrameWnd;
class CMDIChildWnd;
class CDocTemplate;
class CWinApp;
class CWinThread;
class CDataExchange;
class CScrollBar;
class CGdiObject;
class CPen;
class CBrush;
class CFont;
class CBitmap;
class CRgn;
class CPalette;
class CImageList;
class CToolTipCtrl;
class CPrintInfo;
class CCreateContext;
class CPrintDialog;
class CStatusBar;
class CToolBar;
class CControlBar;
class CDialog;

namespace mfcwx {
struct WindowState;
struct DCState;
struct GdiObjectImpl;
}

// ---------------------------------------------------------------------------------------------
// Command targets

class CCmdTarget : public CObject {
    DECLARE_DYNAMIC(CCmdTarget)
public:
    CCmdTarget() = default;
    virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
    void BeginWaitCursor();
    void EndWaitCursor();
    void RestoreWaitCursor();
    void EnableAutomation() {}
    virtual void OnFinalRelease() { delete this; }

protected:
    static const AFX_MSGMAP* GetThisMessageMap();
    virtual const AFX_MSGMAP* GetMessageMap() const;

public:
    // Locates the handler for a message map entry search; used by the dispatcher.
    const AFX_MSGMAP_ENTRY* FindMessageEntry(UINT nMessage, UINT nCode, UINT nID) const;
    BOOL DispatchCommand(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
};

class CCmdUI {
public:
    CCmdUI();
    virtual ~CCmdUI() = default;
    virtual void Enable(BOOL bOn = TRUE);
    virtual void SetCheck(int nCheck = 1);
    virtual void SetRadio(BOOL bOn = TRUE);
    virtual void SetText(const char* lpszText);
    void ContinueRouting() { m_bContinueRouting = TRUE; }
    BOOL DoUpdate(CCmdTarget* pTarget, BOOL bDisableIfNoHndler);

    UINT m_nID;
    UINT m_nIndex;
    CMenu* m_pMenu;
    CMenu* m_pSubMenu;
    CMenu* m_pParentMenu;
    CWnd* m_pOther;
    BOOL m_bEnableChanged;
    BOOL m_bContinueRouting;
    UINT m_nIndexMax;

    // results collected for the wx update event
    bool m_enabled;
    int m_check;
    bool m_checkSet;
    bool m_textSet;
    CString m_text;
};

// ---------------------------------------------------------------------------------------------
// Windows

class CWnd : public CCmdTarget {
    DECLARE_DYNCREATE(CWnd)
public:
    CWnd();
    ~CWnd() override;

    HWND m_hWnd;
    HWND GetSafeHwnd() const { return AfxIsNullThis(this) ? nullptr : m_hWnd; }
    operator HWND() const { return AfxIsNullThis(this) ? nullptr : m_hWnd; }
    bool operator==(const CWnd& wnd) const { return m_hWnd == wnd.m_hWnd; }
    bool operator!=(const CWnd& wnd) const { return m_hWnd != wnd.m_hWnd; }

    static CWnd* FromHandle(HWND hWnd);
    static CWnd* FromHandlePermanent(HWND hWnd);
    static void DeleteTempMap();
    BOOL Attach(HWND hWndNew);
    HWND Detach();
    BOOL SubclassWindow(HWND hWnd);
    BOOL SubclassDlgItem(UINT nID, CWnd* pParent);
    HWND UnsubclassWindow();

    // creation
    virtual BOOL Create(const char* lpszClassName, const char* lpszWindowName, DWORD dwStyle, const RECT& rect,
                        CWnd* pParentWnd, UINT nID, CCreateContext* pContext = nullptr);
    virtual BOOL CreateEx(DWORD dwExStyle, const char* lpszClassName, const char* lpszWindowName, DWORD dwStyle,
                          int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU nIDorHMenu,
                          LPVOID lpParam = nullptr);
    virtual BOOL CreateEx(DWORD dwExStyle, const char* lpszClassName, const char* lpszWindowName, DWORD dwStyle,
                          const RECT& rect, CWnd* pParentWnd, UINT nID, LPVOID lpParam = nullptr);
    virtual BOOL DestroyWindow();
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    virtual void PreSubclassWindow() {}
    virtual void PostNcDestroy() {}

    // message handling
    LRESULT SendMessage(UINT message, WPARAM wParam = 0, LPARAM lParam = 0);
    BOOL PostMessage(UINT message, WPARAM wParam = 0, LPARAM lParam = 0);
    BOOL SendNotifyMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void SendMessageToDescendants(UINT message, WPARAM wParam = 0, LPARAM lParam = 0, BOOL bDeep = TRUE,
                                  BOOL bOnlyPerm = FALSE);
    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    virtual BOOL OnWndMsg(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult);
    virtual LRESULT DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT Default();
    virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
    virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
    virtual BOOL OnChildNotify(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult);
    BOOL ReflectChildNotify(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult);
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    static const MSG* GetCurrentMessage();
    BOOL ExecuteDlgInit(const char* lpszResourceName);
    BOOL ExecuteDlgInit(UINT nIDTemplate);

    // state
    DWORD GetStyle() const;
    DWORD GetExStyle() const;
    BOOL ModifyStyle(DWORD dwRemove, DWORD dwAdd, UINT nFlags = 0);
    BOOL ModifyStyleEx(DWORD dwRemove, DWORD dwAdd, UINT nFlags = 0);
    int GetDlgCtrlID() const;
    int SetDlgCtrlID(int nID);
    CWnd* GetParent() const;
    CWnd* SetParent(CWnd* pWndNewParent);
    CWnd* GetOwner() const;
    void SetOwner(CWnd* pOwnerWnd);
    CWnd* GetParentOwner() const;
    CFrameWnd* GetParentFrame() const;
    CFrameWnd* GetTopLevelFrame() const;
    CWnd* GetTopLevelParent() const;
    CWnd* GetTopLevelOwner() const;
    CWnd* GetTopWindow() const;
    CWnd* GetWindow(UINT nCmd) const;
    CWnd* GetNextWindow(UINT nFlag = GW_HWNDNEXT) const;
    CWnd* GetLastActivePopup() const;
    CWnd* GetDescendantWindow(int nID, BOOL bOnlyPerm = FALSE) const;
    CWnd* ChildWindowFromPoint(POINT point) const;
    static CWnd* WindowFromPoint(POINT point);
    static CWnd* GetActiveWindow();
    CWnd* SetActiveWindow();
    static CWnd* GetForegroundWindow();
    BOOL SetForegroundWindow();
    static CWnd* GetFocus();
    CWnd* SetFocus();
    static CWnd* GetDesktopWindow();
    static CWnd* GetCapture();
    CWnd* SetCapture();
    static CWnd* FindWindow(const char* lpszClassName, const char* lpszWindowName);
    BOOL IsChild(const CWnd* pWnd) const;
    BOOL IsWindowEnabled() const;
    BOOL EnableWindow(BOOL bEnable = TRUE);
    BOOL IsWindowVisible() const;
    BOOL ShowWindow(int nCmdShow);
    BOOL IsIconic() const;
    BOOL IsZoomed() const;
    BOOL FlashWindow(BOOL) { return TRUE; }
    void DragAcceptFiles(BOOL bAccept = TRUE);
    BOOL IsDialogMessage(LPMSG) { return FALSE; }

    // text
    void SetWindowText(const char* lpszString);
    void SetWindowTextA(const char* lpszString) { SetWindowText(lpszString); }
    int GetWindowText(char* lpszStringBuf, int nMaxCount) const;
    void GetWindowText(CString& rString) const;
    int GetWindowTextA(char* lpszStringBuf, int nMaxCount) const { return GetWindowText(lpszStringBuf, nMaxCount); }
    void GetWindowTextA(CString& rString) const { GetWindowText(rString); }
    int GetWindowTextLength() const;
    void SetFont(CFont* pFont, BOOL bRedraw = TRUE);
    CFont* GetFont() const;

    // geometry
    void GetWindowRect(LPRECT lpRect) const;
    void GetClientRect(LPRECT lpRect) const;
    void ClientToScreen(LPPOINT lpPoint) const;
    void ClientToScreen(LPRECT lpRect) const;
    void ScreenToClient(LPPOINT lpPoint) const;
    void ScreenToClient(LPRECT lpRect) const;
    void MapWindowPoints(CWnd* pwndTo, LPPOINT lpPoint, UINT nCount) const;
    void MapWindowPoints(CWnd* pwndTo, LPRECT lpRect) const;
    void MoveWindow(int x, int y, int nWidth, int nHeight, BOOL bRepaint = TRUE);
    void MoveWindow(LPCRECT lpRect, BOOL bRepaint = TRUE);
    BOOL SetWindowPos(const CWnd* pWndInsertAfter, int x, int y, int cx, int cy, UINT nFlags);
    BOOL GetWindowPlacement(WINDOWPLACEMENT* lpwndpl) const;
    BOOL SetWindowPlacement(const WINDOWPLACEMENT* lpwndpl);
    void CenterWindow(CWnd* pAlternateOwner = nullptr);
    void BringWindowToTop();
    void CalcWindowRect(LPRECT lpClientRect, UINT nAdjustType = 0);
    void RepositionBars(UINT, UINT, UINT, UINT = 0, LPRECT = nullptr, LPCRECT = nullptr, BOOL = TRUE) {}

    // painting
    CDC* GetDC();
    CDC* GetWindowDC();
    int ReleaseDC(CDC* pDC);
    void Invalidate(BOOL bErase = TRUE);
    void InvalidateRect(LPCRECT lpRect, BOOL bErase = TRUE);
    void InvalidateRgn(CRgn* pRgn, BOOL bErase = TRUE);
    void ValidateRect(LPCRECT) {}
    void UpdateWindow();
    BOOL RedrawWindow(LPCRECT lpRectUpdate = nullptr, CRgn* prgnUpdate = nullptr, UINT flags = 0);
    void SetRedraw(BOOL bRedraw = TRUE);
    BOOL GetUpdateRect(LPRECT lpRect, BOOL bErase = FALSE);
    CDC* BeginPaint(LPPAINTSTRUCT lpPaint);
    void EndPaint(LPPAINTSTRUCT lpPaint);
    void ScrollWindow(int xAmount, int yAmount, LPCRECT lpRect = nullptr, LPCRECT lpClipRect = nullptr);
    void ShowCaret() {}
    void HideCaret() {}
    BOOL CreateCaret(CBitmap*) { return TRUE; }
    void CreateSolidCaret(int, int) {}
    void SetCaretPos(POINT) {}
    static CPoint GetCaretPos();

    // scrolling
    int GetScrollPos(int nBar) const;
    int SetScrollPos(int nBar, int nPos, BOOL bRedraw = TRUE);
    void GetScrollRange(int nBar, LPINT lpMinPos, LPINT lpMaxPos) const;
    void SetScrollRange(int nBar, int nMinPos, int nMaxPos, BOOL bRedraw = TRUE);
    BOOL SetScrollInfo(int nBar, LPSCROLLINFO lpScrollInfo, BOOL bRedraw = TRUE);
    BOOL GetScrollInfo(int nBar, LPSCROLLINFO lpScrollInfo, UINT nMask = SIF_ALL);
    int GetScrollLimit(int nBar);
    void ShowScrollBar(UINT nBar, BOOL bShow = TRUE);
    BOOL EnableScrollBar(int nSBFlags, UINT nArrowFlags = ESB_ENABLE_BOTH);
    virtual CScrollBar* GetScrollBarCtrl(int nBar) const;

    // timers
    UINT_PTR SetTimer(UINT_PTR nIDEvent, UINT nElapse, void (*lpfnTimer)(HWND, UINT, UINT_PTR, DWORD));
    BOOL KillTimer(UINT_PTR nIDEvent);

    // dialog items
    CWnd* GetDlgItem(int nID) const;
    void GetDlgItem(int nID, HWND* phWnd) const;
    UINT GetDlgItemInt(int nID, BOOL* lpTrans = nullptr, BOOL bSigned = TRUE) const;
    void SetDlgItemInt(int nID, UINT nValue, BOOL bSigned = TRUE);
    int GetDlgItemText(int nID, char* lpStr, int nMaxCount) const;
    int GetDlgItemText(int nID, CString& rString) const;
    void SetDlgItemText(int nID, const char* lpszString);
    UINT IsDlgButtonChecked(int nIDButton) const;
    void CheckDlgButton(int nIDButton, UINT nCheck);
    void CheckRadioButton(int nIDFirstButton, int nIDLastButton, int nIDCheckButton);
    int GetCheckedRadioButton(int nIDFirstButton, int nIDLastButton);
    LRESULT SendDlgItemMessage(int nID, UINT message, WPARAM wParam = 0, LPARAM lParam = 0);
    CWnd* GetNextDlgGroupItem(CWnd* pWndCtl, BOOL bPrevious = FALSE) const;
    CWnd* GetNextDlgTabItem(CWnd* pWndCtl, BOOL bPrevious = FALSE) const;
    BOOL UpdateData(BOOL bSaveAndValidate = TRUE);
    virtual void DoDataExchange(CDataExchange* pDX);

    // menus
    CMenu* GetMenu() const;
    BOOL SetMenu(CMenu* pMenu);
    void DrawMenuBar();
    CMenu* GetSystemMenu(BOOL bRevert) const;

    // help, icons, misc
    virtual void WinHelp(DWORD_PTR dwData, UINT nCmd = HELP_CONTEXT);
    virtual void HtmlHelp(DWORD_PTR dwData, UINT nCmd = 0x000F);
    HICON SetIcon(HICON hIcon, BOOL bBigIcon);
    HICON GetIcon(BOOL bBigIcon) const;
    BOOL EnableToolTips(BOOL bEnable = TRUE);
    void SetWindowContextHelpId(DWORD) {}
    int MessageBox(const char* lpszText, const char* lpszCaption = nullptr, UINT nType = MB_OK);
    int MessageBoxA(const char* lpszText, const char* lpszCaption = nullptr, UINT nType = MB_OK) { return MessageBox(lpszText, lpszCaption, nType); }
    BOOL OpenClipboard();
    BOOL LockWindowUpdate();
    void UnlockWindowUpdate();
    LONG_PTR GetWindowLongPtr(int nIndex) const;
    LONG_PTR SetWindowLongPtr(int nIndex, LONG_PTR dwNewLong);

    // default handlers
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnDestroy();
    afx_msg void OnNcDestroy();
    afx_msg void OnClose();
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnMove(int x, int y);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnSysKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnSysChar(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnNcLButtonDown(UINT nHitTest, CPoint point);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
    afx_msg void OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnKillFocus(CWnd* pNewWnd);
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint pos);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg HCURSOR OnQueryDragIcon();
    afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
    afx_msg UINT OnGetDlgCode();
    afx_msg void OnDropFiles(HDROP hDropInfo);
    afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
    afx_msg void OnMDIActivate(BOOL bActivate, CWnd* pActivateWnd, CWnd* pDeactivateWnd);
    afx_msg void OnEnable(BOOL bEnable);
    afx_msg BOOL OnNcActivate(BOOL bActive);
    afx_msg void OnMenuSelect(UINT nItemID, UINT nFlags, HMENU hSysMenu);
    afx_msg void OnSizing(UINT fwSide, LPRECT pRect);

    // implementation
    wxWindow* GetWx() const { return reinterpret_cast<wxWindow*>(m_hWnd); }
    static CWnd* FromWx(wxWindow* window);
    void AttachWx(wxWindow* window);

protected:
    DECLARE_MESSAGE_MAP()
};

extern const CWnd wndTop;
extern const CWnd wndBottom;
extern const CWnd wndTopMost;
extern const CWnd wndNoTopMost;

// ---------------------------------------------------------------------------------------------
// GDI

class CGdiObject : public CObject {
    DECLARE_DYNCREATE(CGdiObject)
public:
    CGdiObject();
    ~CGdiObject() override;
    HGDIOBJ m_hObject;
    operator HGDIOBJ() const { return AfxIsNullThis(this) ? nullptr : m_hObject; }
    HGDIOBJ GetSafeHandle() const { return AfxIsNullThis(this) ? nullptr : m_hObject; }
    static CGdiObject* FromHandle(HGDIOBJ hObject);
    BOOL Attach(HGDIOBJ hObject);
    HGDIOBJ Detach();
    BOOL DeleteObject();
    int GetObject(int nCount, LPVOID lpObject) const;
    BOOL CreateStockObject(int nIndex);
    BOOL UnrealizeObject() { return TRUE; }
};

class CPen : public CGdiObject {
    DECLARE_DYNAMIC(CPen)
public:
    CPen() = default;
    CPen(int nPenStyle, int nWidth, COLORREF crColor);
    CPen(int nPenStyle, int nWidth, const LOGBRUSH* pLogBrush, int nStyleCount = 0, const DWORD* lpStyle = nullptr);
    BOOL CreatePen(int nPenStyle, int nWidth, COLORREF crColor);
    BOOL CreatePen(int nPenStyle, int nWidth, const LOGBRUSH* pLogBrush, int nStyleCount = 0,
                   const DWORD* lpStyle = nullptr);
    BOOL CreatePenIndirect(LPLOGPEN lpLogPen);
    int GetLogPen(LOGPEN* pLogPen);
    static CPen* FromHandle(HPEN hPen) { return static_cast<CPen*>(CGdiObject::FromHandle(hPen)); }
    operator HPEN() const { return m_hObject; }
};

class CBrush : public CGdiObject {
    DECLARE_DYNAMIC(CBrush)
public:
    CBrush() = default;
    explicit CBrush(COLORREF crColor);
    CBrush(int nIndex, COLORREF crColor);
    explicit CBrush(CBitmap* pBitmap);
    BOOL CreateSolidBrush(COLORREF crColor);
    BOOL CreateHatchBrush(int nIndex, COLORREF crColor);
    BOOL CreatePatternBrush(CBitmap* pBitmap);
    BOOL CreateSysColorBrush(int nIndex);
    BOOL CreateBrushIndirect(const LOGBRUSH* lpLogBrush);
    int GetLogBrush(LOGBRUSH* pLogBrush);
    static CBrush* FromHandle(HBRUSH hBrush) { return static_cast<CBrush*>(CGdiObject::FromHandle(hBrush)); }
    operator HBRUSH() const { return m_hObject; }
};

class CFont : public CGdiObject {
    DECLARE_DYNAMIC(CFont)
public:
    CFont() = default;
    BOOL CreateFontIndirect(const LOGFONT* lpLogFont);
    BOOL CreateFont(int nHeight, int nWidth, int nEscapement, int nOrientation, int nWeight, BYTE bItalic,
                    BYTE bUnderline, BYTE cStrikeOut, BYTE nCharSet, BYTE nOutPrecision, BYTE nClipPrecision,
                    BYTE nQuality, BYTE nPitchAndFamily, const char* lpszFacename);
    BOOL CreateFontA(int nHeight, int nWidth, int nEscapement, int nOrientation, int nWeight, BYTE bItalic,
                     BYTE bUnderline, BYTE cStrikeOut, BYTE nCharSet, BYTE nOutPrecision, BYTE nClipPrecision,
                     BYTE nQuality, BYTE nPitchAndFamily, const char* lpszFacename) {
        return CreateFont(nHeight, nWidth, nEscapement, nOrientation, nWeight, bItalic, bUnderline, cStrikeOut,
                          nCharSet, nOutPrecision, nClipPrecision, nQuality, nPitchAndFamily, lpszFacename);
    }
    BOOL CreatePointFont(int nPointSize, const char* lpszFaceName, CDC* pDC = nullptr);
    BOOL CreatePointFontIndirect(const LOGFONT* lpLogFont, CDC* pDC = nullptr);
    int GetLogFont(LOGFONT* pLogFont);
    static CFont* FromHandle(HFONT hFont) { return static_cast<CFont*>(CGdiObject::FromHandle(hFont)); }
    operator HFONT() const { return m_hObject; }
};

class CBitmap : public CGdiObject {
    DECLARE_DYNAMIC(CBitmap)
public:
    CBitmap() = default;
    BOOL LoadBitmap(const char* lpszResourceName);
    BOOL LoadBitmap(UINT nIDResource);
    BOOL LoadBitmapA(const char* lpszResourceName) { return LoadBitmap(lpszResourceName); }
    BOOL LoadBitmapA(UINT nIDResource) { return LoadBitmap(nIDResource); }
    BOOL LoadOEMBitmap(UINT nIDBitmap);
    BOOL LoadMappedBitmap(UINT nIDBitmap, UINT nFlags = 0, void* lpColorMap = nullptr, int nMapSize = 0);
    BOOL CreateBitmap(int nWidth, int nHeight, UINT nPlanes, UINT nBitcount, const void* lpBits);
    BOOL CreateBitmapIndirect(LPBITMAP lpBitmap);
    BOOL CreateCompatibleBitmap(CDC* pDC, int nWidth, int nHeight);
    BOOL CreateDiscardableBitmap(CDC* pDC, int nWidth, int nHeight);
    int GetBitmap(BITMAP* pBitMap);
    DWORD SetBitmapBits(DWORD dwCount, const void* lpBits);
    DWORD GetBitmapBits(DWORD dwCount, LPVOID lpBits) const;
    CSize SetBitmapDimension(int nWidth, int nHeight);
    CSize GetBitmapDimension() const;
    static CBitmap* FromHandle(HBITMAP hBitmap) { return static_cast<CBitmap*>(CGdiObject::FromHandle(hBitmap)); }
    operator HBITMAP() const { return m_hObject; }
};

class CRgn : public CGdiObject {
    DECLARE_DYNAMIC(CRgn)
public:
    CRgn() = default;
    BOOL CreateRectRgn(int x1, int y1, int x2, int y2);
    BOOL CreateRectRgnIndirect(LPCRECT lpRect);
    BOOL CreateEllipticRgn(int x1, int y1, int x2, int y2);
    BOOL CreateEllipticRgnIndirect(LPCRECT lpRect);
    BOOL CreatePolygonRgn(LPPOINT lpPoints, int nCount, int nMode);
    BOOL CreateRoundRectRgn(int x1, int y1, int x2, int y2, int x3, int y3);
    int CombineRgn(CRgn* pRgn1, CRgn* pRgn2, int nCombineMode);
    int CopyRgn(CRgn* pRgnSrc);
    BOOL PtInRegion(int x, int y) const;
    BOOL PtInRegion(POINT point) const { return PtInRegion(point.x, point.y); }
    BOOL RectInRegion(LPCRECT lpRect) const;
    int GetRgnBox(LPRECT lpRect) const;
    void SetRectRgn(int x1, int y1, int x2, int y2);
    int OffsetRgn(int x, int y);
    static CRgn* FromHandle(HRGN hRgn) { return static_cast<CRgn*>(CGdiObject::FromHandle(hRgn)); }
    operator HRGN() const { return m_hObject; }
};

typedef struct tagLOGPALETTE* LPLOGPALETTE;
class CPalette : public CGdiObject {
    DECLARE_DYNAMIC(CPalette)
public:
    CPalette() = default;
    BOOL CreatePalette(LPLOGPALETTE) { return TRUE; }
    BOOL CreateHalftonePalette(CDC*) { return TRUE; }
    operator HPALETTE() const { return m_hObject; }
};

class CDC : public CObject {
    DECLARE_DYNCREATE(CDC)
public:
    CDC();
    ~CDC() override;

    HDC m_hDC;
    HDC m_hAttribDC;
    operator HDC() const { return AfxIsNullThis(this) ? nullptr : m_hDC; }
    HDC GetSafeHdc() const { return AfxIsNullThis(this) ? nullptr : m_hDC; }
    static CDC* FromHandle(HDC hDC);
    static void DeleteTempMap();
    BOOL Attach(HDC hDC);
    HDC Detach();
    BOOL CreateCompatibleDC(CDC* pDC);
    BOOL CreateDC(const char*, const char*, const char*, const void*) { return FALSE; }
    BOOL CreateIC(const char*, const char*, const char*, const void*) { return FALSE; }
    BOOL DeleteDC();
    CWnd* GetWindow() const;
    BOOL IsPrinting() const { return m_bPrinting; }
    BOOL m_bPrinting;

    int SaveDC();
    BOOL RestoreDC(int nSavedDC);
    CGdiObject* SelectStockObject(int nIndex);
    CPen* SelectObject(CPen* pPen);
    CBrush* SelectObject(CBrush* pBrush);
    CFont* SelectObject(CFont* pFont);
    CBitmap* SelectObject(CBitmap* pBitmap);
    int SelectObject(CRgn* pRgn);
    CGdiObject* SelectObject(CGdiObject* pObject);
    HGDIOBJ SelectObject(HGDIOBJ hObject);
    CPalette* SelectPalette(CPalette* pPalette, BOOL bForceBackground);
    UINT RealizePalette() { return 0; }
    CPen* GetCurrentPen() const;
    CBrush* GetCurrentBrush() const;
    CFont* GetCurrentFont() const;
    CBitmap* GetCurrentBitmap() const;
    int GetDeviceCaps(int nIndex) const;

    COLORREF GetBkColor() const;
    COLORREF SetBkColor(COLORREF crColor);
    int GetBkMode() const;
    int SetBkMode(int nBkMode);
    COLORREF GetTextColor() const;
    COLORREF SetTextColor(COLORREF crColor);
    int GetROP2() const;
    int SetROP2(int nDrawMode);
    int SetStretchBltMode(int nStretchMode);
    int GetStretchBltMode() const;
    UINT SetTextAlign(UINT nFlags);
    UINT GetTextAlign() const;
    int SetTextCharacterExtra(int) { return 0; }
    int SetMapMode(int nMapMode);
    int GetMapMode() const;
    CPoint SetViewportOrg(int x, int y);
    CPoint SetViewportOrg(POINT point) { return SetViewportOrg(point.x, point.y); }
    CPoint GetViewportOrg() const;
    CPoint OffsetViewportOrg(int nWidth, int nHeight);
    CSize SetViewportExt(int cx, int cy);
    CSize SetViewportExt(SIZE size) { return SetViewportExt(size.cx, size.cy); }
    CSize GetViewportExt() const;
    CPoint SetWindowOrg(int x, int y);
    CPoint SetWindowOrg(POINT point) { return SetWindowOrg(point.x, point.y); }
    CPoint GetWindowOrg() const;
    CPoint OffsetWindowOrg(int nWidth, int nHeight);
    CSize SetWindowExt(int cx, int cy);
    CSize SetWindowExt(SIZE size) { return SetWindowExt(size.cx, size.cy); }
    CSize GetWindowExt() const;
    void DPtoLP(LPPOINT lpPoints, int nCount = 1) const;
    void DPtoLP(LPRECT lpRect) const;
    void DPtoLP(LPSIZE lpSize) const;
    void LPtoDP(LPPOINT lpPoints, int nCount = 1) const;
    void LPtoDP(LPRECT lpRect) const;
    void LPtoDP(LPSIZE lpSize) const;

    int GetClipBox(LPRECT lpRect) const;
    int SelectClipRgn(CRgn* pRgn);
    int IntersectClipRect(int x1, int y1, int x2, int y2);
    int IntersectClipRect(LPCRECT lpRect);
    int ExcludeClipRect(int, int, int, int) { return SIMPLEREGION; }
    int ExcludeClipRect(LPCRECT) { return SIMPLEREGION; }
    BOOL PtVisible(int, int) const { return TRUE; }
    BOOL RectVisible(LPCRECT) const { return TRUE; }

    CPoint GetCurrentPosition() const;
    CPoint MoveTo(int x, int y);
    CPoint MoveTo(POINT point) { return MoveTo(point.x, point.y); }
    BOOL LineTo(int x, int y);
    BOOL LineTo(POINT point) { return LineTo(point.x, point.y); }
    BOOL Arc(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4);
    BOOL Arc(LPCRECT lpRect, POINT ptStart, POINT ptEnd);
    BOOL Polyline(const POINT* lpPoints, int nCount);
    BOOL PolyBezier(const POINT* lpPoints, int nCount);
    BOOL PolylineTo(const POINT* lpPoints, int nCount);
    void FillRect(LPCRECT lpRect, CBrush* pBrush);
    void FrameRect(LPCRECT lpRect, CBrush* pBrush);
    void InvertRect(LPCRECT lpRect);
    BOOL DrawIcon(int x, int y, HICON hIcon);
    BOOL DrawIcon(POINT point, HICON hIcon) { return DrawIcon(point.x, point.y, hIcon); }
    BOOL DrawEdge(LPRECT lpRect, UINT nEdge, UINT nFlags);
    BOOL DrawFrameControl(LPRECT lpRect, UINT nType, UINT nState);
    void DrawFocusRect(LPCRECT lpRect);
    BOOL Chord(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4);
    BOOL Ellipse(int x1, int y1, int x2, int y2);
    BOOL Ellipse(LPCRECT lpRect) { return Ellipse(lpRect->left, lpRect->top, lpRect->right, lpRect->bottom); }
    BOOL Pie(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4);
    BOOL Pie(LPCRECT lpRect, POINT ptStart, POINT ptEnd);
    BOOL Polygon(const POINT* lpPoints, int nCount);
    BOOL PolyPolygon(const POINT* lpPoints, const INT* lpPolyCounts, int nCount);
    BOOL Rectangle(int x1, int y1, int x2, int y2);
    BOOL Rectangle(LPCRECT lpRect) { return Rectangle(lpRect->left, lpRect->top, lpRect->right, lpRect->bottom); }
    BOOL RoundRect(int x1, int y1, int x2, int y2, int x3, int y3);
    BOOL RoundRect(LPCRECT lpRect, POINT point);
    void FillSolidRect(LPCRECT lpRect, COLORREF clr);
    void FillSolidRect(int x, int y, int cx, int cy, COLORREF clr);
    void Draw3dRect(LPCRECT lpRect, COLORREF clrTopLeft, COLORREF clrBottomRight);
    void Draw3dRect(int x, int y, int cx, int cy, COLORREF clrTopLeft, COLORREF clrBottomRight);
    BOOL PatBlt(int x, int y, int nWidth, int nHeight, DWORD dwRop);
    BOOL BitBlt(int x, int y, int nWidth, int nHeight, CDC* pSrcDC, int xSrc, int ySrc, DWORD dwRop);
    BOOL StretchBlt(int x, int y, int nWidth, int nHeight, CDC* pSrcDC, int xSrc, int ySrc, int nSrcWidth,
                    int nSrcHeight, DWORD dwRop);
    BOOL TransparentBlt(int xDest, int yDest, int nDestWidth, int nDestHeight, CDC* pSrcDC, int xSrc, int ySrc,
                        int nSrcWidth, int nSrcHeight, UINT clrTransparent);
    COLORREF GetPixel(int x, int y) const;
    COLORREF GetPixel(POINT point) const { return GetPixel(point.x, point.y); }
    COLORREF SetPixel(int x, int y, COLORREF crColor);
    COLORREF SetPixel(POINT point, COLORREF crColor) { return SetPixel(point.x, point.y, crColor); }
    BOOL SetPixelV(int x, int y, COLORREF crColor) { SetPixel(x, y, crColor); return TRUE; }
    BOOL FloodFill(int x, int y, COLORREF crColor);
    BOOL ExtFloodFill(int x, int y, COLORREF crColor, UINT nFillType);
    BOOL FillRgn(CRgn* pRgn, CBrush* pBrush);
    BOOL FrameRgn(CRgn* pRgn, CBrush* pBrush, int nWidth, int nHeight);
    BOOL PaintRgn(CRgn* pRgn);

    BOOL TextOut(int x, int y, const char* lpszString, int nCount);
    BOOL TextOut(int x, int y, const CString& str);
    BOOL ExtTextOut(int x, int y, UINT nOptions, LPCRECT lpRect, const char* lpszString, UINT nCount,
                    LPINT lpDxWidths);
    BOOL ExtTextOut(int x, int y, UINT nOptions, LPCRECT lpRect, const CString& str, LPINT lpDxWidths);
    CSize TabbedTextOut(int x, int y, const char* lpszString, int nCount, int nTabPositions, LPINT lpnTabStopPositions,
                        int nTabOrigin);
    int DrawText(const char* lpszString, int nCount, LPRECT lpRect, UINT nFormat);
    int DrawText(const CString& str, LPRECT lpRect, UINT nFormat);
    CSize GetTextExtent(const char* lpszString, int nCount) const;
    CSize GetTextExtent(const CString& str) const;
    CSize GetOutputTextExtent(const char* lpszString, int nCount) const { return GetTextExtent(lpszString, nCount); }
    CSize GetOutputTextExtent(const CString& str) const { return GetTextExtent(str); }
    CSize GetTabbedTextExtent(const char* lpszString, int nCount, int nTabPositions, LPINT lpnTabStopPositions) const;
    BOOL GetTextExtentExPoint(const char* lpszString, int nCount, int nMaxExtent, LPINT lpnFit, LPINT alpDx,
                              LPSIZE lpSize) const;
    BOOL GetTextMetrics(LPTEXTMETRIC lpMetrics) const;
    BOOL GetOutputTextMetrics(LPTEXTMETRIC lpMetrics) const { return GetTextMetrics(lpMetrics); }
    int GetTextFace(int nCount, char* lpszFacename) const;
    int GetTextFace(CString& rString) const;
    BOOL GetCharWidth(UINT nFirstChar, UINT nLastChar, LPINT lpBuffer) const;
    int SetTextJustification(int, int) { return 1; }

    int StartDoc(const char*) { return 0; }
    int EndDoc() { return 0; }
    int StartPage() { return 0; }
    int EndPage() { return 0; }
    int AbortDoc() { return 0; }

    // implementation: m_hDC points to an mfcwx::DCState owned by this CDC (or by the creator of a
    // temporary wrapper); GetWx() returns the wx device context it draws on.
    wxDC* GetWx() const;
};

class CPaintDC : public CDC {
    DECLARE_DYNAMIC(CPaintDC)
public:
    explicit CPaintDC(CWnd* pWnd);
    ~CPaintDC() override;
    PAINTSTRUCT m_ps;

protected:
    HWND m_hWnd;
};

class CClientDC : public CDC {
    DECLARE_DYNAMIC(CClientDC)
public:
    explicit CClientDC(CWnd* pWnd);
    ~CClientDC() override;

protected:
    HWND m_hWnd;
};

class CWindowDC : public CDC {
    DECLARE_DYNAMIC(CWindowDC)
public:
    explicit CWindowDC(CWnd* pWnd);
    ~CWindowDC() override;

protected:
    HWND m_hWnd;
};

// ---------------------------------------------------------------------------------------------
// Menus

class CMenu : public CObject {
    DECLARE_DYNCREATE(CMenu)
public:
    CMenu();
    ~CMenu() override;
    HMENU m_hMenu;
    operator HMENU() const { return AfxIsNullThis(this) ? nullptr : m_hMenu; }
    HMENU GetSafeHmenu() const { return AfxIsNullThis(this) ? nullptr : m_hMenu; }
    static CMenu* FromHandle(HMENU hMenu);
    BOOL Attach(HMENU hMenu);
    HMENU Detach();
    BOOL CreateMenu();
    BOOL CreatePopupMenu();
    BOOL LoadMenu(const char* lpszResourceName);
    BOOL LoadMenu(UINT nIDResource);
    BOOL DestroyMenu();
    BOOL DeleteMenu(UINT nPosition, UINT nFlags);
    BOOL RemoveMenu(UINT nPosition, UINT nFlags);
    BOOL AppendMenu(UINT nFlags, UINT_PTR nIDNewItem = 0, const char* lpszNewItem = nullptr);
    BOOL InsertMenu(UINT nPosition, UINT nFlags, UINT_PTR nIDNewItem = 0, const char* lpszNewItem = nullptr);
    BOOL ModifyMenu(UINT nPosition, UINT nFlags, UINT_PTR nIDNewItem = 0, const char* lpszNewItem = nullptr);
    UINT CheckMenuItem(UINT nIDCheckItem, UINT nCheck);
    BOOL CheckMenuRadioItem(UINT nIDFirst, UINT nIDLast, UINT nIDItem, UINT nFlags);
    UINT EnableMenuItem(UINT nIDEnableItem, UINT nEnable);
    UINT GetMenuItemCount() const;
    UINT GetMenuItemID(int nPos) const;
    UINT GetMenuState(UINT nID, UINT nFlags) const;
    int GetMenuString(UINT nIDItem, char* lpString, int nMaxCount, UINT nFlags) const;
    int GetMenuString(UINT nIDItem, CString& rString, UINT nFlags) const;
    CMenu* GetSubMenu(int nPos) const;
    BOOL SetDefaultItem(UINT, BOOL = FALSE) { return TRUE; }
    BOOL TrackPopupMenu(UINT nFlags, int x, int y, CWnd* pWnd, LPCRECT lpRect = nullptr);
    BOOL SetMenuItemBitmaps(UINT, UINT, const CBitmap*, const CBitmap*) { return TRUE; }

    // implementation
    wxMenu* GetWxMenu() const;
    wxMenuBar* GetWxMenuBar() const;
};

// ---------------------------------------------------------------------------------------------
// Controls (no data members: methods operate on m_hWnd so temporary CWnd wrappers can be cast)

class CStatic : public CWnd {
    DECLARE_DYNAMIC(CStatic)
public:
    CStatic() = default;
    BOOL Create(const char* lpszText, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID = 0xffff);
    HICON SetIcon(HICON hIcon);
    HICON GetIcon() const;
    HBITMAP SetBitmap(HBITMAP hBitmap);
    HBITMAP GetBitmap() const;
    HCURSOR SetCursor(HCURSOR hCursor);
    HCURSOR GetCursor();
    virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
    BOOL OnChildNotify(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult) override;
};

class CButton : public CWnd {
    DECLARE_DYNAMIC(CButton)
public:
    CButton() = default;
    BOOL Create(const char* lpszCaption, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
    UINT GetState() const;
    void SetState(BOOL bHighlight);
    int GetCheck() const;
    void SetCheck(int nCheck);
    UINT GetButtonStyle() const;
    void SetButtonStyle(UINT nStyle, BOOL bRedraw = TRUE);
    HICON SetIcon(HICON hIcon);
    HICON GetIcon() const;
    HBITMAP SetBitmap(HBITMAP hBitmap);
    HBITMAP GetBitmap() const;
    HCURSOR SetCursor(HCURSOR hCursor);
    HCURSOR GetCursor();
    virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
    BOOL OnChildNotify(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult) override;
};

class CEdit : public CWnd {
    DECLARE_DYNAMIC(CEdit)
public:
    CEdit() = default;
    BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
    BOOL CanUndo() const;
    int GetLineCount() const;
    BOOL GetModify() const;
    void SetModify(BOOL bModified = TRUE);
    void GetRect(LPRECT lpRect) const;
    DWORD GetSel() const;
    void GetSel(int& nStartChar, int& nEndChar) const;
    int GetLine(int nIndex, char* lpszBuffer) const;
    int GetLine(int nIndex, char* lpszBuffer, int nMaxLength) const;
    void EmptyUndoBuffer();
    BOOL FmtLines(BOOL) { return TRUE; }
    void LimitText(int nChars = 0);
    int LineFromChar(int nIndex = -1) const;
    int LineIndex(int nLine = -1) const;
    int LineLength(int nLine = -1) const;
    void LineScroll(int nLines, int nChars = 0);
    void ReplaceSel(const char* lpszNewText, BOOL bCanUndo = FALSE);
    void SetPasswordChar(char ch);
    void SetRect(LPCRECT) {}
    void SetRectNP(LPCRECT) {}
    void SetSel(DWORD dwSelection, BOOL bNoScroll = FALSE);
    void SetSel(int nStartChar, int nEndChar, BOOL bNoScroll = FALSE);
    BOOL SetTabStops(int nTabStops, LPINT rgTabStops);
    void SetTabStops() {}
    BOOL SetTabStops(const int&) { return TRUE; }
    BOOL Undo();
    void Clear();
    void Copy();
    void Cut();
    void Paste();
    BOOL SetReadOnly(BOOL bReadOnly = TRUE);
    int GetFirstVisibleLine() const;
    char GetPasswordChar() const;
    void SetMargins(UINT, UINT) {}
    DWORD GetMargins() const { return 0; }
    void SetLimitText(UINT nMax);
    UINT GetLimitText() const;
    CPoint PosFromChar(UINT nChar) const;
    int CharFromPos(CPoint pt) const;
};

class CListBox : public CWnd {
    DECLARE_DYNAMIC(CListBox)
public:
    CListBox() = default;
    BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
    int GetCount() const;
    int GetHorizontalExtent() const { return 0; }
    void SetHorizontalExtent(int) {}
    int GetTopIndex() const;
    int SetTopIndex(int nIndex);
    DWORD_PTR GetItemData(int nIndex) const;
    int SetItemData(int nIndex, DWORD_PTR dwItemData);
    void* GetItemDataPtr(int nIndex) const { return reinterpret_cast<void*>(GetItemData(nIndex)); }
    int SetItemDataPtr(int nIndex, void* pData) { return SetItemData(nIndex, reinterpret_cast<DWORD_PTR>(pData)); }
    int GetItemRect(int nIndex, LPRECT lpRect) const;
    int GetSel(int nIndex) const;
    int GetText(int nIndex, char* lpszBuffer) const;
    void GetText(int nIndex, CString& rString) const;
    int GetTextLen(int nIndex) const;
    void SetColumnWidth(int) {}
    BOOL SetTabStops(int, LPINT) { return TRUE; }
    void SetTabStops() {}
    BOOL SetTabStops(const int&) { return TRUE; }
    int SetItemHeight(int, UINT) { return 0; }
    int GetItemHeight(int) const { return 16; }
    int FindStringExact(int nIndexStart, const char* lpszFind) const;
    int GetCaretIndex() const;
    int SetCaretIndex(int nIndex, BOOL bScroll = TRUE);
    int GetCurSel() const;
    int SetCurSel(int nSelect);
    int SetSel(int nIndex, BOOL bSelect = TRUE);
    int GetSelCount() const;
    int GetSelItems(int nMaxItems, LPINT rgIndex) const;
    int SelItemRange(BOOL bSelect, int nFirstItem, int nLastItem);
    int AddString(const char* lpszItem);
    int DeleteString(UINT nIndex);
    int InsertString(int nIndex, const char* lpszItem);
    void ResetContent();
    int Dir(UINT, const char*) { return LB_ERR; }
    int FindString(int nStartAfter, const char* lpszItem) const;
    int SelectString(int nStartAfter, const char* lpszItem);
    int GetAnchorIndex() const { return GetCaretIndex(); }
    void SetAnchorIndex(int) {}
    UINT ItemFromPoint(CPoint pt, BOOL& bOutside) const;
};

#define CLBN_CHKCHANGE 40

class CCheckListBox : public CListBox {
    DECLARE_DYNAMIC(CCheckListBox)
public:
    CCheckListBox() = default;
    void SetCheckStyle(UINT) {}
    UINT GetCheckStyle() { return BS_AUTOCHECKBOX; }
    void SetCheck(int nIndex, int nCheck);
    int GetCheck(int nIndex);
    void Enable(int nIndex, BOOL bEnabled = TRUE);
    BOOL IsEnabled(int nIndex);
};

class CComboBox : public CWnd {
    DECLARE_DYNAMIC(CComboBox)
public:
    CComboBox() = default;
    BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
    int GetCount() const;
    int GetCurSel() const;
    int SetCurSel(int nSelect);
    DWORD GetEditSel() const;
    BOOL LimitText(int nMaxChars);
    BOOL SetEditSel(int nStartChar, int nEndChar);
    DWORD_PTR GetItemData(int nIndex) const;
    int SetItemData(int nIndex, DWORD_PTR dwItemData);
    void* GetItemDataPtr(int nIndex) const { return reinterpret_cast<void*>(GetItemData(nIndex)); }
    int SetItemDataPtr(int nIndex, void* pData) { return SetItemData(nIndex, reinterpret_cast<DWORD_PTR>(pData)); }
    int GetLBText(int nIndex, char* lpszText) const;
    void GetLBText(int nIndex, CString& rString) const;
    int GetLBTextLen(int nIndex) const;
    int SetItemHeight(int, UINT) { return 0; }
    int GetItemHeight(int) const { return 16; }
    int FindStringExact(int nIndexStart, const char* lpszFind) const;
    int SetExtendedUI(BOOL = TRUE) { return CB_OKAY; }
    BOOL GetExtendedUI() const { return FALSE; }
    void GetDroppedControlRect(LPRECT lprect) const;
    BOOL GetDroppedState() const { return FALSE; }
    int GetDroppedWidth() const { return 0; }
    int SetDroppedWidth(UINT) { return 0; }
    int GetTopIndex() const { return 0; }
    int SetTopIndex(int) { return 0; }
    void ShowDropDown(BOOL bShowIt = TRUE);
    int AddString(const char* lpszString);
    int DeleteString(UINT nIndex);
    int InsertString(int nIndex, const char* lpszString);
    void ResetContent();
    int Dir(UINT, const char*) { return CB_ERR; }
    int FindString(int nStartAfter, const char* lpszString) const;
    int SelectString(int nStartAfter, const char* lpszString);
    void Clear();
    void Copy();
    void Cut();
    void Paste();
};

class CScrollBar : public CWnd {
    DECLARE_DYNAMIC(CScrollBar)
public:
    CScrollBar() = default;
    BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
    int GetScrollPos() const;
    int SetScrollPos(int nPos, BOOL bRedraw = TRUE);
    void GetScrollRange(LPINT lpMinPos, LPINT lpMaxPos) const;
    void SetScrollRange(int nMinPos, int nMaxPos, BOOL bRedraw = TRUE);
    void ShowScrollBar(BOOL bShow = TRUE);
    BOOL EnableScrollBar(UINT nArrowFlags = ESB_ENABLE_BOTH);
    BOOL SetScrollInfo(LPSCROLLINFO lpScrollInfo, BOOL bRedraw = TRUE);
    BOOL GetScrollInfo(LPSCROLLINFO lpScrollInfo, UINT nMask = SIF_ALL);
    int GetScrollLimit();
};

// ---------------------------------------------------------------------------------------------
// Dialogs and data exchange

class CDataExchange {
public:
    CDataExchange(CWnd* pDlgWnd, BOOL bSaveAndValidate);
    BOOL m_bSaveAndValidate;
    CWnd* m_pDlgWnd;
    HWND m_hWndLastControl;
    BOOL m_bEditLastControl;
    HWND PrepareCtrl(int nIDC);
    HWND PrepareEditCtrl(int nIDC);
    void Fail();
};

class CDialog : public CWnd {
    DECLARE_DYNAMIC(CDialog)
public:
    CDialog();
    explicit CDialog(const char* lpszTemplateName, CWnd* pParentWnd = nullptr);
    explicit CDialog(UINT nIDTemplate, CWnd* pParentWnd = nullptr);
    ~CDialog() override;

    virtual BOOL Create(const char* lpszTemplateName, CWnd* pParentWnd = nullptr);
    virtual BOOL Create(UINT nIDTemplate, CWnd* pParentWnd = nullptr);
    BOOL CreateIndirect(const void* lpDialogTemplate, CWnd* pParentWnd = nullptr, void* lpDialogInit = nullptr);
    BOOL InitModalIndirect(const void* lpDialogTemplate, CWnd* pParentWnd = nullptr, void* lpDialogInit = nullptr);
    virtual INT_PTR DoModal();
    void EndDialog(int nResult);
    void MapDialogRect(LPRECT lpRect) const;
    void SetHelpID(UINT nIDR) { m_nIDHelp = nIDR; }
    void SetDefID(UINT nID);
    DWORD GetDefID() const;
    void NextDlgCtrl() const;
    void PrevDlgCtrl() const;
    void GotoDlgCtrl(CWnd* pWndCtrl);
    BOOL CheckAutoCenter() { return TRUE; }
    virtual BOOL OnInitDialog();
    virtual void OnSetFont(CFont*) {}
    BOOL ContinueModal() { return m_bModalRunning; }
    BOOL DestroyWindow() override;

    UINT m_nIDHelp;
    const char* m_lpszTemplateName;
    UINT m_nIDTemplate;
    CWnd* m_pParentWnd;
    BOOL m_bModalRunning;
    int m_nModalResult;

protected:
    virtual void OnOK();
    virtual void OnCancel();
    afx_msg void OnHelp();
    afx_msg LRESULT HandleInitDialog(WPARAM, LPARAM);
    BOOL CreateFromTemplate(BOOL bModal);
    friend struct mfcwx::WindowState;

    DECLARE_MESSAGE_MAP()
};

void AFXAPI DDX_Control(CDataExchange* pDX, int nIDC, CWnd& rControl);
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, BYTE& value);
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, short& value);
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, int& value);
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, UINT& value);
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, long& value);
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, unsigned long& value);
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, LONG& value);
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, DWORD& value);
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, LONGLONG& value);
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, ULONGLONG& value);
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, CString& value);
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, char* value, int nMaxLen);
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, float& value);
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, double& value);
void AFXAPI DDX_Check(CDataExchange* pDX, int nIDC, int& value);
void AFXAPI DDX_Check(CDataExchange* pDX, int nIDC, bool& value);
void AFXAPI DDX_Radio(CDataExchange* pDX, int nIDC, int& value);
void AFXAPI DDX_LBString(CDataExchange* pDX, int nIDC, CString& value);
void AFXAPI DDX_CBString(CDataExchange* pDX, int nIDC, CString& value);
void AFXAPI DDX_LBIndex(CDataExchange* pDX, int nIDC, int& index);
void AFXAPI DDX_CBIndex(CDataExchange* pDX, int nIDC, int& index);
void AFXAPI DDX_LBStringExact(CDataExchange* pDX, int nIDC, CString& value);
void AFXAPI DDX_CBStringExact(CDataExchange* pDX, int nIDC, CString& value);
void AFXAPI DDX_Scroll(CDataExchange* pDX, int nIDC, int& value);
void AFXAPI DDX_Slider(CDataExchange* pDX, int nIDC, int& value);

void AFXAPI DDV_MaxChars(CDataExchange* pDX, const CString& value, int nChars);
void AFXAPI DDV_MinMaxByte(CDataExchange* pDX, BYTE value, BYTE minVal, BYTE maxVal);
void AFXAPI DDV_MinMaxShort(CDataExchange* pDX, short value, short minVal, short maxVal);
void AFXAPI DDV_MinMaxInt(CDataExchange* pDX, int value, int minVal, int maxVal);
void AFXAPI DDV_MinMaxLong(CDataExchange* pDX, long value, long minVal, long maxVal);
void AFXAPI DDV_MinMaxUInt(CDataExchange* pDX, UINT value, UINT minVal, UINT maxVal);
void AFXAPI DDV_MinMaxDWord(CDataExchange* pDX, DWORD value, DWORD minVal, DWORD maxVal);
void AFXAPI DDV_MinMaxLongLong(CDataExchange* pDX, LONGLONG value, LONGLONG minVal, LONGLONG maxVal);
void AFXAPI DDV_MinMaxULongLong(CDataExchange* pDX, ULONGLONG value, ULONGLONG minVal, ULONGLONG maxVal);
void AFXAPI DDV_MinMaxFloat(CDataExchange* pDX, float const& value, float minVal, float maxVal);
void AFXAPI DDV_MinMaxDouble(CDataExchange* pDX, double const& value, double minVal, double maxVal);
void AFXAPI DDV_MinMaxSlider(CDataExchange* pDX, DWORD value, DWORD minVal, DWORD maxVal);

// ---------------------------------------------------------------------------------------------
// Threads and application

typedef UINT (*AFX_THREADPROC)(LPVOID);

class CWinThread : public CCmdTarget {
    DECLARE_DYNAMIC(CWinThread)
public:
    CWinThread();
    CWinThread(AFX_THREADPROC pfnThreadProc, LPVOID pParam);
    ~CWinThread() override;
    BOOL CreateThread(DWORD dwCreateFlags = 0, UINT nStackSize = 0, LPSECURITY_ATTRIBUTES lpSecurityAttrs = nullptr);
    int GetThreadPriority();
    BOOL SetThreadPriority(int nPriority);
    DWORD SuspendThread();
    DWORD ResumeThread();
    BOOL PostThreadMessage(UINT message, WPARAM wParam, LPARAM lParam);
    virtual BOOL InitInstance();
    virtual int ExitInstance();
    virtual int Run();
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    virtual BOOL PumpMessage();
    virtual BOOL OnIdle(LONG lCount);
    virtual BOOL IsIdleMessage(MSG*) { return TRUE; }
    virtual CWnd* GetMainWnd();
    virtual BOOL ProcessMessageFilter(int code, LPMSG lpMsg);
    operator HANDLE() const { return m_hThread; }

    CWnd* m_pMainWnd;
    CWnd* m_pActiveWnd;
    BOOL m_bAutoDelete;
    HANDLE m_hThread;
    DWORD m_nThreadID;
    AFX_THREADPROC m_pfnThreadProc;
    LPVOID m_pThreadParams;
    MSG m_msgCur;
};

CWinThread* AFXAPI AfxBeginThread(AFX_THREADPROC pfnThreadProc, LPVOID pParam, int nPriority = THREAD_PRIORITY_NORMAL,
                                  UINT nStackSize = 0, DWORD dwCreateFlags = 0,
                                  LPSECURITY_ATTRIBUTES lpSecurityAttrs = nullptr);
CWinThread* AFXAPI AfxBeginThread(CRuntimeClass* pThreadClass, int nPriority = THREAD_PRIORITY_NORMAL,
                                  UINT nStackSize = 0, DWORD dwCreateFlags = 0,
                                  LPSECURITY_ATTRIBUTES lpSecurityAttrs = nullptr);
void AFXAPI AfxEndThread(UINT nExitCode, BOOL bDelete = TRUE);
CWinThread* AFXAPI AfxGetThread();

class CRecentFileList {
public:
    CRecentFileList(UINT nStart, const char* lpszSection, const char* lpszEntryFormat, int nSize,
                    int nMaxDispLen = 30);
    virtual ~CRecentFileList() = default;
    int GetSize() const { return m_nSize; }
    CString& operator[](int nIndex) { return m_arrNames[nIndex]; }
    virtual void Remove(int nIndex);
    virtual void Add(const char* lpszPathName);
    virtual BOOL GetDisplayName(CString& strName, int nIndex, const char* lpszCurDir, int nCurDir,
                                BOOL bAtLeastName = TRUE) const;
    virtual void UpdateMenu(CCmdUI* pCmdUI);
    virtual void ReadList();
    virtual void WriteList();

    int m_nSize;
    std::vector<CString> m_arrNames;
    CString m_strSectionName;
    CString m_strEntryFormat;
    UINT m_nStart;
    int m_nMaxDisplayLength;
};

class CWinApp : public CWinThread {
    DECLARE_DYNAMIC(CWinApp)
public:
    explicit CWinApp(const char* lpszAppName = nullptr);
    ~CWinApp() override;

    HINSTANCE m_hInstance;
    HINSTANCE m_hPrevInstance;
    char* m_lpCmdLine;
    int m_nCmdShow;
    const char* m_pszAppName;
    const char* m_pszRegistryKey;
    const char* m_pszExeName;
    const char* m_pszHelpFilePath;
    const char* m_pszProfileName;
    CRecentFileList* m_pRecentFileList;
    BOOL m_bHelpMode;
    WORD m_nWaitCursorCount;
    HCURSOR m_hcurWaitCursorRestore;
    int m_eHelpType;

    BOOL InitInstance() override;
    int ExitInstance() override;
    int Run() override;
    BOOL OnIdle(LONG lCount) override;
    virtual BOOL InitApplication();
    virtual CDocument* OpenDocumentFile(const char* lpszFileName);
    virtual void AddToRecentFileList(const char* lpszPathName);
    virtual BOOL SaveAllModified();
    virtual void CloseAllDocuments(BOOL bEndSession);
    virtual int DoMessageBox(const char* lpszPrompt, UINT nType, UINT nIDPrompt);
    virtual void DoWaitCursor(int nCode);
    virtual void WinHelp(DWORD_PTR dwData, UINT nCmd = HELP_CONTEXT);
    virtual void WinHelpInternal(DWORD_PTR dwData, UINT nCmd = HELP_CONTEXT);
    virtual void HtmlHelp(DWORD_PTR dwData, UINT nCmd = 0x000F);
    virtual BOOL ProcessShellCommand(class CCommandLineInfo& rCmdInfo);
    virtual void ParseCommandLine(class CCommandLineInfo& rCmdInfo);
    virtual BOOL DoPromptFileName(CString& fileName, UINT nIDSTitle, DWORD lFlags, BOOL bOpenFileDialog,
                                  CDocTemplate* pTemplate);
    virtual BOOL OnDDECommand(LPTSTR lpszCommand);

    void AddDocTemplate(CDocTemplate* pTemplate);
    POSITION GetFirstDocTemplatePosition() const;
    CDocTemplate* GetNextDocTemplate(POSITION& pos) const;
    HCURSOR LoadCursor(const char* lpszResourceName) const;
    HCURSOR LoadCursor(UINT nIDResource) const;
    HCURSOR LoadStandardCursor(const char* lpszCursorName) const;
    HCURSOR LoadOEMCursor(UINT nIDCursor) const;
    HICON LoadIcon(const char* lpszResourceName) const;
    HICON LoadIcon(UINT nIDResource) const;
    HICON LoadStandardIcon(const char* lpszIconName) const;
    HICON LoadOEMIcon(UINT nIDIcon) const;
    UINT GetProfileInt(const char* lpszSection, const char* lpszEntry, int nDefault);
    BOOL WriteProfileInt(const char* lpszSection, const char* lpszEntry, int nValue);
    CString GetProfileString(const char* lpszSection, const char* lpszEntry, const char* lpszDefault = nullptr);
    BOOL WriteProfileString(const char* lpszSection, const char* lpszEntry, const char* lpszValue);
    BOOL GetProfileBinary(const char* lpszSection, const char* lpszEntry, LPBYTE* ppData, UINT* pBytes);
    BOOL WriteProfileBinary(const char* lpszSection, const char* lpszEntry, LPBYTE pData, UINT nBytes);
    void SetRegistryKey(const char* lpszRegistryKey);
    void SetRegistryKey(UINT nIDRegistryKey);
    void LoadStdProfileSettings(UINT nMaxMRU = 4);
    void EnableShellOpen() {}
    void RegisterShellFileTypes(BOOL = FALSE) {}
    void UnregisterShellFileTypes() {}
    void Enable3dControls() {}
    void Enable3dControlsStatic() {}
    void SetDialogBkColor(COLORREF = 0, COLORREF = 0) {}
    void EnableHtmlHelp() {}
    void HideApplication();
    BOOL GetPrinterDeviceDefaults(void*) { return FALSE; }
    void SelectPrinter(HANDLE, HANDLE, BOOL = TRUE) {}
    CWnd* GetMainWnd() override { return m_pMainWnd; }

    afx_msg void OnFileNew();
    afx_msg void OnFileOpen();
    afx_msg void OnFilePrintSetup();
    afx_msg void OnAppExit();
    afx_msg void OnHelp();
    afx_msg void OnHelpFinder();
    afx_msg void OnHelpIndex();
    afx_msg void OnContextHelp();
    afx_msg void OnHelpUsing();
    afx_msg BOOL OnOpenRecentFile(UINT nID);
    afx_msg void OnUpdateRecentFileMenu(CCmdUI* pCmdUI);

    std::vector<CDocTemplate*> m_templates;

protected:
    DECLARE_MESSAGE_MAP()
};

class CCommandLineInfo : public CObject {
public:
    CCommandLineInfo();
    virtual void ParseParam(const char* pszParam, BOOL bFlag, BOOL bLast);
    BOOL m_bShowSplash;
    BOOL m_bRunEmbedded;
    BOOL m_bRunAutomated;
    enum { FileNew, FileOpen, FilePrint, FilePrintTo, FileDDE, AppRegister, AppUnregister, FileNothing = -1 } m_nShellCommand;
    CString m_strFileName;
    CString m_strPrinterName;
    CString m_strDriverName;
    CString m_strPortName;
};

CWinApp* AFXAPI AfxGetApp();
CWnd* AFXAPI AfxGetMainWnd();
HINSTANCE AFXAPI AfxGetInstanceHandle();
HINSTANCE AFXAPI AfxGetResourceHandle();
void AFXAPI AfxSetResourceHandle(HINSTANCE hInstResource);
HINSTANCE AFXAPI AfxFindResourceHandle(const char* lpszName, const char* lpszType);
const char* AFXAPI AfxGetAppName();
int AFXAPI AfxMessageBox(const char* lpszText, UINT nType = MB_OK, UINT nIDHelp = 0);
int AFXAPI AfxMessageBox(UINT nIDPrompt, UINT nType = MB_OK, UINT nIDHelp = (UINT)-1);
void AFXAPI AfxFormatString1(CString& rString, UINT nIDS, const char* lpsz1);
void AFXAPI AfxFormatString2(CString& rString, UINT nIDS, const char* lpsz1, const char* lpsz2);
BOOL AFXAPI AfxExtractSubString(CString& rString, const char* lpszFullString, int iSubString, char chSep = '\n');
const char* AFXAPI AfxRegisterWndClass(UINT nClassStyle, HCURSOR hCursor = nullptr, HBRUSH hbrBackground = nullptr,
                                       HICON hIcon = nullptr);
BOOL AFXAPI AfxRegisterClass(WNDCLASS* lpWndClass);
BOOL AFXAPI AfxInitRichEdit();
BOOL AFXAPI AfxInitRichEdit2();
BOOL AFXAPI AfxOleInit();
void AFXAPI AfxEnableControlContainer();
BOOL AFXAPI AfxSocketInit();
void AFXAPI AfxInitCommonControls();
BOOL AFXAPI AfxCheckMemory();
void AFXAPI AfxGetModuleShortFileName(HINSTANCE hInst, CString& strShortName);
LRESULT AFXAPI AfxCallWndProc(CWnd* pWnd, HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam);

#include "mfcwx/winapi_gui.h"
#include "mfcwx/regkey.h"
#include "mfcwx/extras.h"

template <class A, class B>
inline typename std::common_type<A, B>::type min(A a, B b) { return b < a ? b : a; }
template <class A, class B>
inline typename std::common_type<A, B>::type max(A a, B b) { return a < b ? b : a; }
#include "mfcwx/docview.h"
