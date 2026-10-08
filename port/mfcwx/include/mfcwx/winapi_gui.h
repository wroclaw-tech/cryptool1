#pragma once

// Win32 user/gdi functions operating on HWND/HDC handles (HWND is a wxWindow*, HDC an opaque handle to an internal DC state).

LRESULT SendMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
LRESULT SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL PostMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL PostMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL SendNotifyMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL PostThreadMessage(DWORD idThread, UINT Msg, WPARAM wParam, LPARAM lParam);
void PostQuitMessage(int nExitCode);
UINT RegisterWindowMessage(const char* lpString);
UINT RegisterWindowMessageA(const char* lpString);
BOOL PeekMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
BOOL GetMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
BOOL TranslateMessage(const MSG* lpMsg);
LRESULT DispatchMessage(const MSG* lpMsg);
BOOL WaitMessage();
DWORD MsgWaitForMultipleObjects(DWORD nCount, const HANDLE* pHandles, BOOL fWaitAll, DWORD dwMilliseconds,
                                DWORD dwWakeMask);
LRESULT DefWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
LRESULT CallWindowProc(WNDPROC lpPrevWndFunc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

BOOL IsWindow(HWND hWnd);
BOOL IsWindowVisible(HWND hWnd);
BOOL IsWindowEnabled(HWND hWnd);
BOOL EnableWindow(HWND hWnd, BOOL bEnable);
BOOL ShowWindow(HWND hWnd, int nCmdShow);
BOOL IsIconic(HWND hWnd);
BOOL IsZoomed(HWND hWnd);
BOOL IsChild(HWND hWndParent, HWND hWnd);
HWND GetParent(HWND hWnd);
HWND SetParent(HWND hWndChild, HWND hWndNewParent);
HWND GetWindow(HWND hWnd, UINT uCmd);
HWND GetTopWindow(HWND hWnd);
HWND GetDesktopWindow();
HWND GetActiveWindow();
HWND SetActiveWindow(HWND hWnd);
HWND GetForegroundWindow();
BOOL SetForegroundWindow(HWND hWnd);
HWND GetFocus();
HWND SetFocus(HWND hWnd);
HWND GetCapture();
HWND SetCapture(HWND hWnd);
BOOL ReleaseCapture();
HWND FindWindow(const char* lpClassName, const char* lpWindowName);
HWND FindWindowA(const char* lpClassName, const char* lpWindowName);
HWND WindowFromPoint(POINT Point);
HWND ChildWindowFromPoint(HWND hWndParent, POINT Point);
BOOL DestroyWindow(HWND hWnd);
BOOL CloseWindow(HWND hWnd);
BOOL BringWindowToTop(HWND hWnd);
BOOL SetWindowText(HWND hWnd, const char* lpString);
BOOL SetWindowTextA(HWND hWnd, const char* lpString);
int GetWindowText(HWND hWnd, char* lpString, int nMaxCount);
int GetWindowTextA(HWND hWnd, char* lpString, int nMaxCount);
int GetWindowTextLength(HWND hWnd);
int GetClassName(HWND hWnd, char* lpClassName, int nMaxCount);
LONG GetWindowLong(HWND hWnd, int nIndex);
LONG SetWindowLong(HWND hWnd, int nIndex, LONG dwNewLong);
LONG_PTR GetWindowLongPtr(HWND hWnd, int nIndex);
LONG_PTR SetWindowLongPtr(HWND hWnd, int nIndex, LONG_PTR dwNewLong);
BOOL GetWindowRect(HWND hWnd, LPRECT lpRect);
BOOL GetClientRect(HWND hWnd, LPRECT lpRect);
BOOL ClientToScreen(HWND hWnd, LPPOINT lpPoint);
BOOL ScreenToClient(HWND hWnd, LPPOINT lpPoint);
int MapWindowPoints(HWND hWndFrom, HWND hWndTo, LPPOINT lpPoints, UINT cPoints);
BOOL MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint);
BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);
BOOL InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase);
BOOL ValidateRect(HWND hWnd, const RECT* lpRect);
BOOL UpdateWindow(HWND hWnd);
BOOL RedrawWindow(HWND hWnd, const RECT* lprcUpdate, HRGN hrgnUpdate, UINT flags);
HDC GetDC(HWND hWnd);
HDC GetWindowDC(HWND hWnd);
int ReleaseDC(HWND hWnd, HDC hDC);
HDC BeginPaint(HWND hWnd, LPPAINTSTRUCT lpPaint);
BOOL EndPaint(HWND hWnd, const PAINTSTRUCT* lpPaint);
UINT_PTR SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, TIMERPROC lpTimerFunc);
BOOL KillTimer(HWND hWnd, UINT_PTR uIDEvent);

HWND GetDlgItem(HWND hDlg, int nIDDlgItem);
int GetDlgCtrlID(HWND hWnd);
UINT GetDlgItemInt(HWND hDlg, int nIDDlgItem, BOOL* lpTranslated, BOOL bSigned);
BOOL SetDlgItemInt(HWND hDlg, int nIDDlgItem, UINT uValue, BOOL bSigned);
UINT GetDlgItemText(HWND hDlg, int nIDDlgItem, char* lpString, int cchMax);
UINT GetDlgItemTextA(HWND hDlg, int nIDDlgItem, char* lpString, int cchMax);
BOOL SetDlgItemText(HWND hDlg, int nIDDlgItem, const char* lpString);
BOOL SetDlgItemTextA(HWND hDlg, int nIDDlgItem, const char* lpString);
BOOL CheckDlgButton(HWND hDlg, int nIDButton, UINT uCheck);
UINT IsDlgButtonChecked(HWND hDlg, int nIDButton);
BOOL CheckRadioButton(HWND hDlg, int nIDFirstButton, int nIDLastButton, int nIDCheckButton);
LRESULT SendDlgItemMessage(HWND hDlg, int nIDDlgItem, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL EndDialog(HWND hDlg, INT_PTR nResult);
HWND GetNextDlgTabItem(HWND hDlg, HWND hCtl, BOOL bPrevious);
HWND GetNextDlgGroupItem(HWND hDlg, HWND hCtl, BOOL bPrevious);
BOOL MapDialogRect(HWND hDlg, LPRECT lpRect);
LONG GetDialogBaseUnits();

int MessageBox(HWND hWnd, const char* lpText, const char* lpCaption, UINT uType);
int MessageBoxA(HWND hWnd, const char* lpText, const char* lpCaption, UINT uType);
BOOL WinHelp(HWND hWndMain, const char* lpszHelp, UINT uCommand, ULONG_PTR dwData);
BOOL WinHelpA(HWND hWndMain, const char* lpszHelp, UINT uCommand, ULONG_PTR dwData);

int GetSystemMetrics(int nIndex);
DWORD GetSysColor(int nIndex);
HBRUSH GetSysColorBrush(int nIndex);
BOOL SystemParametersInfo(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni);
BOOL SystemParametersInfoA(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni);
SHORT GetKeyState(int nVirtKey);
SHORT GetAsyncKeyState(int vKey);
BOOL GetKeyboardState(BYTE* lpKeyState);
BOOL GetCursorPos(LPPOINT lpPoint);
BOOL SetCursorPos(int X, int Y);
HCURSOR SetCursor(HCURSOR hCursor);
HCURSOR GetCursor();
int ShowCursor(BOOL bShow);
HCURSOR LoadCursor(HINSTANCE hInstance, const char* lpCursorName);
HCURSOR LoadCursorA(HINSTANCE hInstance, const char* lpCursorName);
HCURSOR LoadCursorFromFile(const char* lpFileName);
HICON LoadIcon(HINSTANCE hInstance, const char* lpIconName);
HICON LoadIconA(HINSTANCE hInstance, const char* lpIconName);
BOOL DestroyIcon(HICON hIcon);
BOOL DestroyCursor(HCURSOR hCursor);
BOOL DrawIcon(HDC hDC, int X, int Y, HICON hIcon);
HANDLE LoadImage(HINSTANCE hInst, const char* name, UINT type, int cx, int cy, UINT fuLoad);
HANDLE LoadImageA(HINSTANCE hInst, const char* name, UINT type, int cx, int cy, UINT fuLoad);
HBITMAP LoadBitmap(HINSTANCE hInstance, const char* lpBitmapName);
HBITMAP LoadBitmapA(HINSTANCE hInstance, const char* lpBitmapName);
int LoadString(HINSTANCE hInstance, UINT uID, char* lpBuffer, int cchBufferMax);
int LoadStringA(HINSTANCE hInstance, UINT uID, char* lpBuffer, int cchBufferMax);
HMENU LoadMenu(HINSTANCE hInstance, const char* lpMenuName);
HACCEL LoadAccelerators(HINSTANCE hInstance, const char* lpTableName);
int TranslateAccelerator(HWND hWnd, HACCEL hAccTable, LPMSG lpMsg);
HRSRC FindResource(HMODULE hModule, const char* lpName, const char* lpType);
HRSRC FindResourceA(HMODULE hModule, const char* lpName, const char* lpType);
HGLOBAL LoadResource(HMODULE hModule, HRSRC hResInfo);
LPVOID LockResource(HGLOBAL hResData);
DWORD SizeofResource(HMODULE hModule, HRSRC hResInfo);
BOOL FreeResource(HGLOBAL hResData);
#define RT_CURSOR MAKEINTRESOURCE(1)
#define RT_BITMAP MAKEINTRESOURCE(2)
#define RT_ICON MAKEINTRESOURCE(3)
#define RT_MENU MAKEINTRESOURCE(4)
#define RT_DIALOG MAKEINTRESOURCE(5)
#define RT_STRING MAKEINTRESOURCE(6)
#define RT_ACCELERATOR MAKEINTRESOURCE(9)
#define RT_RCDATA MAKEINTRESOURCE(10)
#define RT_GROUP_CURSOR MAKEINTRESOURCE(12)
#define RT_GROUP_ICON MAKEINTRESOURCE(14)
#define RT_VERSION MAKEINTRESOURCE(16)
#define RT_DLGINIT MAKEINTRESOURCE(240)
#define RT_HTML MAKEINTRESOURCE(23)

#define IDC_ARROW MAKEINTRESOURCE(32512)
#define IDC_IBEAM MAKEINTRESOURCE(32513)
#define IDC_WAIT MAKEINTRESOURCE(32514)
#define IDC_CROSS MAKEINTRESOURCE(32515)
#define IDC_UPARROW MAKEINTRESOURCE(32516)
#define IDC_SIZE MAKEINTRESOURCE(32640)
#define IDC_ICON MAKEINTRESOURCE(32641)
#define IDC_SIZENWSE MAKEINTRESOURCE(32642)
#define IDC_SIZENESW MAKEINTRESOURCE(32643)
#define IDC_SIZEWE MAKEINTRESOURCE(32644)
#define IDC_SIZENS MAKEINTRESOURCE(32645)
#define IDC_SIZEALL MAKEINTRESOURCE(32646)
#define IDC_NO MAKEINTRESOURCE(32648)
#define IDC_HAND MAKEINTRESOURCE(32649)
#define IDC_APPSTARTING MAKEINTRESOURCE(32650)
#define IDC_HELP MAKEINTRESOURCE(32651)
#define IDI_APPLICATION MAKEINTRESOURCE(32512)
#define IDI_HAND MAKEINTRESOURCE(32513)
#define IDI_QUESTION MAKEINTRESOURCE(32514)
#define IDI_EXCLAMATION MAKEINTRESOURCE(32515)
#define IDI_ASTERISK MAKEINTRESOURCE(32516)
#define IDI_WINLOGO MAKEINTRESOURCE(32517)
#define IDI_WARNING IDI_EXCLAMATION
#define IDI_ERROR IDI_HAND
#define IDI_INFORMATION IDI_ASTERISK

HMENU GetMenu(HWND hWnd);
BOOL SetMenu(HWND hWnd, HMENU hMenu);
HMENU GetSubMenu(HMENU hMenu, int nPos);
HMENU GetSystemMenu(HWND hWnd, BOOL bRevert);
HMENU CreateMenu();
HMENU CreatePopupMenu();
BOOL DestroyMenu(HMENU hMenu);
BOOL AppendMenu(HMENU hMenu, UINT uFlags, UINT_PTR uIDNewItem, const char* lpNewItem);
BOOL AppendMenuA(HMENU hMenu, UINT uFlags, UINT_PTR uIDNewItem, const char* lpNewItem);
BOOL InsertMenu(HMENU hMenu, UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, const char* lpNewItem);
BOOL DeleteMenu(HMENU hMenu, UINT uPosition, UINT uFlags);
BOOL RemoveMenu(HMENU hMenu, UINT uPosition, UINT uFlags);
BOOL ModifyMenu(HMENU hMnu, UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, const char* lpNewItem);
DWORD CheckMenuItem(HMENU hMenu, UINT uIDCheckItem, UINT uCheck);
BOOL EnableMenuItem(HMENU hMenu, UINT uIDEnableItem, UINT uEnable);
int GetMenuItemCount(HMENU hMenu);
UINT GetMenuItemID(HMENU hMenu, int nPos);
UINT GetMenuState(HMENU hMenu, UINT uId, UINT uFlags);
int GetMenuString(HMENU hMenu, UINT uIDItem, char* lpString, int cchMax, UINT flags);
BOOL TrackPopupMenu(HMENU hMenu, UINT uFlags, int x, int y, int nReserved, HWND hWnd, const RECT* prcRect);
BOOL DrawMenuBar(HWND hWnd);

BOOL OpenClipboard(HWND hWndNewOwner);
BOOL CloseClipboard();
BOOL EmptyClipboard();
HANDLE GetClipboardData(UINT uFormat);
HANDLE SetClipboardData(UINT uFormat, HANDLE hMem);
BOOL IsClipboardFormatAvailable(UINT format);
UINT RegisterClipboardFormat(const char* lpszFormat);
UINT RegisterClipboardFormatA(const char* lpszFormat);
UINT DragQueryFile(HDROP hDrop, UINT iFile, char* lpszFile, UINT cch);
UINT DragQueryFileA(HDROP hDrop, UINT iFile, char* lpszFile, UINT cch);
void DragFinish(HDROP hDrop);
void DragAcceptFiles(HWND hWnd, BOOL fAccept);

HGDIOBJ GetStockObject(int i);
BOOL DeleteObject(HGDIOBJ ho);
HGDIOBJ SelectObject(HDC hdc, HGDIOBJ h);
int GetObject(HANDLE h, int c, LPVOID pv);
int GetObjectA(HANDLE h, int c, LPVOID pv);
HBRUSH CreateSolidBrush(COLORREF color);
HBRUSH CreateHatchBrush(int iHatch, COLORREF color);
HBRUSH CreatePatternBrush(HBITMAP hbm);
HPEN CreatePen(int iStyle, int cWidth, COLORREF color);
HFONT CreateFont(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight, DWORD bItalic,
                 DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet, DWORD iOutPrecision, DWORD iClipPrecision,
                 DWORD iQuality, DWORD iPitchAndFamily, const char* pszFaceName);
HFONT CreateFontA(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight, DWORD bItalic,
                  DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet, DWORD iOutPrecision, DWORD iClipPrecision,
                  DWORD iQuality, DWORD iPitchAndFamily, const char* pszFaceName);
HFONT CreateFontIndirect(const LOGFONT* lplf);
HFONT CreateFontIndirectA(const LOGFONT* lplf);
HBITMAP CreateCompatibleBitmap(HDC hdc, int cx, int cy);
HBITMAP CreateBitmap(int nWidth, int nHeight, UINT nPlanes, UINT nBitCount, const void* lpBits);
HDC CreateCompatibleDC(HDC hdc);
BOOL DeleteDC(HDC hdc);
COLORREF SetTextColor(HDC hdc, COLORREF color);
COLORREF SetBkColor(HDC hdc, COLORREF color);
int SetBkMode(HDC hdc, int mode);
BOOL TextOut(HDC hdc, int x, int y, const char* lpString, int c);
BOOL TextOutA(HDC hdc, int x, int y, const char* lpString, int c);
int DrawText(HDC hdc, const char* lpchText, int cchText, LPRECT lprc, UINT format);
int DrawTextA(HDC hdc, const char* lpchText, int cchText, LPRECT lprc, UINT format);
BOOL GetTextExtentPoint32(HDC hdc, const char* lpString, int c, LPSIZE psizl);
BOOL GetTextExtentPoint32A(HDC hdc, const char* lpString, int c, LPSIZE psizl);
BOOL GetTextMetrics(HDC hdc, LPTEXTMETRIC lptm);
BOOL GetTextMetricsA(HDC hdc, LPTEXTMETRIC lptm);
int GetDeviceCaps(HDC hdc, int index);
BOOL BitBlt(HDC hdc, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1, DWORD rop);
BOOL StretchBlt(HDC hdcDest, int xDest, int yDest, int wDest, int hDest, HDC hdcSrc, int xSrc, int ySrc, int wSrc,
                int hSrc, DWORD rop);
BOOL MoveToEx(HDC hdc, int x, int y, LPPOINT lppt);
BOOL LineTo(HDC hdc, int x, int y);
BOOL Rectangle(HDC hdc, int left, int top, int right, int bottom);
BOOL Ellipse(HDC hdc, int left, int top, int right, int bottom);
BOOL Polygon(HDC hdc, const POINT* apt, int cpt);
BOOL Polyline(HDC hdc, const POINT* apt, int cpt);
int FillRect(HDC hDC, const RECT* lprc, HBRUSH hbr);
int FrameRect(HDC hDC, const RECT* lprc, HBRUSH hbr);
COLORREF SetPixel(HDC hdc, int x, int y, COLORREF color);
COLORREF GetPixel(HDC hdc, int x, int y);
BOOL DrawEdge(HDC hdc, LPRECT qrc, UINT edge, UINT grfFlags);
BOOL DrawFocusRect(HDC hDC, const RECT* lprc);
int SaveDC(HDC hdc);
BOOL RestoreDC(HDC hdc, int nSavedDC);
HRGN CreateRectRgn(int x1, int y1, int x2, int y2);
HRGN CreateEllipticRgn(int x1, int y1, int x2, int y2);
int SelectClipRgn(HDC hdc, HRGN hrgn);
int SetDIBitsToDevice(HDC hdc, int xDest, int yDest, DWORD w, DWORD h, int xSrc, int ySrc, UINT StartScan,
                      UINT cLines, const void* lpvBits, const BITMAPINFO* lpbmi, UINT ColorUse);
int StretchDIBits(HDC hdc, int xDest, int yDest, int DestWidth, int DestHeight, int xSrc, int ySrc, int SrcWidth,
                  int SrcHeight, const void* lpBits, const BITMAPINFO* lpbmi, UINT iUsage, DWORD rop);

BOOL SetRect(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom);
BOOL SetRectEmpty(LPRECT lprc);
BOOL CopyRect(LPRECT lprcDst, const RECT* lprcSrc);
BOOL InflateRect(LPRECT lprc, int dx, int dy);
BOOL OffsetRect(LPRECT lprc, int dx, int dy);
BOOL IntersectRect(LPRECT lprcDst, const RECT* lprcSrc1, const RECT* lprcSrc2);
BOOL UnionRect(LPRECT lprcDst, const RECT* lprcSrc1, const RECT* lprcSrc2);
BOOL IsRectEmpty(const RECT* lprc);
BOOL PtInRect(const RECT* lprc, POINT pt);
BOOL EqualRect(const RECT* lprc1, const RECT* lprc2);
