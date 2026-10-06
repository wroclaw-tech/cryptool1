#pragma once
#include "mfcwx/win32.h"
#ifdef __cplusplus
HWND HtmlHelp(HWND hwndCaller, const char* pszFile, UINT uCommand, DWORD_PTR dwData);
HWND HtmlHelpA(HWND hwndCaller, const char* pszFile, UINT uCommand, DWORD_PTR dwData);
#endif
