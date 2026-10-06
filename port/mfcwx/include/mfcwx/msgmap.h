#pragma once

#include <type_traits>

// MFC message map machinery. Handlers are stored as CCmdTarget member pointers together with a
// signature code; CCmdTarget/CWnd dispatch converts them back to the exact handler type.

class CCmdTarget;
class CWnd;
class CDC;
class CCmdUI;
class CScrollBar;
class CMenu;

typedef void (CCmdTarget::*AFX_PMSG)(void);
typedef void (CWnd::*AFX_PMSGW)(void);

enum AfxSig {
    AfxSig_end = 0,
    AfxSigCmd_v,          // void ()
    AfxSigCmd_b,          // BOOL ()
    AfxSigCmd_RANGE,      // void (UINT)
    AfxSigCmd_EX,         // BOOL (UINT)
    AfxSigCmdUI,          // void (CCmdUI*)
    AfxSigCmdUI_RANGE,    // void (CCmdUI*, UINT)
    AfxSigNotify_v,       // void (NMHDR*, LRESULT*)
    AfxSigNotify_b,       // BOOL (NMHDR*, LRESULT*)
    AfxSigNotify_RANGE,   // void (UINT, NMHDR*, LRESULT*)
    AfxSigNotify_EX,      // BOOL (UINT, NMHDR*, LRESULT*)
    AfxSig_vv,            // void ()
    AfxSig_bv,            // BOOL ()
    AfxSig_lwl,           // LRESULT (WPARAM, LPARAM)
    AfxSig_is,            // int (LPCREATESTRUCT)
    AfxSig_bD,            // BOOL (CDC*)
    AfxSig_vwii,          // void (UINT, int, int)            WM_SIZE
    AfxSig_vwwx,          // void (UINT, UINT, UINT)          WM_KEYDOWN/WM_CHAR...
    AfxSig_vwp,           // void (UINT, CPoint)              mouse
    AfxSig_bwsp,          // BOOL (UINT, short, CPoint)       WM_MOUSEWHEEL
    AfxSig_vwwW,          // void (UINT, UINT, CScrollBar*)   WM_HSCROLL/VSCROLL
    AfxSig_hDWw,          // HBRUSH (CDC*, CWnd*, UINT)       WM_CTLCOLOR
    AfxSig_hDw,           // HBRUSH (CDC*, UINT)              WM_CTLCOLOR reflect
    AfxSig_vOWNER,        // void (int, LPDRAWITEMSTRUCT)     WM_DRAWITEM
    AfxSig_vMEASURE,      // void (int, LPMEASUREITEMSTRUCT)  WM_MEASUREITEM
    AfxSig_vW,            // void (CWnd*)                     WM_SETFOCUS/KILLFOCUS
    AfxSig_vWp,           // void (CWnd*, CPoint)             WM_CONTEXTMENU
    AfxSig_bWww,          // BOOL (CWnd*, UINT, UINT)         WM_SETCURSOR
    AfxSig_vbw,           // void (BOOL, UINT)                WM_SHOWWINDOW
    AfxSig_vwl,           // void (UINT, LPARAM)              WM_SYSCOMMAND
    AfxSig_vw_ptr,        // void (UINT_PTR)                  WM_TIMER
    AfxSig_vw_uint,       // void (UINT)                      WM_TIMER, pre-VS2005 handlers
    AfxSig_hv,            // HCURSOR ()                       WM_QUERYDRAGICON
    AfxSig_bHELPINFO,     // BOOL (HELPINFO*)                 WM_HELP
    AfxSig_vPOS,          // void (MINMAXINFO*)               WM_GETMINMAXINFO
    AfxSig_vwWb,          // void (UINT, CWnd*, BOOL)         WM_ACTIVATE
    AfxSig_wv,            // UINT ()                          WM_GETDLGCODE
    AfxSig_vHDROP,        // void (HDROP)                     WM_DROPFILES
    AfxSig_vMwb,          // void (CMenu*, UINT, BOOL)        WM_INITMENUPOPUP
    AfxSig_vii,           // void (int, int)                  WM_MOVE
    AfxSig_vwW,           // void (UINT, CWnd*)
    AfxSig_vbWW,          // void (BOOL, CWnd*, CWnd*)        WM_MDIACTIVATE
    AfxSig_bb,            // BOOL (BOOL)                      WM_NCACTIVATE
    AfxSig_vb,            // void (BOOL)                      WM_ENABLE
    AfxSig_vwwh,          // void (UINT, UINT, HMENU)         WM_MENUSELECT
    AfxSig_vRECT,         // void (UINT, LPRECT)              WM_SIZING/MOVING
    AfxSig_vv_reflect,    // void ()                          reflected control notifications
    AfxSig_bv_reflect,    // BOOL ()                          ON_CONTROL_REFLECT_EX
    AfxSig_vNMHDRpl,      // void (NMHDR*, LRESULT*)          ON_NOTIFY_REFLECT
    AfxSig_bNMHDRpl,      // BOOL (NMHDR*, LRESULT*)          ON_NOTIFY_REFLECT_EX
    AfxSig_vv_event,      // OLE event sink, unused
};

#define CN_COMMAND 0
#define CN_UPDATE_COMMAND_UI ((UINT)(-1))
#define CN_EVENT ((UINT)(-2))
#define CN_OLEEVENT ((UINT)(-3))
#define WM_REFLECT_BASE 0xBC00

struct AFX_MSGMAP_ENTRY {
    UINT nMessage;
    UINT nCode;
    UINT nID;
    UINT nLastID;
    UINT_PTR nSig;
    AFX_PMSG pfn;
};

struct AFX_MSGMAP {
    const AFX_MSGMAP* (*pfnGetBaseMap)();
    const AFX_MSGMAP_ENTRY* lpEntries;
};

struct AFX_CMDHANDLERINFO {
    CCmdTarget* pTarget;
    AFX_PMSG pmf;
};

// Old code declares OnTimer(UINT), newer code OnTimer(UINT_PTR); both are accepted.
template <class C>
constexpr UINT_PTR AfxSigTimer(void (C::*)(UINT_PTR)) { return AfxSig_vw_ptr; }
template <class C, class U = UINT, class = typename std::enable_if<!std::is_same<U, UINT_PTR>::value>::type>
constexpr UINT_PTR AfxSigTimer(void (C::*)(UINT)) { return AfxSig_vw_uint; }
template <class C>
AFX_PMSG AfxPmsgTimer(void (C::*f)(UINT_PTR)) { return (AFX_PMSG)(AFX_PMSGW)static_cast<void (CWnd::*)(UINT_PTR)>(f); }
template <class C, class U = UINT, class = typename std::enable_if<!std::is_same<U, UINT_PTR>::value>::type>
AFX_PMSG AfxPmsgTimer(void (C::*f)(UINT)) { return (AFX_PMSG)(AFX_PMSGW)(void (CWnd::*)(UINT))static_cast<void (CWnd::*)(UINT)>(f); }

#define DECLARE_MESSAGE_MAP() \
protected: \
    static const AFX_MSGMAP* GetThisMessageMap(); \
    const AFX_MSGMAP* GetMessageMap() const override;

#define BEGIN_MESSAGE_MAP(theClass, baseClass) \
    const AFX_MSGMAP* theClass::GetMessageMap() const { return GetThisMessageMap(); } \
    const AFX_MSGMAP* theClass::GetThisMessageMap() { \
        typedef theClass ThisClass; \
        typedef baseClass TheBaseClass; \
        (void)sizeof(ThisClass*); \
        static const AFX_MSGMAP_ENTRY _messageEntries[] = {

#define BEGIN_TEMPLATE_MESSAGE_MAP(theClass, type_name, baseClass) \
    template <typename type_name> \
    const AFX_MSGMAP* theClass<type_name>::GetMessageMap() const { return GetThisMessageMap(); } \
    template <typename type_name> \
    const AFX_MSGMAP* theClass<type_name>::GetThisMessageMap() { \
        typedef theClass<type_name> ThisClass; \
        typedef baseClass TheBaseClass; \
        static const AFX_MSGMAP_ENTRY _messageEntries[] = {

#define END_MESSAGE_MAP() \
    {0, 0, 0, 0, AfxSig_end, (AFX_PMSG)0}}; \
    static const AFX_MSGMAP messageMap = {&TheBaseClass::GetThisMessageMap, &_messageEntries[0]}; \
    return &messageMap; \
    }

#define AFX_MFN(sig, fn) ((AFX_PMSG)(static_cast<sig>(fn)))
#define AFX_MFNW(sig, fn) ((AFX_PMSG)(AFX_PMSGW)(static_cast<sig>(fn)))

// command and notification handlers
#define ON_COMMAND(id, memberFxn) \
    {WM_COMMAND, CN_COMMAND, (UINT)(WORD)(id), (UINT)(WORD)(id), AfxSigCmd_v, AFX_MFN(void (CCmdTarget::*)(void), memberFxn)},
#define ON_COMMAND_EX(id, memberFxn) \
    {WM_COMMAND, CN_COMMAND, (UINT)(WORD)(id), (UINT)(WORD)(id), AfxSigCmd_EX, AFX_MFN(BOOL (CCmdTarget::*)(UINT), memberFxn)},
#define ON_COMMAND_RANGE(id, idLast, memberFxn) \
    {WM_COMMAND, CN_COMMAND, (UINT)(WORD)(id), (UINT)(WORD)(idLast), AfxSigCmd_RANGE, AFX_MFN(void (CCmdTarget::*)(UINT), memberFxn)},
#define ON_COMMAND_EX_RANGE(id, idLast, memberFxn) \
    {WM_COMMAND, CN_COMMAND, (UINT)(WORD)(id), (UINT)(WORD)(idLast), AfxSigCmd_EX, AFX_MFN(BOOL (CCmdTarget::*)(UINT), memberFxn)},
#define ON_UPDATE_COMMAND_UI(id, memberFxn) \
    {WM_COMMAND, CN_UPDATE_COMMAND_UI, (UINT)(WORD)(id), (UINT)(WORD)(id), AfxSigCmdUI, AFX_MFN(void (CCmdTarget::*)(CCmdUI*), memberFxn)},
#define ON_UPDATE_COMMAND_UI_RANGE(id, idLast, memberFxn) \
    {WM_COMMAND, CN_UPDATE_COMMAND_UI, (UINT)(WORD)(id), (UINT)(WORD)(idLast), AfxSigCmdUI, AFX_MFN(void (CCmdTarget::*)(CCmdUI*), memberFxn)},
#define ON_CONTROL(wNotifyCode, id, memberFxn) \
    {WM_COMMAND, (UINT)(WORD)(wNotifyCode), (UINT)(WORD)(id), (UINT)(WORD)(id), AfxSigCmd_v, AFX_MFN(void (CCmdTarget::*)(void), memberFxn)},
#define ON_CONTROL_RANGE(wNotifyCode, id, idLast, memberFxn) \
    {WM_COMMAND, (UINT)(WORD)(wNotifyCode), (UINT)(WORD)(id), (UINT)(WORD)(idLast), AfxSigCmd_RANGE, AFX_MFN(void (CCmdTarget::*)(UINT), memberFxn)},
#define ON_NOTIFY(wNotifyCode, id, memberFxn) \
    {WM_NOTIFY, (UINT)(wNotifyCode), (UINT)(WORD)(id), (UINT)(WORD)(id), AfxSigNotify_v, AFX_MFN(void (CCmdTarget::*)(NMHDR*, LRESULT*), memberFxn)},
#define ON_NOTIFY_RANGE(wNotifyCode, id, idLast, memberFxn) \
    {WM_NOTIFY, (UINT)(wNotifyCode), (UINT)(WORD)(id), (UINT)(WORD)(idLast), AfxSigNotify_RANGE, AFX_MFN(void (CCmdTarget::*)(UINT, NMHDR*, LRESULT*), memberFxn)},
#define ON_NOTIFY_EX(wNotifyCode, id, memberFxn) \
    {WM_NOTIFY, (UINT)(wNotifyCode), (UINT)(WORD)(id), (UINT)(WORD)(id), AfxSigNotify_EX, AFX_MFN(BOOL (CCmdTarget::*)(UINT, NMHDR*, LRESULT*), memberFxn)},
#define ON_NOTIFY_EX_RANGE(wNotifyCode, id, idLast, memberFxn) \
    {WM_NOTIFY, (UINT)(wNotifyCode), (UINT)(WORD)(id), (UINT)(WORD)(idLast), AfxSigNotify_EX, AFX_MFN(BOOL (CCmdTarget::*)(UINT, NMHDR*, LRESULT*), memberFxn)},

#define ON_BN_CLICKED(id, memberFxn) ON_CONTROL(BN_CLICKED, id, memberFxn)
#define ON_BN_DOUBLECLICKED(id, memberFxn) ON_CONTROL(BN_DOUBLECLICKED, id, memberFxn)
#define ON_BN_SETFOCUS(id, memberFxn) ON_CONTROL(BN_SETFOCUS, id, memberFxn)
#define ON_BN_KILLFOCUS(id, memberFxn) ON_CONTROL(BN_KILLFOCUS, id, memberFxn)
#define ON_BN_HILITE(id, memberFxn) ON_CONTROL(BN_HILITE, id, memberFxn)
#define ON_BN_UNHILITE(id, memberFxn) ON_CONTROL(BN_UNHILITE, id, memberFxn)
#define ON_BN_PUSHED(id, memberFxn) ON_CONTROL(BN_PUSHED, id, memberFxn)
#define ON_BN_UNPUSHED(id, memberFxn) ON_CONTROL(BN_UNPUSHED, id, memberFxn)
#define ON_EN_SETFOCUS(id, memberFxn) ON_CONTROL(EN_SETFOCUS, id, memberFxn)
#define ON_EN_KILLFOCUS(id, memberFxn) ON_CONTROL(EN_KILLFOCUS, id, memberFxn)
#define ON_EN_CHANGE(id, memberFxn) ON_CONTROL(EN_CHANGE, id, memberFxn)
#define ON_EN_UPDATE(id, memberFxn) ON_CONTROL(EN_UPDATE, id, memberFxn)
#define ON_EN_ERRSPACE(id, memberFxn) ON_CONTROL(EN_ERRSPACE, id, memberFxn)
#define ON_EN_MAXTEXT(id, memberFxn) ON_CONTROL(EN_MAXTEXT, id, memberFxn)
#define ON_EN_HSCROLL(id, memberFxn) ON_CONTROL(EN_HSCROLL, id, memberFxn)
#define ON_EN_VSCROLL(id, memberFxn) ON_CONTROL(EN_VSCROLL, id, memberFxn)
#define ON_STN_CLICKED(id, memberFxn) ON_CONTROL(STN_CLICKED, id, memberFxn)
#define ON_STN_DBLCLK(id, memberFxn) ON_CONTROL(STN_DBLCLK, id, memberFxn)
#define ON_STN_ENABLE(id, memberFxn) ON_CONTROL(STN_ENABLE, id, memberFxn)
#define ON_STN_DISABLE(id, memberFxn) ON_CONTROL(STN_DISABLE, id, memberFxn)
#define ON_CBN_ERRSPACE(id, memberFxn) ON_CONTROL(CBN_ERRSPACE, id, memberFxn)
#define ON_CBN_SELCHANGE(id, memberFxn) ON_CONTROL(CBN_SELCHANGE, id, memberFxn)
#define ON_CBN_DBLCLK(id, memberFxn) ON_CONTROL(CBN_DBLCLK, id, memberFxn)
#define ON_CBN_SETFOCUS(id, memberFxn) ON_CONTROL(CBN_SETFOCUS, id, memberFxn)
#define ON_CBN_KILLFOCUS(id, memberFxn) ON_CONTROL(CBN_KILLFOCUS, id, memberFxn)
#define ON_CBN_EDITCHANGE(id, memberFxn) ON_CONTROL(CBN_EDITCHANGE, id, memberFxn)
#define ON_CBN_EDITUPDATE(id, memberFxn) ON_CONTROL(CBN_EDITUPDATE, id, memberFxn)
#define ON_CBN_DROPDOWN(id, memberFxn) ON_CONTROL(CBN_DROPDOWN, id, memberFxn)
#define ON_CBN_CLOSEUP(id, memberFxn) ON_CONTROL(CBN_CLOSEUP, id, memberFxn)
#define ON_CBN_SELENDOK(id, memberFxn) ON_CONTROL(CBN_SELENDOK, id, memberFxn)
#define ON_CBN_SELENDCANCEL(id, memberFxn) ON_CONTROL(CBN_SELENDCANCEL, id, memberFxn)
#define ON_LBN_ERRSPACE(id, memberFxn) ON_CONTROL(LBN_ERRSPACE, id, memberFxn)
#define ON_LBN_SELCHANGE(id, memberFxn) ON_CONTROL(LBN_SELCHANGE, id, memberFxn)
#define ON_LBN_DBLCLK(id, memberFxn) ON_CONTROL(LBN_DBLCLK, id, memberFxn)
#define ON_LBN_SELCANCEL(id, memberFxn) ON_CONTROL(LBN_SELCANCEL, id, memberFxn)
#define ON_LBN_SETFOCUS(id, memberFxn) ON_CONTROL(LBN_SETFOCUS, id, memberFxn)
#define ON_LBN_KILLFOCUS(id, memberFxn) ON_CONTROL(LBN_KILLFOCUS, id, memberFxn)

// reflected notifications (handled by the control itself)
#define ON_CONTROL_REFLECT(wNotifyCode, memberFxn) \
    {WM_COMMAND + WM_REFLECT_BASE, (UINT)(WORD)(wNotifyCode), 0, 0, AfxSig_vv_reflect, AFX_MFN(void (CCmdTarget::*)(void), memberFxn)},
#define ON_CONTROL_REFLECT_EX(wNotifyCode, memberFxn) \
    {WM_COMMAND + WM_REFLECT_BASE, (UINT)(WORD)(wNotifyCode), 0, 0, AfxSig_bv_reflect, AFX_MFN(BOOL (CCmdTarget::*)(void), memberFxn)},
#define ON_NOTIFY_REFLECT(wNotifyCode, memberFxn) \
    {WM_NOTIFY + WM_REFLECT_BASE, (UINT)(wNotifyCode), 0, 0, AfxSig_vNMHDRpl, AFX_MFN(void (CCmdTarget::*)(NMHDR*, LRESULT*), memberFxn)},
#define ON_NOTIFY_REFLECT_EX(wNotifyCode, memberFxn) \
    {WM_NOTIFY + WM_REFLECT_BASE, (UINT)(wNotifyCode), 0, 0, AfxSig_bNMHDRpl, AFX_MFN(BOOL (CCmdTarget::*)(NMHDR*, LRESULT*), memberFxn)},
#define ON_EN_CHANGE_REFLECT(memberFxn) ON_CONTROL_REFLECT(EN_CHANGE, memberFxn)
#define ON_WM_CTLCOLOR_REFLECT() \
    {WM_CTLCOLOR + WM_REFLECT_BASE, 0, 0, 0, AfxSig_hDw, AFX_MFNW(HBRUSH (CWnd::*)(CDC*, UINT), &ThisClass::CtlColor)},
#define ON_WM_DRAWITEM_REFLECT() \
    {WM_DRAWITEM + WM_REFLECT_BASE, 0, 0, 0, AfxSig_vOWNER, AFX_MFNW(void (CWnd::*)(LPDRAWITEMSTRUCT), &ThisClass::DrawItem)},
#define ON_WM_HSCROLL_REFLECT() \
    {WM_HSCROLL + WM_REFLECT_BASE, 0, 0, 0, AfxSig_vwwW, AFX_MFNW(void (CWnd::*)(UINT, UINT), &ThisClass::HScroll)},
#define ON_WM_VSCROLL_REFLECT() \
    {WM_VSCROLL + WM_REFLECT_BASE, 0, 0, 0, AfxSig_vwwW, AFX_MFNW(void (CWnd::*)(UINT, UINT), &ThisClass::VScroll)},

// generic messages
#define ON_MESSAGE(message, memberFxn) \
    {(UINT)(message), 0, 0, 0, AfxSig_lwl, AFX_MFNW(LRESULT (CWnd::*)(WPARAM, LPARAM), memberFxn)},
#define ON_REGISTERED_MESSAGE(nMessageVariable, memberFxn) \
    {0xC000, 0, 0, 0, (UINT_PTR)(UINT*)(&nMessageVariable), AFX_MFNW(LRESULT (CWnd::*)(WPARAM, LPARAM), memberFxn)},
#define ON_THREAD_MESSAGE(message, memberFxn) \
    {(UINT)(message), 0, 0, 0, AfxSig_lwl, (AFX_PMSG)(static_cast<void (CCmdTarget::*)(WPARAM, LPARAM)>(memberFxn))},
#define ON_EVENT(theClass, id, dispid, pfnHandler, vtsParams)

// window messages with fixed handler names
#define ON_WM_CREATE() {WM_CREATE, 0, 0, 0, AfxSig_is, AFX_MFNW(int (CWnd::*)(LPCREATESTRUCT), &ThisClass::OnCreate)},
#define ON_WM_DESTROY() {WM_DESTROY, 0, 0, 0, AfxSig_vv, AFX_MFNW(void (CWnd::*)(void), &ThisClass::OnDestroy)},
#define ON_WM_NCDESTROY() {WM_NCDESTROY, 0, 0, 0, AfxSig_vv, AFX_MFNW(void (CWnd::*)(void), &ThisClass::OnNcDestroy)},
#define ON_WM_CLOSE() {WM_CLOSE, 0, 0, 0, AfxSig_vv, AFX_MFNW(void (CWnd::*)(void), &ThisClass::OnClose)},
#define ON_WM_PAINT() {WM_PAINT, 0, 0, 0, AfxSig_vv, AFX_MFNW(void (CWnd::*)(void), &ThisClass::OnPaint)},
#define ON_WM_ERASEBKGND() {WM_ERASEBKGND, 0, 0, 0, AfxSig_bD, AFX_MFNW(BOOL (CWnd::*)(CDC*), &ThisClass::OnEraseBkgnd)},
#define ON_WM_SIZE() {WM_SIZE, 0, 0, 0, AfxSig_vwii, AFX_MFNW(void (CWnd::*)(UINT, int, int), &ThisClass::OnSize)},
#define ON_WM_MOVE() {WM_MOVE, 0, 0, 0, AfxSig_vii, AFX_MFNW(void (CWnd::*)(int, int), &ThisClass::OnMove)},
#define ON_WM_TIMER() {WM_TIMER, 0, 0, 0, AfxSigTimer(&ThisClass::OnTimer), AfxPmsgTimer(&ThisClass::OnTimer)},
#define ON_WM_KEYDOWN() {WM_KEYDOWN, 0, 0, 0, AfxSig_vwwx, AFX_MFNW(void (CWnd::*)(UINT, UINT, UINT), &ThisClass::OnKeyDown)},
#define ON_WM_KEYUP() {WM_KEYUP, 0, 0, 0, AfxSig_vwwx, AFX_MFNW(void (CWnd::*)(UINT, UINT, UINT), &ThisClass::OnKeyUp)},
#define ON_WM_CHAR() {WM_CHAR, 0, 0, 0, AfxSig_vwwx, AFX_MFNW(void (CWnd::*)(UINT, UINT, UINT), &ThisClass::OnChar)},
#define ON_WM_SYSKEYDOWN() {WM_SYSKEYDOWN, 0, 0, 0, AfxSig_vwwx, AFX_MFNW(void (CWnd::*)(UINT, UINT, UINT), &ThisClass::OnSysKeyDown)},
#define ON_WM_SYSKEYUP() {WM_SYSKEYUP, 0, 0, 0, AfxSig_vwwx, AFX_MFNW(void (CWnd::*)(UINT, UINT, UINT), &ThisClass::OnSysKeyUp)},
#define ON_WM_SYSCHAR() {WM_SYSCHAR, 0, 0, 0, AfxSig_vwwx, AFX_MFNW(void (CWnd::*)(UINT, UINT, UINT), &ThisClass::OnSysChar)},
#define ON_WM_MOUSEMOVE() {WM_MOUSEMOVE, 0, 0, 0, AfxSig_vwp, AFX_MFNW(void (CWnd::*)(UINT, CPoint), &ThisClass::OnMouseMove)},
#define ON_WM_LBUTTONDOWN() {WM_LBUTTONDOWN, 0, 0, 0, AfxSig_vwp, AFX_MFNW(void (CWnd::*)(UINT, CPoint), &ThisClass::OnLButtonDown)},
#define ON_WM_LBUTTONUP() {WM_LBUTTONUP, 0, 0, 0, AfxSig_vwp, AFX_MFNW(void (CWnd::*)(UINT, CPoint), &ThisClass::OnLButtonUp)},
#define ON_WM_LBUTTONDBLCLK() {WM_LBUTTONDBLCLK, 0, 0, 0, AfxSig_vwp, AFX_MFNW(void (CWnd::*)(UINT, CPoint), &ThisClass::OnLButtonDblClk)},
#define ON_WM_RBUTTONDOWN() {WM_RBUTTONDOWN, 0, 0, 0, AfxSig_vwp, AFX_MFNW(void (CWnd::*)(UINT, CPoint), &ThisClass::OnRButtonDown)},
#define ON_WM_RBUTTONUP() {WM_RBUTTONUP, 0, 0, 0, AfxSig_vwp, AFX_MFNW(void (CWnd::*)(UINT, CPoint), &ThisClass::OnRButtonUp)},
#define ON_WM_RBUTTONDBLCLK() {WM_RBUTTONDBLCLK, 0, 0, 0, AfxSig_vwp, AFX_MFNW(void (CWnd::*)(UINT, CPoint), &ThisClass::OnRButtonDblClk)},
#define ON_WM_MBUTTONDOWN() {WM_MBUTTONDOWN, 0, 0, 0, AfxSig_vwp, AFX_MFNW(void (CWnd::*)(UINT, CPoint), &ThisClass::OnMButtonDown)},
#define ON_WM_MBUTTONUP() {WM_MBUTTONUP, 0, 0, 0, AfxSig_vwp, AFX_MFNW(void (CWnd::*)(UINT, CPoint), &ThisClass::OnMButtonUp)},
#define ON_WM_MOUSEWHEEL() {WM_MOUSEWHEEL, 0, 0, 0, AfxSig_bwsp, AFX_MFNW(BOOL (CWnd::*)(UINT, short, CPoint), &ThisClass::OnMouseWheel)},
#define ON_WM_HSCROLL() {WM_HSCROLL, 0, 0, 0, AfxSig_vwwW, AFX_MFNW(void (CWnd::*)(UINT, UINT, CScrollBar*), &ThisClass::OnHScroll)},
#define ON_WM_VSCROLL() {WM_VSCROLL, 0, 0, 0, AfxSig_vwwW, AFX_MFNW(void (CWnd::*)(UINT, UINT, CScrollBar*), &ThisClass::OnVScroll)},
#define ON_WM_CTLCOLOR() {WM_CTLCOLOR, 0, 0, 0, AfxSig_hDWw, AFX_MFNW(HBRUSH (CWnd::*)(CDC*, CWnd*, UINT), &ThisClass::OnCtlColor)},
#define ON_WM_DRAWITEM() {WM_DRAWITEM, 0, 0, 0, AfxSig_vOWNER, AFX_MFNW(void (CWnd::*)(int, LPDRAWITEMSTRUCT), &ThisClass::OnDrawItem)},
#define ON_WM_MEASUREITEM() {WM_MEASUREITEM, 0, 0, 0, AfxSig_vMEASURE, AFX_MFNW(void (CWnd::*)(int, LPMEASUREITEMSTRUCT), &ThisClass::OnMeasureItem)},
#define ON_WM_SETFOCUS() {WM_SETFOCUS, 0, 0, 0, AfxSig_vW, AFX_MFNW(void (CWnd::*)(CWnd*), &ThisClass::OnSetFocus)},
#define ON_WM_KILLFOCUS() {WM_KILLFOCUS, 0, 0, 0, AfxSig_vW, AFX_MFNW(void (CWnd::*)(CWnd*), &ThisClass::OnKillFocus)},
#define ON_WM_CONTEXTMENU() {WM_CONTEXTMENU, 0, 0, 0, AfxSig_vWp, AFX_MFNW(void (CWnd::*)(CWnd*, CPoint), &ThisClass::OnContextMenu)},
#define ON_WM_SETCURSOR() {WM_SETCURSOR, 0, 0, 0, AfxSig_bWww, AFX_MFNW(BOOL (CWnd::*)(CWnd*, UINT, UINT), &ThisClass::OnSetCursor)},
#define ON_WM_SHOWWINDOW() {WM_SHOWWINDOW, 0, 0, 0, AfxSig_vbw, AFX_MFNW(void (CWnd::*)(BOOL, UINT), &ThisClass::OnShowWindow)},
#define ON_WM_SYSCOMMAND() {WM_SYSCOMMAND, 0, 0, 0, AfxSig_vwl, AFX_MFNW(void (CWnd::*)(UINT, LPARAM), &ThisClass::OnSysCommand)},
#define ON_WM_QUERYDRAGICON() {WM_QUERYDRAGICON, 0, 0, 0, AfxSig_hv, AFX_MFNW(HCURSOR (CWnd::*)(void), &ThisClass::OnQueryDragIcon)},
#define ON_WM_HELPINFO() {WM_HELP, 0, 0, 0, AfxSig_bHELPINFO, AFX_MFNW(BOOL (CWnd::*)(HELPINFO*), &ThisClass::OnHelpInfo)},
#define ON_WM_GETMINMAXINFO() {WM_GETMINMAXINFO, 0, 0, 0, AfxSig_vPOS, AFX_MFNW(void (CWnd::*)(MINMAXINFO*), &ThisClass::OnGetMinMaxInfo)},
#define ON_WM_ACTIVATE() {WM_ACTIVATE, 0, 0, 0, AfxSig_vwWb, AFX_MFNW(void (CWnd::*)(UINT, CWnd*, BOOL), &ThisClass::OnActivate)},
#define ON_WM_GETDLGCODE() {WM_GETDLGCODE, 0, 0, 0, AfxSig_wv, AFX_MFNW(UINT (CWnd::*)(void), &ThisClass::OnGetDlgCode)},
#define ON_WM_DROPFILES() {WM_DROPFILES, 0, 0, 0, AfxSig_vHDROP, AFX_MFNW(void (CWnd::*)(HDROP), &ThisClass::OnDropFiles)},
#define ON_WM_INITMENUPOPUP() {WM_INITMENUPOPUP, 0, 0, 0, AfxSig_vMwb, AFX_MFNW(void (CWnd::*)(CMenu*, UINT, BOOL), &ThisClass::OnInitMenuPopup)},
#define ON_WM_MDIACTIVATE() {WM_MDIACTIVATE, 0, 0, 0, AfxSig_vbWW, AFX_MFNW(void (CWnd::*)(BOOL, CWnd*, CWnd*), &ThisClass::OnMDIActivate)},
#define ON_WM_ENABLE() {WM_ENABLE, 0, 0, 0, AfxSig_vb, AFX_MFNW(void (CWnd::*)(BOOL), &ThisClass::OnEnable)},
#define ON_WM_NCACTIVATE() {WM_NCACTIVATE, 0, 0, 0, AfxSig_bb, AFX_MFNW(BOOL (CWnd::*)(BOOL), &ThisClass::OnNcActivate)},
#define ON_WM_MENUSELECT() {WM_MENUSELECT, 0, 0, 0, AfxSig_vwwh, AFX_MFNW(void (CWnd::*)(UINT, UINT, HMENU), &ThisClass::OnMenuSelect)},
#define ON_WM_SIZING() {WM_SIZING, 0, 0, 0, AfxSig_vRECT, AFX_MFNW(void (CWnd::*)(UINT, LPRECT), &ThisClass::OnSizing)},
#define ON_WM_NCLBUTTONDOWN() {WM_NCLBUTTONDOWN, 0, 0, 0, AfxSig_vwp, AFX_MFNW(void (CWnd::*)(UINT, CPoint), &ThisClass::OnNcLButtonDown)},
#define ON_WM_NCHITTEST()
#define ON_WM_NCPAINT()
#define ON_WM_NCCALCSIZE()
#define ON_WM_WINDOWPOSCHANGED()
#define ON_WM_WINDOWPOSCHANGING()
#define ON_WM_SETTINGCHANGE()
#define ON_WM_SYSCOLORCHANGE()
#define ON_WM_PALETTECHANGED()
#define ON_WM_QUERYNEWPALETTE()
#define ON_WM_CAPTURECHANGED()
#define ON_WM_MOUSEACTIVATE()
#define ON_WM_ACTIVATEAPP()
#define ON_WM_ENTERIDLE()
