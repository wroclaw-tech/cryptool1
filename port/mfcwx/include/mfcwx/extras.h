#pragma once

// Assorted Win32/MFC declarations used by the application that do not fit elsewhere.

#define CS_VREDRAW 0x0001
#define CS_HREDRAW 0x0002
#define CS_DBLCLKS 0x0008
#define CS_OWNDC 0x0020
#define CS_CLASSDC 0x0040
#define CS_PARENTDC 0x0080
#define CS_NOCLOSE 0x0200
#define CS_SAVEBITS 0x0800
#define CS_BYTEALIGNCLIENT 0x1000
#define CS_GLOBALCLASS 0x4000
#define GCL_HCURSOR (-12)
#define GCL_HICON (-14)
#define GCL_HBRBACKGROUND (-10)
#define GCLP_HCURSOR (-12)
#define GCLP_HICON (-14)
#define SM_CXDRAG 68
#define SM_CYDRAG 69
#define TCS_EX_FLATSEPARATORS 0x00000001
#define TCS_EX_REGISTERDROP 0x00000002
#define LOCALE_IMEASURE 0x0000000D
#define LOCALE_SDECIMAL 0x0000000E
#define LOCALE_STHOUSAND 0x0000000F
#define LOCALE_SLANGUAGE 0x00000002
#define LOCALE_SENGLANGUAGE 0x00001001
#define LOCALE_SABBREVLANGNAME 0x00000003
#define LOCALE_IDEFAULTANSICODEPAGE 0x00001004
#define UNUSED(x) (void)(x)
#define UNUSED_ALWAYS(x) (void)(x)

LONG GetClassLong(HWND hWnd, int nIndex);
LONG SetClassLong(HWND hWnd, int nIndex, LONG dwNewLong);
ULONG_PTR SetClassLongPtr(HWND hWnd, int nIndex, LONG_PTR dwNewLong);
BOOL GetClassInfo(HINSTANCE hInstance, const char* lpClassName, WNDCLASS* lpWndClass);
ATOM RegisterClass(const WNDCLASS* lpWndClass);
BOOL UnregisterClass(const char* lpClassName, HINSTANCE hInstance);
HWND CreateWindowEx(DWORD dwExStyle, const char* lpClassName, const char* lpWindowName, DWORD dwStyle, int X, int Y,
                    int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);
HWND CreateWindow(const char* lpClassName, const char* lpWindowName, DWORD dwStyle, int X, int Y, int nWidth,
                  int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);
HWND GetNextWindow(HWND hWnd, UINT wCmd);
BOOL DestroyCaret();
BOOL CreateCaret(HWND hWnd, HBITMAP hBitmap, int nWidth, int nHeight);
BOOL ShowCaret(HWND hWnd);
BOOL HideCaret(HWND hWnd);
BOOL SetCaretPos(int X, int Y);
int GetLocaleInfo(LCID Locale, DWORD LCType, char* lpLCData, int cchData);
int GetLocaleInfoA(LCID Locale, DWORD LCType, char* lpLCData, int cchData);
BOOL GetOpenFileName(OPENFILENAME* lpofn);
BOOL GetSaveFileName(OPENFILENAME* lpofn);
BOOL GetOpenFileNameA(OPENFILENAME* lpofn);
BOOL GetSaveFileNameA(OPENFILENAME* lpofn);
BOOL ChooseColor(CHOOSECOLOR* lpcc);
BOOL MessageBeep(UINT uType);
BOOL FlashWindow(HWND hWnd, BOOL bInvert);

#define PD_ALLPAGES 0x00000000
#define PD_SELECTION 0x00000001
#define PD_PAGENUMS 0x00000002
#define PD_NOSELECTION 0x00000004
#define PD_NOPAGENUMS 0x00000008
#define PD_COLLATE 0x00000010
#define PD_PRINTTOFILE 0x00000020
#define PD_PRINTSETUP 0x00000040
#define PD_RETURNDC 0x00000100
#define PD_USEDEVMODECOPIES 0x00040000
#define PD_DISABLEPRINTTOFILE 0x00080000
#define PD_HIDEPRINTTOFILE 0x00100000
typedef struct tagPDA {
    DWORD lStructSize;
    HWND hwndOwner;
    HGLOBAL hDevMode, hDevNames;
    HDC hDC;
    DWORD Flags;
    WORD nFromPage, nToPage, nMinPage, nMaxPage, nCopies;
    HINSTANCE hInstance;
    LPARAM lCustData;
} PRINTDLG, *LPPRINTDLG;
typedef struct _devicemodeA { char dmDeviceName[32]; WORD dmSpecVersion, dmDriverVersion, dmSize, dmDriverExtra; DWORD dmFields; short dmOrientation, dmPaperSize, dmPaperLength, dmPaperWidth; } DEVMODE, *LPDEVMODE;
typedef struct tagDEVNAMES { WORD wDriverOffset, wDeviceOffset, wOutputOffset, wDefault; } DEVNAMES, *LPDEVNAMES;
#define DMORIENT_PORTRAIT 1
#define DMORIENT_LANDSCAPE 2

typedef struct tagPALETTEENTRY { BYTE peRed, peGreen, peBlue, peFlags; } PALETTEENTRY, *LPPALETTEENTRY;
typedef struct tagLOGPALETTE { WORD palVersion, palNumEntries; PALETTEENTRY palPalEntry[1]; } LOGPALETTE;
#define PC_RESERVED 0x01
#define PC_EXPLICIT 0x02
#define PC_NOCOLLAPSE 0x04

#define ListView_GetSubItemRect(hwnd, iItem, iSubItem, code, prc) \
    (BOOL)SendMessage((hwnd), LVM_GETSUBITEMRECT, (WPARAM)(int)(iItem), \
                      ((prc) ? ((((LPRECT)(prc))->top = (iSubItem)), (((LPRECT)(prc))->left = (code)), (LPARAM)(prc)) : (LPARAM)(LPRECT)NULL))
#define ListView_GetTopIndex(hwnd) (int)SendMessage((hwnd), LVM_GETTOPINDEX, 0, 0)
#define ListView_GetItemText(hwndLV, i, iSubItem_, pszText_, cchTextMax_) \
    { LVITEM _lvi; _lvi.iSubItem = (iSubItem_); _lvi.cchTextMax = (cchTextMax_); _lvi.pszText = (pszText_); \
      SendMessage((hwndLV), LVM_GETITEMTEXT, (WPARAM)(i), (LPARAM)&_lvi); }
#define ListView_SetExtendedListViewStyle(hwnd, style) SendMessage((hwnd), LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (style))
#define ListView_GetItemCount(hwnd) (int)SendMessage((hwnd), LVM_GETITEMCOUNT, 0, 0)

class CWaitCursor {
public:
    CWaitCursor();
    ~CWaitCursor();
    void Restore() {}
};

// WGL: OpenGL rendering into a window. mfcwx backs the window's client area with a wxGLCanvas.
typedef struct tagPIXELFORMATDESCRIPTOR {
    WORD nSize, nVersion;
    DWORD dwFlags;
    BYTE iPixelType, cColorBits, cRedBits, cRedShift, cGreenBits, cGreenShift, cBlueBits, cBlueShift, cAlphaBits,
        cAlphaShift, cAccumBits, cAccumRedBits, cAccumGreenBits, cAccumBlueBits, cAccumAlphaBits, cDepthBits,
        cStencilBits, cAuxBuffers, iLayerType, bReserved;
    DWORD dwLayerMask, dwVisibleMask, dwDamageMask;
} PIXELFORMATDESCRIPTOR, *PPIXELFORMATDESCRIPTOR, *LPPIXELFORMATDESCRIPTOR;
#define PFD_TYPE_RGBA 0
#define PFD_TYPE_COLORINDEX 1
#define PFD_MAIN_PLANE 0
#define PFD_DOUBLEBUFFER 0x00000001
#define PFD_STEREO 0x00000002
#define PFD_DRAW_TO_WINDOW 0x00000004
#define PFD_DRAW_TO_BITMAP 0x00000008
#define PFD_SUPPORT_GDI 0x00000010
#define PFD_SUPPORT_OPENGL 0x00000020
#define PFD_GENERIC_FORMAT 0x00000040
#define PFD_NEED_PALETTE 0x00000080
#define PFD_NEED_SYSTEM_PALETTE 0x00000100
#define PFD_SWAP_EXCHANGE 0x00000200
#define PFD_GENERIC_ACCELERATED 0x00001000
int ChoosePixelFormat(HDC hdc, const PIXELFORMATDESCRIPTOR* ppfd);
BOOL SetPixelFormat(HDC hdc, int format, const PIXELFORMATDESCRIPTOR* ppfd);
int GetPixelFormat(HDC hdc);
int DescribePixelFormat(HDC hdc, int iPixelFormat, UINT nBytes, LPPIXELFORMATDESCRIPTOR ppfd);
BOOL SwapBuffers(HDC hdc);
HGLRC wglCreateContext(HDC hdc);
BOOL wglDeleteContext(HGLRC hglrc);
BOOL wglMakeCurrent(HDC hdc, HGLRC hglrc);
HGLRC wglGetCurrentContext();
HDC wglGetCurrentDC();
void* wglGetProcAddress(const char* name);
