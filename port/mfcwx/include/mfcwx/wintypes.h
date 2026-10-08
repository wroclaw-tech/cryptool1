#pragma once

/* Win32 base types. C compatible. 32-bit widths follow the Windows LLP64 model. */

#include <stdint.h>
#include <stddef.h>
#include <wchar.h>

#ifndef _MFCWX_WINTYPES
#define _MFCWX_WINTYPES

#define WINAPI
#define WINAPIV
#define APIENTRY
#define CALLBACK
#define PASCAL
#define _pascal
#define __pascal
#define FAR
#define NEAR
#define far
#define near
#define _far
#define _near
#define __far
#define __near
#define __stdcall
#define _stdcall
#define __cdecl
#define _cdecl
#define __fastcall
#define _fastcall
#define __declspec(x)
#define __forceinline inline
#define _inline inline
#define __inline inline
#define __w64
#define __int8 char
#define __int16 short
#define __int32 int
#define __int64 long long
#define CONST const
#define VOID void
#define IN
#define OUT
#define OPTIONAL
#define UNALIGNED
#define AFXAPI
#define AFX_CDECL
#define AFX_STATIC static
#define AFX_STATIC_DATA static
#define AFX_INLINE inline
#define AFX_EXT_CLASS
#define AFX_EXT_API
#define AFX_EXT_DATA
#define AFX_CORE_DATA
#define AFX_DATA
#define AFX_DATADEF
#define AFX_COMDAT
#define AFX_NOVTABLE
#define AFX_IMPORT_DATA
#define AFX_API_EXPORT
#define DECLSPEC_NOVTABLE
#define DECLSPEC_IMPORT
#define DECLSPEC_EXPORT

typedef int BOOL;
typedef unsigned char BOOLEAN;
typedef unsigned char BYTE;
typedef unsigned char byte;
typedef unsigned short WORD;
typedef uint32_t DWORD;
typedef unsigned int UINT;
typedef int INT;
typedef int32_t LONG;
typedef uint32_t ULONG;
typedef short SHORT;
typedef unsigned short USHORT;
typedef char CHAR;
typedef unsigned char UCHAR;
typedef char CCHAR;
typedef float FLOAT;
typedef double DOUBLE;
typedef long long LONGLONG;
typedef unsigned long long ULONGLONG;
typedef long long INT64;
typedef unsigned long long UINT64;
typedef int32_t INT32;
typedef uint32_t UINT32;
typedef int16_t INT16;
typedef uint16_t UINT16;
typedef int8_t INT8;
typedef uint8_t UINT8;
typedef uint32_t DWORD32;
typedef uint64_t DWORD64;
typedef uint64_t QWORD;
typedef intptr_t INT_PTR;
typedef uintptr_t UINT_PTR;
typedef intptr_t LONG_PTR;
typedef uintptr_t ULONG_PTR;
typedef uintptr_t DWORD_PTR;
typedef size_t SIZE_T;
typedef ptrdiff_t SSIZE_T;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
typedef WORD ATOM;
typedef DWORD COLORREF;
typedef DWORD LCID;
typedef WORD LANGID;
typedef long HRESULT;
typedef DWORD ACCESS_MASK;

typedef char TCHAR;
typedef unsigned char _TUCHAR;
typedef char _TCHAR;
typedef char _TSCHAR;
typedef wchar_t WCHAR;
typedef char* LPSTR;
typedef char* PSTR;
typedef char* NPSTR;
typedef const char* LPCSTR;
typedef const char* PCSTR;
typedef char* LPTSTR;
typedef char* PTSTR;
typedef const char* LPCTSTR;
typedef const char* PCTSTR;
typedef wchar_t* LPWSTR;
typedef wchar_t* PWSTR;
typedef const wchar_t* LPCWSTR;
typedef const wchar_t* PCWSTR;
typedef wchar_t OLECHAR;
typedef wchar_t* LPOLESTR;
typedef const wchar_t* LPCOLESTR;
typedef wchar_t* BSTR;
typedef unsigned char* LPBYTE;
typedef unsigned char* PBYTE;
typedef BOOL* LPBOOL;
typedef BOOL* PBOOL;
typedef WORD* LPWORD;
typedef WORD* PWORD;
typedef DWORD* LPDWORD;
typedef DWORD* PDWORD;
typedef LONG* LPLONG;
typedef LONG* PLONG;
typedef int* LPINT;
typedef int* PINT;
typedef UINT* LPUINT;
typedef UINT* PUINT;
typedef void* LPVOID;
typedef void* PVOID;
typedef const void* LPCVOID;
typedef COLORREF* LPCOLORREF;

#define DECLARE_HANDLE(name) struct name##__ { int unused; }; typedef struct name##__* name
typedef void* HANDLE;
typedef HANDLE* PHANDLE;
typedef HANDLE* LPHANDLE;
DECLARE_HANDLE(HWND);
DECLARE_HANDLE(HINSTANCE);
DECLARE_HANDLE(HDC);
DECLARE_HANDLE(HGDIOBJ);
DECLARE_HANDLE(HMENU);
DECLARE_HANDLE(HACCEL);
DECLARE_HANDLE(HICON);
DECLARE_HANDLE(HKEY);
DECLARE_HANDLE(HTREEITEM);
DECLARE_HANDLE(HIMAGELIST);
DECLARE_HANDLE(HDROP);
DECLARE_HANDLE(HGLRC);
DECLARE_HANDLE(HDESK);
DECLARE_HANDLE(HHOOK);
DECLARE_HANDLE(HMONITOR);
DECLARE_HANDLE(HENHMETAFILE);
DECLARE_HANDLE(HMETAFILE);
DECLARE_HANDLE(HRSRC);
DECLARE_HANDLE(HTASK);
typedef HINSTANCE HMODULE;
typedef HICON HCURSOR;
typedef HGDIOBJ HBITMAP;
typedef HGDIOBJ HBRUSH;
typedef HGDIOBJ HPEN;
typedef HGDIOBJ HFONT;
typedef HGDIOBJ HRGN;
typedef HGDIOBJ HPALETTE;
typedef HANDLE HGLOBAL;
typedef HANDLE HLOCAL;
typedef HANDLE GLOBALHANDLE;
typedef HANDLE LOCALHANDLE;
typedef int HFILE;
typedef HKEY* PHKEY;
typedef LONG LSTATUS;
typedef DWORD REGSAM;

#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define HFILE_ERROR ((HFILE)-1)
#define HKEY_CLASSES_ROOT ((HKEY)(uintptr_t)0x80000000)
#define HKEY_CURRENT_USER ((HKEY)(uintptr_t)0x80000001)
#define HKEY_LOCAL_MACHINE ((HKEY)(uintptr_t)0x80000002)
#define HKEY_USERS ((HKEY)(uintptr_t)0x80000003)
#define HWND_TOP ((HWND)0)
#define HWND_BOTTOM ((HWND)1)
#define HWND_TOPMOST ((HWND)(intptr_t)-1)
#define HWND_NOTOPMOST ((HWND)(intptr_t)-2)
#define HWND_DESKTOP ((HWND)0)

typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef INT_PTR (*DLGPROC)(HWND, UINT, WPARAM, LPARAM);
typedef void (*TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);
typedef INT_PTR (*FARPROC)(void);
typedef INT_PTR (*PROC)(void);
typedef DWORD (*LPTHREAD_START_ROUTINE)(LPVOID);

typedef struct tagRECT { LONG left, top, right, bottom; } RECT, *PRECT, *LPRECT, *NPRECT;
typedef const RECT* LPCRECT;
typedef struct tagPOINT { LONG x, y; } POINT, *PPOINT, *LPPOINT, *NPPOINT;
typedef struct tagSIZE { LONG cx, cy; } SIZE, *PSIZE, *LPSIZE, SIZEL, *PSIZEL, *LPSIZEL;
typedef struct tagPOINTS { SHORT x, y; } POINTS, *PPOINTS, *LPPOINTS;

typedef struct tagMSG {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
} MSG, *PMSG, *LPMSG;

typedef struct _FILETIME { DWORD dwLowDateTime, dwHighDateTime; } FILETIME, *PFILETIME, *LPFILETIME;
typedef struct _SYSTEMTIME {
    WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

typedef union _LARGE_INTEGER {
    struct { DWORD LowPart; LONG HighPart; } u;
    struct { DWORD LowPart; LONG HighPart; };
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;
typedef union _ULARGE_INTEGER {
    struct { DWORD LowPart; DWORD HighPart; } u;
    struct { DWORD LowPart; DWORD HighPart; };
    ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

typedef struct _SECURITY_ATTRIBUTES {
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

typedef struct _OVERLAPPED {
    ULONG_PTR Internal, InternalHigh;
    DWORD Offset, OffsetHigh;
    HANDLE hEvent;
} OVERLAPPED, *LPOVERLAPPED;

typedef struct _WIN32_FIND_DATAA {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
    DWORD nFileSizeHigh, nFileSizeLow;
    DWORD dwReserved0, dwReserved1;
    CHAR cFileName[260];
    CHAR cAlternateFileName[14];
} WIN32_FIND_DATAA, WIN32_FIND_DATA, *PWIN32_FIND_DATA, *LPWIN32_FIND_DATA;

typedef struct _STARTUPINFOA {
    DWORD cb;
    LPSTR lpReserved, lpDesktop, lpTitle;
    DWORD dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
    WORD wShowWindow, cbReserved2;
    LPBYTE lpReserved2;
    HANDLE hStdInput, hStdOutput, hStdError;
} STARTUPINFOA, STARTUPINFO, *LPSTARTUPINFO;

typedef struct _PROCESS_INFORMATION {
    HANDLE hProcess, hThread;
    DWORD dwProcessId, dwThreadId;
} PROCESS_INFORMATION, *PPROCESS_INFORMATION, *LPPROCESS_INFORMATION;

typedef struct tagLOGFONTA {
    LONG lfHeight, lfWidth, lfEscapement, lfOrientation, lfWeight;
    BYTE lfItalic, lfUnderline, lfStrikeOut, lfCharSet, lfOutPrecision, lfClipPrecision, lfQuality, lfPitchAndFamily;
    CHAR lfFaceName[32];
} LOGFONTA, LOGFONT, *PLOGFONT, *LPLOGFONT;

typedef struct tagTEXTMETRICA {
    LONG tmHeight, tmAscent, tmDescent, tmInternalLeading, tmExternalLeading, tmAveCharWidth, tmMaxCharWidth,
        tmWeight, tmOverhang, tmDigitizedAspectX, tmDigitizedAspectY;
    BYTE tmFirstChar, tmLastChar, tmDefaultChar, tmBreakChar, tmItalic, tmUnderlined, tmStruckOut, tmPitchAndFamily,
        tmCharSet;
} TEXTMETRICA, TEXTMETRIC, *PTEXTMETRIC, *LPTEXTMETRIC;

typedef struct tagLOGPEN { UINT lopnStyle; POINT lopnWidth; COLORREF lopnColor; } LOGPEN, *PLOGPEN, *LPLOGPEN;
typedef struct tagLOGBRUSH { UINT lbStyle; COLORREF lbColor; ULONG_PTR lbHatch; } LOGBRUSH, *PLOGBRUSH, *LPLOGBRUSH;

typedef struct tagBITMAP {
    LONG bmType, bmWidth, bmHeight, bmWidthBytes;
    WORD bmPlanes, bmBitsPixel;
    LPVOID bmBits;
} BITMAP, *PBITMAP, *LPBITMAP;

#pragma pack(push, 2)
typedef struct tagBITMAPFILEHEADER {
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1, bfReserved2;
    DWORD bfOffBits;
} BITMAPFILEHEADER, *LPBITMAPFILEHEADER, *PBITMAPFILEHEADER;
#pragma pack(pop)

typedef struct tagBITMAPINFOHEADER {
    DWORD biSize;
    LONG biWidth, biHeight;
    WORD biPlanes, biBitCount;
    DWORD biCompression, biSizeImage;
    LONG biXPelsPerMeter, biYPelsPerMeter;
    DWORD biClrUsed, biClrImportant;
} BITMAPINFOHEADER, *LPBITMAPINFOHEADER, *PBITMAPINFOHEADER;
typedef struct tagRGBQUAD { BYTE rgbBlue, rgbGreen, rgbRed, rgbReserved; } RGBQUAD, *LPRGBQUAD;
typedef struct tagBITMAPINFO { BITMAPINFOHEADER bmiHeader; RGBQUAD bmiColors[1]; } BITMAPINFO, *LPBITMAPINFO, *PBITMAPINFO;

typedef struct tagPAINTSTRUCT {
    HDC hdc;
    BOOL fErase;
    RECT rcPaint;
    BOOL fRestore, fIncUpdate;
    BYTE rgbReserved[32];
} PAINTSTRUCT, *PPAINTSTRUCT, *LPPAINTSTRUCT;

typedef struct tagDRAWITEMSTRUCT {
    UINT CtlType, CtlID, itemID, itemAction, itemState;
    HWND hwndItem;
    HDC hDC;
    RECT rcItem;
    ULONG_PTR itemData;
} DRAWITEMSTRUCT, *PDRAWITEMSTRUCT, *LPDRAWITEMSTRUCT;

typedef struct tagMEASUREITEMSTRUCT {
    UINT CtlType, CtlID, itemID, itemWidth, itemHeight;
    ULONG_PTR itemData;
} MEASUREITEMSTRUCT, *PMEASUREITEMSTRUCT, *LPMEASUREITEMSTRUCT;

typedef struct tagCOMPAREITEMSTRUCT {
    UINT CtlType, CtlID;
    HWND hwndItem;
    UINT itemID1;
    ULONG_PTR itemData1;
    UINT itemID2;
    ULONG_PTR itemData2;
    DWORD dwLocaleId;
} COMPAREITEMSTRUCT, *LPCOMPAREITEMSTRUCT;

typedef struct tagDELETEITEMSTRUCT {
    UINT CtlType, CtlID, itemID;
    HWND hwndItem;
    ULONG_PTR itemData;
} DELETEITEMSTRUCT, *LPDELETEITEMSTRUCT;

typedef struct tagNMHDR { HWND hwndFrom; UINT_PTR idFrom; UINT code; } NMHDR, *LPNMHDR;
typedef struct _HD_ITEMA {
    UINT mask;
    int cxy;
    LPSTR pszText;
    HBITMAP hbm;
    int cchTextMax, fmt;
    LPARAM lParam;
    int iImage, iOrder;
} HDITEM, HD_ITEM, *LPHDITEM;
typedef struct tagNMHEADERA { NMHDR hdr; int iItem; int iButton; HDITEM* pitem; } NMHEADER, HD_NOTIFY, *LPNMHEADER;

typedef struct tagHELPINFO {
    UINT cbSize;
    int iContextType;
    int iCtrlId;
    HANDLE hItemHandle;
    DWORD_PTR dwContextId;
    POINT MousePos;
} HELPINFO, *LPHELPINFO;

typedef struct tagMINMAXINFO {
    POINT ptReserved, ptMaxSize, ptMaxPosition, ptMinTrackSize, ptMaxTrackSize;
} MINMAXINFO, *PMINMAXINFO, *LPMINMAXINFO;

typedef struct tagCREATESTRUCTA {
    LPVOID lpCreateParams;
    HINSTANCE hInstance;
    HMENU hMenu;
    HWND hwndParent;
    int cy, cx, y, x;
    LONG style;
    LPCSTR lpszName, lpszClass;
    DWORD dwExStyle;
} CREATESTRUCTA, CREATESTRUCT, *LPCREATESTRUCT;

typedef struct tagWINDOWPLACEMENT {
    UINT length, flags, showCmd;
    POINT ptMinPosition, ptMaxPosition;
    RECT rcNormalPosition;
} WINDOWPLACEMENT, *PWINDOWPLACEMENT, *LPWINDOWPLACEMENT;

typedef struct tagWINDOWPOS {
    HWND hwnd, hwndInsertAfter;
    int x, y, cx, cy;
    UINT flags;
} WINDOWPOS, *LPWINDOWPOS, *PWINDOWPOS;

typedef struct tagSCROLLINFO {
    UINT cbSize, fMask;
    int nMin, nMax;
    UINT nPage;
    int nPos, nTrackPos;
} SCROLLINFO, *LPSCROLLINFO;
typedef const SCROLLINFO* LPCSCROLLINFO;

typedef struct tagACCEL { BYTE fVirt; WORD key; WORD cmd; } ACCEL, *LPACCEL;

typedef struct tagWNDCLASSA {
    UINT style;
    WNDPROC lpfnWndProc;
    int cbClsExtra, cbWndExtra;
    HINSTANCE hInstance;
    HICON hIcon;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    LPCSTR lpszMenuName, lpszClassName;
} WNDCLASSA, WNDCLASS, *LPWNDCLASS;

typedef struct tagNMUPDOWN { NMHDR hdr; int iPos; int iDelta; } NMUPDOWN, *LPNMUPDOWN, NM_UPDOWN;

typedef struct tagLVITEMA {
    UINT mask;
    int iItem, iSubItem;
    UINT state, stateMask;
    LPSTR pszText;
    int cchTextMax, iImage;
    LPARAM lParam;
    int iIndent;
} LVITEMA, LVITEM, LV_ITEM, *LPLVITEM;

typedef struct tagLVCOLUMNA {
    UINT mask;
    int fmt, cx;
    LPSTR pszText;
    int cchTextMax, iSubItem, iImage, iOrder;
} LVCOLUMNA, LVCOLUMN, LV_COLUMN, *LPLVCOLUMN;

typedef struct tagNMLISTVIEW {
    NMHDR hdr;
    int iItem, iSubItem;
    UINT uNewState, uOldState, uChanged;
    POINT ptAction;
    LPARAM lParam;
} NMLISTVIEW, NM_LISTVIEW, *LPNMLISTVIEW;

typedef struct tagNMITEMACTIVATE {
    NMHDR hdr;
    int iItem, iSubItem;
    UINT uNewState, uOldState, uChanged;
    POINT ptAction;
    LPARAM lParam;
    UINT uKeyFlags;
} NMITEMACTIVATE, *LPNMITEMACTIVATE;

typedef struct tagLVDISPINFO { NMHDR hdr; LVITEMA item; } NMLVDISPINFO, LV_DISPINFO, *LPNMLVDISPINFO;
typedef struct tagLVKEYDOWN { NMHDR hdr; WORD wVKey; UINT flags; } NMLVKEYDOWN, LV_KEYDOWN, *LPNMLVKEYDOWN;
typedef struct tagLVHITTESTINFO { POINT pt; UINT flags; int iItem, iSubItem, iGroup; } LVHITTESTINFO, *LPLVHITTESTINFO;
typedef struct tagLVFINDINFOA { UINT flags; LPCSTR psz; LPARAM lParam; POINT pt; UINT vkDirection; } LVFINDINFO, *LPFINDINFO;

typedef struct tagTVITEMA {
    UINT mask;
    HTREEITEM hItem;
    UINT state, stateMask;
    LPSTR pszText;
    int cchTextMax, iImage, iSelectedImage, cChildren;
    LPARAM lParam;
} TVITEMA, TVITEM, TV_ITEM, *LPTVITEM;
typedef struct tagTVINSERTSTRUCTA { HTREEITEM hParent, hInsertAfter; TVITEMA item; } TVINSERTSTRUCT, *LPTVINSERTSTRUCT;
typedef struct tagNMTREEVIEWA {
    NMHDR hdr;
    UINT action;
    TVITEMA itemOld, itemNew;
    POINT ptDrag;
} NMTREEVIEW, NM_TREEVIEW, *LPNMTREEVIEW;

typedef struct tagTCITEMA {
    UINT mask;
    DWORD dwState, dwStateMask;
    LPSTR pszText;
    int cchTextMax, iImage;
    LPARAM lParam;
} TCITEMA, TCITEM, TC_ITEM, *LPTCITEM;

typedef struct tagTOOLINFOA {
    UINT cbSize, uFlags;
    HWND hwnd;
    UINT_PTR uId;
    RECT rect;
    HINSTANCE hinst;
    LPSTR lpszText;
    LPARAM lParam;
} TOOLINFOA, TOOLINFO, *LPTOOLINFO;

typedef struct tagNMTTDISPINFOA {
    NMHDR hdr;
    LPSTR lpszText;
    char szText[80];
    HINSTANCE hinst;
    UINT uFlags;
    LPARAM lParam;
} NMTTDISPINFO, TOOLTIPTEXT, TOOLTIPTEXTA, *LPNMTTDISPINFO, *LPTOOLTIPTEXT;

typedef struct _charrange { LONG cpMin, cpMax; } CHARRANGE;
typedef struct _textrange { CHARRANGE chrg; LPSTR lpstrText; } TEXTRANGE;
typedef struct _findtextexa { CHARRANGE chrg; LPCSTR lpstrText; CHARRANGE chrgText; } FINDTEXTEX;
typedef struct _findtext { CHARRANGE chrg; LPCSTR lpstrText; } FINDTEXT;
typedef DWORD (*EDITSTREAMCALLBACK)(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG* pcb);
typedef struct _editstream { DWORD_PTR dwCookie; DWORD dwError; EDITSTREAMCALLBACK pfnCallback; } EDITSTREAM;
typedef struct _charformat {
    UINT cbSize;
    DWORD dwMask, dwEffects;
    LONG yHeight, yOffset;
    COLORREF crTextColor;
    BYTE bCharSet, bPitchAndFamily;
    char szFaceName[32];
} CHARFORMAT, CHARFORMATA;
typedef struct _charformat2 {
    UINT cbSize;
    DWORD dwMask, dwEffects;
    LONG yHeight, yOffset;
    COLORREF crTextColor;
    BYTE bCharSet, bPitchAndFamily;
    char szFaceName[32];
    WORD wWeight;
    SHORT sSpacing;
    COLORREF crBackColor;
    LCID lcid;
    DWORD dwReserved;
    SHORT sStyle;
    WORD wKerning;
    BYTE bUnderlineType, bAnimation, bRevAuthor, bReserved1;
} CHARFORMAT2, CHARFORMAT2A;
typedef struct _paraformat {
    UINT cbSize;
    DWORD dwMask;
    WORD wNumbering, wReserved;
    LONG dxStartIndent, dxRightIndent, dxOffset;
    WORD wAlignment;
    SHORT cTabCount;
    LONG rgxTabs[32];
} PARAFORMAT;
typedef struct _msgfilter { NMHDR nmhdr; UINT msg; WPARAM wParam; LPARAM lParam; } MSGFILTER;
typedef struct _selchange { NMHDR nmhdr; CHARRANGE chrg; WORD seltyp; } SELCHANGE;
typedef struct _enlink { NMHDR nmhdr; UINT msg; WPARAM wParam; LPARAM lParam; CHARRANGE chrg; } ENLINK;

typedef struct tagOFNA {
    DWORD lStructSize;
    HWND hwndOwner;
    HINSTANCE hInstance;
    LPCSTR lpstrFilter;
    LPSTR lpstrCustomFilter;
    DWORD nMaxCustFilter, nFilterIndex;
    LPSTR lpstrFile;
    DWORD nMaxFile;
    LPSTR lpstrFileTitle;
    DWORD nMaxFileTitle;
    LPCSTR lpstrInitialDir, lpstrTitle;
    DWORD Flags;
    WORD nFileOffset, nFileExtension;
    LPCSTR lpstrDefExt;
    LPARAM lCustData;
    LPVOID lpfnHook;
    LPCSTR lpTemplateName;
} OPENFILENAMEA, OPENFILENAME, *LPOPENFILENAME;

#define OFN_READONLY 0x00000001
#define OFN_OVERWRITEPROMPT 0x00000002
#define OFN_HIDEREADONLY 0x00000004
#define OFN_NOCHANGEDIR 0x00000008
#define OFN_SHOWHELP 0x00000010
#define OFN_ENABLEHOOK 0x00000020
#define OFN_ENABLETEMPLATE 0x00000040
#define OFN_NOVALIDATE 0x00000100
#define OFN_ALLOWMULTISELECT 0x00000200
#define OFN_EXTENSIONDIFFERENT 0x00000400
#define OFN_PATHMUSTEXIST 0x00000800
#define OFN_FILEMUSTEXIST 0x00001000
#define OFN_CREATEPROMPT 0x00002000
#define OFN_SHAREAWARE 0x00004000
#define OFN_NOREADONLYRETURN 0x00008000
#define OFN_NOTESTFILECREATE 0x00010000
#define OFN_EXPLORER 0x00080000
#define OFN_NODEREFERENCELINKS 0x00100000
#define OFN_LONGNAMES 0x00200000
#define OFN_ENABLESIZING 0x00800000

typedef struct tagCHOOSECOLORA {
    DWORD lStructSize;
    HWND hwndOwner;
    HWND hInstance;
    COLORREF rgbResult;
    COLORREF* lpCustColors;
    DWORD Flags;
    LPARAM lCustData;
    LPVOID lpfnHook;
    LPCSTR lpTemplateName;
} CHOOSECOLOR, *LPCHOOSECOLOR;
#define CC_RGBINIT 0x00000001
#define CC_FULLOPEN 0x00000002

typedef struct tagNONCLIENTMETRICSA {
    UINT cbSize;
    int iBorderWidth, iScrollWidth, iScrollHeight, iCaptionWidth, iCaptionHeight;
    LOGFONTA lfCaptionFont;
    int iSmCaptionWidth, iSmCaptionHeight;
    LOGFONTA lfSmCaptionFont;
    int iMenuWidth, iMenuHeight;
    LOGFONTA lfMenuFont, lfStatusFont, lfMessageFont;
} NONCLIENTMETRICS;

typedef struct _MEMORYSTATUS {
    DWORD dwLength, dwMemoryLoad;
    SIZE_T dwTotalPhys, dwAvailPhys, dwTotalPageFile, dwAvailPageFile, dwTotalVirtual, dwAvailVirtual;
} MEMORYSTATUS, *LPMEMORYSTATUS;

typedef struct _SYSTEM_INFO {
    WORD wProcessorArchitecture, wReserved;
    DWORD dwPageSize;
    LPVOID lpMinimumApplicationAddress, lpMaximumApplicationAddress;
    DWORD_PTR dwActiveProcessorMask;
    DWORD dwNumberOfProcessors, dwProcessorType, dwAllocationGranularity;
    WORD wProcessorLevel, wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;

typedef struct _OSVERSIONINFOA {
    DWORD dwOSVersionInfoSize, dwMajorVersion, dwMinorVersion, dwBuildNumber, dwPlatformId;
    CHAR szCSDVersion[128];
} OSVERSIONINFO, OSVERSIONINFOA, *LPOSVERSIONINFO;

typedef struct _CRITICAL_SECTION { void* impl; } CRITICAL_SECTION, *LPCRITICAL_SECTION;

typedef struct _GUID { DWORD Data1; WORD Data2, Data3; BYTE Data4[8]; } GUID, IID, CLSID;

#endif /* _MFCWX_WINTYPES */
