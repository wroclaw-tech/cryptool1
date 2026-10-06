#pragma once

// Document/view architecture, frames and control bars.

class CCreateContext {
public:
    CCreateContext() = default;
    CRuntimeClass* m_pNewViewClass = nullptr;
    CDocument* m_pCurrentDoc = nullptr;
    CDocTemplate* m_pNewDocTemplate = nullptr;
    CView* m_pLastView = nullptr;
    CFrameWnd* m_pCurrentFrame = nullptr;
};

class CPrintDialog;

class CPrintInfo {
public:
    CPrintInfo();
    ~CPrintInfo();
    void SetMinPage(UINT nMinPage) { m_nMinPage = nMinPage; }
    void SetMaxPage(UINT nMaxPage) { m_nMaxPage = nMaxPage; }
    UINT GetMinPage() const { return m_nMinPage; }
    UINT GetMaxPage() const { return m_nMaxPage; }
    UINT GetFromPage() const { return m_nMinPage; }
    UINT GetToPage() const { return m_nMaxPage; }
    UINT GetOffsetPage() const { return 0; }
    CPrintDialog* m_pPD;
    BOOL m_bDirect;
    BOOL m_bPreview;
    BOOL m_bContinuePrinting;
    UINT m_nCurPage;
    UINT m_nNumPreviewPages;
    CString m_strPageDesc;
    LPVOID m_lpUserData;
    CRect m_rectDraw;
    UINT m_nMinPage;
    UINT m_nMaxPage;
};

class CDocument : public CCmdTarget {
    DECLARE_DYNAMIC(CDocument)
public:
    CDocument();
    ~CDocument() override;

    const CString& GetTitle() const { return m_strTitle; }
    virtual void SetTitle(const char* lpszTitle);
    const CString& GetPathName() const { return m_strPathName; }
    virtual void SetPathName(const char* lpszPathName, BOOL bAddToMRU = TRUE);
    CDocTemplate* GetDocTemplate() const { return m_pDocTemplate; }
    virtual BOOL IsModified() { return m_bModified; }
    virtual void SetModifiedFlag(BOOL bModified = TRUE) { m_bModified = bModified; }
    virtual POSITION GetFirstViewPosition() const;
    virtual CView* GetNextView(POSITION& rPosition) const;
    void UpdateAllViews(CView* pSender, LPARAM lHint = 0, CObject* pHint = nullptr);
    void AddView(CView* pView);
    void RemoveView(CView* pView);
    virtual void DeleteContents() {}
    virtual BOOL OnNewDocument();
    virtual BOOL OnOpenDocument(const char* lpszPathName);
    virtual BOOL OnSaveDocument(const char* lpszPathName);
    virtual void OnCloseDocument();
    virtual void ReportSaveLoadException(const char* lpszPathName, CException* e, BOOL bSaving, UINT nIDPDefault);
    virtual CFile* GetFile(const char* lpszFileName, UINT nOpenFlags, CFileException* pError);
    virtual void ReleaseFile(CFile* pFile, BOOL bAbort);
    virtual BOOL CanCloseFrame(CFrameWnd* pFrame);
    virtual BOOL SaveModified();
    virtual void PreCloseFrame(CFrameWnd* pFrame) {}
    virtual BOOL DoSave(const char* lpszPathName, BOOL bReplace = TRUE);
    virtual BOOL DoFileSave();
    virtual void OnChangedViewList();
    virtual void SetModifiedFlagNoUpdate(BOOL b) { m_bModified = b; }
    virtual void OnIdle() {}
    void SendInitialUpdate();
    BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) override;

    afx_msg void OnFileClose();
    afx_msg void OnFileSave();
    afx_msg void OnFileSaveAs();

    CString m_strTitle;
    CString m_strPathName;
    CDocTemplate* m_pDocTemplate;
    std::vector<CView*> m_viewList;
    BOOL m_bModified;
    BOOL m_bAutoDelete;
    BOOL m_bEmbedded;

protected:
    DECLARE_MESSAGE_MAP()
};

class CView : public CWnd {
    DECLARE_DYNAMIC(CView)
public:
    CView();
    ~CView() override;

    CDocument* GetDocument() const { return m_pDocument; }
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) override;
    virtual void OnInitialUpdate();
    virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
    virtual void OnDraw(CDC* pDC) = 0;
    virtual void OnPrepareDC(CDC* pDC, CPrintInfo* pInfo = nullptr);
    virtual void OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView);
    virtual void OnActivateFrame(UINT, CFrameWnd*) {}
    virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
    virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
    virtual void OnPrint(CDC* pDC, CPrintInfo* pInfo);
    virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);
    virtual void OnEndPrintPreview(CDC*, CPrintInfo*, POINT, void*) {}
    virtual BOOL IsSelected(const CObject*) const { return FALSE; }
    virtual BOOL OnScrollBy(CSize, BOOL = TRUE) { return FALSE; }
    virtual BOOL OnScroll(UINT, UINT, BOOL = TRUE) { return FALSE; }
    BOOL DoPreparePrinting(CPrintInfo* pInfo);
    void PostNcDestroy() override;

    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnDestroy();
    afx_msg void OnPaint();
    afx_msg void OnFilePrint();
    afx_msg void OnFilePrintPreview();

    CDocument* m_pDocument;

protected:
    DECLARE_MESSAGE_MAP()
};

class CScrollView : public CView {
    DECLARE_DYNAMIC(CScrollView)
public:
    CScrollView();
    static const SIZE sizeDefault;
    void SetScrollSizes(int nMapMode, SIZE sizeTotal, const SIZE& sizePage = sizeDefault,
                        const SIZE& sizeLine = sizeDefault);
    void SetScaleToFitSize(SIZE sizeTotal);
    CPoint GetScrollPosition() const;
    CPoint GetDeviceScrollPosition() const;
    CSize GetTotalSize() const { return m_totalLog; }
    void ScrollToPosition(POINT pt);
    void FillOutsideRect(CDC* pDC, CBrush* pBrush);
    void ResizeParentToFit(BOOL bShrinkOnly = TRUE);
    void OnPrepareDC(CDC* pDC, CPrintInfo* pInfo = nullptr) override;

    int m_nMapMode;
    CSize m_totalLog;
    CSize m_totalDev;
    CSize m_pageDev;
    CSize m_lineDev;
};

class CFrameWnd : public CWnd {
    DECLARE_DYNCREATE(CFrameWnd)
public:
    static const CRect rectDefault;
    CFrameWnd();
    ~CFrameWnd() override;

    virtual BOOL Create(const char* lpszClassName, const char* lpszWindowName, DWORD dwStyle = WS_OVERLAPPEDWINDOW,
                        const RECT& rect = rectDefault, CWnd* pParentWnd = nullptr, const char* lpszMenuName = nullptr,
                        DWORD dwExStyle = 0, CCreateContext* pContext = nullptr);
    virtual BOOL LoadFrame(UINT nIDResource, DWORD dwDefaultStyle = WS_OVERLAPPEDWINDOW | 0x00008000L,
                           CWnd* pParentWnd = nullptr, CCreateContext* pContext = nullptr);
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) override;
    BOOL OnCommand(WPARAM wParam, LPARAM lParam) override;
    BOOL PreTranslateMessage(MSG* pMsg) override;
    void PostNcDestroy() override;

    virtual CFrameWnd* GetActiveFrame();
    CView* GetActiveView() const;
    void SetActiveView(CView* pViewNew, BOOL bNotify = TRUE);
    virtual CDocument* GetActiveDocument();
    virtual void InitialUpdateFrame(CDocument* pDoc, BOOL bMakeVisible);
    virtual void ActivateFrame(int nCmdShow = -1);
    virtual void OnUpdateFrameTitle(BOOL bAddToTitle);
    virtual void OnUpdateFrameMenu(HMENU hMenuAlt);
    virtual void RecalcLayout(BOOL bNotify = TRUE);
    virtual void GetMessageString(UINT nID, CString& rMessage) const;
    virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
    virtual void SetMessageText(const char* lpszText);
    void SetMessageText(UINT nID);
    CWnd* CreateView(CCreateContext* pContext, UINT nID = AFX_IDW_PANE_FIRST);
    void SetTitle(const char* lpszTitle) { m_strTitle = lpszTitle; }
    CString GetTitle() const { return m_strTitle; }
    void EnableDocking(DWORD dwDockStyle);
    void DockControlBar(CControlBar* pBar, UINT nDockBarID = 0, LPCRECT lpRect = nullptr);
    void FloatControlBar(CControlBar*, CPoint, DWORD = CBRS_ALIGN_TOP) {}
    void ShowControlBar(CControlBar* pBar, BOOL bShow, BOOL bDelay);
    CControlBar* GetControlBar(UINT nID);
    BOOL IsTracking() const { return FALSE; }
    void BeginModalState() {}
    void EndModalState() {}
    BOOL InModalState() const { return FALSE; }
    void SetDockState(const void*) {}
    void LoadBarState(const char*) {}
    void SaveBarState(const char*) const {}
    void OnSetPreviewMode(BOOL, void*) {}
    void UpdateFrameTitleForDocument(const char* lpszDocName);
    void NotifyFloatingWindows(DWORD) {}
    BOOL IsFrameWnd() const { return TRUE; }
    HMENU GetDefaultMenu() { return m_hMenuDefault; }

    afx_msg int OnCreate(LPCREATESTRUCT lpcs);
    afx_msg void OnClose();
    afx_msg void OnDestroy();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnUpdateControlBarMenu(CCmdUI* pCmdUI);
    afx_msg BOOL OnBarCheck(UINT nID);
    afx_msg void OnHelp();
    afx_msg void OnContextHelp();

    CString m_strTitle;
    UINT m_nIDHelp;
    HMENU m_hMenuDefault;
    HACCEL m_hAccelTable;
    BOOL m_bAutoMenuEnable;
    UINT m_nIdleFlags;
    CView* m_pViewActive;
    std::vector<CControlBar*> m_listControlBars;
    CStatusBar* m_pStatusBar;

protected:
    DECLARE_MESSAGE_MAP()
};

class CMDIChildWnd;

class CMDIFrameWnd : public CFrameWnd {
    DECLARE_DYNAMIC(CMDIFrameWnd)
public:
    CMDIFrameWnd();
    BOOL LoadFrame(UINT nIDResource, DWORD dwDefaultStyle = WS_OVERLAPPEDWINDOW | 0x00008000L,
                   CWnd* pParentWnd = nullptr, CCreateContext* pContext = nullptr) override;
    BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) override;
    CFrameWnd* GetActiveFrame() override;
    void OnUpdateFrameTitle(BOOL bAddToTitle) override;
    void OnUpdateFrameMenu(HMENU hMenuAlt) override;
    BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext) override;
    void RecalcLayout(BOOL bNotify = TRUE) override;

    void MDIActivate(CWnd* pWndActivate);
    CMDIChildWnd* MDIGetActive(BOOL* pbMaximized = nullptr) const;
    void MDIIconArrange() {}
    void MDIMaximize(CWnd* pWnd);
    void MDINext();
    void MDIPrev();
    void MDIRestore(CWnd* pWnd);
    CMenu* MDISetMenu(CMenu* pFrameMenu, CMenu* pWindowMenu);
    void MDITile();
    void MDICascade();
    void MDITile(int) { MDITile(); }
    void MDICascade(int) { MDICascade(); }
    CMDIChildWnd* CreateNewChild(CRuntimeClass* pClass, UINT nResource, HMENU hMenu = nullptr, HACCEL hAccel = nullptr);
    HWND m_hWndMDIClient;

    afx_msg void OnWindowNew();
    afx_msg void OnWindowCascade();
    afx_msg void OnWindowTile();
    afx_msg void OnUpdateMDIWindowCmd(CCmdUI* pCmdUI);

    // implementation: all MDI children in creation order; notifies children and menus of tab switches
    std::vector<CMDIChildWnd*> m_children;
    void OnChildActivated();

protected:
    DECLARE_MESSAGE_MAP()
};

class CMDIChildWnd : public CFrameWnd {
    DECLARE_DYNCREATE(CMDIChildWnd)
public:
    CMDIChildWnd();
    ~CMDIChildWnd() override;
    virtual BOOL Create(const char* lpszClassName, const char* lpszWindowName,
                        DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_OVERLAPPEDWINDOW, const RECT& rect = rectDefault,
                        CMDIFrameWnd* pParentWnd = nullptr, CCreateContext* pContext = nullptr);
    BOOL LoadFrame(UINT nIDResource, DWORD dwDefaultStyle = WS_CHILD | WS_VISIBLE | WS_OVERLAPPEDWINDOW | 0x00008000L,
                   CWnd* pParentWnd = nullptr, CCreateContext* pContext = nullptr) override;
    BOOL DestroyWindow() override;
    BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    void ActivateFrame(int nCmdShow = -1) override;
    void OnUpdateFrameMenu(BOOL bActive, CWnd* pActivateWnd, HMENU hMenuAlt);
    void OnUpdateFrameTitle(BOOL bAddToTitle) override;
    CMDIFrameWnd* GetMDIFrame() const;
    void MDIDestroy();
    void MDIActivate();
    void MDIMaximize();
    void MDIRestore();
    void SetHandles(HMENU hMenu, HACCEL hAccel) { m_hMenuShared = hMenu; m_hAccelTable = hAccel; }
    BOOL PreTranslateMessage(MSG* pMsg) override;

    HMENU m_hMenuShared;
    CMDIFrameWnd* m_pMDIFrame;
    bool m_bActive;

protected:
    DECLARE_MESSAGE_MAP()
};

class CDocTemplate : public CCmdTarget {
    DECLARE_DYNAMIC(CDocTemplate)
public:
    enum DocStringIndex { windowTitle, docName, fileNewName, filterName, filterExt, regFileTypeId, regFileTypeName };
    enum Confidence { noAttempt, maybeAttemptForeign, maybeAttemptNative, yesAttemptForeign, yesAttemptNative, yesAlreadyOpen };

    CDocTemplate(UINT nIDResource, CRuntimeClass* pDocClass, CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass);
    ~CDocTemplate() override;

    virtual void LoadTemplate();
    virtual POSITION GetFirstDocPosition() const = 0;
    virtual CDocument* GetNextDoc(POSITION& rPos) const = 0;
    virtual void AddDocument(CDocument* pDoc);
    virtual void RemoveDocument(CDocument* pDoc);
    virtual BOOL GetDocString(CString& rString, enum DocStringIndex index) const;
    virtual CDocument* CreateNewDocument();
    virtual CFrameWnd* CreateNewFrame(CDocument* pDoc, CFrameWnd* pOther);
    virtual void InitialUpdateFrame(CFrameWnd* pFrame, CDocument* pDoc, BOOL bMakeVisible = TRUE);
    virtual BOOL SaveAllModified();
    virtual void CloseAllDocuments(BOOL bEndSession);
    virtual CDocument* OpenDocumentFile(const char* lpszPathName, BOOL bMakeVisible = TRUE) = 0;
    virtual void SetDefaultTitle(CDocument* pDocument) = 0;
    virtual Confidence MatchDocType(const char* lpszPathName, CDocument*& rpDocMatch);
    void SetContainerInfo(UINT) {}
    void SetServerInfo(UINT, UINT = 0, CRuntimeClass* = nullptr) {}

    UINT m_nIDResource;
    CRuntimeClass* m_pDocClass;
    CRuntimeClass* m_pFrameClass;
    CRuntimeClass* m_pViewClass;
    CString m_strDocStrings;
    BOOL m_bAutoDelete;
};

class CMultiDocTemplate : public CDocTemplate {
    DECLARE_DYNAMIC(CMultiDocTemplate)
public:
    CMultiDocTemplate(UINT nIDResource, CRuntimeClass* pDocClass, CRuntimeClass* pFrameClass,
                      CRuntimeClass* pViewClass);
    ~CMultiDocTemplate() override;
    void LoadTemplate() override;
    POSITION GetFirstDocPosition() const override;
    CDocument* GetNextDoc(POSITION& rPos) const override;
    void AddDocument(CDocument* pDoc) override;
    void RemoveDocument(CDocument* pDoc) override;
    CDocument* OpenDocumentFile(const char* lpszPathName, BOOL bMakeVisible = TRUE) override;
    void SetDefaultTitle(CDocument* pDocument) override;

    HMENU m_hMenuShared;
    HACCEL m_hAccelTable;
    std::vector<CDocument*> m_docList;
    UINT m_nUntitledCount;
};

class CSingleDocTemplate : public CDocTemplate {
    DECLARE_DYNAMIC(CSingleDocTemplate)
public:
    CSingleDocTemplate(UINT nIDResource, CRuntimeClass* pDocClass, CRuntimeClass* pFrameClass,
                       CRuntimeClass* pViewClass);
    POSITION GetFirstDocPosition() const override;
    CDocument* GetNextDoc(POSITION& rPos) const override;
    void AddDocument(CDocument* pDoc) override;
    void RemoveDocument(CDocument* pDoc) override;
    CDocument* OpenDocumentFile(const char* lpszPathName, BOOL bMakeVisible = TRUE) override;
    void SetDefaultTitle(CDocument* pDocument) override;
    CDocument* m_pOnlyDoc;
};

// ---------------------------------------------------------------------------------------------
// Control bars

class CControlBar : public CWnd {
    DECLARE_DYNAMIC(CControlBar)
public:
    CControlBar();
    DWORD GetBarStyle() { return m_dwStyle; }
    void SetBarStyle(DWORD dwStyle) { m_dwStyle = dwStyle; }
    void EnableDocking(DWORD dwDockStyle) { m_dwDockStyle = dwDockStyle; }
    BOOL IsFloating() const { return FALSE; }
    CFrameWnd* GetDockingFrame() const { return GetParentFrame(); }
    virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz);
    DWORD m_dwStyle;
    DWORD m_dwDockStyle;
};

#define TBBS_BUTTON 0x0000
#define TBBS_SEPARATOR 0x0001
#define TBBS_CHECKBOX 0x0002
#define TBBS_GROUP 0x0004
#define TBBS_CHECKGROUP (TBBS_GROUP | TBBS_CHECKBOX)
#define TBBS_CHECKED 0x0400
#define TBBS_PRESSED 0x0200
#define TBBS_DISABLED 0x0100
#define TBBS_INDETERMINATE 0x0800
#define TBBS_HIDDEN 0x1000

class CToolBar : public CControlBar {
    DECLARE_DYNAMIC(CToolBar)
public:
    CToolBar();
    BOOL Create(CWnd* pParentWnd, DWORD dwStyle = WS_CHILD | WS_VISIBLE | CBRS_TOP, UINT nID = AFX_IDW_TOOLBAR);
    BOOL CreateEx(CWnd* pParentWnd, DWORD dwCtrlStyle = TBSTYLE_FLAT,
                  DWORD dwStyle = WS_CHILD | WS_VISIBLE | CBRS_ALIGN_TOP, CRect rcBorders = CRect(0, 0, 0, 0),
                  UINT nID = AFX_IDW_TOOLBAR);
    BOOL LoadToolBar(const char* lpszResourceName);
    BOOL LoadToolBar(UINT nIDResource);
    BOOL LoadBitmap(const char* lpszResourceName);
    BOOL LoadBitmap(UINT nIDResource);
    BOOL SetButtons(const UINT* lpIDArray, int nIDCount);
    void SetSizes(SIZE sizeButton, SIZE sizeImage);
    int CommandToIndex(UINT nIDFind) const;
    UINT GetItemID(int nIndex) const;
    void GetItemRect(int nIndex, LPRECT lpRect) const;
    UINT GetButtonStyle(int nIndex) const;
    void SetButtonStyle(int nIndex, UINT nStyle);
    int GetCount() const;
    void SetButtonText(int, const char*) {}
    void SetHeight(int) {}

    std::vector<UINT> m_buttonIDs;
    UINT m_nResourceID;
};

#define SBPS_NORMAL 0x0000
#define SBPS_NOBORDERS 0x0100
#define SBPS_POPOUT 0x0200
#define SBPS_OWNERDRAW 0x1000
#define SBPS_DISABLED 0x04000000
#define SBPS_STRETCH 0x08000000

class CStatusBar : public CControlBar {
    DECLARE_DYNAMIC(CStatusBar)
public:
    CStatusBar();
    BOOL Create(CWnd* pParentWnd, DWORD dwStyle = WS_CHILD | WS_VISIBLE | CBRS_BOTTOM, UINT nID = AFX_IDW_STATUS_BAR);
    BOOL CreateEx(CWnd* pParentWnd, DWORD dwCtrlStyle = 0, DWORD dwStyle = WS_CHILD | WS_VISIBLE | CBRS_BOTTOM,
                  UINT nID = AFX_IDW_STATUS_BAR);
    BOOL SetIndicators(const UINT* lpIDArray, int nIDCount);
    int CommandToIndex(UINT nIDFind) const;
    UINT GetItemID(int nIndex) const;
    void GetItemRect(int nIndex, LPRECT lpRect) const;
    CString GetPaneText(int nIndex) const;
    void GetPaneText(int nIndex, CString& rString) const;
    BOOL SetPaneText(int nIndex, const char* lpszNewText, BOOL bUpdate = TRUE);
    void GetPaneInfo(int nIndex, UINT& nID, UINT& nStyle, int& cxWidth) const;
    void SetPaneInfo(int nIndex, UINT nID, UINT nStyle, int cxWidth);
    UINT GetPaneStyle(int nIndex) const;
    void SetPaneStyle(int nIndex, UINT nStyle);
    CString GetText() const { return GetPaneText(0); }
    void SetText(const char* lpsz) { SetPaneText(0, lpsz); }

    std::vector<UINT> m_indicators;
};

class CDialogBar : public CControlBar {
public:
    BOOL Create(CWnd*, UINT, UINT, UINT) { return FALSE; }
};
