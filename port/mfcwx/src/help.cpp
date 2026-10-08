#include "windows_impl.h"

#include "htmlhelp.h"

#include <wx/artprov.h>
#include <wx/choicdlg.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/filesys.h>
#include <wx/html/helpctrl.h>
#include <wx/html/helpdlg.h>
#include <wx/html/helpfrm.h>
#include <wx/html/helpwnd.h>
#include <wx/html/htmlwin.h>
#include <wx/mstream.h>
#include <wx/tokenzr.h>
#include <wx/toolbar.h>
#include <wx/wfstream.h>

#include <algorithm>
#include <fstream>

namespace mfcwx {

namespace {

const char kIndexFileName[] = "CrypTool.helpindex";
const char kDataFileScheme[] = "cryptool-data:";

struct HelpIndex {
    wxString dir;
    wxString baseUrl;
    wxString title;
    wxString defaultTopic;
    wxString contentsFile;
    wxString indexFile;
    std::map<DWORD, wxString> contexts;
    std::multimap<wxString, wxString> alinks;
    std::multimap<wxString, wxString> keywords;
    std::map<wxString, wxString> titles;
    std::map<wxString, wxString> pages;
};

wxString HelpRoot() { return wxFileName::DirName(wxString::FromUTF8(GetDataDirectory())).GetPath(); }

wxString HelpDirectory() {
    return HelpRoot() + wxFILE_SEP_PATH + (strcmp(GetResourceLanguage(), "de") == 0 ? "hlp_de" : "hlp_en");
}

wxString FileUrl(const wxString& path) { return wxFileSystem::FileNameToURL(wxFileName(path)); }

// Help pages are served through a "cthelp:" protocol so they can be adjusted before wxHTML parses them.
const char kHelpProtocol[] = "cthelp";

wxString HelpUrl(const wxString& path) { return kHelpProtocol + FileUrl(path).Mid(4); }

wxString PathFromHelpUrl(const wxString& url) {
    return wxFileSystem::URLToFileName("file" + url.BeforeFirst('#').Mid(strlen(kHelpProtocol))).GetFullPath();
}

std::unique_ptr<HelpIndex> ReadIndex(const wxString& dir) {
    std::ifstream in(std::string(wxFileName(dir, kIndexFileName).GetFullPath().utf8_str()), std::ios::binary);
    std::string line;
    if (!in || !std::getline(in, line) || line.compare(0, 18, "CrypToolHelpIndex\t") != 0)
        return nullptr;
    std::unique_ptr<HelpIndex> index(new HelpIndex);
    index->dir = dir;
    index->baseUrl = HelpUrl(dir + wxFILE_SEP_PATH);
    if (!index->baseUrl.EndsWith("/"))
        index->baseUrl += "/";
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        wxArrayString f = wxSplit(wxString::FromUTF8(line.c_str()), '\t', 0);
        if (f.size() < 2)
            continue;
        const wxString& kind = f[0];
        if (kind == "title")
            index->title = f[1];
        else if (kind == "default")
            index->defaultTopic = f[1];
        else if (kind == "contents")
            index->contentsFile = f[1];
        else if (kind == "index")
            index->indexFile = f[1];
        else if (f.size() < 3)
            continue;
        else if (kind == "context")
            index->contexts[static_cast<DWORD>(wxAtol(f[1]))] = f[2];
        else if (kind == "alink")
            index->alinks.emplace(f[1].Lower(), f[2]);
        else if (kind == "keyword")
            index->keywords.emplace(f[1].Lower(), f[2]);
        else if (kind == "topic") {
            index->titles[f[1]] = f[2];
            index->pages[f[1].Lower()] = f[1];
        }
    }
    return index;
}

const HelpIndex* CurrentIndex() {
    static std::map<wxString, std::unique_ptr<HelpIndex>> cache;
    wxString dir = HelpDirectory();
    auto it = cache.find(dir);
    if (it == cache.end() || !it->second)
        it = cache.emplace(dir, ReadIndex(dir)).first;
    if (!it->second) {
        cache.erase(it);
        wxLogDebug("mfcwx: no help index in %s", dir);
        return nullptr;
    }
    return it->second.get();
}

// Topic and image references in the help are not reliably cased.
wxString ResolveCaseInsensitive(const wxString& path) {
    if (wxFileExists(path))
        return path;
    wxString root = HelpRoot();
    if (!path.StartsWith(root + wxFILE_SEP_PATH))
        return wxString();
    wxString resolved = root;
    wxStringTokenizer parts(path.Mid(root.length() + 1), wxFILE_SEP_PATH, wxTOKEN_STRTOK);
    while (parts.HasMoreTokens()) {
        wxString part = parts.GetNextToken();
        wxString exact = resolved + wxFILE_SEP_PATH + part;
        if (wxFileName::Exists(exact)) {
            resolved = exact;
            continue;
        }
        wxDir dir(resolved);
        wxString name, match;
        for (bool more = dir.IsOpened() && dir.GetFirst(&name); more; more = dir.GetNext(&name))
            if (name.CmpNoCase(part) == 0) {
                match = name;
                break;
            }
        if (match.empty())
            return wxString();
        resolved += wxFILE_SEP_PATH + match;
    }
    return wxFileExists(resolved) ? resolved : wxString();
}

wxString BrowserUrl(const wxString& url) {
    if (!url.StartsWith(wxString(kHelpProtocol) + ":"))
        return url;
    wxString anchor = url.AfterFirst('#');
    wxString path = ResolveCaseInsensitive(PathFromHelpUrl(url));
    return FileUrl(path.empty() ? PathFromHelpUrl(url) : path) + (anchor.empty() ? wxString() : "#" + anchor);
}

// Pages without a charset are Windows-1252; script-only file links (helper.js parser()) become plain links.
std::string PreparePage(const std::string& page) {
    static const std::string open = "onclick=\"parser('";
    static const std::string close = "')\"";
    std::string lower(page);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(tolower(c)); });
    std::string out;
    if (lower.find("charset") == std::string::npos)
        out = "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=windows-1252\">\n";
    size_t pos = 0;
    for (size_t hit; (hit = lower.find(open, pos)) != std::string::npos;) {
        size_t start = hit + open.size();
        size_t end = page.find(close, start);
        if (end == std::string::npos)
            break;
        bool web = lower.compare(start, 5, "http:") == 0 || lower.compare(start, 6, "https:") == 0;
        out += page.substr(pos, hit - pos) + "href=\"" + (web ? "" : kDataFileScheme) +
               page.substr(start, end - start) + "\"";
        pos = end + close.size();
    }
    return out + page.substr(pos);
}

class HelpFSHandler : public wxFileSystemHandler {
public:
    bool CanOpen(const wxString& location) override { return GetProtocol(location) == kHelpProtocol; }

    wxFSFile* OpenFile(wxFileSystem&, const wxString& location) override {
        wxString path = ResolveCaseInsensitive(PathFromHelpUrl(location));
        if (path.empty())
            return nullptr;
        std::unique_ptr<wxInputStream> stream(new wxFFileInputStream(path));
        if (!stream->IsOk())
            return nullptr;
        wxString lower = path.Lower();
        if (lower.EndsWith(".html") || lower.EndsWith(".htm")) {
            std::string page(static_cast<size_t>(stream->GetLength()), '\0');
            stream->Read(&page[0], page.size());
            page = PreparePage(page.substr(0, stream->LastRead()));
            wxMemoryOutputStream copy;
            copy.Write(page.data(), page.size());
            stream.reset(new wxMemoryInputStream(copy));
        }
        return new wxFSFile(stream.release(), location.BeforeFirst('#'), GetMimeTypeFromExt(path),
                            GetAnchor(location), wxDateTime(wxFileModificationTime(path)));
    }
};

wxString UiString(UINT id, const char* fallback) {
    std::string s;
    return LoadResourceString(id, s) && !s.empty() ? ToWx(s.c_str()) : wxString(fallback);
}

void OpenDataFile(wxWindow* parent, const wxString& relative) {
    wxString rel = relative;
    rel.Replace("\\", "/");
    wxString path = ResolveCaseInsensitive(HelpRoot() + wxFILE_SEP_PATH + rel);
    if (path.empty() || !wxLaunchDefaultApplication(path)) {
        wxString text = wxString::Format("%s\n%s", UiString(AFX_IDP_FAILED_TO_LAUNCH_HELP, "Failed to launch help."),
                                         HelpRoot() + wxFILE_SEP_PATH + rel);
        ShowMessageBox(parent, FromWx(text).c_str(), nullptr, MB_OK | MB_ICONEXCLAMATION);
    }
}

bool IsExternalUrl(const wxString& href) {
    wxString scheme = href.BeforeFirst(':');
    if (scheme.length() < 2 || scheme.length() == href.length())
        return false;
    for (wxUniChar c : scheme)
        if (!wxIsalnum(c) && c != '+' && c != '-' && c != '.')
            return false;
    return scheme.CmpNoCase("file") != 0;
}

int OpenInBrowserId() {
    static const int id = wxWindow::NewControlId();
    return id;
}

template <class Base>
class HelpWindowShell : public Base {
public:
    explicit HelpWindowShell(wxHtmlHelpData* data) : Base(data) {
        this->Bind(wxEVT_HTML_LINK_CLICKED, &HelpWindowShell::OnLink, this);
        this->Bind(wxEVT_TOOL, &HelpWindowShell::OnOpenInBrowser, this, OpenInBrowserId());
    }

    void AddToolbarButtons(wxToolBar* toolBar, int) override {
        toolBar->AddSeparator();
        toolBar->AddTool(OpenInBrowserId(), _("Open in browser"), wxArtProvider::GetBitmap(wxART_HELP_PAGE, wxART_TOOLBAR),
                         _("Open this page in the web browser"));
    }

private:
    void OnLink(wxHtmlLinkEvent& event) {
        const wxMouseEvent* mouse = event.GetLinkInfo().GetEvent();
        wxString href = event.GetLinkInfo().GetHref();
        if (mouse && !mouse->LeftUp()) {
            event.Skip();
        } else if (href.StartsWith(kDataFileScheme)) {
            OpenDataFile(this, href.Mid(strlen(kDataFileScheme)));
        } else if (IsExternalUrl(href)) {
            wxLaunchDefaultBrowser(href);
        } else {
            event.Skip();
        }
    }

    void OnOpenInBrowser(wxCommandEvent&) {
        wxHtmlWindow* html = this->GetHelpWindow() ? this->GetHelpWindow()->GetHtmlWindow() : nullptr;
        if (!html || html->GetOpenedPage().empty())
            return;
        wxString url = html->GetOpenedPage();
        if (!html->GetOpenedAnchor().empty())
            url += "#" + html->GetOpenedAnchor();
        wxLaunchDefaultBrowser(BrowserUrl(url));
    }
};

using HelpFrame = HelpWindowShell<wxHtmlHelpFrame>;
using HelpDialog = HelpWindowShell<wxHtmlHelpDialog>;

wxSize DefaultHelpSize(wxWindow* w) { return w->FromDIP(wxSize(1000, 720)); }

class HelpController : public wxHtmlHelpController {
public:
    explicit HelpController(const HelpIndex& index) : wxHtmlHelpController(wxHF_DEFAULT_STYLE) {
        wxString title = index.title;
        title.Replace("%", "%%");
        SetTitleFormat(title.empty() ? wxString("%s") : title + " - %s");
        wxFileSystem fs;
        std::unique_ptr<wxFSFile> book(fs.OpenFile(FileUrl(wxFileName(index.dir, kIndexFileName).GetFullPath())));
        m_loaded = book && GetHelpData()->AddBookParam(*book, wxFONTENCODING_SYSTEM, index.title, index.contentsFile,
                                                        index.indexFile, index.defaultTopic, index.baseUrl);
    }

    bool IsLoaded() const { return m_loaded; }
    wxHtmlHelpFrame* Frame() const { return m_helpFrame; }

protected:
    wxHtmlHelpFrame* CreateHelpFrame(wxHtmlHelpData* data) override {
        HelpFrame* frame = new HelpFrame(data);
        frame->SetController(this);
        frame->SetTitleFormat(m_titleFormat);
#if wxUSE_CONFIG
        frame->Create(m_parentWindow, wxID_ANY, wxEmptyString, m_FrameStyle, m_Config, m_ConfigRoot);
        bool configured = m_Config != nullptr;
#else
        frame->Create(m_parentWindow, wxID_ANY, wxEmptyString, m_FrameStyle);
        bool configured = false;
#endif
        frame->SetShouldPreventAppExit(m_shouldPreventAppExit);
        if (!configured) {
            frame->SetSize(DefaultHelpSize(frame));
            frame->Centre();
        }
        m_helpFrame = frame;
        return frame;
    }

private:
    bool m_loaded = false;
};

enum class Pane { Page, Contents, Index };

struct HelpRequest {
    wxString topic;
    Pane pane = Pane::Page;
};

struct HelpState {
    HelpController* controller = nullptr;
    wxString controllerDir;
    std::vector<std::unique_ptr<HelpController>> controllers;
    HelpDialog* modalHelp = nullptr;
    HelpRequest pending;
    bool pendingScheduled = false;
};

// Never destroyed: the controllers must not outlive the wx windows they point to at exit.
HelpState& State() {
    static HelpState* state = new HelpState;
    return *state;
}

HelpController* ControllerFor(const HelpIndex& index) {
    HelpState& st = State();
    if (st.controller && st.controllerDir == index.dir)
        return st.controller;
    if (st.controller)
        st.controller->Quit();
    static bool registered = false;
    if (!registered) {
        registered = true;
        wxFileSystem::AddHandler(new HelpFSHandler);
    }
    std::unique_ptr<HelpController> c(new HelpController(index));
    if (!c->IsLoaded())
        return nullptr;
    st.controller = c.get();
    st.controllerDir = index.dir;
    st.controllers.push_back(std::move(c));
    return st.controller;
}

wxDialog* ActiveModalDialog() {
    wxDialog* found = nullptr;
    for (wxWindow* w : wxTopLevelWindows) {
        wxDialog* d = wxDynamicCast(w, wxDialog);
        if (d && d->IsModal() && d->IsShown())
            found = d;
    }
    return found;
}

HWND PseudoHelpWindow() {
    static char tag;
    return reinterpret_cast<HWND>(&tag);
}

HWND HelpWindowHandle() {
    HelpState& st = State();
    if (st.modalHelp)
        return ToHwnd(st.modalHelp);
    if (st.controller && st.controller->Frame())
        return ToHwnd(st.controller->Frame());
    return PseudoHelpWindow();
}

void Present(wxHtmlHelpWindow* window, const HelpRequest& request) {
    if (request.pane == Pane::Contents)
        window->DisplayContents();
    else if (request.pane == Pane::Index)
        window->DisplayIndex();
    if (!request.topic.empty())
        window->Display(request.topic);
}

void RunModalHelp(wxDialog* parent, HelpController* controller, const wxString& title, const HelpRequest& request) {
    HelpState& st = State();
    HelpDialog dlg(controller->GetHelpData());
    dlg.Create(parent, wxID_ANY, wxEmptyString, wxHF_DEFAULT_STYLE);
    if (!title.empty())
        dlg.SetTitle(title);
    dlg.SetSize(DefaultHelpSize(&dlg));
    dlg.CentreOnParent();
    Present(dlg.GetHelpWindow(), request);
    st.modalHelp = &dlg;
    dlg.ShowModal();
    st.modalHelp = nullptr;
}

void Display(const HelpRequest& request) {
    HelpState& st = State();
    const HelpIndex* index = CurrentIndex();
    HelpController* controller = index ? ControllerFor(*index) : nullptr;
    if (!controller)
        return;
    if (st.modalHelp) {
        Present(st.modalHelp->GetHelpWindow(), request);
        st.modalHelp->Raise();
    } else if (wxDialog* modal = ActiveModalDialog()) {
        RunModalHelp(modal, controller, index->title, request);
    } else {
        if (request.pane == Pane::Contents)
            controller->DisplayContents();
        else if (request.pane == Pane::Index)
            controller->DisplayIndex();
        if (!request.topic.empty())
            controller->Display(request.topic);
        if (wxHtmlHelpFrame* frame = controller->Frame()) {
            frame->Show();
            frame->Raise();
        }
    }
}

void ShowPending() {
    HelpState& st = State();
    if (!st.pendingScheduled)
        return;
    st.pendingScheduled = false;
    Display(st.pending);
}

// Frames get no input while a modal dialog runs (macOS, GTK), so the help is then shown modally, deferred.
HWND Show(const HelpIndex& index, const HelpRequest& request) {
    if (!ControllerFor(index))
        return nullptr;
    HelpState& st = State();
    if (!st.modalHelp && !ActiveModalDialog()) {
        Display(request);
    } else {
        st.pending = request;
        if (!st.pendingScheduled) {
            st.pendingScheduled = true;
            wxTheApp->CallAfter(&ShowPending);
        }
    }
    return HelpWindowHandle();
}

// "file.chm::/topic.html>window" or "topic.html#anchor" to a topic file relative to the help directory.
wxString TopicFromReference(const wxString& reference) {
    wxString topic = reference;
    int sep = topic.Find("::");
    if (sep != wxNOT_FOUND)
        topic = topic.Mid(sep + 2);
    topic = topic.BeforeFirst('>');
    topic.Replace("\\", "/");
    while (topic.StartsWith("/"))
        topic.Remove(0, 1);
    return topic;
}

// A topic as the help data expects it: relative to the help directory, in the file's case.
wxString TopicPath(const HelpIndex& index, const wxString& topic) {
    wxString file = topic.BeforeFirst('#');
    wxString anchor = topic.AfterFirst('#');
    auto page = index.pages.find(file.Lower());
    if (page != index.pages.end()) {
        file = page->second;
    } else {
        wxString path = ResolveCaseInsensitive(index.dir + wxFILE_SEP_PATH + file);
        if (file.empty() || path.empty())
            return wxString();
        file = path.Mid(index.dir.length() + 1);
        file.Replace("\\", "/");
    }
    return file + (anchor.empty() ? wxString() : "#" + anchor);
}

HWND ShowTopic(const HelpIndex& index, const wxString& topic, Pane pane = Pane::Page) {
    if (IsExternalUrl(topic) && !topic.Lower().StartsWith("mk:") && !topic.Lower().StartsWith("ms-its:")) {
        wxLaunchDefaultBrowser(topic);
        return PseudoHelpWindow();
    }
    HelpRequest request;
    request.topic = TopicPath(index, topic);
    request.pane = pane;
    if (request.topic.empty() && pane == Pane::Page)
        return nullptr;
    return Show(index, request);
}

wxWindow* DialogParent(HWND caller) {
    HelpState& st = State();
    if (st.modalHelp)
        return st.modalHelp;
    if (wxDialog* modal = ActiveModalDialog())
        return modal;
    if (st.controller && st.controller->Frame() && st.controller->Frame()->IsShown())
        return st.controller->Frame();
    return caller ? ToWx(caller) : MainWxWindow();
}

HWND LinkLookup(HWND caller, const HelpIndex& index, const HH_AKLINK* link, bool alink) {
    if (!link || !link->pszKeywords)
        return nullptr;
    const std::multimap<wxString, wxString>& table = alink ? index.alinks : index.keywords;
    std::vector<wxString> pages;
    wxStringTokenizer keywords(ToWx(link->pszKeywords), ";", wxTOKEN_STRTOK);
    while (keywords.HasMoreTokens()) {
        wxString keyword = keywords.GetNextToken().Trim().Trim(false).Lower();
        auto range = table.equal_range(keyword);
        for (auto it = range.first; it != range.second; ++it)
            if (std::find(pages.begin(), pages.end(), it->second) == pages.end())
                pages.push_back(it->second);
    }
    if (pages.empty()) {
        if (link->pszUrl && *link->pszUrl)
            return ShowTopic(index, TopicFromReference(ToWx(link->pszUrl)));
        if (link->fIndexOnFail)
            return ShowTopic(index, wxString(), Pane::Index);
        if (link->pszMsgText && *link->pszMsgText) {
            ShowMessageBox(DialogParent(caller), link->pszMsgText, link->pszMsgTitle, MB_OK | MB_ICONINFORMATION);
            return HelpWindowHandle();
        }
        return nullptr;
    }
    wxString page = pages.front();
    if (pages.size() > 1) {
        wxArrayString choices;
        for (const wxString& p : pages) {
            auto t = index.titles.find(p);
            choices.Add(t != index.titles.end() && !t->second.empty() ? t->second : p);
        }
        wxSingleChoiceDialog dlg(DialogParent(caller), _("Click a topic, then click Display."), _("Topics Found"),
                                 choices);
        if (dlg.ShowModal() != wxID_OK)
            return HelpWindowHandle();
        page = pages[static_cast<size_t>(dlg.GetSelection())];
    }
    return ShowTopic(index, page);
}

void CloseAll() {
    HelpState& st = State();
    st.pendingScheduled = false;
    if (st.modalHelp)
        st.modalHelp->EndModal(wxID_CANCEL);
    if (st.controller)
        st.controller->Quit();
}

HWND HtmlHelpOnMain(HWND caller, const char* pszFile, UINT uCommand, DWORD_PTR dwData) {
    switch (uCommand) {
    case HH_INITIALIZE:
        if (dwData)
            *reinterpret_cast<DWORD*>(dwData) = 1;
        return PseudoHelpWindow();
    case HH_UNINITIALIZE:
    case HH_CLOSE_ALL:
        CloseAll();
        return PseudoHelpWindow();
    case HH_SET_WIN_TYPE:
    case HH_SYNC:
        return PseudoHelpWindow();
    case HH_GET_WIN_TYPE:
        if (dwData)
            *reinterpret_cast<HH_WINTYPE**>(dwData) = nullptr;
        return nullptr;
    case HH_GET_WIN_HANDLE: {
        HWND h = HelpWindowHandle();
        return h == PseudoHelpWindow() ? nullptr : h;
    }
    default:
        break;
    }

    const HelpIndex* index = CurrentIndex();
    if (!index)
        return nullptr;
    switch (uCommand) {
    case HH_DISPLAY_TOPIC: {
        wxString topic;
        if (dwData)
            topic = TopicFromReference(ToWx(reinterpret_cast<const char*>(dwData)));
        else if (pszFile && strstr(pszFile, "::"))
            topic = TopicFromReference(ToWx(pszFile));
        return ShowTopic(*index, topic.empty() ? index->defaultTopic : topic);
    }
    case HH_DISPLAY_TOC:
    case HH_DISPLAY_SEARCH:
        return ShowTopic(*index, wxString(), Pane::Contents);
    case HH_DISPLAY_INDEX:
        return ShowTopic(*index, wxString(), Pane::Index);
    case HH_HELP_CONTEXT: {
        auto it = index->contexts.find(static_cast<DWORD>(dwData));
        return it == index->contexts.end() ? nullptr : ShowTopic(*index, it->second);
    }
    case HH_ALINK_LOOKUP:
    case HH_KEYWORD_LOOKUP:
        return LinkLookup(caller, *index, reinterpret_cast<const HH_AKLINK*>(dwData), uCommand == HH_ALINK_LOOKUP);
    default:
        return nullptr;
    }
}

} // namespace

} // namespace mfcwx

using namespace mfcwx;

HWND HtmlHelp(HWND hwndCaller, const char* pszFile, UINT uCommand, DWORD_PTR dwData) {
    return OnMain([&]() -> HWND { return HtmlHelpOnMain(hwndCaller, pszFile, uCommand, dwData); });
}

HWND HtmlHelpA(HWND hwndCaller, const char* pszFile, UINT uCommand, DWORD_PTR dwData) {
    return HtmlHelp(hwndCaller, pszFile, uCommand, dwData);
}

int GetMenuStringW(HMENU hMenu, UINT uIDItem, LPWSTR lpString, int cchMax, UINT flags) {
    int length = ::GetMenuString(hMenu, uIDItem, nullptr, 0, flags);
    if (!lpString || cchMax <= 0)
        return length;
    std::vector<char> ansi(static_cast<size_t>(length) + 1);
    ::GetMenuString(hMenu, uIDItem, ansi.data(), length + 1, flags);
    int n = std::min(length, cchMax - 1);
    for (int i = 0; i < n; ++i)
        lpString[i] = AnsiToUnicode(static_cast<unsigned char>(ansi[static_cast<size_t>(i)]));
    lpString[n] = 0;
    return n;
}

void CWinApp::WinHelp(DWORD_PTR dwData, UINT nCmd) {
    m_bHelpMode = FALSE;
    WinHelpInternal(dwData, nCmd);
}

void CWinApp::WinHelpInternal(DWORD_PTR dwData, UINT nCmd) {
    m_bHelpMode = FALSE;
    switch (nCmd) {
    case HELP_CONTEXT:
    case HELP_CONTEXTPOPUP:
        HtmlHelp(dwData, HH_HELP_CONTEXT);
        break;
    case HELP_CONTENTS:
        HtmlHelp(0, HH_DISPLAY_TOC);
        break;
    case HELP_KEY:
    case HELP_PARTIALKEY: {
        HH_AKLINK link = {};
        link.cbStruct = sizeof link;
        link.pszKeywords = reinterpret_cast<const char*>(dwData);
        link.fIndexOnFail = TRUE;
        HtmlHelp(reinterpret_cast<DWORD_PTR>(&link), HH_KEYWORD_LOOKUP);
        break;
    }
    case HELP_QUIT:
        HtmlHelp(0, HH_CLOSE_ALL);
        break;
    default:
        HtmlHelp(0, HH_DISPLAY_TOPIC);
        break;
    }
}

void CWinApp::HtmlHelp(DWORD_PTR dwData, UINT nCmd) {
    m_bHelpMode = FALSE;
    CWnd* main = AfxGetMainWnd();
    if (!::HtmlHelp(main ? main->m_hWnd : nullptr, m_pszHelpFilePath, nCmd, dwData))
        AfxMessageBox(FromWx(UiString(AFX_IDP_FAILED_TO_LAUNCH_HELP, "Failed to launch help.")).c_str(),
                      MB_OK | MB_ICONINFORMATION);
}
