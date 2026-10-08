#include "windows_impl.h"

#include <wx/dcclient.h>
#include <wx/dcmemory.h>
#include <wx/dialog.h>
#include <wx/timer.h>
#include <wx/toplevel.h>

#include <cstdlib>

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

} // namespace

void StartSnapshotTimerFromEnvironment() {
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
