#include "windows_impl.h"

#include "afxdlgs.h"
#include "afxext.h"
#include "afxrich.h"

#include <wx/aui/auibook.h>
#include <wx/statusbr.h>
#include <wx/toolbar.h>

namespace mfcwx {

namespace {

struct AFX_NOTIFY_LOCAL {
    LRESULT* pResult;
    NMHDR* pNMHDR;
};

CString ResourceString(UINT id) {
    CString s;
    s.LoadString(id);
    return s;
}

// Main window of an MDI application: menu, toolbar, status bar and a notebook of child frames.
class MfcFrame : public wxFrame {
public:
    MfcFrame(wxWindow* parent, const wxString& title, const wxPoint& pos, const wxSize& size, long style)
        : wxFrame(parent, wxID_ANY, title, pos, size, style) {
        Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& e) {
            if (IsManagedWindow(this) && PermanentWnd(this)) {
                e.Veto();
                DispatchMessageTo(ToHwnd(this), WM_CLOSE, 0, 0);
            } else {
                e.Skip();
            }
        });
        Bind(wxEVT_MENU, [this](wxCommandEvent& e) {
            int id = MenuWinId(e.GetId());
            if (id >= ID_FILE_MRU_FILE1 && id <= ID_FILE_MRU_FILE16 && e.GetId() >= wxID_FILE1 && e.GetId() <= wxID_FILE9)
                id = ID_FILE_MRU_FILE1 + (e.GetId() - wxID_FILE1);
            DispatchMessageTo(ToHwnd(this), WM_COMMAND, MAKEWPARAM(id, 0), 0);
        });
        Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { UpdateCommandUI(this, e); });
        Bind(wxEVT_MENU_HIGHLIGHT, [this](wxMenuEvent& e) {
            CWnd* p = PermanentWnd(this);
            if (p && p->IsKindOf(RUNTIME_CLASS(CFrameWnd)) && e.GetMenuId() >= kIdOffset) {
                CString text;
                static_cast<CFrameWnd*>(p)->GetMessageString(static_cast<UINT>(MenuWinId(e.GetMenuId())), text);
                static_cast<CFrameWnd*>(p)->SetMessageText(text);
            } else {
                e.Skip();
            }
        });
        Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& e) {
            if (!PreTranslateKey(this, e, e.AltDown() ? WM_SYSKEYDOWN : WM_KEYDOWN))
                e.Skip();
        });
    }

    static void UpdateCommandUI(wxWindow* frame, wxUpdateUIEvent& e) {
        CWnd* p = PermanentWnd(frame);
        if (!p || e.GetId() < kIdOffset - 1 || !p->IsKindOf(RUNTIME_CLASS(CFrameWnd))) {
            if (e.GetId() == wxID_ABOUT || e.GetId() == wxID_EXIT)
                e.Enable(true);
            return;
        }
        auto* frameWnd = static_cast<CFrameWnd*>(p);
        CCmdUI ui;
        ui.m_nID = static_cast<UINT>(MenuWinId(e.GetId()));
        ui.m_pOther = nullptr;
        ui.DoUpdate(frameWnd, frameWnd->m_bAutoMenuEnable);
        e.Enable(ui.m_enabled);
        if (ui.m_checkSet)
            e.Check(ui.m_check != 0);
        if (ui.m_textSet)
            e.SetText(ToWx(ui.m_text));
    }
};

class MfcChildPanel : public wxPanel {
public:
    explicit MfcChildPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE) {}
};

wxAuiNotebook* ClientNotebook(CMDIFrameWnd* frame) {
    return frame && frame->m_hWndMDIClient ? wxDynamicCast(ToWx(frame->m_hWndMDIClient), wxAuiNotebook) : nullptr;
}

CMDIFrameWnd* MainMDIFrame() {
    CWinApp* app = AfxGetApp();
    CWnd* main = app ? app->m_pMainWnd : nullptr;
    if (main && main->IsKindOf(RUNTIME_CLASS(CMDIFrameWnd)))
        return static_cast<CMDIFrameWnd*>(main);
    return nullptr;
}

} // namespace

wxWindow* MainWxWindow() {
    CWinApp* app = AfxGetApp();
    return app && app->m_pMainWnd && app->m_pMainWnd->m_hWnd ? app->m_pMainWnd->GetWx() : nullptr;
}

} // namespace mfcwx

using namespace mfcwx;

// ---------------------------------------------------------------------------------------------
// CPrintInfo

CPrintInfo::CPrintInfo()
    : m_pPD(nullptr), m_bDirect(FALSE), m_bPreview(FALSE), m_bContinuePrinting(TRUE), m_nCurPage(1),
      m_nNumPreviewPages(1), m_lpUserData(nullptr), m_nMinPage(1), m_nMaxPage(0xFFFF) {}

CPrintInfo::~CPrintInfo() {}

// ---------------------------------------------------------------------------------------------
// CDocument

IMPLEMENT_DYNAMIC(CDocument, CCmdTarget)

BEGIN_MESSAGE_MAP(CDocument, CCmdTarget)
    ON_COMMAND(ID_FILE_CLOSE, &CDocument::OnFileClose)
    ON_COMMAND(ID_FILE_SAVE, &CDocument::OnFileSave)
    ON_COMMAND(ID_FILE_SAVE_AS, &CDocument::OnFileSaveAs)
END_MESSAGE_MAP()

CDocument::CDocument() : m_pDocTemplate(nullptr), m_bModified(FALSE), m_bAutoDelete(TRUE), m_bEmbedded(FALSE) {}

CDocument::~CDocument() {
    if (m_pDocTemplate)
        m_pDocTemplate->RemoveDocument(this);
}

void CDocument::SetTitle(const char* lpszTitle) {
    m_strTitle = lpszTitle;
    for (CView* v : m_viewList) {
        CFrameWnd* frame = v->GetParentFrame();
        if (frame)
            frame->OnUpdateFrameTitle(TRUE);
    }
}

void CDocument::SetPathName(const char* lpszPathName, BOOL bAddToMRU) {
    m_strPathName = lpszPathName;
    int start = 0;
    for (int i = 0; i < m_strPathName.GetLength(); ++i) {
        char c = m_strPathName[i];
        if (c == '\\' || c == '/' || c == ':')
            start = i + 1;
    }
    SetTitle(m_strPathName.Mid(start));
    if (bAddToMRU && AfxGetApp())
        AfxGetApp()->AddToRecentFileList(lpszPathName);
}

POSITION CDocument::GetFirstViewPosition() const {
    return m_viewList.empty() ? nullptr : reinterpret_cast<POSITION>(static_cast<uintptr_t>(1));
}

CView* CDocument::GetNextView(POSITION& rPosition) const {
    size_t i = static_cast<size_t>(reinterpret_cast<uintptr_t>(rPosition)) - 1;
    CView* v = i < m_viewList.size() ? m_viewList[i] : nullptr;
    rPosition = i + 1 < m_viewList.size() ? reinterpret_cast<POSITION>(static_cast<uintptr_t>(i + 2)) : nullptr;
    return v;
}

void CDocument::UpdateAllViews(CView* pSender, LPARAM lHint, CObject* pHint) {
    std::vector<CView*> views = m_viewList;
    for (CView* v : views)
        if (v != pSender)
            v->OnUpdate(pSender, lHint, pHint);
}

void CDocument::AddView(CView* pView) {
    if (std::find(m_viewList.begin(), m_viewList.end(), pView) == m_viewList.end()) {
        m_viewList.push_back(pView);
        pView->m_pDocument = this;
        OnChangedViewList();
    }
}

void CDocument::RemoveView(CView* pView) {
    auto it = std::find(m_viewList.begin(), m_viewList.end(), pView);
    if (it == m_viewList.end())
        return;
    m_viewList.erase(it);
    pView->m_pDocument = nullptr;
    OnChangedViewList();
}

void CDocument::OnChangedViewList() {
    if (m_viewList.empty() && m_bAutoDelete)
        OnCloseDocument();
}

void CDocument::SendInitialUpdate() {
    std::vector<CView*> views = m_viewList;
    for (CView* v : views)
        v->OnInitialUpdate();
}

BOOL CDocument::OnNewDocument() {
    DeleteContents();
    m_strPathName.Empty();
    SetModifiedFlag(FALSE);
    return TRUE;
}

CFile* CDocument::GetFile(const char* lpszFileName, UINT nOpenFlags, CFileException* pError) {
    auto* f = new CFile;
    if (!f->Open(lpszFileName, nOpenFlags, pError)) {
        delete f;
        return nullptr;
    }
    return f;
}

void CDocument::ReleaseFile(CFile* pFile, BOOL bAbort) {
    if (bAbort)
        pFile->Abort();
    else
        pFile->Close();
    delete pFile;
}

BOOL CDocument::OnOpenDocument(const char* lpszPathName) {
    CFileException fe;
    CFile* pFile = GetFile(lpszPathName, CFile::modeRead | CFile::shareDenyWrite, &fe);
    if (!pFile) {
        ReportSaveLoadException(lpszPathName, &fe, FALSE, AFX_IDP_FAILED_TO_OPEN_DOC);
        return FALSE;
    }
    DeleteContents();
    SetModifiedFlag(TRUE);
    try {
        CArchive ar(pFile, CArchive::load | CArchive::bNoFlushOnDelete);
        ar.m_pDocument = this;
        ar.m_strFileName = lpszPathName;
        Serialize(ar);
        ar.Close();
        ReleaseFile(pFile, FALSE);
    } catch (CException* e) {
        ReleaseFile(pFile, TRUE);
        DeleteContents();
        ReportSaveLoadException(lpszPathName, e, FALSE, AFX_IDP_FAILED_TO_OPEN_DOC);
        e->Delete();
        return FALSE;
    }
    SetModifiedFlag(FALSE);
    return TRUE;
}

BOOL CDocument::OnSaveDocument(const char* lpszPathName) {
    CFileException fe;
    CFile* pFile = GetFile(lpszPathName, CFile::modeCreate | CFile::modeReadWrite | CFile::shareExclusive, &fe);
    if (!pFile) {
        ReportSaveLoadException(lpszPathName, &fe, TRUE, AFX_IDP_INVALID_FILENAME);
        return FALSE;
    }
    try {
        CArchive ar(pFile, CArchive::store | CArchive::bNoFlushOnDelete);
        ar.m_pDocument = this;
        ar.m_strFileName = lpszPathName;
        Serialize(ar);
        ar.Close();
        ReleaseFile(pFile, FALSE);
    } catch (CException* e) {
        ReleaseFile(pFile, TRUE);
        ReportSaveLoadException(lpszPathName, e, TRUE, AFX_IDP_FAILED_TO_SAVE_DOC);
        e->Delete();
        return FALSE;
    }
    SetModifiedFlag(FALSE);
    return TRUE;
}

void CDocument::ReportSaveLoadException(const char* lpszPathName, CException* e, BOOL bSaving, UINT nIDPDefault) {
    char msg[512];
    if (e && e->GetErrorMessage(msg, sizeof msg)) {
        AfxMessageBox(msg, MB_ICONEXCLAMATION);
        return;
    }
    CString prompt;
    AfxFormatString1(prompt, nIDPDefault, lpszPathName);
    if (prompt.IsEmpty())
        prompt.Format(bSaving ? "Failed to save document %s." : "Failed to open document %s.", lpszPathName);
    AfxMessageBox(prompt, MB_ICONEXCLAMATION);
}

void CDocument::OnCloseDocument() {
    BOOL autoDelete = m_bAutoDelete;
    m_bAutoDelete = FALSE;
    std::vector<CView*> views = m_viewList;
    for (CView* v : views) {
        CFrameWnd* frame = v->GetParentFrame();
        if (frame) {
            PreCloseFrame(frame);
            frame->DestroyWindow();
        }
    }
    m_bAutoDelete = autoDelete;
    DeleteContents();
    if (m_bAutoDelete)
        delete this;
}

BOOL CDocument::CanCloseFrame(CFrameWnd* pFrame) {
    int views = 0;
    for (CView* v : m_viewList)
        if (v->GetParentFrame() == pFrame)
            ++views;
    if (static_cast<size_t>(views) < m_viewList.size())
        return TRUE;
    return SaveModified();
}

BOOL CDocument::SaveModified() {
    if (!IsModified())
        return TRUE;
    CString name = m_strPathName.IsEmpty() ? m_strTitle : m_strPathName;
    CString prompt;
    AfxFormatString1(prompt, AFX_IDP_ASK_TO_SAVE, name);
    if (prompt.IsEmpty())
        prompt.Format("Save changes to %s?", name.GetString());
    switch (AfxMessageBox(prompt, MB_YESNOCANCEL | MB_ICONQUESTION)) {
    case IDCANCEL:
        return FALSE;
    case IDYES:
        return DoFileSave();
    default:
        return TRUE;
    }
}

BOOL CDocument::DoFileSave() {
    if (m_strPathName.IsEmpty() || access(NativePath(m_strPathName).c_str(), W_OK) != 0 && access(NativePath(m_strPathName).c_str(), F_OK) == 0)
        return DoSave(nullptr);
    return DoSave(m_strPathName);
}

BOOL CDocument::DoSave(const char* lpszPathName, BOOL bReplace) {
    CString newName = lpszPathName ? lpszPathName : "";
    if (newName.IsEmpty()) {
        newName = m_strPathName.IsEmpty() ? m_strTitle : m_strPathName;
        if (!AfxGetApp()->DoPromptFileName(newName, AFX_IDS_SAVEFILE, OFN_HIDEREADONLY | OFN_PATHMUSTEXIST,
                                           FALSE, m_pDocTemplate))
            return FALSE;
    }
    if (!OnSaveDocument(newName))
        return FALSE;
    if (bReplace)
        SetPathName(newName);
    return TRUE;
}

void CDocument::OnFileClose() {
    if (!SaveModified())
        return;
    OnCloseDocument();
}

void CDocument::OnFileSave() { DoFileSave(); }
void CDocument::OnFileSaveAs() { DoSave(nullptr); }

BOOL CDocument::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) {
    if (CCmdTarget::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
        return TRUE;
    return m_pDocTemplate && m_pDocTemplate->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

// ---------------------------------------------------------------------------------------------
// CView

IMPLEMENT_DYNAMIC(CView, CWnd)

BEGIN_MESSAGE_MAP(CView, CWnd)
    ON_WM_PAINT()
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_COMMAND(ID_FILE_PRINT, &CView::OnFilePrint)
    ON_COMMAND(ID_FILE_PRINT_DIRECT, &CView::OnFilePrint)
    ON_COMMAND(ID_FILE_PRINT_PREVIEW, &CView::OnFilePrintPreview)
END_MESSAGE_MAP()

CView::CView() : m_pDocument(nullptr) {}

CView::~CView() {
    if (m_pDocument)
        m_pDocument->RemoveView(this);
}

BOOL CView::PreCreateWindow(CREATESTRUCT& cs) {
    cs.style |= WS_CLIPSIBLINGS;
    return TRUE;
}

int CView::OnCreate(LPCREATESTRUCT lpcs) {
    auto* context = static_cast<CCreateContext*>(lpcs->lpCreateParams);
    if (context && context->m_pCurrentDoc)
        context->m_pCurrentDoc->AddView(this);
    return 0;
}

void CView::OnDestroy() {
    CFrameWnd* frame = GetParentFrame();
    if (frame && frame->GetActiveView() == this)
        frame->SetActiveView(nullptr, FALSE);
}

void CView::PostNcDestroy() { delete this; }

void CView::OnPaint() {
    CPaintDC dc(this);
    OnPrepareDC(&dc);
    OnDraw(&dc);
}

BOOL CView::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) {
    if (CWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
        return TRUE;
    return m_pDocument && m_pDocument->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

void CView::OnInitialUpdate() { OnUpdate(nullptr, 0, nullptr); }
void CView::OnUpdate(CView*, LPARAM, CObject*) { Invalidate(TRUE); }
void CView::OnPrepareDC(CDC*, CPrintInfo*) {}
void CView::OnActivateView(BOOL bActivate, CView*, CView*) {
    if (bActivate && m_hWnd)
        SetFocus();
}
BOOL CView::OnPreparePrinting(CPrintInfo* pInfo) { return DoPreparePrinting(pInfo); }
void CView::OnBeginPrinting(CDC*, CPrintInfo*) {}
void CView::OnPrint(CDC* pDC, CPrintInfo*) { OnDraw(pDC); }
void CView::OnEndPrinting(CDC*, CPrintInfo*) {}
BOOL CView::DoPreparePrinting(CPrintInfo*) { return FALSE; }

void CView::OnFilePrint() {
    AfxMessageBox("Printing is not supported in this version.", MB_ICONINFORMATION);
}

void CView::OnFilePrintPreview() { OnFilePrint(); }

IMPLEMENT_DYNAMIC(CScrollView, CView)

const SIZE CScrollView::sizeDefault = {0, 0};

CScrollView::CScrollView() : m_nMapMode(MM_TEXT) {}

void CScrollView::SetScrollSizes(int nMapMode, SIZE sizeTotal, const SIZE& sizePage, const SIZE& sizeLine) {
    m_nMapMode = nMapMode;
    m_totalLog = sizeTotal;
    m_totalDev = sizeTotal;
    m_pageDev = sizePage;
    m_lineDev = sizeLine;
    if (!m_hWnd)
        return;
    SCROLLINFO si;
    memset(&si, 0, sizeof si);
    si.fMask = SIF_RANGE | SIF_PAGE;
    CRect rc;
    GetClientRect(&rc);
    si.nMax = m_totalDev.cx - 1;
    si.nPage = static_cast<UINT>(rc.Width());
    SetScrollInfo(SB_HORZ, &si, TRUE);
    si.nMax = m_totalDev.cy - 1;
    si.nPage = static_cast<UINT>(rc.Height());
    SetScrollInfo(SB_VERT, &si, TRUE);
}

void CScrollView::SetScaleToFitSize(SIZE sizeTotal) { SetScrollSizes(MM_TEXT, sizeTotal); }
CPoint CScrollView::GetScrollPosition() const { return CPoint(GetScrollPos(SB_HORZ), GetScrollPos(SB_VERT)); }
CPoint CScrollView::GetDeviceScrollPosition() const { return GetScrollPosition(); }
void CScrollView::ScrollToPosition(POINT pt) {
    SetScrollPos(SB_HORZ, pt.x);
    SetScrollPos(SB_VERT, pt.y);
    Invalidate();
}
void CScrollView::FillOutsideRect(CDC*, CBrush*) {}
void CScrollView::ResizeParentToFit(BOOL) {}
void CScrollView::OnPrepareDC(CDC* pDC, CPrintInfo* pInfo) {
    CView::OnPrepareDC(pDC, pInfo);
    CPoint pos = GetScrollPosition();
    pDC->SetViewportOrg(-pos.x, -pos.y);
}

IMPLEMENT_DYNAMIC(CFormView, CScrollView)
CFormView::CFormView(UINT nIDTemplate) : m_nIDTemplate(nIDTemplate) {}
CFormView::CFormView(const char* lpszTemplateName) : m_nIDTemplate(static_cast<UINT>(ResRef::From(lpszTemplateName).id)) {}
void CFormView::OnInitialUpdate() {
    const rc::Dialog* tmpl = FindDialog(ResRef::FromId(m_nIDTemplate));
    if (tmpl && m_hWnd && GetWx()->GetChildren().empty()) {
        EnsureState(GetWx()).baseUnits = DialogUnitsToPixels(nullptr, 0, 0, 4, 8).GetSize();
        CreateDialogControls(GetWx(), *tmpl);
        ApplyDlgInit(this, tmpl->id);
    }
    UpdateData(FALSE);
    CScrollView::OnInitialUpdate();
}

// ---------------------------------------------------------------------------------------------
// CFrameWnd

IMPLEMENT_DYNCREATE(CFrameWnd, CWnd)

BEGIN_MESSAGE_MAP(CFrameWnd, CWnd)
    ON_WM_CREATE()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_SIZE()
    ON_WM_SETFOCUS()
    ON_UPDATE_COMMAND_UI(ID_VIEW_STATUS_BAR, &CFrameWnd::OnUpdateControlBarMenu)
    ON_UPDATE_COMMAND_UI(ID_VIEW_TOOLBAR, &CFrameWnd::OnUpdateControlBarMenu)
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_CAPS, &CFrameWnd::OnUpdateKeyIndicator)
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_NUM, &CFrameWnd::OnUpdateKeyIndicator)
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_SCRL, &CFrameWnd::OnUpdateKeyIndicator)
    ON_COMMAND_EX(ID_VIEW_STATUS_BAR, &CFrameWnd::OnBarCheck)
    ON_COMMAND_EX(ID_VIEW_TOOLBAR, &CFrameWnd::OnBarCheck)
    ON_COMMAND(ID_HELP, &CFrameWnd::OnHelp)
    ON_COMMAND(ID_CONTEXT_HELP, &CFrameWnd::OnContextHelp)
END_MESSAGE_MAP()

const CRect CFrameWnd::rectDefault(CW_USEDEFAULT, CW_USEDEFAULT, 0, 0);

CFrameWnd::CFrameWnd()
    : m_nIDHelp(0), m_hMenuDefault(nullptr), m_hAccelTable(nullptr), m_bAutoMenuEnable(TRUE), m_nIdleFlags(0),
      m_pViewActive(nullptr), m_pStatusBar(nullptr) {}

CFrameWnd::~CFrameWnd() {}

BOOL CFrameWnd::PreCreateWindow(CREATESTRUCT&) { return TRUE; }

void CFrameWnd::PostNcDestroy() { delete this; }

BOOL CFrameWnd::Create(const char*, const char* lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd,
                       const char* lpszMenuName, DWORD dwExStyle, CCreateContext* pContext) {
    return OnMain([&]() -> BOOL {
        CREATESTRUCT cs;
        memset(&cs, 0, sizeof cs);
        cs.style = static_cast<LONG>(dwStyle);
        cs.dwExStyle = dwExStyle;
        cs.lpszName = lpszWindowName;
        cs.x = rect.left;
        cs.y = rect.top;
        cs.cx = rect.right - rect.left;
        cs.cy = rect.bottom - rect.top;
        cs.lpCreateParams = pContext;
        if (!PreCreateWindow(cs))
            return FALSE;
        long style = wxDEFAULT_FRAME_STYLE;
        wxPoint pos = cs.x == CW_USEDEFAULT ? wxDefaultPosition : wxPoint(cs.x, cs.y);
        wxSize size = cs.cx <= 0 ? wxSize(1000, 720) : wxSize(cs.cx, cs.cy);
        auto* frame = new MfcFrame(pParentWnd && pParentWnd->m_hWnd ? pParentWnd->GetWx() : nullptr,
                                   ToWx(lpszWindowName ? lpszWindowName : ""), pos, size, style);
        HookWindow(frame, ControlKind::Frame, 0, static_cast<DWORD>(cs.style), dwExStyle);
        EnsureState(frame).text = ToWx(lpszWindowName ? lpszWindowName : "");
        AttachWx(frame);
        if (lpszMenuName) {
            const rc::Menu* menu = FindMenu(ResRef::From(lpszMenuName));
            if (menu) {
                wxMenuBar* bar = BuildMenuBar(*menu);
                frame->SetMenuBar(bar);
                m_hMenuDefault = HandleFromWxMenuBar(bar);
            }
        }
        if (SendMessage(WM_CREATE, 0, reinterpret_cast<LPARAM>(&cs)) == -1) {
            DestroyWindow();
            return FALSE;
        }
        return m_hWnd != nullptr;
    });
}

BOOL CFrameWnd::LoadFrame(UINT nIDResource, DWORD dwDefaultStyle, CWnd* pParentWnd, CCreateContext* pContext) {
    CString title = ResourceString(nIDResource);
    int nl = title.Find('\n');
    if (nl >= 0)
        title = title.Left(nl);
    m_strTitle = title;
    const rc::AccelTable* accel = FindAccelTable(ResRef::FromId(nIDResource));
    SetMenuAccelerators(accel);
    BOOL ok = Create(nullptr, title, dwDefaultStyle, rectDefault, pParentWnd, MAKEINTRESOURCE(nIDResource), 0, pContext);
    SetMenuAccelerators(nullptr);
    if (!ok)
        return FALSE;
    m_nIDHelp = nIDResource;
    m_hAccelTable = reinterpret_cast<HACCEL>(const_cast<rc::AccelTable*>(accel));
    OnMain([&] {
        wxIcon icon = LoadIconResource(ResRef::FromId(nIDResource));
        if (icon.IsOk())
            static_cast<wxTopLevelWindow*>(GetWx())->SetIcon(icon);
        if (accel) {
            std::vector<wxAcceleratorEntry> entries;
            wxMenuBar* bar = static_cast<wxFrame*>(GetWx())->GetMenuBar();
            for (int i = 0; i < accel->itemCount; ++i) {
                const rc::Accel& a = accel->items[i];
                if (bar && bar->FindItem(MenuWxId(a.id)))
                    continue;
                int flags = (a.flags & FCONTROL ? wxACCEL_CTRL : 0) | (a.flags & FALT ? wxACCEL_ALT : 0) |
                            (a.flags & FSHIFT ? wxACCEL_SHIFT : 0);
                int key = a.flags & FVIRTKEY ? WxKeyFromVirtual(static_cast<UINT>(a.key)) : a.key;
                if (!(a.flags & FVIRTKEY) && a.key > 0 && a.key < 32) {
                    flags |= wxACCEL_CTRL;
                    key = 'A' + a.key - 1;
                }
                if (key)
                    entries.emplace_back(flags, key, MenuWxId(a.id));
            }
            if (!entries.empty())
                GetWx()->SetAcceleratorTable(wxAcceleratorTable(static_cast<int>(entries.size()), entries.data()));
        }
    });
    OnUpdateFrameTitle(TRUE);
    return TRUE;
}

int CFrameWnd::OnCreate(LPCREATESTRUCT lpcs) {
    auto* context = static_cast<CCreateContext*>(lpcs->lpCreateParams);
    return OnCreateClient(lpcs, context) ? 0 : -1;
}

BOOL CFrameWnd::OnCreateClient(LPCREATESTRUCT, CCreateContext* pContext) {
    if (pContext && pContext->m_pNewViewClass)
        return CreateView(pContext, AFX_IDW_PANE_FIRST) != nullptr;
    return TRUE;
}

CWnd* CFrameWnd::CreateView(CCreateContext* pContext, UINT nID) {
    CObject* obj = pContext->m_pNewViewClass->CreateObject();
    auto* view = dynamic_cast<CWnd*>(obj);
    if (!view) {
        delete obj;
        return nullptr;
    }
    CRect rc;
    GetClientRect(&rc);
    if (!view->Create(nullptr, nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER, rc, this, nID, pContext))
        return nullptr;
    RecalcLayout();
    return view;
}

void CFrameWnd::OnClose() {
    CWinApp* app = AfxGetApp();
    if (app && app->m_pMainWnd == this) {
        if (!app->SaveAllModified())
            return;
        app->CloseAllDocuments(FALSE);
        DestroyWindow();
        return;
    }
    CDocument* doc = GetActiveDocument();
    if (doc && !doc->CanCloseFrame(this))
        return;
    if (doc && doc->m_viewList.size() == 1) {
        doc->OnCloseDocument();
        return;
    }
    DestroyWindow();
}

void CFrameWnd::OnDestroy() {
    CWinApp* app = AfxGetApp();
    if (app && app->m_pMainWnd == this) {
        app->m_pMainWnd = nullptr;
        PostToMainThread([] {
            if (wxTheApp)
                wxTheApp->ExitMainLoop();
        });
    }
}

void CFrameWnd::OnSize(UINT, int, int) { RecalcLayout(); }

void CFrameWnd::OnSetFocus(CWnd*) {
    if (m_pViewActive && m_pViewActive->m_hWnd)
        m_pViewActive->SetFocus();
}

void CFrameWnd::RecalcLayout(BOOL) {
    OnMain([&] {
        if (!m_hWnd)
            return;
        wxWindow* frame = GetWx();
        wxSize client = frame->GetClientSize();
        for (wxWindow* c : frame->GetChildren()) {
            WindowState* st = GetState(c);
            if (st && st->winId == AFX_IDW_PANE_FIRST)
                c->SetSize(0, 0, client.x, client.y);
        }
    });
}

BOOL CFrameWnd::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) {
    CView* view = GetActiveView();
    if (view && view->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
        return TRUE;
    if (CWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
        return TRUE;
    CWinApp* app = AfxGetApp();
    return app && app->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

BOOL CFrameWnd::OnCommand(WPARAM wParam, LPARAM lParam) { return CWnd::OnCommand(wParam, lParam); }

BOOL CFrameWnd::PreTranslateMessage(MSG* pMsg) { return CWnd::PreTranslateMessage(pMsg); }

CFrameWnd* CFrameWnd::GetActiveFrame() { return this; }

CView* CFrameWnd::GetActiveView() const {
    if (m_pViewActive)
        return m_pViewActive;
    if (!m_hWnd)
        return nullptr;
    for (wxWindow* c : GetWx()->GetChildren()) {
        CWnd* p = PermanentWnd(c);
        if (p && p->IsKindOf(RUNTIME_CLASS(CView)))
            return static_cast<CView*>(p);
    }
    return nullptr;
}

void CFrameWnd::SetActiveView(CView* pViewNew, BOOL bNotify) {
    CView* old = m_pViewActive;
    if (old == pViewNew)
        return;
    m_pViewActive = pViewNew;
    if (bNotify) {
        if (old)
            old->OnActivateView(FALSE, pViewNew, old);
        if (pViewNew)
            pViewNew->OnActivateView(TRUE, pViewNew, old);
    }
}

CDocument* CFrameWnd::GetActiveDocument() {
    CView* v = GetActiveView();
    return v ? v->GetDocument() : nullptr;
}

void CFrameWnd::InitialUpdateFrame(CDocument* pDoc, BOOL bMakeVisible) {
    CView* view = GetActiveView();
    if (m_hWnd)
        SendMessageToDescendants(WM_INITIALUPDATE, 0, 0, TRUE, TRUE);
    std::vector<CView*> views;
    if (pDoc)
        views = pDoc->m_viewList;
    else if (view)
        views.push_back(view);
    for (CView* v : views)
        if (v->GetParentFrame() == this)
            v->OnInitialUpdate();
    if (view)
        SetActiveView(view, TRUE);
    if (bMakeVisible)
        ActivateFrame();
    OnUpdateFrameTitle(TRUE);
}

void CFrameWnd::ActivateFrame(int nCmdShow) {
    ShowWindow(nCmdShow == -1 ? SW_SHOW : nCmdShow);
    BringWindowToTop();
}

void CFrameWnd::OnUpdateFrameTitle(BOOL bAddToTitle) {
    CDocument* doc = GetActiveDocument();
    if (bAddToTitle && doc)
        UpdateFrameTitleForDocument(doc->GetTitle());
    else
        UpdateFrameTitleForDocument(nullptr);
}

void CFrameWnd::UpdateFrameTitleForDocument(const char* lpszDocName) {
    CString title = m_strTitle;
    if (lpszDocName && *lpszDocName)
        title = title.IsEmpty() ? CString(lpszDocName) : title + " - " + lpszDocName;
    SetWindowText(title);
}

void CFrameWnd::OnUpdateFrameMenu(HMENU) {}

void CFrameWnd::GetMessageString(UINT nID, CString& rMessage) const {
    rMessage.LoadString(nID);
    int nl = rMessage.Find('\n');
    if (nl >= 0)
        rMessage = rMessage.Left(nl);
}

void CFrameWnd::SetMessageText(const char* lpszText) {
    OnMain([&] {
        auto* frame = wxDynamicCast(GetTopLevelFrame() && GetTopLevelFrame()->m_hWnd ? GetTopLevelFrame()->GetWx() : nullptr, wxFrame);
        if (frame && frame->GetStatusBar())
            frame->GetStatusBar()->SetStatusText(ToWx(lpszText ? lpszText : ""), 0);
    });
}

void CFrameWnd::SetMessageText(UINT nID) {
    CString s;
    GetMessageString(nID, s);
    SetMessageText(s);
}

void CFrameWnd::EnableDocking(DWORD) {}
void CFrameWnd::DockControlBar(CControlBar*, UINT, LPCRECT) {}

void CFrameWnd::ShowControlBar(CControlBar* pBar, BOOL bShow, BOOL) {
    if (pBar && pBar->m_hWnd) {
        pBar->ShowWindow(bShow ? SW_SHOW : SW_HIDE);
        OnMain([&] {
            if (m_hWnd)
                GetWx()->SendSizeEvent();
        });
    }
}

CControlBar* CFrameWnd::GetControlBar(UINT nID) {
    for (CControlBar* b : m_listControlBars)
        if (static_cast<UINT>(b->GetDlgCtrlID()) == nID)
            return b;
    return nullptr;
}

void CFrameWnd::OnUpdateControlBarMenu(CCmdUI* pCmdUI) {
    CControlBar* bar = GetControlBar(pCmdUI->m_nID);
    if (!bar) {
        pCmdUI->ContinueRouting();
        return;
    }
    pCmdUI->SetCheck(bar->IsWindowVisible());
}

void CFrameWnd::OnUpdateKeyIndicator(CCmdUI* pCmdUI) {
    int vk = 0;
    switch (pCmdUI->m_nID) {
    case ID_INDICATOR_CAPS: vk = VK_CAPITAL; break;
    case ID_INDICATOR_NUM: vk = VK_NUMLOCK; break;
    case ID_INDICATOR_SCRL: vk = VK_SCROLL; break;
    }
    pCmdUI->Enable(vk && (::GetKeyState(vk) & 1));
}

BOOL CFrameWnd::OnBarCheck(UINT nID) {
    CControlBar* bar = GetControlBar(nID);
    if (!bar)
        return FALSE;
    ShowControlBar(bar, !bar->IsWindowVisible(), FALSE);
    return TRUE;
}

void CFrameWnd::OnHelp() {
    CWinApp* app = AfxGetApp();
    if (app)
        app->WinHelpInternal(0, HELP_FINDER);
}

void CFrameWnd::OnContextHelp() { OnHelp(); }

// ---------------------------------------------------------------------------------------------
// MDI

IMPLEMENT_DYNAMIC(CMDIFrameWnd, CFrameWnd)

BEGIN_MESSAGE_MAP(CMDIFrameWnd, CFrameWnd)
    ON_COMMAND(ID_WINDOW_NEW, &CMDIFrameWnd::OnWindowNew)
    ON_COMMAND(ID_WINDOW_CASCADE, &CMDIFrameWnd::OnWindowCascade)
    ON_COMMAND(ID_WINDOW_TILE_HORZ, &CMDIFrameWnd::OnWindowTile)
    ON_COMMAND(ID_WINDOW_TILE_VERT, &CMDIFrameWnd::OnWindowTile)
    ON_COMMAND(ID_WINDOW_ARRANGE, &CMDIFrameWnd::OnWindowCascade)
    ON_UPDATE_COMMAND_UI(ID_WINDOW_CASCADE, &CMDIFrameWnd::OnUpdateMDIWindowCmd)
    ON_UPDATE_COMMAND_UI(ID_WINDOW_TILE_HORZ, &CMDIFrameWnd::OnUpdateMDIWindowCmd)
    ON_UPDATE_COMMAND_UI(ID_WINDOW_TILE_VERT, &CMDIFrameWnd::OnUpdateMDIWindowCmd)
    ON_UPDATE_COMMAND_UI(ID_WINDOW_ARRANGE, &CMDIFrameWnd::OnUpdateMDIWindowCmd)
    ON_UPDATE_COMMAND_UI(ID_WINDOW_NEW, &CMDIFrameWnd::OnUpdateMDIWindowCmd)
END_MESSAGE_MAP()

CMDIFrameWnd::CMDIFrameWnd() : m_hWndMDIClient(nullptr) {}

BOOL CMDIFrameWnd::LoadFrame(UINT nIDResource, DWORD dwDefaultStyle, CWnd* pParentWnd, CCreateContext* pContext) {
    if (!CFrameWnd::LoadFrame(nIDResource, dwDefaultStyle, pParentWnd, pContext))
        return FALSE;
    return TRUE;
}

BOOL CMDIFrameWnd::OnCreateClient(LPCREATESTRUCT, CCreateContext*) {
    return OnMain([&]() -> BOOL {
        wxWindow* frame = GetWx();
        auto* book = new wxAuiNotebook(frame, ToWxId(AFX_IDW_PANE_FIRST), wxDefaultPosition, frame->GetClientSize(),
                                       wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_CLOSE_ON_ALL_TABS | wxAUI_NB_WINDOWLIST_BUTTON);
        HookWindow(book, ControlKind::Generic, AFX_IDW_PANE_FIRST, WS_CHILD | WS_VISIBLE, 0);
        m_hWndMDIClient = ToHwnd(book);
        auto* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(book, 1, wxEXPAND);
        frame->SetSizer(sizer);
        book->Bind(wxEVT_AUINOTEBOOK_PAGE_CHANGED, [this](wxAuiNotebookEvent& e) {
            e.Skip();
            OnChildActivated();
        });
        book->Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE, [book](wxAuiNotebookEvent& e) {
            e.Veto();
            wxWindow* page = book->GetPage(static_cast<size_t>(e.GetSelection()));
            if (page && IsManagedWindow(page))
                DispatchMessageTo(ToHwnd(page), WM_CLOSE, 0, 0);
        });
        return TRUE;
    });
}

void CMDIFrameWnd::OnChildActivated() {
    CMDIChildWnd* active = MDIGetActive();
    for (CMDIChildWnd* child : m_children) {
        bool now = child == active;
        if (child->m_bActive != now) {
            child->m_bActive = now;
            CView* view = child->GetActiveView();
            if (view)
                view->OnActivateView(now, now ? view : nullptr, now ? nullptr : view);
            child->SendMessage(WM_MDIACTIVATE, reinterpret_cast<WPARAM>(now ? nullptr : child->m_hWnd),
                               reinterpret_cast<LPARAM>(now ? child->m_hWnd : nullptr));
        }
    }
    OnUpdateFrameMenu(active ? active->m_hMenuShared : nullptr);
    OnUpdateFrameTitle(TRUE);
}

BOOL CMDIFrameWnd::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) {
    CMDIChildWnd* child = MDIGetActive();
    if (child && child->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
        return TRUE;
    return CFrameWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

CFrameWnd* CMDIFrameWnd::GetActiveFrame() {
    CMDIChildWnd* child = MDIGetActive();
    return child ? static_cast<CFrameWnd*>(child) : this;
}

void CMDIFrameWnd::OnUpdateFrameTitle(BOOL bAddToTitle) {
    CMDIChildWnd* child = MDIGetActive();
    CDocument* doc = child ? child->GetActiveDocument() : nullptr;
    UpdateFrameTitleForDocument(bAddToTitle && doc ? doc->GetTitle().GetString() : nullptr);
}

void CMDIFrameWnd::OnUpdateFrameMenu(HMENU hMenuAlt) {
    OnMain([&] {
        auto* frame = wxDynamicCast(GetWx(), wxFrame);
        if (!frame)
            return;
        wxMenuBar* bar = WxMenuBarFromHandle(hMenuAlt ? hMenuAlt : m_hMenuDefault);
        if (bar && frame->GetMenuBar() != bar)
            frame->SetMenuBar(bar);
    });
}

void CMDIFrameWnd::RecalcLayout(BOOL) {
    OnMain([&] {
        if (m_hWnd)
            GetWx()->Layout();
    });
}

void CMDIFrameWnd::MDIActivate(CWnd* pWndActivate) {
    if (!pWndActivate || !pWndActivate->m_hWnd)
        return;
    OnMain([&] {
        wxAuiNotebook* book = ClientNotebook(this);
        if (!book)
            return;
        int index = book->GetPageIndex(pWndActivate->GetWx());
        if (index != wxNOT_FOUND && book->GetSelection() != index)
            book->SetSelection(static_cast<size_t>(index));
        else
            OnChildActivated();
    });
}

CMDIChildWnd* CMDIFrameWnd::MDIGetActive(BOOL* pbMaximized) const {
    if (pbMaximized)
        *pbMaximized = TRUE;
    return OnMain([&]() -> CMDIChildWnd* {
        wxAuiNotebook* book = ClientNotebook(const_cast<CMDIFrameWnd*>(this));
        if (!book || book->GetSelection() == wxNOT_FOUND)
            return nullptr;
        CWnd* p = PermanentWnd(book->GetPage(static_cast<size_t>(book->GetSelection())));
        return p && p->IsKindOf(RUNTIME_CLASS(CMDIChildWnd)) ? static_cast<CMDIChildWnd*>(p) : nullptr;
    });
}

void CMDIFrameWnd::MDIMaximize(CWnd* pWnd) { MDIActivate(pWnd); }
void CMDIFrameWnd::MDIRestore(CWnd* pWnd) { MDIActivate(pWnd); }

void CMDIFrameWnd::MDINext() {
    OnMain([&] {
        if (wxAuiNotebook* book = ClientNotebook(this))
            book->AdvanceSelection(true);
    });
}

void CMDIFrameWnd::MDIPrev() {
    OnMain([&] {
        if (wxAuiNotebook* book = ClientNotebook(this))
            book->AdvanceSelection(false);
    });
}

CMenu* CMDIFrameWnd::MDISetMenu(CMenu* pFrameMenu, CMenu*) {
    if (pFrameMenu)
        OnUpdateFrameMenu(pFrameMenu->m_hMenu);
    return nullptr;
}

void CMDIFrameWnd::MDITile() {}
void CMDIFrameWnd::MDICascade() {}

CMDIChildWnd* CMDIFrameWnd::CreateNewChild(CRuntimeClass* pClass, UINT nResource, HMENU hMenu, HACCEL hAccel) {
    auto* child = static_cast<CMDIChildWnd*>(pClass->CreateObject());
    if (!child->LoadFrame(nResource, WS_CHILD | WS_VISIBLE | WS_OVERLAPPEDWINDOW, this, nullptr))
        return nullptr;
    child->SetHandles(hMenu, hAccel);
    child->InitialUpdateFrame(nullptr, TRUE);
    return child;
}

void CMDIFrameWnd::OnWindowNew() {}
void CMDIFrameWnd::OnWindowCascade() {}
void CMDIFrameWnd::OnWindowTile() {}
void CMDIFrameWnd::OnUpdateMDIWindowCmd(CCmdUI* pCmdUI) { pCmdUI->Enable(MDIGetActive() != nullptr); }

IMPLEMENT_DYNCREATE(CMDIChildWnd, CFrameWnd)

BEGIN_MESSAGE_MAP(CMDIChildWnd, CFrameWnd)
END_MESSAGE_MAP()

CMDIChildWnd::CMDIChildWnd() : m_hMenuShared(nullptr), m_pMDIFrame(nullptr), m_bActive(false) {}

CMDIChildWnd::~CMDIChildWnd() {
    if (m_pMDIFrame) {
        auto& kids = m_pMDIFrame->m_children;
        kids.erase(std::remove(kids.begin(), kids.end(), this), kids.end());
    }
}

BOOL CMDIChildWnd::PreCreateWindow(CREATESTRUCT&) { return TRUE; }

BOOL CMDIChildWnd::Create(const char*, const char* lpszWindowName, DWORD dwStyle, const RECT&, CMDIFrameWnd* pParentWnd,
                          CCreateContext* pContext) {
    CMDIFrameWnd* mdi = pParentWnd ? pParentWnd : MainMDIFrame();
    if (!mdi)
        return FALSE;
    return OnMain([&]() -> BOOL {
        wxAuiNotebook* book = ClientNotebook(mdi);
        if (!book)
            return FALSE;
        CREATESTRUCT cs;
        memset(&cs, 0, sizeof cs);
        cs.style = static_cast<LONG>(dwStyle);
        cs.lpszName = lpszWindowName;
        cs.lpCreateParams = pContext;
        if (!PreCreateWindow(cs))
            return FALSE;
        auto* panel = new MfcChildPanel(book);
        HookWindow(panel, ControlKind::MDIChild, 0, static_cast<DWORD>(cs.style), 0);
        EnsureState(panel).text = ToWx(lpszWindowName ? lpszWindowName : "");
        EnsureState(panel).owner = mdi->m_hWnd;
        AttachWx(panel);
        m_pMDIFrame = mdi;
        mdi->m_children.push_back(this);
        panel->Bind(wxEVT_CLOSE_WINDOW, [panel](wxCloseEvent& e) {
            if (IsManagedWindow(panel) && PermanentWnd(panel)) {
                e.Veto();
                DispatchMessageTo(ToHwnd(panel), WM_CLOSE, 0, 0);
            } else {
                e.Skip();
            }
        });
        if (SendMessage(WM_CREATE, 0, reinterpret_cast<LPARAM>(&cs)) == -1) {
            DestroyWindow();
            return FALSE;
        }
        book->AddPage(panel, ToWx(lpszWindowName ? lpszWindowName : ""), false);
        return m_hWnd != nullptr;
    });
}

BOOL CMDIChildWnd::LoadFrame(UINT nIDResource, DWORD dwDefaultStyle, CWnd* pParentWnd, CCreateContext* pContext) {
    m_nIDHelp = nIDResource;
    CString title = ResourceString(nIDResource);
    int nl = title.Find('\n');
    if (nl >= 0)
        title = title.Left(nl);
    CMDIFrameWnd* mdi = pParentWnd && pParentWnd->IsKindOf(RUNTIME_CLASS(CMDIFrameWnd)) ? static_cast<CMDIFrameWnd*>(pParentWnd)
                                                                                           : MainMDIFrame();
    return Create(nullptr, title, dwDefaultStyle, rectDefault, mdi, pContext);
}

BOOL CMDIChildWnd::DestroyWindow() {
    CMDIFrameWnd* mdi = m_pMDIFrame;
    if (mdi) {
        auto& kids = mdi->m_children;
        kids.erase(std::remove(kids.begin(), kids.end(), this), kids.end());
        m_pMDIFrame = nullptr;
    }
    if (!m_hWnd)
        return FALSE;
    return OnMain([&]() -> BOOL {
        wxWindow* panel = GetWx();
        wxAuiNotebook* book = ClientNotebook(mdi);
        WindowState* st = GetState(panel);
        if (st && !st->destroyNotified) {
            st->destroyNotified = true;
            SendMessage(WM_DESTROY);
        }
        if (book) {
            int index = book->GetPageIndex(panel);
            if (index != wxNOT_FOUND)
                book->RemovePage(static_cast<size_t>(index));
        }
        DestroyWxWindow(panel);
        if (mdi)
            PostToMainThread([mdi] {
                if (mdi->m_hWnd)
                    mdi->OnChildActivated();
            });
        return TRUE;
    });
}

void CMDIChildWnd::ActivateFrame(int nCmdShow) {
    if (nCmdShow == SW_HIDE) {
        ShowWindow(SW_HIDE);
        return;
    }
    CMDIFrameWnd* mdi = GetMDIFrame();
    if (mdi)
        mdi->MDIActivate(this);
}

void CMDIChildWnd::OnUpdateFrameMenu(BOOL bActive, CWnd*, HMENU hMenuAlt) {
    CMDIFrameWnd* mdi = GetMDIFrame();
    if (mdi)
        mdi->OnUpdateFrameMenu(bActive ? (hMenuAlt ? hMenuAlt : m_hMenuShared) : nullptr);
}

void CMDIChildWnd::OnUpdateFrameTitle(BOOL bAddToTitle) {
    CDocument* doc = GetActiveDocument();
    CString title = doc && bAddToTitle ? doc->GetTitle() : m_strTitle;
    OnMain([&] {
        if (!m_hWnd)
            return;
        EnsureState(GetWx()).text = ToWx(title);
        if (wxAuiNotebook* book = ClientNotebook(GetMDIFrame())) {
            int index = book->GetPageIndex(GetWx());
            if (index != wxNOT_FOUND)
                book->SetPageText(static_cast<size_t>(index), ToWx(title));
        }
    });
    if (CMDIFrameWnd* mdi = GetMDIFrame())
        if (mdi->MDIGetActive() == this)
            mdi->OnUpdateFrameTitle(TRUE);
}

CMDIFrameWnd* CMDIChildWnd::GetMDIFrame() const { return m_pMDIFrame ? m_pMDIFrame : MainMDIFrame(); }
void CMDIChildWnd::MDIDestroy() { DestroyWindow(); }
void CMDIChildWnd::MDIActivate() { ActivateFrame(); }
void CMDIChildWnd::MDIMaximize() { ActivateFrame(); }
void CMDIChildWnd::MDIRestore() { ActivateFrame(); }
BOOL CMDIChildWnd::PreTranslateMessage(MSG* pMsg) { return CFrameWnd::PreTranslateMessage(pMsg); }

// ---------------------------------------------------------------------------------------------
// Document templates

IMPLEMENT_DYNAMIC(CDocTemplate, CCmdTarget)
IMPLEMENT_DYNAMIC(CMultiDocTemplate, CDocTemplate)
IMPLEMENT_DYNAMIC(CSingleDocTemplate, CDocTemplate)

CDocTemplate::CDocTemplate(UINT nIDResource, CRuntimeClass* pDocClass, CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass)
    : m_nIDResource(nIDResource), m_pDocClass(pDocClass), m_pFrameClass(pFrameClass), m_pViewClass(pViewClass),
      m_bAutoDelete(TRUE) {
    LoadTemplate();
}

CDocTemplate::~CDocTemplate() {}

void CDocTemplate::LoadTemplate() {
    if (m_strDocStrings.IsEmpty())
        m_strDocStrings.LoadString(m_nIDResource);
}

void CDocTemplate::AddDocument(CDocument* pDoc) { pDoc->m_pDocTemplate = this; }

void CDocTemplate::RemoveDocument(CDocument* pDoc) {
    if (pDoc->m_pDocTemplate == this)
        pDoc->m_pDocTemplate = nullptr;
}

BOOL CDocTemplate::GetDocString(CString& rString, enum DocStringIndex index) const {
    return AfxExtractSubString(rString, m_strDocStrings, static_cast<int>(index), '\n');
}

CDocument* CDocTemplate::CreateNewDocument() {
    if (!m_pDocClass)
        return nullptr;
    auto* doc = dynamic_cast<CDocument*>(m_pDocClass->CreateObject());
    if (doc)
        AddDocument(doc);
    return doc;
}

CFrameWnd* CDocTemplate::CreateNewFrame(CDocument* pDoc, CFrameWnd* pOther) {
    CCreateContext context;
    context.m_pCurrentFrame = pOther;
    context.m_pCurrentDoc = pDoc;
    context.m_pNewViewClass = m_pViewClass;
    context.m_pNewDocTemplate = this;
    if (!m_pFrameClass)
        return nullptr;
    auto* frame = dynamic_cast<CFrameWnd*>(m_pFrameClass->CreateObject());
    if (!frame)
        return nullptr;
    if (!frame->LoadFrame(m_nIDResource, WS_OVERLAPPEDWINDOW, nullptr, &context))
        return nullptr;
    return frame;
}

void CDocTemplate::InitialUpdateFrame(CFrameWnd* pFrame, CDocument* pDoc, BOOL bMakeVisible) {
    pFrame->InitialUpdateFrame(pDoc, bMakeVisible);
}

BOOL CDocTemplate::SaveAllModified() {
    POSITION pos = GetFirstDocPosition();
    while (pos) {
        CDocument* doc = GetNextDoc(pos);
        if (doc && !doc->SaveModified())
            return FALSE;
    }
    return TRUE;
}

void CDocTemplate::CloseAllDocuments(BOOL) {
    for (;;) {
        POSITION pos = GetFirstDocPosition();
        if (!pos)
            break;
        CDocument* doc = GetNextDoc(pos);
        if (!doc)
            break;
        doc->OnCloseDocument();
    }
}

CDocTemplate::Confidence CDocTemplate::MatchDocType(const char* lpszPathName, CDocument*& rpDocMatch) {
    rpDocMatch = nullptr;
    POSITION pos = GetFirstDocPosition();
    while (pos) {
        CDocument* doc = GetNextDoc(pos);
        if (doc && doc->GetPathName().CompareNoCase(lpszPathName) == 0) {
            rpDocMatch = doc;
            return yesAlreadyOpen;
        }
    }
    CString ext;
    if (GetDocString(ext, filterExt) && !ext.IsEmpty()) {
        CString path(lpszPathName);
        if (path.GetLength() >= ext.GetLength() && path.Right(ext.GetLength()).CompareNoCase(ext) == 0)
            return yesAttemptNative;
    }
    return yesAttemptForeign;
}

CMultiDocTemplate::CMultiDocTemplate(UINT nIDResource, CRuntimeClass* pDocClass, CRuntimeClass* pFrameClass,
                                     CRuntimeClass* pViewClass)
    : CDocTemplate(nIDResource, pDocClass, pFrameClass, pViewClass), m_hMenuShared(nullptr), m_hAccelTable(nullptr),
      m_nUntitledCount(0) {
    LoadTemplate();
}

CMultiDocTemplate::~CMultiDocTemplate() {}

void CMultiDocTemplate::LoadTemplate() {
    CDocTemplate::LoadTemplate();
    if (m_hMenuShared)
        return;
    const rc::Menu* menu = FindMenu(ResRef::FromId(m_nIDResource));
    if (menu && wxTheApp) {
        SetMenuAccelerators(FindAccelTable(ResRef::FromId(m_nIDResource)));
        if (!FindAccelTable(ResRef::FromId(m_nIDResource)))
            SetMenuAccelerators(FindAccelTable(ResRef::FromId(128)));
        m_hMenuShared = HandleFromWxMenuBar(BuildMenuBar(*menu));
        SetMenuAccelerators(nullptr);
    }
}

POSITION CMultiDocTemplate::GetFirstDocPosition() const {
    return m_docList.empty() ? nullptr : reinterpret_cast<POSITION>(static_cast<uintptr_t>(1));
}

CDocument* CMultiDocTemplate::GetNextDoc(POSITION& rPos) const {
    size_t i = static_cast<size_t>(reinterpret_cast<uintptr_t>(rPos)) - 1;
    CDocument* d = i < m_docList.size() ? m_docList[i] : nullptr;
    rPos = i + 1 < m_docList.size() ? reinterpret_cast<POSITION>(static_cast<uintptr_t>(i + 2)) : nullptr;
    return d;
}

void CMultiDocTemplate::AddDocument(CDocument* pDoc) {
    CDocTemplate::AddDocument(pDoc);
    m_docList.push_back(pDoc);
}

void CMultiDocTemplate::RemoveDocument(CDocument* pDoc) {
    CDocTemplate::RemoveDocument(pDoc);
    m_docList.erase(std::remove(m_docList.begin(), m_docList.end(), pDoc), m_docList.end());
}

CDocument* CMultiDocTemplate::OpenDocumentFile(const char* lpszPathName, BOOL bMakeVisible) {
    LoadTemplate();
    CDocument* doc = CreateNewDocument();
    if (!doc)
        return nullptr;
    BOOL autoDelete = doc->m_bAutoDelete;
    doc->m_bAutoDelete = FALSE;
    CFrameWnd* frame = CreateNewFrame(doc, nullptr);
    doc->m_bAutoDelete = autoDelete;
    if (!frame) {
        delete doc;
        return nullptr;
    }
    if (frame->IsKindOf(RUNTIME_CLASS(CMDIChildWnd)))
        static_cast<CMDIChildWnd*>(frame)->SetHandles(m_hMenuShared, m_hAccelTable);
    if (!lpszPathName) {
        SetDefaultTitle(doc);
        if (!doc->OnNewDocument()) {
            frame->DestroyWindow();
            return nullptr;
        }
        ++m_nUntitledCount;
    } else {
        CWaitCursor wait;
        if (!doc->OnOpenDocument(lpszPathName)) {
            frame->DestroyWindow();
            return nullptr;
        }
        doc->SetPathName(lpszPathName);
    }
    InitialUpdateFrame(frame, doc, bMakeVisible);
    return doc;
}

void CMultiDocTemplate::SetDefaultTitle(CDocument* pDocument) {
    CString name;
    if (!GetDocString(name, docName) || name.IsEmpty())
        name = "Untitled";
    CString title;
    title.Format("%s%u", name.GetString(), m_nUntitledCount + 1);
    pDocument->SetTitle(title);
}

CSingleDocTemplate::CSingleDocTemplate(UINT nIDResource, CRuntimeClass* pDocClass, CRuntimeClass* pFrameClass,
                                       CRuntimeClass* pViewClass)
    : CDocTemplate(nIDResource, pDocClass, pFrameClass, pViewClass), m_pOnlyDoc(nullptr) {}

POSITION CSingleDocTemplate::GetFirstDocPosition() const {
    return m_pOnlyDoc ? reinterpret_cast<POSITION>(static_cast<uintptr_t>(1)) : nullptr;
}

CDocument* CSingleDocTemplate::GetNextDoc(POSITION& rPos) const {
    CDocument* d = rPos ? m_pOnlyDoc : nullptr;
    rPos = nullptr;
    return d;
}

void CSingleDocTemplate::AddDocument(CDocument* pDoc) {
    CDocTemplate::AddDocument(pDoc);
    m_pOnlyDoc = pDoc;
}

void CSingleDocTemplate::RemoveDocument(CDocument* pDoc) {
    CDocTemplate::RemoveDocument(pDoc);
    if (m_pOnlyDoc == pDoc)
        m_pOnlyDoc = nullptr;
}

CDocument* CSingleDocTemplate::OpenDocumentFile(const char* lpszPathName, BOOL bMakeVisible) {
    if (m_pOnlyDoc) {
        if (!m_pOnlyDoc->SaveModified())
            return nullptr;
        m_pOnlyDoc->OnCloseDocument();
    }
    CDocument* doc = CreateNewDocument();
    if (!doc)
        return nullptr;
    CFrameWnd* frame = CreateNewFrame(doc, nullptr);
    if (!frame)
        return nullptr;
    if (!lpszPathName) {
        SetDefaultTitle(doc);
        doc->OnNewDocument();
    } else if (!doc->OnOpenDocument(lpszPathName)) {
        frame->DestroyWindow();
        return nullptr;
    } else {
        doc->SetPathName(lpszPathName);
    }
    InitialUpdateFrame(frame, doc, bMakeVisible);
    return doc;
}

void CSingleDocTemplate::SetDefaultTitle(CDocument* pDocument) { pDocument->SetTitle("Untitled"); }

// ---------------------------------------------------------------------------------------------
// Control bars

IMPLEMENT_DYNAMIC(CControlBar, CWnd)
IMPLEMENT_DYNAMIC(CToolBar, CControlBar)
IMPLEMENT_DYNAMIC(CStatusBar, CControlBar)

CControlBar::CControlBar() : m_dwStyle(0), m_dwDockStyle(0) {}
CSize CControlBar::CalcFixedLayout(BOOL, BOOL) { return CSize(0, 0); }
void CControlBar::OnUpdateCmdUI(CFrameWnd*, BOOL) {}

CToolBar::CToolBar() : m_nResourceID(0) {}

BOOL CToolBar::Create(CWnd* pParentWnd, DWORD dwStyle, UINT nID) {
    if (!pParentWnd || !pParentWnd->m_hWnd)
        return FALSE;
    m_dwStyle = dwStyle;
    return OnMain([&]() -> BOOL {
        auto* frame = wxDynamicCast(pParentWnd->GetWx(), wxFrame);
        if (!frame)
            return FALSE;
        wxToolBar* tb = frame->CreateToolBar(wxTB_HORIZONTAL | wxTB_FLAT | wxTB_NODIVIDER, ToWxId(static_cast<int>(nID)));
        HookWindow(tb, ControlKind::ToolBar, static_cast<int>(nID), dwStyle, 0);
        AttachWx(tb);
        if (pParentWnd->IsKindOf(RUNTIME_CLASS(CFrameWnd)))
            static_cast<CFrameWnd*>(pParentWnd)->m_listControlBars.push_back(this);
        return TRUE;
    });
}

BOOL CToolBar::CreateEx(CWnd* pParentWnd, DWORD, DWORD dwStyle, CRect, UINT nID) { return Create(pParentWnd, dwStyle, nID); }

BOOL CToolBar::LoadToolBar(UINT nIDResource) { return LoadToolBar(MAKEINTRESOURCE(nIDResource)); }

BOOL CToolBar::LoadToolBar(const char* lpszResourceName) {
    ResRef ref = ResRef::From(lpszResourceName);
    const rc::Toolbar* res = FindToolbar(ref);
    if (!res || !m_hWnd)
        return FALSE;
    m_nResourceID = static_cast<UINT>(ref.id);
    m_buttonIDs.assign(res->buttons, res->buttons + res->buttonCount);
    return OnMain([&]() -> BOOL {
        auto* tb = wxDynamicCast(GetWx(), wxToolBar);
        wxBitmap strip = LoadBitmapResource(ref);
        int w = res->buttonWidth;
        int h = res->buttonHeight;
        wxImage image = strip.IsOk() ? strip.ConvertToImage() : wxImage();
        if (image.IsOk())
            image.SetMaskColour(192, 192, 192);
        int imageIndex = 0;
        for (int i = 0; i < res->buttonCount; ++i) {
            int id = res->buttons[i];
            if (id == 0) {
                tb->AddSeparator();
                continue;
            }
            wxBitmap bmp;
            if (image.IsOk() && (imageIndex + 1) * w <= image.GetWidth())
                bmp = wxBitmap(image.GetSubImage(wxRect(imageIndex * w, 0, w, std::min(h, image.GetHeight()))));
            ++imageIndex;
            CString prompt = ResourceString(static_cast<UINT>(id));
            CString tip;
            int nl = prompt.Find('\n');
            if (nl >= 0) {
                tip = prompt.Mid(nl + 1);
                prompt = prompt.Left(nl);
            }
            tb->AddTool(MenuWxId(id), ToWx(tip), bmp.IsOk() ? bmp : wxBitmap(w, h), ToWx(tip), wxITEM_NORMAL);
            tb->SetToolLongHelp(MenuWxId(id), ToWx(prompt));
        }
        tb->Realize();
        return TRUE;
    });
}

BOOL CToolBar::LoadBitmap(const char*) { return TRUE; }
BOOL CToolBar::LoadBitmap(UINT) { return TRUE; }
BOOL CToolBar::SetButtons(const UINT* lpIDArray, int nIDCount) {
    m_buttonIDs.assign(lpIDArray, lpIDArray + nIDCount);
    return TRUE;
}
void CToolBar::SetSizes(SIZE, SIZE) {}
int CToolBar::CommandToIndex(UINT nIDFind) const {
    for (size_t i = 0; i < m_buttonIDs.size(); ++i)
        if (m_buttonIDs[i] == nIDFind)
            return static_cast<int>(i);
    return -1;
}
UINT CToolBar::GetItemID(int nIndex) const {
    return nIndex >= 0 && static_cast<size_t>(nIndex) < m_buttonIDs.size() ? m_buttonIDs[static_cast<size_t>(nIndex)] : 0;
}
void CToolBar::GetItemRect(int, LPRECT lpRect) const { ::SetRectEmpty(lpRect); }
UINT CToolBar::GetButtonStyle(int nIndex) const { return GetItemID(nIndex) ? TBBS_BUTTON : TBBS_SEPARATOR; }
void CToolBar::SetButtonStyle(int, UINT) {}
int CToolBar::GetCount() const { return static_cast<int>(m_buttonIDs.size()); }

CStatusBar::CStatusBar() {}

BOOL CStatusBar::Create(CWnd* pParentWnd, DWORD dwStyle, UINT nID) {
    if (!pParentWnd || !pParentWnd->m_hWnd)
        return FALSE;
    m_dwStyle = dwStyle;
    return OnMain([&]() -> BOOL {
        auto* frame = wxDynamicCast(pParentWnd->GetWx(), wxFrame);
        if (!frame)
            return FALSE;
        wxStatusBar* sb = frame->CreateStatusBar(1, wxSTB_DEFAULT_STYLE, ToWxId(static_cast<int>(nID)));
        HookWindow(sb, ControlKind::StatusBar, static_cast<int>(nID), dwStyle, 0);
        AttachWx(sb);
        if (pParentWnd->IsKindOf(RUNTIME_CLASS(CFrameWnd))) {
            static_cast<CFrameWnd*>(pParentWnd)->m_listControlBars.push_back(this);
            static_cast<CFrameWnd*>(pParentWnd)->m_pStatusBar = this;
        }
        return TRUE;
    });
}

BOOL CStatusBar::CreateEx(CWnd* pParentWnd, DWORD, DWORD dwStyle, UINT nID) { return Create(pParentWnd, dwStyle, nID); }

BOOL CStatusBar::SetIndicators(const UINT* lpIDArray, int nIDCount) {
    m_indicators.assign(lpIDArray, lpIDArray + nIDCount);
    return OnMain([&]() -> BOOL {
        auto* sb = wxDynamicCast(GetWx(), wxStatusBar);
        if (!sb)
            return FALSE;
        std::vector<int> widths(static_cast<size_t>(nIDCount), -1);
        sb->SetFieldsCount(nIDCount);
        for (int i = 0; i < nIDCount; ++i) {
            if (lpIDArray[i] == ID_SEPARATOR)
                continue;
            CString text = ResourceString(lpIDArray[i]);
            widths[static_cast<size_t>(i)] = sb->GetTextExtent(ToWx(text)).x + 16;
            sb->SetStatusText(ToWx(text), i);
        }
        sb->SetStatusWidths(nIDCount, widths.data());
        m_paneStyles.assign(static_cast<size_t>(nIDCount), SBPS_NORMAL);
        m_paneTexts.assign(static_cast<size_t>(nIDCount), CString());
        return TRUE;
    });
}

int CStatusBar::CommandToIndex(UINT nIDFind) const {
    for (size_t i = 0; i < m_indicators.size(); ++i)
        if (m_indicators[i] == nIDFind)
            return static_cast<int>(i);
    return -1;
}

UINT CStatusBar::GetItemID(int nIndex) const {
    return nIndex >= 0 && static_cast<size_t>(nIndex) < m_indicators.size() ? m_indicators[static_cast<size_t>(nIndex)] : 0;
}

void CStatusBar::GetItemRect(int nIndex, LPRECT lpRect) const {
    OnMain([&] {
        wxRect r;
        auto* sb = wxDynamicCast(GetWx(), wxStatusBar);
        if (sb)
            sb->GetFieldRect(nIndex, r);
        lpRect->left = r.x;
        lpRect->top = r.y;
        lpRect->right = r.x + r.width;
        lpRect->bottom = r.y + r.height;
    });
}

CString CStatusBar::GetPaneText(int nIndex) const {
    if (nIndex >= 0 && static_cast<size_t>(nIndex) < m_paneStyles.size() && (m_paneStyles[static_cast<size_t>(nIndex)] & SBPS_DISABLED))
        return m_paneTexts[static_cast<size_t>(nIndex)];
    return OnMain([&]() -> CString {
        auto* sb = wxDynamicCast(GetWx(), wxStatusBar);
        return sb && nIndex >= 0 && nIndex < sb->GetFieldsCount() ? CStr(sb->GetStatusText(nIndex)) : CString();
    });
}

void CStatusBar::GetPaneText(int nIndex, CString& rString) const { rString = GetPaneText(nIndex); }

BOOL CStatusBar::SetPaneText(int nIndex, const char* lpszNewText, BOOL) {
    const char* text = lpszNewText ? lpszNewText : "";
    return OnMain([&]() -> BOOL {
        auto* sb = wxDynamicCast(GetWx(), wxStatusBar);
        if (!sb || nIndex < 0 || nIndex >= sb->GetFieldsCount())
            return FALSE;
        size_t i = static_cast<size_t>(nIndex);
        if (i < m_paneStyles.size() && (m_paneStyles[i] & SBPS_DISABLED)) {
            m_paneTexts[i] = text;
            return TRUE;
        }
        wxString wxText = ToWx(text);
        if (sb->GetStatusText(nIndex) != wxText)
            sb->SetStatusText(wxText, nIndex);
        return TRUE;
    });
}

void CStatusBar::GetPaneInfo(int nIndex, UINT& nID, UINT& nStyle, int& cxWidth) const {
    nID = GetItemID(nIndex);
    nStyle = SBPS_NORMAL;
    CRect r;
    GetItemRect(nIndex, &r);
    cxWidth = r.Width();
}

void CStatusBar::SetPaneInfo(int nIndex, UINT nID, UINT nStyle, int cxWidth) {
    if (nIndex < 0)
        return;
    if (static_cast<size_t>(nIndex) < m_indicators.size())
        m_indicators[static_cast<size_t>(nIndex)] = nID;
    OnMain([&] {
        auto* sb = wxDynamicCast(GetWx(), wxStatusBar);
        if (!sb || nIndex >= sb->GetFieldsCount())
            return;
        std::vector<int> widths(static_cast<size_t>(sb->GetFieldsCount()));
        for (int i = 0; i < sb->GetFieldsCount(); ++i)
            widths[static_cast<size_t>(i)] = sb->GetStatusWidth(i);
        widths[static_cast<size_t>(nIndex)] = (nStyle & SBPS_STRETCH) ? -1 : cxWidth + 8;
        sb->SetStatusWidths(sb->GetFieldsCount(), widths.data());
    });
}

UINT CStatusBar::GetPaneStyle(int nIndex) const {
    return nIndex >= 0 && static_cast<size_t>(nIndex) < m_paneStyles.size() ? m_paneStyles[static_cast<size_t>(nIndex)] : SBPS_NORMAL;
}

void CStatusBar::SetPaneStyle(int nIndex, UINT nStyle) {
    if (nIndex < 0 || static_cast<size_t>(nIndex) >= m_paneStyles.size())
        return;
    size_t i = static_cast<size_t>(nIndex);
    bool wasDisabled = (m_paneStyles[i] & SBPS_DISABLED) != 0;
    bool disabled = (nStyle & SBPS_DISABLED) != 0;
    if (wasDisabled == disabled) {
        m_paneStyles[i] = nStyle;
        return;
    }
    CString text = GetPaneText(nIndex);
    m_paneStyles[i] = nStyle & ~SBPS_DISABLED;
    if (disabled) {
        SetPaneText(nIndex, "");
        m_paneTexts[i] = text;
        m_paneStyles[i] = nStyle;
    } else {
        SetPaneText(nIndex, text);
    }
}

namespace {

class CStatusCmdUI : public CCmdUI {
public:
    void Enable(BOOL bOn) override {
        m_bEnableChanged = TRUE;
        auto* bar = static_cast<CStatusBar*>(m_pOther);
        UINT style = bar->GetPaneStyle(static_cast<int>(m_nIndex)) & ~static_cast<UINT>(SBPS_DISABLED);
        if (!bOn)
            style |= SBPS_DISABLED;
        bar->SetPaneStyle(static_cast<int>(m_nIndex), style);
    }

    void SetCheck(int nCheck) override {
        auto* bar = static_cast<CStatusBar*>(m_pOther);
        UINT style = bar->GetPaneStyle(static_cast<int>(m_nIndex)) & ~static_cast<UINT>(SBPS_POPOUT);
        if (nCheck)
            style |= SBPS_POPOUT;
        bar->SetPaneStyle(static_cast<int>(m_nIndex), style);
    }

    void SetText(const char* lpszText) override { static_cast<CStatusBar*>(m_pOther)->SetPaneText(static_cast<int>(m_nIndex), lpszText); }
};

} // namespace

void CStatusBar::OnUpdateCmdUI(CFrameWnd* pTarget, BOOL bDisableIfNoHndler) {
    if (!pTarget || !m_hWnd)
        return;
    CStatusCmdUI state;
    state.m_pOther = this;
    state.m_nIndexMax = static_cast<UINT>(m_indicators.size());
    for (state.m_nIndex = 0; state.m_nIndex < state.m_nIndexMax; ++state.m_nIndex) {
        state.m_nID = m_indicators[state.m_nIndex];
        if (state.m_nID == 0)
            continue;
        if (CWnd::OnCmdMsg(state.m_nID, static_cast<int>(CN_UPDATE_COMMAND_UI), &state, nullptr))
            continue;
        state.DoUpdate(pTarget, bDisableIfNoHndler);
    }
}

// ---------------------------------------------------------------------------------------------
// Rich edit view/document

IMPLEMENT_DYNAMIC(CRichEditDoc, CDocument)
IMPLEMENT_DYNCREATE(CRichEditView, CView)

BEGIN_MESSAGE_MAP(CRichEditView, CView)
    ON_WM_CREATE()
END_MESSAGE_MAP()

CRichEditDoc::CRichEditDoc() : m_bRTF(FALSE) {}
CRichEditCntrItem* CRichEditDoc::CreateClientItem(void*) const { return nullptr; }
CRichEditView* CRichEditDoc::GetView() const {
    for (CView* v : m_viewList)
        if (v->IsKindOf(RUNTIME_CLASS(CRichEditView)))
            return static_cast<CRichEditView*>(v);
    return nullptr;
}

void CRichEditDoc::Serialize(CArchive& ar) {
    CRichEditView* view = GetView();
    if (view)
        view->Serialize(ar);
}

CRichEditView::CRichEditView() : m_nWordWrap(WrapToWindow) {}

int CRichEditView::OnCreate(LPCREATESTRUCT lpcs) { return CView::OnCreate(lpcs); }

void CRichEditView::OnInitialUpdate() { CView::OnInitialUpdate(); }

void CRichEditView::Serialize(CArchive& ar) {
    CFile* f = ar.GetFile();
    if (!f)
        return;
    if (ar.IsLoading()) {
        ULONGLONG len = f->GetLength();
        std::string data(static_cast<size_t>(len), '\0');
        if (len)
            ar.Read(&data[0], static_cast<UINT>(len));
        SetWindowText(data.c_str());
    } else {
        CString text;
        GetWindowText(text);
        ar.Write(text.GetString(), static_cast<UINT>(text.GetLength()));
    }
}

long CRichEditView::GetTextLength() const { return GetWindowTextLength(); }

void CRichEditView::SetCharFormat(CHARFORMAT2 cf) {
    SendMessage(EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));
}

CHARFORMAT2& CRichEditView::GetCharFormatSelection() {
    static CHARFORMAT2 cf;
    memset(&cf, 0, sizeof cf);
    cf.cbSize = sizeof cf;
    SendMessage(EM_GETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));
    return cf;
}

// ---------------------------------------------------------------------------------------------
// Recent file list and command line

CRecentFileList::CRecentFileList(UINT nStart, const char* lpszSection, const char* lpszEntryFormat, int nSize,
                                 int nMaxDispLen)
    : m_nSize(nSize), m_arrNames(static_cast<size_t>(nSize)), m_strSectionName(lpszSection),
      m_strEntryFormat(lpszEntryFormat), m_nStart(nStart), m_nMaxDisplayLength(nMaxDispLen) {}

void CRecentFileList::Remove(int nIndex) {
    if (nIndex < 0 || nIndex >= m_nSize)
        return;
    m_arrNames.erase(m_arrNames.begin() + nIndex);
    m_arrNames.push_back(CString());
}

void CRecentFileList::Add(const char* lpszPathName) {
    CString path(lpszPathName);
    for (int i = 0; i < m_nSize; ++i)
        if (m_arrNames[static_cast<size_t>(i)].CompareNoCase(path) == 0) {
            Remove(i);
            break;
        }
    m_arrNames.insert(m_arrNames.begin(), path);
    m_arrNames.resize(static_cast<size_t>(m_nSize));
    WriteList();
}

BOOL CRecentFileList::GetDisplayName(CString& strName, int nIndex, const char*, int, BOOL) const {
    if (nIndex < 0 || nIndex >= m_nSize || m_arrNames[static_cast<size_t>(nIndex)].IsEmpty())
        return FALSE;
    strName = m_arrNames[static_cast<size_t>(nIndex)];
    int slash = strName.ReverseFind('\\');
    if (slash >= 0)
        strName = strName.Mid(slash + 1);
    return TRUE;
}

void CRecentFileList::UpdateMenu(CCmdUI* pCmdUI) { pCmdUI->Enable(!m_arrNames.empty() && !m_arrNames[0].IsEmpty()); }

void CRecentFileList::ReadList() {
    CWinApp* app = AfxGetApp();
    if (!app)
        return;
    for (int i = 0; i < m_nSize; ++i) {
        CString entry;
        entry.Format(m_strEntryFormat, i + 1);
        m_arrNames[static_cast<size_t>(i)] = app->GetProfileString(m_strSectionName, entry, "");
    }
}

void CRecentFileList::WriteList() {
    CWinApp* app = AfxGetApp();
    if (!app)
        return;
    for (int i = 0; i < m_nSize; ++i) {
        CString entry;
        entry.Format(m_strEntryFormat, i + 1);
        app->WriteProfileString(m_strSectionName, entry, m_arrNames[static_cast<size_t>(i)]);
    }
}

CCommandLineInfo::CCommandLineInfo()
    : m_bShowSplash(TRUE), m_bRunEmbedded(FALSE), m_bRunAutomated(FALSE), m_nShellCommand(FileNew) {}

void CCommandLineInfo::ParseParam(const char* pszParam, BOOL bFlag, BOOL) {
    if (bFlag)
        return;
    if (m_strFileName.IsEmpty()) {
        m_strFileName = pszParam;
        m_nShellCommand = FileOpen;
    }
}
