#include "internal.h"

#include <wx/filename.h>
#include <wx/image.h>
#include <wx/stdpaths.h>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace mfcwx {

namespace {

struct ResourceContext {
    std::vector<const rc::Module*> modules;
    std::string language = "en";
    std::string dataDir;
    std::string defaultDataDir;
    std::mutex bitmapMutex;
    std::unordered_map<std::string, wxBitmap> bitmapCache;
};

ResourceContext& Ctx() {
    static ResourceContext ctx;
    return ctx;
}

bool SameName(const char* a, const char* b) { return a && b && strcasecmp(a, b) == 0; }

// Languages to search, in order of preference.
std::vector<const rc::Language*> SearchOrder() {
    std::vector<const rc::Language*> order;
    const std::string& lang = Ctx().language;
    auto add = [&order](const rc::Language* l) {
        if (l && std::find(order.begin(), order.end(), l) == order.end())
            order.push_back(l);
    };
    for (const rc::Module* m : Ctx().modules) {
        for (int i = 0; i < m->languageCount; ++i)
            if (lang == m->languages[i]->code)
                add(m->languages[i]);
    }
    for (const char* fallback : {"en", "de", "neutral"}) {
        for (const rc::Module* m : Ctx().modules)
            for (int i = 0; i < m->languageCount; ++i)
                if (strcmp(m->languages[i]->code, fallback) == 0)
                    add(m->languages[i]);
    }
    for (const rc::Module* m : Ctx().modules)
        for (int i = 0; i < m->languageCount; ++i)
            add(m->languages[i]);
    return order;
}

int CodePageForLanguage(const char* code) {
    for (const rc::Module* m : Ctx().modules)
        for (int i = 0; i < m->languageCount; ++i)
            if (strcmp(m->languages[i]->code, code) == 0 && m->languages[i]->codepage)
                return m->languages[i]->codepage;
    return 1252;
}

} // namespace

ResRef ResRef::From(const char* lpszName) {
    ResRef r;
    if (!lpszName)
        return r;
    if (IS_INTRESOURCE(lpszName)) {
        r.id = static_cast<int>(reinterpret_cast<uintptr_t>(lpszName));
    } else if (lpszName[0] == '#') {
        r.id = atoi(lpszName + 1);
    } else {
        r.name = lpszName;
    }
    return r;
}

bool ResRef::Matches(int otherId, const char* otherName) const {
    if (!name.empty())
        return SameName(name.c_str(), otherName);
    return id != 0 && id == otherId;
}

void RegisterResourceModule(const rc::Module* module) {
    if (module && std::find(Ctx().modules.begin(), Ctx().modules.end(), module) == Ctx().modules.end())
        Ctx().modules.push_back(module);
}

void SetResourceLanguage(const char* code) {
    Ctx().language = code ? code : "en";
    SetAnsiCodePage(CodePageForLanguage(Ctx().language.c_str()));
}

const char* GetResourceLanguage() { return Ctx().language.c_str(); }

std::vector<std::string> AvailableResourceLanguages() {
    std::vector<std::string> codes;
    for (const rc::Module* m : Ctx().modules)
        for (int i = 0; i < m->languageCount; ++i) {
            const rc::Language* l = m->languages[i];
            if (l->dialogCount > 10 && std::find(codes.begin(), codes.end(), l->code) == codes.end())
                codes.push_back(l->code);
        }
    return codes;
}

const rc::Dialog* FindDialog(const ResRef& ref) {
    for (const rc::Language* l : SearchOrder())
        for (int i = 0; i < l->dialogCount; ++i)
            if (ref.Matches(l->dialogs[i].id, l->dialogs[i].name))
                return &l->dialogs[i];
    return nullptr;
}

const rc::Menu* FindMenu(const ResRef& ref) {
    for (const rc::Language* l : SearchOrder())
        for (int i = 0; i < l->menuCount; ++i)
            if (ref.Matches(l->menus[i].id, l->menus[i].name))
                return &l->menus[i];
    return nullptr;
}

const rc::AccelTable* FindAccelTable(const ResRef& ref) {
    for (const rc::Language* l : SearchOrder())
        for (int i = 0; i < l->accelCount; ++i)
            if (ref.Matches(l->accels[i].id, l->accels[i].name))
                return &l->accels[i];
    return nullptr;
}

const rc::Toolbar* FindToolbar(const ResRef& ref) {
    for (const rc::Language* l : SearchOrder())
        for (int i = 0; i < l->toolbarCount; ++i)
            if (ref.Matches(l->toolbars[i].id, l->toolbars[i].name))
                return &l->toolbars[i];
    return nullptr;
}

std::string FindFileResource(const char* type, const ResRef& ref) {
    for (const rc::Language* l : SearchOrder())
        for (int i = 0; i < l->fileCount; ++i) {
            const rc::FileResource& f = l->files[i];
            if ((!type || SameName(type, f.type)) && ref.Matches(f.id, f.name))
                return GetDataDirectory() + "/" + f.path;
        }
    return std::string();
}

std::vector<const rc::DlgInitEntry*> FindDlgInit(int dialogId) {
    std::vector<const rc::DlgInitEntry*> result;
    for (const rc::Language* l : SearchOrder()) {
        for (int i = 0; i < l->dlgInitCount; ++i)
            if (l->dlgInits[i].dialogId == dialogId)
                result.push_back(&l->dlgInits[i]);
        if (!result.empty())
            break;
    }
    return result;
}

bool LoadResourceString(UINT id, std::string& out) {
    for (const rc::Language* l : SearchOrder()) {
        const rc::StringEntry* begin = l->strings;
        const rc::StringEntry* end = l->strings + l->stringCount;
        const rc::StringEntry* it = std::lower_bound(
            begin, end, static_cast<int>(id), [](const rc::StringEntry& e, int v) { return e.id < v; });
        if (it != end && it->id == static_cast<int>(id)) {
            out = Utf8ToAnsi(it->text);
            return true;
        }
    }
    return false;
}

const rc::VersionInfo* GetVersionInfo() {
    for (const rc::Language* l : SearchOrder())
        if (l->version)
            return l->version;
    return nullptr;
}

std::string GetDataDirectory() {
    if (Ctx().dataDir.empty()) {
        wxString env;
        if (wxGetEnv("MFCWX_DATA_DIR", &env) && wxDirExists(env)) {
            Ctx().dataDir = std::string(env.utf8_str());
        } else {
            wxString installed = wxStandardPaths::Get().GetResourcesDir();
#if !defined(__WXOSX__)
            wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
            wxString relative = exe.GetPath() + "/../share/" + exe.GetName();
            if (wxDirExists(relative + "/res")) {
                wxFileName dir = wxFileName::DirName(relative);
                dir.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
                installed = dir.GetPath();
            }
#endif
            if (wxDirExists(installed + "/res") || Ctx().defaultDataDir.empty())
                Ctx().dataDir = std::string(installed.utf8_str());
            else
                Ctx().dataDir = Ctx().defaultDataDir;
        }
    }
    return Ctx().dataDir;
}

void SetDefaultDataDirectory(const std::string& dir) { Ctx().defaultDataDir = dir; }

void SetDataDirectory(const std::string& dir) { Ctx().dataDir = dir; }

wxBitmap LoadBitmapResource(const ResRef& ref) {
    std::string path = FindFileResource("BITMAP", ref);
    if (path.empty())
        path = FindFileResource(nullptr, ref);
    if (path.empty())
        return wxBitmap();
    std::lock_guard<std::mutex> lock(Ctx().bitmapMutex);
    auto it = Ctx().bitmapCache.find(path);
    if (it != Ctx().bitmapCache.end())
        return it->second;
    if (!wxImage::FindHandler(wxBITMAP_TYPE_BMP))
        wxInitAllImageHandlers();
    wxLogNull noLog;
    wxImage image;
    if (!image.LoadFile(wxString::FromUTF8(path), wxBITMAP_TYPE_ANY))
        return wxBitmap();
    wxBitmap bitmap(image);
    Ctx().bitmapCache.emplace(path, bitmap);
    return bitmap;
}

wxIcon LoadIconResource(const ResRef& ref) {
    std::string path = FindFileResource("ICON", ref);
    wxIcon icon;
    if (path.empty())
        return icon;
    if (!wxImage::FindHandler(wxBITMAP_TYPE_ICO))
        wxInitAllImageHandlers();
    wxLogNull noLog;
    wxImage image;
    if (image.LoadFile(wxString::FromUTF8(path), wxBITMAP_TYPE_ICO))
        icon.CopyFromBitmap(wxBitmap(image));
    return icon;
}

wxCursor LoadCursorResource(const ResRef& ref) {
    std::string path = FindFileResource("CURSOR", ref);
    if (path.empty())
        return wxCursor();
    if (!wxImage::FindHandler(wxBITMAP_TYPE_CUR))
        wxInitAllImageHandlers();
    wxLogNull noLog;
    wxImage image;
    if (!image.LoadFile(wxString::FromUTF8(path), wxBITMAP_TYPE_CUR))
        return wxCursor();
    return wxCursor(image);
}

wxString MenuTextToWx(const char* utf8Text) {
    wxString text = wxString::FromUTF8(utf8Text ? utf8Text : "");
    int tab = text.Find('\t');
    if (tab != wxNOT_FOUND)
        text = text.Left(tab);
    return text;
}

} // namespace mfcwx

// ---------------------------------------------------------------------------------------------
// Win32 resource functions

int LoadString(HINSTANCE, UINT uID, char* lpBuffer, int cchBufferMax) {
    if (!lpBuffer || cchBufferMax <= 0)
        return 0;
    std::string s;
    if (!mfcwx::LoadResourceString(uID, s)) {
        lpBuffer[0] = 0;
        return 0;
    }
    int n = std::min(static_cast<int>(s.size()), cchBufferMax - 1);
    memcpy(lpBuffer, s.data(), static_cast<size_t>(n));
    lpBuffer[n] = 0;
    return n;
}

int LoadStringA(HINSTANCE hInstance, UINT uID, char* lpBuffer, int cchBufferMax) {
    return LoadString(hInstance, uID, lpBuffer, cchBufferMax);
}

namespace {

struct ResourceHandle {
    std::string type;
    mfcwx::ResRef ref;
    std::vector<char> data;
};

} // namespace

HRSRC FindResource(HMODULE, const char* lpName, const char* lpType) {
    mfcwx::ResRef ref = mfcwx::ResRef::From(lpName);
    std::string type;
    if (IS_INTRESOURCE(lpType)) {
        switch (static_cast<int>(reinterpret_cast<uintptr_t>(lpType))) {
        case 1: type = "CURSOR"; break;
        case 2: type = "BITMAP"; break;
        case 3: type = "ICON"; break;
        case 23: type = "HTML"; break;
        default: break;
        }
    } else if (lpType) {
        type = lpType;
    }
    if (type == "RT_DLGINIT" || (IS_INTRESOURCE(lpType) && reinterpret_cast<uintptr_t>(lpType) == 240))
        return nullptr;
    std::string path = mfcwx::FindFileResource(type.empty() ? nullptr : type.c_str(), ref);
    if (path.empty())
        return nullptr;
    auto* h = new ResourceHandle{type, ref, {}};
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        delete h;
        return nullptr;
    }
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0)
        h->data.insert(h->data.end(), buf, buf + n);
    fclose(f);
    return reinterpret_cast<HRSRC>(h);
}

HRSRC FindResourceA(HMODULE hModule, const char* lpName, const char* lpType) {
    return FindResource(hModule, lpName, lpType);
}

HGLOBAL LoadResource(HMODULE, HRSRC hResInfo) { return reinterpret_cast<HGLOBAL>(hResInfo); }

LPVOID LockResource(HGLOBAL hResData) {
    if (!hResData)
        return nullptr;
    auto* h = reinterpret_cast<ResourceHandle*>(hResData);
    return h->data.empty() ? nullptr : h->data.data();
}

DWORD SizeofResource(HMODULE, HRSRC hResInfo) {
    if (!hResInfo)
        return 0;
    return static_cast<DWORD>(reinterpret_cast<ResourceHandle*>(hResInfo)->data.size());
}

BOOL FreeResource(HGLOBAL) { return TRUE; }
