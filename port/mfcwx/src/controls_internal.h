#pragma once

// Control internals shared by controls.cpp, listctrl.cpp and ctrlclasses.cpp.

#include "windows_impl.h"

#include <string>
#include <utility>
#include <vector>

class wxTextCtrl;

namespace mfcwx {

// Kind-specific data of a control, kept in WindowState::extra. Each control kind uses one type.
template <class T>
T& Extra(WindowState& st) {
    if (!st.extra) {
        st.extra = new T();
        st.deleteExtra = [&st] { delete static_cast<T*>(st.extra); st.extra = nullptr; };
    }
    return *static_cast<T*>(st.extra);
}

struct TabExtra {
    std::vector<std::pair<wxString, LPARAM>> items;
    int current = -1;
    wxSize itemSize;
};

// controls.cpp: tab header geometry in client coordinates.
int TabHeaderHeight(wxWindow* w);
std::vector<wxRect> TabRects(wxWindow* w);

// listctrl.cpp
void BindListViewEvents(wxWindow* w);
LRESULT ListViewProc(wxWindow* w, WindowState& st, UINT msg, WPARAM wParam, LPARAM lParam, bool& handled);

// ctrlclasses.cpp: inserts RTF (ANSI code page) at the selection of a rich edit control.
void InsertRtf(wxTextCtrl* t, bool multiline, const std::string& rtf);

} // namespace mfcwx
