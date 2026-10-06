#include <string>

#include <wx/dcclient.h>
#include <wx/dcmemory.h>
#include <wx/window.h>

#include "afxwin.h"

bool SnapshotWindow(HWND hWnd, const std::string& pngPath) {
    CWnd* wnd = CWnd::FromHandle(hWnd);
    if (!wnd)
        return false;
    wxWindow* w = wnd->GetWx();
    wxClientDC src(w);
    wxSize size = w->GetClientSize();
    wxBitmap bmp(size);
    wxMemoryDC dst(bmp);
    dst.Blit(0, 0, size.x, size.y, &src, 0, 0);
    dst.SelectObject(wxNullBitmap);
    return bmp.SaveFile(wxString::FromUTF8(pngPath.c_str()), wxBITMAP_TYPE_PNG);
}
