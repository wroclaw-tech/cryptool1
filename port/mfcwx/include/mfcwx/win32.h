#pragma once

/* Win32 API subset implemented on top of wxWidgets / POSIX. C compatible declarations;
   functions taking or returning C++ types live in the afx headers. */

#include "mfcwx/wintypes.h"
#include "mfcwx/winconst.h"
#include "mfcwx/crtcompat.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAKEWORD(a, b) ((WORD)(((BYTE)((DWORD_PTR)(a) & 0xff)) | ((WORD)((BYTE)((DWORD_PTR)(b) & 0xff))) << 8))
#define MAKELONG(a, b) ((LONG)(((WORD)((DWORD_PTR)(a) & 0xffff)) | ((DWORD)((WORD)((DWORD_PTR)(b) & 0xffff))) << 16))
#define LOWORD(l) ((WORD)((DWORD_PTR)(l) & 0xffff))
#define HIWORD(l) ((WORD)((DWORD_PTR)(l) >> 16))
#define LOBYTE(w) ((BYTE)((DWORD_PTR)(w) & 0xff))
#define HIBYTE(w) ((BYTE)((DWORD_PTR)(w) >> 8))
#define MAKEWPARAM(l, h) ((WPARAM)(DWORD)MAKELONG(l, h))
#define MAKELPARAM(l, h) ((LPARAM)(DWORD)MAKELONG(l, h))
#define MAKELRESULT(l, h) ((LRESULT)(DWORD)MAKELONG(l, h))
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#define GET_WHEEL_DELTA_WPARAM(wParam) ((short)HIWORD(wParam))
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
#define PALETTERGB(r, g, b) (0x02000000 | RGB(r, g, b))
#define GetRValue(rgb) (LOBYTE(rgb))
#define GetGValue(rgb) (LOBYTE(((WORD)(rgb)) >> 8))
#define GetBValue(rgb) (LOBYTE((rgb) >> 16))
#define MAKEINTRESOURCEA(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCE(i) MAKEINTRESOURCEA(i)
#define IS_INTRESOURCE(r) ((((ULONG_PTR)(r)) >> 16) == 0)
#define MAKELANGID(p, s) ((((WORD)(s)) << 10) | (WORD)(p))
#define PRIMARYLANGID(lgid) ((WORD)(lgid) & 0x3ff)
#define SUBLANGID(lgid) ((WORD)(lgid) >> 10)
#define MAKELCID(lgid, srtid) ((DWORD)((((DWORD)((WORD)(srtid))) << 16) | ((DWORD)((WORD)(lgid)))))
#define LANGIDFROMLCID(lcid) ((WORD)(lcid))
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define S_OK ((HRESULT)0L)
#define S_FALSE ((HRESULT)1L)
#define E_FAIL ((HRESULT)0x80004005L)
#define E_NOTIMPL ((HRESULT)0x80004001L)
#define E_OUTOFMEMORY ((HRESULT)0x8007000EL)
#define E_INVALIDARG ((HRESULT)0x80070057L)
#define UNREFERENCED_PARAMETER(P) (void)(P)
#define DBG_UNREFERENCED_PARAMETER(P) (void)(P)
#define DBG_UNREFERENCED_LOCAL_VARIABLE(V) (void)(V)
#define ZeroMemory(d, l) memset((d), 0, (l))
#define SecureZeroMemory(d, l) memset((d), 0, (l))
#define FillMemory(d, l, f) memset((d), (f), (l))
#define CopyMemory(d, s, l) memcpy((d), (s), (l))
#define MoveMemory(d, s, l) memmove((d), (s), (l))
#define RtlZeroMemory ZeroMemory
#define RtlCopyMemory CopyMemory
#define RtlMoveMemory MoveMemory
#define RtlFillMemory FillMemory
#define _countof(a) (sizeof(a) / sizeof((a)[0]))
#define ARRAYSIZE(a) _countof(a)
#undef _T
#undef _TEXT
#undef TEXT
#define TEXT(s) s
#define __TEXT(s) s
#define _T(s) s
#define _TEXT(s) s
#define MAKEPOINTS(l) (*((POINTS*)&(l)))

/* threads / timing / process */
DWORD GetTickCount(void);
ULONGLONG GetTickCount64(void);
void Sleep(DWORD ms);
DWORD GetLastError(void);
void SetLastError(DWORD err);
DWORD GetCurrentThreadId(void);
DWORD GetCurrentProcessId(void);
HANDLE GetCurrentProcess(void);
HANDLE GetCurrentThread(void);
BOOL QueryPerformanceCounter(LARGE_INTEGER* c);
BOOL QueryPerformanceFrequency(LARGE_INTEGER* f);
void GetSystemTime(LPSYSTEMTIME st);
void GetLocalTime(LPSYSTEMTIME st);
void GetSystemTimeAsFileTime(LPFILETIME ft);
BOOL SystemTimeToFileTime(const SYSTEMTIME* st, LPFILETIME ft);
BOOL FileTimeToSystemTime(const FILETIME* ft, LPSYSTEMTIME st);
BOOL FileTimeToLocalFileTime(const FILETIME* ft, LPFILETIME lft);
void GetSystemInfo(LPSYSTEM_INFO si);
BOOL GetVersionEx(LPOSVERSIONINFO vi);
DWORD GetVersion(void);
void GlobalMemoryStatus(LPMEMORYSTATUS ms);
BOOL SetThreadPriority(HANDLE thread, int priority);
int GetThreadPriority(HANDLE thread);
BOOL SetPriorityClass(HANDLE process, DWORD cls);
DWORD WaitForSingleObject(HANDLE h, DWORD ms);
DWORD WaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL waitAll, DWORD ms);
BOOL CloseHandle(HANDLE h);
BOOL GetExitCodeThread(HANDLE thread, LPDWORD code);
BOOL GetExitCodeProcess(HANDLE process, LPDWORD code);
BOOL TerminateThread(HANDLE thread, DWORD code);
BOOL TerminateProcess(HANDLE process, UINT code);
HANDLE CreateThread(LPSECURITY_ATTRIBUTES sa, SIZE_T stack, LPTHREAD_START_ROUTINE fn, LPVOID param, DWORD flags,
                    LPDWORD threadId);
DWORD ResumeThread(HANDLE thread);
DWORD SuspendThread(HANDLE thread);
void ExitThread(DWORD code);
HANDLE CreateEventA(LPSECURITY_ATTRIBUTES sa, BOOL manualReset, BOOL initialState, LPCSTR name);
#define CreateEvent CreateEventA
BOOL SetEvent(HANDLE h);
BOOL ResetEvent(HANDLE h);
BOOL PulseEvent(HANDLE h);
HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES sa, BOOL initialOwner, LPCSTR name);
#define CreateMutex CreateMutexA
BOOL ReleaseMutex(HANDLE h);
HANDLE CreateSemaphoreA(LPSECURITY_ATTRIBUTES sa, LONG initial, LONG max, LPCSTR name);
#define CreateSemaphore CreateSemaphoreA
BOOL ReleaseSemaphore(HANDLE h, LONG count, LPLONG previous);
void InitializeCriticalSection(LPCRITICAL_SECTION cs);
void DeleteCriticalSection(LPCRITICAL_SECTION cs);
void EnterCriticalSection(LPCRITICAL_SECTION cs);
void LeaveCriticalSection(LPCRITICAL_SECTION cs);
BOOL TryEnterCriticalSection(LPCRITICAL_SECTION cs);
LONG InterlockedIncrement(LONG volatile* p);
LONG InterlockedDecrement(LONG volatile* p);
LONG InterlockedExchange(LONG volatile* p, LONG v);
LONG InterlockedExchangeAdd(LONG volatile* p, LONG v);
LONG InterlockedCompareExchange(LONG volatile* p, LONG exchange, LONG comparand);
BOOL CreateProcessA(LPCSTR app, LPSTR cmdLine, LPSECURITY_ATTRIBUTES pa, LPSECURITY_ATTRIBUTES ta, BOOL inherit,
                    DWORD flags, LPVOID env, LPCSTR curDir, LPSTARTUPINFO si, LPPROCESS_INFORMATION pi);
#define CreateProcess CreateProcessA
UINT WinExec(LPCSTR cmdLine, UINT show);
HINSTANCE ShellExecuteA(HWND hwnd, LPCSTR op, LPCSTR file, LPCSTR params, LPCSTR dir, int show);
#define ShellExecute ShellExecuteA
void ExitProcess(UINT code);
DWORD FormatMessageA(DWORD flags, LPCVOID source, DWORD msgId, DWORD langId, LPSTR buffer, DWORD size, va_list* args);
#define FormatMessage FormatMessageA
HLOCAL LocalAlloc(UINT flags, SIZE_T bytes);
HLOCAL LocalFree(HLOCAL mem);
HGLOBAL GlobalAlloc(UINT flags, SIZE_T bytes);
HGLOBAL GlobalFree(HGLOBAL mem);
LPVOID GlobalLock(HGLOBAL mem);
BOOL GlobalUnlock(HGLOBAL mem);
SIZE_T GlobalSize(HGLOBAL mem);
HGLOBAL GlobalReAlloc(HGLOBAL mem, SIZE_T bytes, UINT flags);
DWORD GetEnvironmentVariableA(LPCSTR name, LPSTR buffer, DWORD size);
#define GetEnvironmentVariable GetEnvironmentVariableA
BOOL SetEnvironmentVariableA(LPCSTR name, LPCSTR value);
#define SetEnvironmentVariable SetEnvironmentVariableA
DWORD ExpandEnvironmentStringsA(LPCSTR src, LPSTR dst, DWORD size);
#define ExpandEnvironmentStrings ExpandEnvironmentStringsA
LPSTR GetCommandLineA(void);
#define GetCommandLine GetCommandLineA
UINT SetErrorMode(UINT mode);
void OutputDebugStringA(LPCSTR s);
#define OutputDebugString OutputDebugStringA
void DebugBreak(void);
BOOL Beep(DWORD freq, DWORD duration);
BOOL MessageBeep(UINT type);
int MulDiv(int number, int numerator, int denominator);
BOOL IsBadReadPtr(const void* p, UINT_PTR cb);
BOOL IsBadWritePtr(LPVOID p, UINT_PTR cb);
DWORD GetUserDefaultLangID(void);
LCID GetUserDefaultLCID(void);
LCID GetSystemDefaultLCID(void);
LCID GetThreadLocale(void);
BOOL SetThreadLocale(LCID lcid);
UINT GetACP(void);
UINT GetOEMCP(void);
int MultiByteToWideChar(UINT cp, DWORD flags, LPCSTR src, int srcLen, LPWSTR dst, int dstLen);
int WideCharToMultiByte(UINT cp, DWORD flags, LPCWSTR src, int srcLen, LPSTR dst, int dstLen, LPCSTR defChar,
                        LPBOOL usedDef);
BOOL GetUserNameA(LPSTR buffer, LPDWORD size);
#define GetUserName GetUserNameA
BOOL GetComputerNameA(LPSTR buffer, LPDWORD size);
#define GetComputerName GetComputerNameA

/* modules */
HMODULE LoadLibraryA(LPCSTR name);
#define LoadLibrary LoadLibraryA
HMODULE LoadLibraryExA(LPCSTR name, HANDLE file, DWORD flags);
#define LoadLibraryEx LoadLibraryExA
BOOL FreeLibrary(HMODULE mod);
FARPROC GetProcAddress(HMODULE mod, LPCSTR name);
HMODULE GetModuleHandleA(LPCSTR name);
#define GetModuleHandle GetModuleHandleA
DWORD GetModuleFileNameA(HMODULE mod, LPSTR buffer, DWORD size);
#define GetModuleFileName GetModuleFileNameA

/* files and directories */
HANDLE CreateFileA(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disposition, DWORD flags,
                   HANDLE templ);
#define CreateFile CreateFileA
BOOL ReadFile(HANDLE f, LPVOID buffer, DWORD toRead, LPDWORD read, LPOVERLAPPED ov);
BOOL WriteFile(HANDLE f, LPCVOID buffer, DWORD toWrite, LPDWORD written, LPOVERLAPPED ov);
DWORD SetFilePointer(HANDLE f, LONG distance, PLONG distanceHigh, DWORD method);
BOOL SetEndOfFile(HANDLE f);
DWORD GetFileSize(HANDLE f, LPDWORD high);
BOOL GetFileSizeEx(HANDLE f, PLARGE_INTEGER size);
BOOL FlushFileBuffers(HANDLE f);
BOOL GetFileTime(HANDLE f, LPFILETIME creation, LPFILETIME access, LPFILETIME write);
BOOL DeleteFileA(LPCSTR name);
#define DeleteFile DeleteFileA
BOOL CopyFileA(LPCSTR from, LPCSTR to, BOOL failIfExists);
#define CopyFile CopyFileA
BOOL MoveFileA(LPCSTR from, LPCSTR to);
#define MoveFile MoveFileA
BOOL MoveFileExA(LPCSTR from, LPCSTR to, DWORD flags);
#define MoveFileEx MoveFileExA
#define MOVEFILE_REPLACE_EXISTING 0x00000001
#define MOVEFILE_COPY_ALLOWED 0x00000002
BOOL CreateDirectoryA(LPCSTR path, LPSECURITY_ATTRIBUTES sa);
#define CreateDirectory CreateDirectoryA
BOOL RemoveDirectoryA(LPCSTR path);
#define RemoveDirectory RemoveDirectoryA
BOOL SetCurrentDirectoryA(LPCSTR path);
#define SetCurrentDirectory SetCurrentDirectoryA
DWORD GetCurrentDirectoryA(DWORD size, LPSTR buffer);
#define GetCurrentDirectory GetCurrentDirectoryA
DWORD GetTempPathA(DWORD size, LPSTR buffer);
#define GetTempPath GetTempPathA
UINT GetTempFileNameA(LPCSTR path, LPCSTR prefix, UINT unique, LPSTR buffer);
#define GetTempFileName GetTempFileNameA
DWORD GetFileAttributesA(LPCSTR name);
#define GetFileAttributes GetFileAttributesA
BOOL SetFileAttributesA(LPCSTR name, DWORD attrs);
#define SetFileAttributes SetFileAttributesA
DWORD GetFullPathNameA(LPCSTR name, DWORD size, LPSTR buffer, LPSTR* filePart);
#define GetFullPathName GetFullPathNameA
DWORD GetShortPathNameA(LPCSTR longPath, LPSTR shortPath, DWORD size);
#define GetShortPathName GetShortPathNameA
DWORD GetLongPathNameA(LPCSTR shortPath, LPSTR longPath, DWORD size);
#define GetLongPathName GetLongPathNameA
HANDLE FindFirstFileA(LPCSTR pattern, LPWIN32_FIND_DATA data);
#define FindFirstFile FindFirstFileA
BOOL FindNextFileA(HANDLE find, LPWIN32_FIND_DATA data);
#define FindNextFile FindNextFileA
BOOL FindClose(HANDLE find);
UINT GetWindowsDirectoryA(LPSTR buffer, UINT size);
#define GetWindowsDirectory GetWindowsDirectoryA
UINT GetSystemDirectoryA(LPSTR buffer, UINT size);
#define GetSystemDirectory GetSystemDirectoryA
UINT GetDriveTypeA(LPCSTR root);
#define GetDriveType GetDriveTypeA
BOOL GetDiskFreeSpaceExA(LPCSTR dir, PULARGE_INTEGER freeToCaller, PULARGE_INTEGER total, PULARGE_INTEGER totalFree);
#define GetDiskFreeSpaceEx GetDiskFreeSpaceExA

/* ini files, mapped onto simple INI parsing */
DWORD GetPrivateProfileStringA(LPCSTR app, LPCSTR key, LPCSTR def, LPSTR ret, DWORD size, LPCSTR file);
#define GetPrivateProfileString GetPrivateProfileStringA
UINT GetPrivateProfileIntA(LPCSTR app, LPCSTR key, INT def, LPCSTR file);
#define GetPrivateProfileInt GetPrivateProfileIntA
BOOL WritePrivateProfileStringA(LPCSTR app, LPCSTR key, LPCSTR value, LPCSTR file);
#define WritePrivateProfileString WritePrivateProfileStringA
DWORD GetPrivateProfileSectionA(LPCSTR app, LPSTR ret, DWORD size, LPCSTR file);
#define GetPrivateProfileSection GetPrivateProfileSectionA

/* registry, mapped onto wxConfig */
LONG RegOpenKeyExA(HKEY key, LPCSTR sub, DWORD options, REGSAM sam, PHKEY result);
#define RegOpenKeyEx RegOpenKeyExA
LONG RegOpenKeyA(HKEY key, LPCSTR sub, PHKEY result);
#define RegOpenKey RegOpenKeyA
LONG RegCreateKeyExA(HKEY key, LPCSTR sub, DWORD reserved, LPSTR cls, DWORD options, REGSAM sam,
                     LPSECURITY_ATTRIBUTES sa, PHKEY result, LPDWORD disposition);
#define RegCreateKeyEx RegCreateKeyExA
LONG RegCreateKeyA(HKEY key, LPCSTR sub, PHKEY result);
#define RegCreateKey RegCreateKeyA
LONG RegCloseKey(HKEY key);
LONG RegQueryValueExA(HKEY key, LPCSTR name, LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD size);
#define RegQueryValueEx RegQueryValueExA
LONG RegSetValueExA(HKEY key, LPCSTR name, DWORD reserved, DWORD type, const BYTE* data, DWORD size);
#define RegSetValueEx RegSetValueExA
LONG RegDeleteValueA(HKEY key, LPCSTR name);
#define RegDeleteValue RegDeleteValueA
LONG RegDeleteKeyA(HKEY key, LPCSTR sub);
#define RegDeleteKey RegDeleteKeyA
LONG RegEnumKeyExA(HKEY key, DWORD index, LPSTR name, LPDWORD nameSize, LPDWORD reserved, LPSTR cls, LPDWORD clsSize,
                   PFILETIME lastWrite);
#define RegEnumKeyEx RegEnumKeyExA

/* strings */
int lstrlenA(LPCSTR s);
#define lstrlen lstrlenA
LPSTR lstrcpyA(LPSTR d, LPCSTR s);
#define lstrcpy lstrcpyA
LPSTR lstrcpynA(LPSTR d, LPCSTR s, int n);
#define lstrcpyn lstrcpynA
LPSTR lstrcatA(LPSTR d, LPCSTR s);
#define lstrcat lstrcatA
int lstrcmpA(LPCSTR a, LPCSTR b);
#define lstrcmp lstrcmpA
int lstrcmpiA(LPCSTR a, LPCSTR b);
#define lstrcmpi lstrcmpiA
int wsprintfA(LPSTR buf, LPCSTR fmt, ...);
#define wsprintf wsprintfA
int wvsprintfA(LPSTR buf, LPCSTR fmt, va_list args);
#define wvsprintf wvsprintfA
LPSTR CharUpperA(LPSTR s);
#define CharUpper CharUpperA
LPSTR CharLowerA(LPSTR s);
#define CharLower CharLowerA
DWORD CharUpperBuffA(LPSTR s, DWORD len);
#define CharUpperBuff CharUpperBuffA
DWORD CharLowerBuffA(LPSTR s, DWORD len);
#define CharLowerBuff CharLowerBuffA
LPSTR CharNextA(LPCSTR s);
#define CharNext CharNextA
LPSTR CharPrevA(LPCSTR start, LPCSTR s);
#define CharPrev CharPrevA
BOOL IsCharAlphaA(CHAR c);
#define IsCharAlpha IsCharAlphaA
BOOL IsCharAlphaNumericA(CHAR c);
#define IsCharAlphaNumeric IsCharAlphaNumericA
BOOL IsCharUpperA(CHAR c);
#define IsCharUpper IsCharUpperA
BOOL IsCharLowerA(CHAR c);
#define IsCharLower IsCharLowerA
BOOL IsDBCSLeadByte(BYTE c);
BOOL OemToCharA(LPCSTR src, LPSTR dst);
#define OemToChar OemToCharA
BOOL CharToOemA(LPCSTR src, LPSTR dst);
#define CharToOem CharToOemA

#ifdef __cplusplus
}
#endif
