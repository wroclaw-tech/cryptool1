#pragma once
#include "mfcwx/win32.h"
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
#ifdef __cplusplus
HWND HtmlHelp(HWND hwndCaller, const char* pszFile, UINT uCommand, DWORD_PTR dwData);
HWND HtmlHelpA(HWND hwndCaller, const char* pszFile, UINT uCommand, DWORD_PTR dwData);
#endif
