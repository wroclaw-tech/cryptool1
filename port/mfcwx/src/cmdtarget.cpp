#include "internal.h"

// ---------------------------------------------------------------------------------------------
// CCmdTarget and message map lookup

CRuntimeClass CCmdTarget::classCCmdTarget = {"CCmdTarget", sizeof(CCmdTarget), 0xFFFF, nullptr,
                                              &CObject::GetThisClass, nullptr};
static AFX_CLASSINIT _init_CCmdTarget(&CCmdTarget::classCCmdTarget);
CRuntimeClass* CCmdTarget::GetThisClass() { return &CCmdTarget::classCCmdTarget; }
CRuntimeClass* CCmdTarget::GetRuntimeClass() const { return &CCmdTarget::classCCmdTarget; }

const AFX_MSGMAP* CCmdTarget::GetThisMessageMap() {
    static const AFX_MSGMAP_ENTRY entries[] = {{0, 0, 0, 0, AfxSig_end, nullptr}};
    static const AFX_MSGMAP map = {nullptr, &entries[0]};
    return &map;
}

const AFX_MSGMAP* CCmdTarget::GetMessageMap() const { return GetThisMessageMap(); }

const AFX_MSGMAP_ENTRY* CCmdTarget::FindMessageEntry(UINT nMessage, UINT nCode, UINT nID) const {
    for (const AFX_MSGMAP* map = GetMessageMap(); map; map = map->pfnGetBaseMap ? map->pfnGetBaseMap() : nullptr) {
        for (const AFX_MSGMAP_ENTRY* e = map->lpEntries; e->nSig != AfxSig_end; ++e) {
            if (e->nMessage == 0xC000) {
                if (nMessage >= 0xC000 && *reinterpret_cast<UINT*>(e->nSig) == nMessage)
                    return e;
                continue;
            }
            if (e->nMessage != nMessage || e->nCode != nCode)
                continue;
            if (nMessage == WM_COMMAND || nMessage == WM_NOTIFY) {
                if (nID >= e->nID && nID <= e->nLastID)
                    return e;
            } else {
                return e;
            }
        }
        if (map->pfnGetBaseMap == nullptr || map->pfnGetBaseMap() == map)
            break;
    }
    return nullptr;
}

struct AFX_NOTIFY {
    LRESULT* pResult;
    NMHDR* pNMHDR;
};

BOOL CCmdTarget::DispatchCommand(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) {
    UINT message = WM_COMMAND;
    UINT code = static_cast<UINT>(nCode);
    if (static_cast<UINT>(HIWORD(static_cast<DWORD>(nCode))) == WM_NOTIFY && nCode != static_cast<int>(CN_UPDATE_COMMAND_UI)) {
        message = WM_NOTIFY;
        code = static_cast<UINT>(static_cast<short>(LOWORD(static_cast<DWORD>(nCode))));
    }
    const AFX_MSGMAP_ENTRY* e = FindMessageEntry(message, code, nID);
    if (!e && message == WM_NOTIFY)
        e = FindMessageEntry(message, static_cast<UINT>(LOWORD(static_cast<DWORD>(nCode))), nID);
    if (!e)
        return FALSE;
    if (pHandlerInfo) {
        pHandlerInfo->pTarget = this;
        pHandlerInfo->pmf = e->pfn;
        return TRUE;
    }
    switch (e->nSig) {
    case AfxSigCmd_v:
        (this->*e->pfn)();
        return TRUE;
    case AfxSigCmd_b:
        return (this->*reinterpret_cast<BOOL (CCmdTarget::*)()>(e->pfn))();
    case AfxSigCmd_RANGE:
        (this->*reinterpret_cast<void (CCmdTarget::*)(UINT)>(e->pfn))(nID);
        return TRUE;
    case AfxSigCmd_EX:
        return (this->*reinterpret_cast<BOOL (CCmdTarget::*)(UINT)>(e->pfn))(nID);
    case AfxSigCmdUI: {
        auto* pCmdUI = static_cast<CCmdUI*>(pExtra);
        pCmdUI->m_bContinueRouting = FALSE;
        (this->*reinterpret_cast<void (CCmdTarget::*)(CCmdUI*)>(e->pfn))(pCmdUI);
        return !pCmdUI->m_bContinueRouting;
    }
    case AfxSigNotify_v: {
        auto* notify = static_cast<AFX_NOTIFY*>(pExtra);
        (this->*reinterpret_cast<void (CCmdTarget::*)(NMHDR*, LRESULT*)>(e->pfn))(notify->pNMHDR, notify->pResult);
        return TRUE;
    }
    case AfxSigNotify_b: {
        auto* notify = static_cast<AFX_NOTIFY*>(pExtra);
        return (this->*reinterpret_cast<BOOL (CCmdTarget::*)(NMHDR*, LRESULT*)>(e->pfn))(notify->pNMHDR,
                                                                                         notify->pResult);
    }
    case AfxSigNotify_RANGE: {
        auto* notify = static_cast<AFX_NOTIFY*>(pExtra);
        (this->*reinterpret_cast<void (CCmdTarget::*)(UINT, NMHDR*, LRESULT*)>(e->pfn))(nID, notify->pNMHDR,
                                                                                         notify->pResult);
        return TRUE;
    }
    case AfxSigNotify_EX: {
        auto* notify = static_cast<AFX_NOTIFY*>(pExtra);
        return (this->*reinterpret_cast<BOOL (CCmdTarget::*)(UINT, NMHDR*, LRESULT*)>(e->pfn))(
            nID, notify->pNMHDR, notify->pResult);
    }
    default:
        return FALSE;
    }
}

BOOL CCmdTarget::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) {
    return DispatchCommand(nID, nCode, pExtra, pHandlerInfo);
}

void CCmdTarget::BeginWaitCursor() { wxBeginBusyCursor(); }
void CCmdTarget::EndWaitCursor() {
    if (wxIsBusy())
        wxEndBusyCursor();
}
void CCmdTarget::RestoreWaitCursor() {}

// ---------------------------------------------------------------------------------------------
// CCmdUI

CCmdUI::CCmdUI()
    : m_nID(0), m_nIndex(0), m_pMenu(nullptr), m_pSubMenu(nullptr), m_pParentMenu(nullptr), m_pOther(nullptr),
      m_bEnableChanged(FALSE), m_bContinueRouting(FALSE), m_nIndexMax(0), m_enabled(true), m_check(0),
      m_checkSet(false), m_textSet(false) {}

void CCmdUI::Enable(BOOL bOn) {
    m_enabled = bOn != FALSE;
    m_bEnableChanged = TRUE;
}

void CCmdUI::SetCheck(int nCheck) {
    m_check = nCheck;
    m_checkSet = true;
}

void CCmdUI::SetRadio(BOOL bOn) { SetCheck(bOn ? 1 : 0); }

void CCmdUI::SetText(const char* lpszText) {
    m_text = lpszText;
    m_textSet = true;
}

BOOL CCmdUI::DoUpdate(CCmdTarget* pTarget, BOOL bDisableIfNoHndler) {
    m_bEnableChanged = FALSE;
    BOOL handled = pTarget->OnCmdMsg(m_nID, static_cast<int>(CN_UPDATE_COMMAND_UI), this, nullptr);
    if (bDisableIfNoHndler && !m_bEnableChanged) {
        AFX_CMDHANDLERINFO info;
        info.pTarget = nullptr;
        BOOL hasHandler = pTarget->OnCmdMsg(m_nID, CN_COMMAND, this, &info);
        Enable(hasHandler);
    }
    return handled;
}
