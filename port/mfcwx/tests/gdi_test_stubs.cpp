// Stand-ins for symbols of other mfcwx parts so the GDI tests link with the GDI sources only.

#include "internal.h"

#include <cstdio>

AFX_CLASSINIT::AFX_CLASSINIT(CRuntimeClass*) {}

CRuntimeClass CObject::classCObject = {"CObject", sizeof(CObject), 0xFFFF, nullptr, nullptr, nullptr};
CRuntimeClass* CObject::GetThisClass() { return &classCObject; }
CRuntimeClass* CObject::GetRuntimeClass() const { return &classCObject; }
void CObject::Serialize(CArchive&) {}

BOOL CRuntimeClass::IsDerivedFrom(const CRuntimeClass* pBaseClass) const {
    for (const CRuntimeClass* c = this; c; c = c->GetBaseClass())
        if (c == pBaseClass)
            return TRUE;
    return FALSE;
}

BOOL CObject::IsKindOf(const CRuntimeClass* pClass) const { return GetRuntimeClass()->IsDerivedFrom(pClass); }

void AfxAssertFailedLine(const char* file, int line, const char* expr) {
    fprintf(stderr, "ASSERT failed: %s:%d %s\n", file, line, expr ? expr : "");
}

void AfxDebugBreak() {}

CWnd* CWnd::FromHandle(HWND) { return nullptr; }

namespace mfcwx {

bool LoadResourceString(UINT, std::string&) { return false; }

ResRef ResRef::From(const char* lpszName) {
    ResRef r;
    if (IS_INTRESOURCE(lpszName))
        r.id = static_cast<int>(reinterpret_cast<uintptr_t>(lpszName));
    else if (lpszName)
        r.name = lpszName;
    return r;
}

wxBitmap LoadBitmapResource(const ResRef& ref) {
    if (ref.id != 1 && ref.name != "TEST")
        return wxNullBitmap;
    wxImage img(4, 2, true);
    img.SetRGB(0, 0, 255, 0, 0);
    img.SetRGB(3, 1, 0, 0, 255);
    return wxBitmap(img);
}

} // namespace mfcwx
