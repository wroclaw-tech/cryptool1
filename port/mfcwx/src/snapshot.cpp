#include "windows_impl.h"

#include <wx/dcclient.h>
#include <wx/dcmemory.h>
#include <wx/aui/auibook.h>
#include <wx/dialog.h>
#include <wx/timer.h>
#include <wx/toplevel.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

namespace mfcwx {

bool SnapshotWxWindow(wxWindow* w, const std::string& pngPath);

#ifndef __APPLE__
bool SnapshotWxWindow(wxWindow* w, const std::string& pngPath) {
    if (!w)
        return false;
    wxClientDC src(w);
    wxSize size = w->GetClientSize();
    if (size.x <= 0 || size.y <= 0)
        return false;
    wxBitmap bmp(size);
    wxMemoryDC dst(bmp);
    dst.Blit(0, 0, size.x, size.y, &src, 0, 0);
    dst.SelectObject(wxNullBitmap);
    return bmp.SaveFile(wxString::FromUTF8(pngPath.c_str()), wxBITMAP_TYPE_PNG);
}
#endif

namespace {

// Development aid: MFCWX_SNAPSHOT_DIR=dir saves PNGs of all shown top-level windows every
// MFCWX_SNAPSHOT_INTERVAL ms (default 1500) for MFCWX_SNAPSHOT_COUNT rounds (default 1).
// MFCWX_SNAPSHOT_DISMISS=1 cancels the active modal dialog after each round; the process exits
// after the last round.
class SnapshotTimer : public wxTimer {
public:
    SnapshotTimer(std::string dir, int rounds, bool dismiss) : m_dir(std::move(dir)), m_rounds(rounds), m_dismiss(dismiss) {}

    void Notify() override {
        ++m_round;
        int index = 0;
        for (wxWindow* w : wxTopLevelWindows) {
            if (!w->IsShown())
                continue;
            wxString title = w->GetLabel();
            std::string name;
            for (wxUniChar c : title)
                name += (c.IsAscii() && isalnum(static_cast<int>(c.GetValue()))) ? static_cast<char>(c.GetValue()) : '_';
            char path[1024];
            snprintf(path, sizeof path, "%s/%02d_%d_%s.png", m_dir.c_str(), m_round, index++, name.substr(0, 40).c_str());
            bool ok = SnapshotWxWindow(w, path);
            fprintf(stderr, "mfcwx snapshot %s %s (%dx%d)\n", ok ? "saved" : "FAILED", path, w->GetSize().x, w->GetSize().y);
        }
        if (m_round >= m_rounds) {
            fflush(stderr);
            _exit(0);
        }
        if (m_dismiss) {
            for (wxWindow* w : wxTopLevelWindows) {
                auto* d = wxDynamicCast(w, wxDialog);
                if (d && d->IsModal()) {
                    if (CWnd* wnd = WrapperFor(d))
                        wnd->PostMessage(WM_COMMAND, IDCANCEL, 0);
                    else
                        d->EndModal(wxID_CANCEL);
                    break;
                }
            }
        }
    }

private:
    std::string m_dir;
    int m_rounds;
    bool m_dismiss;
    int m_round = 0;
};

std::string SafeName(const wxString& title) {
    std::string name;
    for (wxUniChar c : title)
        name += (c.IsAscii() && isalnum(static_cast<int>(c.GetValue()))) ? static_cast<char>(c.GetValue()) : '_';
    return name.substr(0, 40);
}

wxAuiTabCtrl* FindTabCtrl(wxWindow* w) {
    if (auto* t = wxDynamicCast(w, wxAuiTabCtrl))
        return t;
    for (wxWindow* c : w->GetChildren())
        if (wxAuiTabCtrl* t = FindTabCtrl(c))
            return t;
    return nullptr;
}

void ClickTab(int index, bool closeButton) {
    wxWindow* main = MainWxWindow();
    wxAuiTabCtrl* tabs = main ? FindTabCtrl(main) : nullptr;
    if (!tabs || index < 0 || static_cast<size_t>(index) >= tabs->GetPageCount()) {
        fprintf(stderr, "mfcwx automation: no tab %d\n", index);
        return;
    }
    wxRect r = tabs->GetPage(static_cast<size_t>(index)).rect;
    wxPoint pt = closeButton ? wxPoint(r.GetRight() - 12, r.y + r.height / 2) : wxPoint(r.x + r.width / 3, r.y + r.height / 2);
    for (wxEventType type : {wxEVT_MOTION, wxEVT_LEFT_DOWN, wxEVT_LEFT_UP}) {
        wxMouseEvent e(type);
        e.SetEventObject(tabs);
        e.SetPosition(pt);
        e.m_leftDown = type == wxEVT_LEFT_DOWN;
        tabs->GetEventHandler()->ProcessEvent(e);
    }
}

wxWindow* ActiveDialog() {
    wxWindow* found = nullptr;
    for (wxWindow* w : wxTopLevelWindows) {
        auto* d = wxDynamicCast(w, wxDialog);
        if (d && d->IsShown())
            found = d;
    }
    return found;
}

// Development aid: MFCWX_AUTOMATION=file runs one step per line on a timer, for UI tests:
//   command <id>        WM_COMMAND to the main window
//   button <id>         WM_COMMAND (BN_CLICKED) to the topmost dialog
//   text <id> <text>    sets the text of a control of the topmost dialog
//   tab <n> / closetab <n>   clicks the n-th MDI tab or its close button
//   tabs                print the MDI tabs
//   wait <ms>
//   snapshot <label>    PNGs of all shown top-level windows into MFCWX_SNAPSHOT_DIR (or .)
//   quit
class AutomationTimer : public wxTimer {
public:
    AutomationTimer(std::vector<std::string> steps, std::string dir) : m_steps(std::move(steps)), m_dir(std::move(dir)) {}

    void Notify() override {
        if (m_waitUntil != 0 && wxGetLocalTimeMillis() < m_waitUntil)
            return;
        m_waitUntil = 0;
        if (m_next >= m_steps.size()) {
            fprintf(stderr, "mfcwx automation: done\n");
            fflush(stderr);
            _exit(0);
        }
        std::string line = m_steps[m_next++];
        std::istringstream in(line);
        std::string op;
        in >> op;
        fprintf(stderr, "mfcwx automation: %s\n", line.c_str());
        if (op == "command" || op == "button") {
            int id = 0;
            in >> id;
            CWnd* target = op == "command" ? AfxGetMainWnd() : WrapperFor(ActiveDialog());
            if (target && target->m_hWnd)
                target->PostMessage(WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), 0);
            else
                fprintf(stderr, "mfcwx automation: no target window\n");
        } else if (op == "text") {
            int id = 0;
            in >> id;
            std::string text;
            std::getline(in, text);
            if (!text.empty() && text[0] == ' ')
                text.erase(0, 1);
            CWnd* dlg = WrapperFor(ActiveDialog());
            if (dlg && dlg->m_hWnd)
                ::SetDlgItemText(dlg->m_hWnd, id, text.c_str());
        } else if (op == "tab" || op == "closetab") {
            int index = 0;
            in >> index;
            ClickTab(index, op == "closetab");
        } else if (op == "tabs") {
            wxWindow* main = MainWxWindow();
            wxAuiTabCtrl* tabs = main ? FindTabCtrl(main) : nullptr;
            if (tabs)
                for (size_t i = 0; i < tabs->GetPageCount(); ++i)
                    fprintf(stderr, "mfcwx automation: tab %zu \"%s\"%s\n", i,
                            static_cast<const char*>(tabs->GetPage(i).caption.utf8_str()),
                            tabs->GetPage(i).active ? " (active)" : "");
        } else if (op == "wait") {
            long ms = 0;
            in >> ms;
            m_waitUntil = wxGetLocalTimeMillis() + ms;
        } else if (op == "snapshot") {
            std::string label;
            in >> label;
            int index = 0;
            for (wxWindow* w : wxTopLevelWindows) {
                if (!w->IsShown())
                    continue;
                char path[1024];
                snprintf(path, sizeof path, "%s/%s_%d_%s.png", m_dir.c_str(), label.c_str(), index++,
                         SafeName(w->GetLabel()).c_str());
                bool ok = SnapshotWxWindow(w, path);
                fprintf(stderr, "mfcwx snapshot %s %s\n", ok ? "saved" : "FAILED", path);
            }
        } else if (op == "quit") {
            m_next = m_steps.size();
        }
    }

private:
    std::vector<std::string> m_steps;
    std::string m_dir;
    size_t m_next = 0;
    wxLongLong m_waitUntil = 0;
};

} // namespace

void StartSnapshotTimerFromEnvironment() {
    if (const char* script = getenv("MFCWX_AUTOMATION")) {
        std::ifstream f(script);
        std::vector<std::string> steps;
        for (std::string line; std::getline(f, line);)
            if (!line.empty() && line[0] != '#')
                steps.push_back(line);
        const char* outDir = getenv("MFCWX_SNAPSHOT_DIR");
        static AutomationTimer automation(steps, outDir && *outDir ? outDir : ".");
        automation.Start(200);
        return;
    }
    const char* dir = getenv("MFCWX_SNAPSHOT_DIR");
    if (!dir || !*dir)
        return;
    const char* interval = getenv("MFCWX_SNAPSHOT_INTERVAL");
    const char* count = getenv("MFCWX_SNAPSHOT_COUNT");
    const char* dismiss = getenv("MFCWX_SNAPSHOT_DISMISS");
    static SnapshotTimer timer(dir, count ? std::max(1, atoi(count)) : 1, dismiss && *dismiss == '1');
    timer.Start(interval ? std::max(100, atoi(interval)) : 1500);
}

} // namespace mfcwx
