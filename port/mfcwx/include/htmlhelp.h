#pragma once
#include "mfcwx/win32.h"

#ifndef HH_SET_WIN_TYPE
#define HH_SET_WIN_TYPE 0x0004
#define HH_GET_WIN_TYPE 0x0005
#define HH_GET_WIN_HANDLE 0x0006
#define HH_SYNC 0x0009
#define HH_GET_LAST_ERROR 0x0014
#endif

#define HH_MAX_TABS 19

#define HHWIN_PROP_TAB_AUTOHIDESHOW (1 << 0)
#define HHWIN_PROP_ONTOP (1 << 1)
#define HHWIN_PROP_NOTITLEBAR (1 << 2)
#define HHWIN_PROP_NODEF_STYLES (1 << 3)
#define HHWIN_PROP_NODEF_EXSTYLES (1 << 4)
#define HHWIN_PROP_TRI_PANE (1 << 5)
#define HHWIN_PROP_NOTB_TEXT (1 << 6)
#define HHWIN_PROP_POST_QUIT (1 << 7)
#define HHWIN_PROP_AUTO_SYNC (1 << 8)
#define HHWIN_PROP_TRACKING (1 << 9)
#define HHWIN_PROP_TAB_SEARCH (1 << 10)
#define HHWIN_PROP_NO_TOOLBAR (1 << 15)

#define HHWIN_PARAM_PROPERTIES (1 << 1)
#define HHWIN_PARAM_STYLES (1 << 2)
#define HHWIN_PARAM_EXSTYLES (1 << 3)
#define HHWIN_PARAM_RECT (1 << 4)
#define HHWIN_PARAM_NAV_WIDTH (1 << 5)
#define HHWIN_PARAM_SHOWSTATE (1 << 6)

typedef DWORD HH_INFOTYPE, *PHH_INFOTYPE;

typedef struct tagHH_AKLINK {
    int cbStruct;
    BOOL fReserved;
    const char* pszKeywords;
    const char* pszUrl;
    const char* pszMsgText;
    const char* pszMsgTitle;
    const char* pszWindow;
    BOOL fIndexOnFail;
} HH_AKLINK, *PHH_AKLINK;

typedef struct tagHH_WINTYPE {
    int cbStruct;
    BOOL fUniCodeStrings;
    const char* pszType;
    DWORD fsValidMembers;
    DWORD fsWinProperties;
    const char* pszCaption;
    DWORD dwStyles;
    DWORD dwExStyles;
    RECT rcWindowPos;
    int nShowState;
    HWND hwndHelp;
    HWND hwndCaller;
    HH_INFOTYPE* paInfoTypes;
    HWND hwndToolBar;
    HWND hwndNavigation;
    HWND hwndHTML;
    int iNavWidth;
    RECT rcHTML;
    const char* pszToc;
    const char* pszIndex;
    const char* pszFile;
    const char* pszHome;
    DWORD fsToolBarFlags;
    BOOL fNotExpanded;
    int curNavType;
    int tabpos;
    int idNotify;
    BYTE tabOrder[HH_MAX_TABS + 1];
    int cHistory;
    const char* pszJump1;
    const char* pszJump2;
    const char* pszUrlJump1;
    const char* pszUrlJump2;
    RECT rcMinSize;
    int cbInfoTypes;
    const char* pszCustomTabs;
} HH_WINTYPE, *PHH_WINTYPE;

#ifdef __cplusplus
// Reads <data dir>/hlp_de or hlp_en (UI language) with the index from build_help_index.py.
HWND HtmlHelp(HWND hwndCaller, const char* pszFile, UINT uCommand, DWORD_PTR dwData);
HWND HtmlHelpA(HWND hwndCaller, const char* pszFile, UINT uCommand, DWORD_PTR dwData);
#endif
