#pragma once

#include <string>

// The ANSI code page plays the role of the Windows system code page: CString, char* APIs and
// documents hold bytes in this encoding; mfcwx converts at every boundary to the toolkit.
namespace mfcwx {

void SetAnsiCodePage(int cp);
int GetAnsiCodePage();

wchar_t AnsiToUnicode(unsigned char ch);
// Returns false if ch has no representation; out is set to '?'.
bool UnicodeToAnsi(wchar_t ch, char& out);

std::string AnsiToUtf8(const char* s, int length = -1);
std::string Utf8ToAnsi(const char* utf8);

// A path as used by the Windows code (ANSI bytes, '\\' or '/' separators) to a native UTF-8 path.
std::string NativePath(const char* path);
// A native UTF-8 path to the form the Windows code expects.
std::string AppPath(const char* nativePath);

} // namespace mfcwx
