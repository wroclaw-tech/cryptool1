#pragma once

// Internal contracts of the non-GUI runtime (kernel objects, paths, errors).

#include "mfcwx/win32.h"

#include <functional>
#include <string>

namespace mfcwx {

// Thrown by ExitThread/AfxEndThread and caught by the thread trampoline.
struct ThreadExit {
    DWORD code;
    bool deleteThread;
};

// Creates a kernel thread object running body (now, or on ResumeThread with CREATE_SUSPENDED).
// The thread is signaled after body returns; body's result is the exit code.
HANDLE StartThread(std::function<DWORD()> body, DWORD flags, size_t stackSize, DWORD* threadId);
bool IsProcessMainThread();

// File kernel objects wrap a POSIX descriptor. With owns == false CloseHandle leaves fd open.
HANDLE HandleFromFd(int fd, const std::string& nativePath, bool owns = true);
int FdFromHandle(HANDLE h);

// Native path for a path given by the application, resolving letter case per component
// when the exact path does not exist (Windows paths are case-insensitive).
std::string FsPath(const char* appPath);
// Absolute, normalized native path ("." and ".." removed); relative paths are based on the cwd.
std::string AbsoluteNativePath(const char* appPath);
// Absolute path in the application's form ('\\' separators, ANSI).
std::string FullAppPath(const char* appPath);

std::string ExecutablePath();
// LoadLibrary/GetModuleHandle succeed for this DLL base name (case-insensitive) with a pseudo module
// whose GetProcAddress returns NULL; used for libraries built into the toolkit (Scintilla).
void RegisterBuiltinModule(const char* baseName);
std::string TempDirectory();
std::string UserConfigDirectory();
void SetCommandLineOverride(const char* commandLine);

DWORD ErrnoToWin32(int err);
void SetLastErrorFromErrno(int err);

// FILETIME counts 100 ns intervals since 1601-01-01 UTC.
constexpr LONGLONG kFileTimeUnixEpoch = 116444736000000000LL;
inline FILETIME MakeFileTime(time_t sec, long nsec = 0) {
    ULONGLONG v = static_cast<ULONGLONG>(static_cast<LONGLONG>(sec) * 10000000LL + nsec / 100 + kFileTimeUnixEpoch);
    FILETIME ft;
    ft.dwLowDateTime = static_cast<DWORD>(v);
    ft.dwHighDateTime = static_cast<DWORD>(v >> 32);
    return ft;
}
inline LONGLONG FileTimeTicks(const FILETIME& ft) {
    return static_cast<LONGLONG>((static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime);
}
inline time_t FileTimeToUnix(const FILETIME& ft) {
    LONGLONG t = FileTimeTicks(ft) - kFileTimeUnixEpoch;
    return static_cast<time_t>(t >= 0 ? t / 10000000LL : -((-t + 9999999LL) / 10000000LL));
}

// Case mapping and classification of single ANSI characters in the current code page.
char AnsiToUpper(char ch);
char AnsiToLower(char ch);
bool AnsiIsAlpha(char ch);
bool AnsiIsUpper(char ch);
bool AnsiIsLower(char ch);

} // namespace mfcwx
