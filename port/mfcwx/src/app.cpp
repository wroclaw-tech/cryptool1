#include "windows_impl.h"

#include "afxdlgs.h"
#include "mfcwx/app.h"
#include "runtime.h"

#include <wx/cmdline.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/intl.h>
#include <wx/stdpaths.h>

int __argc = 0;
char** __argv = nullptr;

namespace mfcwx {

CWinApp*& AppInstance() {
    static CWinApp* app = nullptr;
    return app;
}

namespace {

class MfcWxApp : public wxApp {
public:
    bool OnInit() override;
    int OnExit() override;
    void OnIdle(wxIdleEvent& e);
    void OnFatalException() override {}

private:
    LONG m_idleCount = 0;
};

std::string ArgsToCommandLine(int argc, wxChar** argv) {
    std::string line;
    for (int i = 1; i < argc; ++i) {
        if (!line.empty())
            line += ' ';
        std::string a = FromWx(wxString(argv[i]));
        if (a.find(' ') != std::string::npos)
            line += "\"" + a + "\"";
        else
            line += a;
    }
    return line;
}

std::string ChooseLanguage() {
    std::vector<std::string> available = AvailableResourceLanguages();
    auto has = [&](const std::string& code) {
        return std::find(available.begin(), available.end(), code) != available.end();
    };
    wxString env;
    if (wxGetEnv("MFCWX_LANG", &env) && has(std::string(env.utf8_str())))
        return std::string(env.utf8_str());
    HKEY key = nullptr;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\mfcwx", 0, KEY_READ, &key) == ERROR_SUCCESS) {
        char buf[16] = {0};
        DWORD size = sizeof buf - 1, type = 0;
        LONG r = RegQueryValueEx(key, "Language", nullptr, &type, reinterpret_cast<LPBYTE>(buf), &size);
        RegCloseKey(key);
        if (r == ERROR_SUCCESS && has(buf))
            return buf;
    }
    const wxLanguageInfo* info = wxLocale::GetLanguageInfo(wxLocale::GetSystemLanguage());
    std::string code = info ? std::string(info->CanonicalName.Left(2).Lower().utf8_str()) : "en";
    if (code == "sr")
        code = "rs";
    return has(code) ? code : "en";
}

void PrepareEnvironment(int argc, wxChar** argv) {
    SetResourceLanguage(ChooseLanguage().c_str());
    std::string temp = TempDirectory();
    std::string tempApp = AppPath(temp.c_str());
    while (!tempApp.empty() && tempApp.back() == '\\')
        tempApp.pop_back();
    wxSetEnv("TEMP", ToWx(tempApp.c_str()));
    wxSetEnv("TMP", ToWx(tempApp.c_str()));
    if (!wxGetEnv("APPDATA", nullptr)) {
        wxString home = wxGetHomeDir();
#ifdef __APPLE__
        wxString appData = home + "/Library/Application Support";
#else
        wxString appData;
        if (!wxGetEnv("XDG_DATA_HOME", &appData) || !appData.StartsWith("/"))
            appData = home + "/.local/share";
#endif
        wxFileName::Mkdir(appData, 0700, wxPATH_MKDIR_FULL);
        wxSetEnv("APPDATA", ToWx(AppPath(FromWx(appData).c_str()).c_str()));
        if (!wxGetEnv("USERPROFILE", nullptr))
            wxSetEnv("USERPROFILE", ToWx(AppPath(FromWx(home).c_str()).c_str()));
    }
    // The Windows code locates its data files next to the program it finds in the command line.
    std::string program = AppPath((GetDataDirectory() + "/CrypTool").c_str());
    std::string commandLine = "\"" + program + "\"";
    std::string args = ArgsToCommandLine(argc, argv);
    if (!args.empty())
        commandLine += " " + args;
    SetCommandLineOverride(commandLine.c_str());
}

bool MfcWxApp::OnInit() {
    wxInitAllImageHandlers();
    SetAppName("CrypTool");
    SetVendorName("CrypTool");
    CWinApp* app = AppInstance();
    if (!app)
        return false;
    PrepareEnvironment(argc, argv);
    static std::vector<std::string> ansiArgs;
    static std::vector<char*> argPointers;
    for (int i = 0; i < argc; ++i)
        ansiArgs.push_back(FromWx(wxString(argv[i])));
    for (std::string& a : ansiArgs)
        argPointers.push_back(&a[0]);
    argPointers.push_back(nullptr);
    __argc = argc;
    __argv = argPointers.data();
    static std::string cmdLine = ArgsToCommandLine(argc, argv);
    app->m_lpCmdLine = &cmdLine[0];
    app->m_nCmdShow = SW_SHOWNORMAL;
    Bind(wxEVT_IDLE, &MfcWxApp::OnIdle, this);
    StartSnapshotTimerFromEnvironment();
    if (!app->InitApplication() || !app->InitInstance()) {
        app->ExitInstance();
        return false;
    }
    return app->m_pMainWnd != nullptr || !wxTopLevelWindows.empty();
}

int MfcWxApp::OnExit() {
    CWinApp* app = AppInstance();
    return app ? app->ExitInstance() : 0;
}

void MfcWxApp::OnIdle(wxIdleEvent& e) {
    e.Skip();
    PurgeTemporaryGdiWrappers();
    CWinApp* app = AppInstance();
    if (app && app->OnIdle(m_idleCount++))
        e.RequestMore();
    else
        m_idleCount = 0;
}

UINT ButtonsFor(UINT type) { return type & MB_TYPEMASK; }

} // namespace

int ShowMessageBox(wxWindow* parent, const char* text, const char* caption, UINT nType) {
    if (!parent)
        parent = MainWxWindow();
    long style = 0;
    UINT buttons = ButtonsFor(nType);
    switch (buttons) {
    case MB_OKCANCEL:
    case MB_RETRYCANCEL:
        style = wxOK | wxCANCEL;
        break;
    case MB_YESNO:
        style = wxYES_NO;
        break;
    case MB_YESNOCANCEL:
    case MB_ABORTRETRYIGNORE:
    case MB_CANCELTRYCONTINUE:
        style = wxYES_NO | wxCANCEL;
        break;
    default:
        style = wxOK;
        break;
    }
    switch (nType & MB_ICONMASK) {
    case MB_ICONHAND: style |= wxICON_ERROR; break;
    case MB_ICONQUESTION: style |= wxICON_QUESTION; break;
    case MB_ICONEXCLAMATION: style |= wxICON_WARNING; break;
    case MB_ICONASTERISK: style |= wxICON_INFORMATION; break;
    default: break;
    }
    if ((nType & MB_DEFMASK) == MB_DEFBUTTON2) {
        if (style & wxYES_NO)
            style |= wxNO_DEFAULT;
        else if (style & wxCANCEL)
            style |= wxCANCEL_DEFAULT;
    }
    wxString title = caption ? ToWx(caption) : wxString(ToWx(AfxGetAppName()));
    wxMessageDialog dlg(parent, ToWx(text ? text : ""), title, style);
    if (buttons == MB_RETRYCANCEL)
        dlg.SetOKCancelLabels(_("&Retry"), _("Cancel"));
    else if (buttons == MB_ABORTRETRYIGNORE)
        dlg.SetYesNoCancelLabels(_("&Abort"), _("&Retry"), _("&Ignore"));
    else if (buttons == MB_CANCELTRYCONTINUE)
        dlg.SetYesNoCancelLabels(_("&Try Again"), _("&Continue"), _("Cancel"));
    int r = dlg.ShowModal();
    switch (buttons) {
    case MB_OKCANCEL:
        return r == wxID_OK ? IDOK : IDCANCEL;
    case MB_RETRYCANCEL:
        return r == wxID_OK ? IDRETRY : IDCANCEL;
    case MB_YESNO:
        return r == wxID_YES ? IDYES : IDNO;
    case MB_YESNOCANCEL:
        return r == wxID_YES ? IDYES : r == wxID_NO ? IDNO : IDCANCEL;
    case MB_ABORTRETRYIGNORE:
        return r == wxID_YES ? IDABORT : r == wxID_NO ? IDRETRY : IDIGNORE;
    case MB_CANCELTRYCONTINUE:
        return r == wxID_YES ? IDTRYAGAIN : r == wxID_NO ? IDCONTINUE : IDCANCEL;
    default:
        return IDOK;
    }
}

BOOL ShowContextHelp(CWnd* pWnd, HELPINFO*) {
    if (!pWnd || !pWnd->m_hWnd)
        return FALSE;
    wxWindow* top = wxGetTopLevelParent(pWnd->GetWx());
    CWnd* p = PermanentWnd(top);
    if (p)
        p->SendMessage(WM_COMMAND, ID_HELP, 0);
    return TRUE;
}

} // namespace mfcwx

using namespace mfcwx;

wxIMPLEMENT_APP_NO_MAIN(MfcWxApp);

int main(int argc, char** argv) { return wxEntry(argc, argv); }

// ---------------------------------------------------------------------------------------------
// CWinApp

IMPLEMENT_DYNAMIC(CWinApp, CWinThread)

BEGIN_MESSAGE_MAP(CWinApp, CWinThread)
    ON_COMMAND_EX_RANGE(ID_FILE_MRU_FILE1, ID_FILE_MRU_FILE16, &CWinApp::OnOpenRecentFile)
    ON_UPDATE_COMMAND_UI(ID_FILE_MRU_FILE1, &CWinApp::OnUpdateRecentFileMenu)
END_MESSAGE_MAP()

CWinApp::CWinApp(const char* lpszAppName)
    : m_hInstance(reinterpret_cast<HINSTANCE>(static_cast<uintptr_t>(0x400000))), m_hPrevInstance(nullptr),
      m_lpCmdLine(nullptr), m_nCmdShow(SW_SHOWNORMAL), m_pszAppName(lpszAppName), m_pszRegistryKey(nullptr),
      m_pszExeName("CrypTool"), m_pszHelpFilePath(nullptr), m_pszProfileName(nullptr), m_pRecentFileList(nullptr),
      m_bHelpMode(FALSE), m_nWaitCursorCount(0), m_hcurWaitCursorRestore(nullptr), m_eHelpType(0) {
    AppInstance() = this;
}

CWinApp::~CWinApp() {
    for (CDocTemplate* t : m_templates)
        delete t;
    m_templates.clear();
    delete m_pRecentFileList;
    if (AppInstance() == this)
        AppInstance() = nullptr;
}

BOOL CWinApp::InitApplication() { return TRUE; }
BOOL CWinApp::InitInstance() { return TRUE; }

int CWinApp::ExitInstance() {
    if (m_pRecentFileList)
        m_pRecentFileList->WriteList();
    return 0;
}

int CWinApp::Run() { return wxTheApp ? wxTheApp->MainLoop() : 0; }

BOOL CWinApp::OnIdle(LONG lCount) {
    if (lCount <= 0) {
        if (m_pMainWnd && m_pMainWnd->m_hWnd && m_pMainWnd->IsKindOf(RUNTIME_CLASS(CFrameWnd))) {
            auto* frame = static_cast<CFrameWnd*>(m_pMainWnd);
            std::vector<CControlBar*> bars = frame->m_listControlBars;
            for (CControlBar* bar : bars)
                bar->OnUpdateCmdUI(frame, FALSE);
        }
        for (CDocTemplate* t : m_templates) {
            POSITION pos = t->GetFirstDocPosition();
            while (pos)
                if (CDocument* d = t->GetNextDoc(pos))
                    d->OnIdle();
        }
    }
    return FALSE;
}

void CWinApp::AddDocTemplate(CDocTemplate* pTemplate) { m_templates.push_back(pTemplate); }

POSITION CWinApp::GetFirstDocTemplatePosition() const {
    return m_templates.empty() ? nullptr : reinterpret_cast<POSITION>(static_cast<uintptr_t>(1));
}

CDocTemplate* CWinApp::GetNextDocTemplate(POSITION& pos) const {
    size_t i = static_cast<size_t>(reinterpret_cast<uintptr_t>(pos)) - 1;
    CDocTemplate* t = i < m_templates.size() ? m_templates[i] : nullptr;
    pos = i + 1 < m_templates.size() ? reinterpret_cast<POSITION>(static_cast<uintptr_t>(i + 2)) : nullptr;
    return t;
}

CDocument* CWinApp::OpenDocumentFile(const char* lpszFileName) {
    CDocTemplate* best = nullptr;
    CDocTemplate::Confidence bestMatch = CDocTemplate::noAttempt;
    CDocument* open = nullptr;
    for (CDocTemplate* t : m_templates) {
        CDocument* match = nullptr;
        CDocTemplate::Confidence c = t->MatchDocType(lpszFileName, match);
        if (c > bestMatch) {
            bestMatch = c;
            best = t;
        }
        if (c == CDocTemplate::yesAlreadyOpen) {
            open = match;
            break;
        }
    }
    if (open) {
        POSITION pos = open->GetFirstViewPosition();
        if (CView* v = pos ? open->GetNextView(pos) : nullptr)
            if (CFrameWnd* f = v->GetParentFrame())
                f->ActivateFrame();
        return open;
    }
    return best ? best->OpenDocumentFile(lpszFileName) : nullptr;
}

void CWinApp::AddToRecentFileList(const char* lpszPathName) {
    if (m_pRecentFileList)
        m_pRecentFileList->Add(lpszPathName);
}

BOOL CWinApp::SaveAllModified() {
    for (CDocTemplate* t : m_templates)
        if (!t->SaveAllModified())
            return FALSE;
    return TRUE;
}

void CWinApp::CloseAllDocuments(BOOL bEndSession) {
    for (CDocTemplate* t : m_templates)
        t->CloseAllDocuments(bEndSession);
}

int CWinApp::DoMessageBox(const char* lpszPrompt, UINT nType, UINT) {
    return ShowMessageBox(nullptr, lpszPrompt, m_pszAppName, nType);
}

void CWinApp::DoWaitCursor(int nCode) {
    OnMain([&] {
        if (nCode > 0) {
            ++m_nWaitCursorCount;
            wxBeginBusyCursor();
        } else if (nCode < 0 && m_nWaitCursorCount > 0) {
            --m_nWaitCursorCount;
            if (wxIsBusy())
                wxEndBusyCursor();
        }
    });
}

BOOL CWinApp::DoPromptFileName(CString& fileName, UINT nIDSTitle, DWORD lFlags, BOOL bOpenFileDialog,
                               CDocTemplate* pTemplate) {
    CString filter;
    std::vector<CDocTemplate*> templates;
    if (pTemplate)
        templates.push_back(pTemplate);
    else
        templates = m_templates;
    CString defExt;
    for (CDocTemplate* t : templates) {
        CString name, ext;
        if (t->GetDocString(name, CDocTemplate::filterName) && t->GetDocString(ext, CDocTemplate::filterExt) &&
            !name.IsEmpty() && !ext.IsEmpty()) {
            filter += name + "|*" + ext + "|";
            if (defExt.IsEmpty())
                defExt = ext.Mid(1);
        }
    }
    CString all;
    if (!all.LoadString(AFX_IDS_ALLFILTER))
        all = "All Files (*.*)";
    filter += all + "|*|";
    CFileDialog dlg(bOpenFileDialog, bOpenFileDialog ? nullptr : defExt.GetString(), fileName,
                    lFlags | OFN_HIDEREADONLY | (bOpenFileDialog ? OFN_FILEMUSTEXIST : OFN_OVERWRITEPROMPT), filter);
    dlg.m_strTitle.LoadString(nIDSTitle);
    if (dlg.DoModal() != IDOK)
        return FALSE;
    fileName = dlg.GetPathName();
    return TRUE;
}

BOOL CWinApp::ProcessShellCommand(CCommandLineInfo& rCmdInfo) {
    switch (rCmdInfo.m_nShellCommand) {
    case CCommandLineInfo::FileNew:
        OnCmdMsg(ID_FILE_NEW, 0, nullptr, nullptr);
        return TRUE;
    case CCommandLineInfo::FileOpen:
        return OpenDocumentFile(rCmdInfo.m_strFileName) != nullptr;
    default:
        return TRUE;
    }
}

void CWinApp::ParseCommandLine(CCommandLineInfo& rCmdInfo) {
    if (!wxTheApp)
        return;
    for (int i = 1; i < wxTheApp->argc; ++i) {
        std::string a = FromWx(wxTheApp->argv[i]);
        if (a.compare(0, 4, "-psn") == 0)
            continue;
        bool flag = !a.empty() && (a[0] == '-' || a[0] == '/') && a.find('.') == std::string::npos;
        std::string param = flag ? a.substr(1) : AppPath(wxTheApp->argv[i].utf8_str());
        rCmdInfo.ParseParam(param.c_str(), flag, i == wxTheApp->argc - 1);
    }
}

HCURSOR CWinApp::LoadCursor(const char* lpszResourceName) const { return ::LoadCursor(m_hInstance, lpszResourceName); }
HCURSOR CWinApp::LoadCursor(UINT nIDResource) const { return ::LoadCursor(m_hInstance, MAKEINTRESOURCE(nIDResource)); }
HCURSOR CWinApp::LoadStandardCursor(const char* lpszCursorName) const { return ::LoadCursor(nullptr, lpszCursorName); }
HCURSOR CWinApp::LoadOEMCursor(UINT nIDCursor) const { return ::LoadCursor(nullptr, MAKEINTRESOURCE(nIDCursor)); }
HICON CWinApp::LoadIcon(const char* lpszResourceName) const { return ::LoadIcon(m_hInstance, lpszResourceName); }
HICON CWinApp::LoadIcon(UINT nIDResource) const { return ::LoadIcon(m_hInstance, MAKEINTRESOURCE(nIDResource)); }
HICON CWinApp::LoadStandardIcon(const char* lpszIconName) const { return ::LoadIcon(nullptr, lpszIconName); }
HICON CWinApp::LoadOEMIcon(UINT nIDIcon) const { return ::LoadIcon(nullptr, MAKEINTRESOURCE(nIDIcon)); }

namespace {

CString SectionKey(const CWinApp* app, const char* section) {
    CString key = "Software\\";
    key += app->m_pszRegistryKey && *app->m_pszRegistryKey ? app->m_pszRegistryKey : "CrypTool";
    key += "\\";
    key += app->m_pszAppName ? app->m_pszAppName : "CrypTool";
    if (section && *section) {
        key += "\\";
        key += section;
    }
    return key;
}

} // namespace

UINT CWinApp::GetProfileInt(const char* lpszSection, const char* lpszEntry, int nDefault) {
    HKEY key = nullptr;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, SectionKey(this, lpszSection), 0, KEY_READ, &key) != ERROR_SUCCESS)
        return static_cast<UINT>(nDefault);
    DWORD value = 0, size = sizeof value, type = 0;
    LONG r = RegQueryValueEx(key, lpszEntry, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size);
    RegCloseKey(key);
    return r == ERROR_SUCCESS && type == REG_DWORD ? value : static_cast<UINT>(nDefault);
}

BOOL CWinApp::WriteProfileInt(const char* lpszSection, const char* lpszEntry, int nValue) {
    HKEY key = nullptr;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, SectionKey(this, lpszSection), 0, nullptr, 0, KEY_WRITE, nullptr, &key,
                       nullptr) != ERROR_SUCCESS)
        return FALSE;
    DWORD v = static_cast<DWORD>(nValue);
    LONG r = RegSetValueEx(key, lpszEntry, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&v), sizeof v);
    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

CString CWinApp::GetProfileString(const char* lpszSection, const char* lpszEntry, const char* lpszDefault) {
    HKEY key = nullptr;
    CString def = lpszDefault ? lpszDefault : "";
    if (RegOpenKeyEx(HKEY_CURRENT_USER, SectionKey(this, lpszSection), 0, KEY_READ, &key) != ERROR_SUCCESS)
        return def;
    DWORD size = 0, type = 0;
    if (RegQueryValueEx(key, lpszEntry, nullptr, &type, nullptr, &size) != ERROR_SUCCESS || type != REG_SZ) {
        RegCloseKey(key);
        return def;
    }
    CString value;
    RegQueryValueEx(key, lpszEntry, nullptr, &type, reinterpret_cast<LPBYTE>(value.GetBuffer(static_cast<int>(size) + 1)), &size);
    value.ReleaseBuffer();
    RegCloseKey(key);
    return value;
}

BOOL CWinApp::WriteProfileString(const char* lpszSection, const char* lpszEntry, const char* lpszValue) {
    HKEY key = nullptr;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, SectionKey(this, lpszSection), 0, nullptr, 0, KEY_WRITE, nullptr, &key,
                       nullptr) != ERROR_SUCCESS)
        return FALSE;
    LONG r;
    if (!lpszEntry)
        r = ERROR_SUCCESS;
    else if (!lpszValue)
        r = RegDeleteValue(key, lpszEntry);
    else
        r = RegSetValueEx(key, lpszEntry, 0, REG_SZ, reinterpret_cast<const BYTE*>(lpszValue),
                          static_cast<DWORD>(strlen(lpszValue) + 1));
    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

BOOL CWinApp::GetProfileBinary(const char* lpszSection, const char* lpszEntry, LPBYTE* ppData, UINT* pBytes) {
    *ppData = nullptr;
    *pBytes = 0;
    HKEY key = nullptr;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, SectionKey(this, lpszSection), 0, KEY_READ, &key) != ERROR_SUCCESS)
        return FALSE;
    DWORD size = 0, type = 0;
    if (RegQueryValueEx(key, lpszEntry, nullptr, &type, nullptr, &size) != ERROR_SUCCESS) {
        RegCloseKey(key);
        return FALSE;
    }
    *ppData = new BYTE[size ? size : 1];
    RegQueryValueEx(key, lpszEntry, nullptr, &type, *ppData, &size);
    *pBytes = size;
    RegCloseKey(key);
    return TRUE;
}

BOOL CWinApp::WriteProfileBinary(const char* lpszSection, const char* lpszEntry, LPBYTE pData, UINT nBytes) {
    HKEY key = nullptr;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, SectionKey(this, lpszSection), 0, nullptr, 0, KEY_WRITE, nullptr, &key,
                       nullptr) != ERROR_SUCCESS)
        return FALSE;
    LONG r = RegSetValueEx(key, lpszEntry, 0, REG_BINARY, pData, nBytes);
    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

void CWinApp::SetRegistryKey(const char* lpszRegistryKey) {
    m_pszRegistryKey = strdup(lpszRegistryKey && *lpszRegistryKey ? lpszRegistryKey : "CrypTool");
}

void CWinApp::SetRegistryKey(UINT nIDRegistryKey) {
    CString key;
    key.LoadString(nIDRegistryKey);
    SetRegistryKey(key);
}

void CWinApp::LoadStdProfileSettings(UINT nMaxMRU) {
    if (nMaxMRU && !m_pRecentFileList) {
        m_pRecentFileList = new CRecentFileList(0, "Recent File List", "File%d", static_cast<int>(nMaxMRU));
        m_pRecentFileList->ReadList();
    }
}

void CWinApp::HideApplication() {
    if (m_pMainWnd)
        m_pMainWnd->ShowWindow(SW_HIDE);
}

void CWinApp::OnFileNew() {
    if (!m_templates.empty())
        m_templates.front()->OpenDocumentFile(nullptr);
}

void CWinApp::OnFileOpen() {
    CString name;
    if (DoPromptFileName(name, AFX_IDS_OPENFILE, OFN_HIDEREADONLY | OFN_FILEMUSTEXIST, TRUE, nullptr))
        OpenDocumentFile(name);
}

void CWinApp::OnFilePrintSetup() {}

void CWinApp::OnAppExit() {
    if (m_pMainWnd)
        m_pMainWnd->SendMessage(WM_CLOSE);
    else if (wxTheApp)
        wxTheApp->ExitMainLoop();
}

void CWinApp::OnHelp() { WinHelpInternal(0, HELP_FINDER); }
void CWinApp::OnHelpFinder() { WinHelpInternal(0, HELP_FINDER); }
void CWinApp::OnHelpIndex() { WinHelpInternal(0, HELP_INDEX); }
void CWinApp::OnContextHelp() { WinHelpInternal(0, HELP_FINDER); }
void CWinApp::OnHelpUsing() { WinHelpInternal(0, HELP_HELPONHELP); }

BOOL CWinApp::OnOpenRecentFile(UINT nID) {
    if (!m_pRecentFileList)
        return FALSE;
    int index = static_cast<int>(nID - ID_FILE_MRU_FILE1);
    if (index < 0 || index >= m_pRecentFileList->GetSize())
        return FALSE;
    CString path = (*m_pRecentFileList)[index];
    if (path.IsEmpty())
        return FALSE;
    if (!OpenDocumentFile(path))
        m_pRecentFileList->Remove(index);
    return TRUE;
}

void CWinApp::OnUpdateRecentFileMenu(CCmdUI* pCmdUI) {
    if (m_pRecentFileList)
        m_pRecentFileList->UpdateMenu(pCmdUI);
    else
        pCmdUI->Enable(FALSE);
}

// ---------------------------------------------------------------------------------------------
// Global helpers

CWinApp* AFXAPI AfxGetApp() { return AppInstance(); }

CWnd* AFXAPI AfxGetMainWnd() {
    CWinApp* app = AppInstance();
    return app ? app->m_pMainWnd : nullptr;
}

HINSTANCE AFXAPI AfxGetInstanceHandle() {
    CWinApp* app = AppInstance();
    return app ? app->m_hInstance : reinterpret_cast<HINSTANCE>(static_cast<uintptr_t>(0x400000));
}

HINSTANCE AFXAPI AfxGetResourceHandle() { return AfxGetInstanceHandle(); }
void AFXAPI AfxSetResourceHandle(HINSTANCE) {}
HINSTANCE AFXAPI AfxFindResourceHandle(const char*, const char*) { return AfxGetInstanceHandle(); }

const char* AFXAPI AfxGetAppName() {
    CWinApp* app = AppInstance();
    return app && app->m_pszAppName ? app->m_pszAppName : "CrypTool";
}

int AFXAPI AfxMessageBox(const char* lpszText, UINT nType, UINT nIDHelp) {
    CWinApp* app = AppInstance();
    return OnMain([&]() -> int {
        if (app)
            return app->DoMessageBox(lpszText, nType, nIDHelp);
        return ShowMessageBox(nullptr, lpszText, nullptr, nType);
    });
}

int AFXAPI AfxMessageBox(UINT nIDPrompt, UINT nType, UINT nIDHelp) {
    CString text;
    text.LoadString(nIDPrompt);
    return AfxMessageBox(text, nType, nIDHelp == static_cast<UINT>(-1) ? nIDPrompt : nIDHelp);
}

void AFXAPI AfxFormatString1(CString& rString, UINT nIDS, const char* lpsz1) {
    rString.LoadString(nIDS);
    rString.Replace("%1", lpsz1 ? lpsz1 : "");
}

void AFXAPI AfxFormatString2(CString& rString, UINT nIDS, const char* lpsz1, const char* lpsz2) {
    rString.LoadString(nIDS);
    CString result;
    const char* p = rString;
    while (*p) {
        if (p[0] == '%' && (p[1] == '1' || p[1] == '2')) {
            result += p[1] == '1' ? (lpsz1 ? lpsz1 : "") : (lpsz2 ? lpsz2 : "");
            p += 2;
        } else {
            result += *p++;
        }
    }
    rString = result;
}

BOOL AFXAPI AfxExtractSubString(CString& rString, const char* lpszFullString, int iSubString, char chSep) {
    rString.Empty();
    if (!lpszFullString)
        return FALSE;
    while (iSubString--) {
        lpszFullString = strchr(lpszFullString, chSep);
        if (!lpszFullString)
            return FALSE;
        ++lpszFullString;
    }
    const char* end = strchr(lpszFullString, chSep);
    int len = end ? static_cast<int>(end - lpszFullString) : static_cast<int>(strlen(lpszFullString));
    rString = CString(lpszFullString, len);
    return TRUE;
}

const char* AFXAPI AfxRegisterWndClass(UINT, HCURSOR, HBRUSH, HICON) { return "AfxWnd"; }
BOOL AFXAPI AfxRegisterClass(WNDCLASS* lpWndClass) { return RegisterClass(lpWndClass) != 0; }
BOOL AFXAPI AfxInitRichEdit() { return TRUE; }
BOOL AFXAPI AfxInitRichEdit2() { return TRUE; }
BOOL AFXAPI AfxOleInit() { return TRUE; }
void AFXAPI AfxEnableControlContainer() {}
BOOL AFXAPI AfxSocketInit() { return TRUE; }
void AFXAPI AfxInitCommonControls() {}
BOOL AFXAPI AfxCheckMemory() { return TRUE; }

void AFXAPI AfxGetModuleShortFileName(HINSTANCE, CString& strShortName) {
    char buf[_MAX_PATH];
    GetModuleFileName(nullptr, buf, sizeof buf);
    strShortName = buf;
}

LRESULT AFXAPI AfxCallWndProc(CWnd* pWnd, HWND, UINT nMsg, WPARAM wParam, LPARAM lParam) {
    return pWnd ? pWnd->WindowProc(nMsg, wParam, lParam) : 0;
}

CWaitCursor::CWaitCursor() {
    if (CWinApp* app = AfxGetApp())
        app->DoWaitCursor(1);
}

CWaitCursor::~CWaitCursor() {
    if (CWinApp* app = AfxGetApp())
        app->DoWaitCursor(-1);
}

BOOL CWinApp::OnDDECommand(LPTSTR) { return FALSE; }
