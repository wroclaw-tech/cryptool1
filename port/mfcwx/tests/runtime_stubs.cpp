// Stand-ins for GUI-side symbols so the runtime tests link without the window/app parts.

#include "afxwin.h"

#include <string>
#include <unistd.h>

CRuntimeClass CCmdTarget::classCCmdTarget = {"CCmdTarget", sizeof(CCmdTarget), 0xFFFF, nullptr,
                                              &CObject::GetThisClass, nullptr};
static AFX_CLASSINIT _init_CCmdTarget(&CCmdTarget::classCCmdTarget);
CRuntimeClass* CCmdTarget::GetThisClass() { return &CCmdTarget::classCCmdTarget; }
CRuntimeClass* CCmdTarget::GetRuntimeClass() const { return &CCmdTarget::classCCmdTarget; }
BOOL CCmdTarget::OnCmdMsg(UINT, int, void*, AFX_CMDHANDLERINFO*) { return FALSE; }
const AFX_MSGMAP* CCmdTarget::GetThisMessageMap() { return nullptr; }
const AFX_MSGMAP* CCmdTarget::GetMessageMap() const { return nullptr; }

CWinApp* AFXAPI AfxGetApp() { return nullptr; }

int AFXAPI AfxMessageBox(const char* lpszText, UINT, UINT) {
    fprintf(stderr, "AfxMessageBox: %s\n", lpszText ? lpszText : "");
    return IDOK;
}

int AFXAPI AfxMessageBox(UINT nIDPrompt, UINT, UINT) {
    fprintf(stderr, "AfxMessageBox: #%u\n", nIDPrompt);
    return IDOK;
}

namespace mfcwx {

bool LoadResourceString(UINT, std::string&) { return false; }

std::string GetDataDirectory() {
    char buf[4096];
    return getcwd(buf, sizeof buf) ? std::string(buf) : std::string("/");
}

} // namespace mfcwx
