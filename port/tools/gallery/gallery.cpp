// Opens every dialog template of CrypTool.rc in a language and saves a PNG snapshot of each.
//   dialog_gallery [--lang de] [--out dir] [--only 101,102]

#include "stdafx_gallery.h"

#include "mfcwx/app.h"
#include "mfcwx/rcdata.h"

#include <set>
#include <string>

extern const mfcwx::rc::Module g_rcModule_CrypTool;
bool SnapshotWindow(HWND hWnd, const std::string& pngPath);

namespace {

struct RegisterModule {
    RegisterModule() {
        mfcwx::RegisterResourceModule(&g_rcModule_CrypTool);
        mfcwx::SetDefaultDataDirectory(CRYPTOOL_DEV_DATA_DIR);
    }
} registerModule;

class GalleryDialog : public CDialog {
public:
    explicit GalleryDialog(UINT id) : CDialog(id) {}
    BOOL OnInitDialog() override { return TRUE; }
};

class GalleryApp : public CWinApp {
public:
    BOOL InitInstance() override;
};

GalleryApp theApp;

BOOL GalleryApp::InitInstance() {
    std::string lang = "en";
    std::string out = "gallery";
    std::set<int> only;
    for (int i = 1; i + 1 < __argc; ++i) {
        std::string a = __argv[i];
        if (a == "--lang")
            lang = __argv[++i];
        else if (a == "--out")
            out = __argv[++i];
        else if (a == "--only") {
            std::string list = __argv[++i];
            size_t pos = 0;
            while (pos < list.size()) {
                size_t comma = list.find(',', pos);
                only.insert(atoi(list.substr(pos, comma - pos).c_str()));
                pos = comma == std::string::npos ? list.size() : comma + 1;
            }
        }
    }
    mfcwx::SetResourceLanguage(lang.c_str());
    _mkdir(out.c_str());
    const mfcwx::rc::Language* language = nullptr;
    for (int i = 0; i < g_rcModule_CrypTool.languageCount; ++i)
        if (lang == g_rcModule_CrypTool.languages[i]->code)
            language = g_rcModule_CrypTool.languages[i];
    if (!language)
        return FALSE;
    int saved = 0;
    for (int i = 0; i < language->dialogCount; ++i) {
        const mfcwx::rc::Dialog& d = language->dialogs[i];
        if (!d.id || (!only.empty() && !only.count(d.id)))
            continue;
        GalleryDialog dlg(static_cast<UINT>(d.id));
        if (!dlg.Create(static_cast<UINT>(d.id), nullptr))
            continue;
        dlg.ShowWindow(SW_SHOW);
        dlg.UpdateWindow();
        MSG msg;
        for (int k = 0; k < 5; ++k)
            PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
        char path[1024];
        snprintf(path, sizeof path, "%s/%s_%05d_%s.png", out.c_str(), lang.c_str(), d.id, d.name ? d.name : "");
        if (SnapshotWindow(dlg.m_hWnd, path))
            ++saved;
        dlg.DestroyWindow();
    }
    printf("saved %d dialog snapshots to %s\n", saved, out.c_str());
    return FALSE;
}

} // namespace
