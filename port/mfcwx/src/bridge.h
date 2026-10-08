#pragma once

#include <wx/wx.h>

#include "afxwin.h"
#include "mfcwx/codepage.h"

namespace mfcwx {

wxString ToWx(const char* s, int length = -1);
inline wxString ToWx(const CString& s) { return ToWx(s.GetString(), s.GetLength()); }
std::string FromWx(const wxString& s);
inline CString CStr(const wxString& s) { std::string a = FromWx(s); return CString(a.data(), static_cast<int>(a.size())); }
inline wxString NativePathWx(const char* path) { return wxString::FromUTF8(NativePath(path)); }

} // namespace mfcwx
