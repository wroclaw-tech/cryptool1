#pragma once

/* Microsoft C runtime extensions on top of the POSIX C library. C compatible. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/timeb.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _MFCWX_CRTCOMPAT
#define _MFCWX_CRTCOMPAT

#define _CRT_SECURE_NO_WARNINGS
#define _MAX_PATH 260
#define _MAX_DRIVE 3
#define _MAX_DIR 256
#define _MAX_FNAME 256
#define _MAX_EXT 256

#define _O_RDONLY O_RDONLY
#define _O_WRONLY O_WRONLY
#define _O_RDWR O_RDWR
#define _O_APPEND O_APPEND
#define _O_CREAT O_CREAT
#define _O_TRUNC O_TRUNC
#define _O_EXCL O_EXCL
#define _O_BINARY 0
#define _O_TEXT 0
#define O_BINARY 0
#define O_TEXT 0
#define _S_IREAD S_IRUSR
#define _S_IWRITE S_IWUSR
#define _S_IFDIR S_IFDIR
#define _S_IFREG S_IFREG
#define _S_IFMT S_IFMT
#define _SH_DENYNO 0x40
#define _SH_DENYRW 0x10
#define _SH_DENYWR 0x20
#define _SH_DENYRD 0x30

#define _stat stat
#define _stati64 stat
#define _fstat fstat
#define _fstati64 fstat
#define _off_t off_t
#define _fsize_t unsigned long
#define _timeb timeb
#define _ftime(t) ftime(t)
#define _fseeki64 fseeko
#define _ftelli64 ftello
#define _memicmp mfcwx_memicmp
#define memicmp mfcwx_memicmp
#define __time64_t time_t
#define _time64 time
#define _localtime64 localtime
#define _gmtime64 gmtime
#define _mktime64 mktime
#define _ctime64 ctime
#define _difftime64 difftime

#define _open mfcwx_open
#define _tell(fd) lseek((fd), 0, SEEK_CUR)
#define _telli64(fd) lseek((fd), 0, SEEK_CUR)
#define _lseeki64 _lseek
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _strcmpi strcasecmp
#define stricmp strcasecmp
#define strnicmp strncasecmp
#define strcmpi strcasecmp
#define _mbscmp(a, b) strcmp((const char*)(a), (const char*)(b))
#define _mbsicmp(a, b) strcasecmp((const char*)(a), (const char*)(b))
#define _mbslen(a) strlen((const char*)(a))
#define _mbschr(a, c) ((unsigned char*)strchr((const char*)(a), (c)))
#define _mbsrchr(a, c) ((unsigned char*)strrchr((const char*)(a), (c)))
#define _mbsstr(a, b) ((unsigned char*)strstr((const char*)(a), (const char*)(b)))
#define _mbsinc(p) ((p) + 1)
#define _mbsdec(s, p) ((p) - 1)
#define _ismbblead(c) 0
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#define _vscprintf(fmt, args) vsnprintf(NULL, 0, (fmt), (args))
#define _scprintf(...) snprintf(NULL, 0, __VA_ARGS__)
#define _snscanf sscanf
#define _strtoi64 strtoll
#define _strtoui64 strtoull
#define _atoi64 atoll
#define _wtoi(s) ((int)wcstol((s), NULL, 10))
#define _isnan isnan
#define _finite isfinite
#define _hypot hypot
#define _copysign copysign
#define _chgsign(x) (-(x))
#define _logb logb
#define _nextafter nextafter
#define _j0 j0
#define _j1 j1
#define _y0 y0
#define _y1 y1
#define _fpclass(x) fpclassify(x)
#define _CrtSetDbgFlag(x) 0
#define _CrtDumpMemoryLeaks() 0
#define _CrtCheckMemory() 1
#define _CrtSetBreakAlloc(x) 0
#define _CrtSetReportMode(a, b) 0
#define _CRTDBG_ALLOC_MEM_DF 0x01
#define _CRTDBG_LEAK_CHECK_DF 0x20
#define _CRTDBG_REPORT_FLAG (-1)
#define _CRT_WARN 0
#define _CRT_ERROR 1
#define _CRT_ASSERT 2
#define _CRTDBG_MODE_DEBUG 0x2
#define _ASSERTE(expr) ((void)0)
#define _ASSERT(expr) ((void)0)
#define _RPT0(rptno, msg) ((void)0)
#define _RPT1(rptno, msg, a1) ((void)0)
#define _RPT2(rptno, msg, a1, a2) ((void)0)
#define _RPT3(rptno, msg, a1, a2, a3) ((void)0)
#define _RPTF0(rptno, msg) ((void)0)
#define _RPTF1(rptno, msg, a1) ((void)0)
#define _alloca alloca
#define _msize(p) mfcwx_msize(p)
#define _heapchk() (-2)
#define _HEAPOK (-2)
#define __min(a, b) (((a) < (b)) ? (a) : (b))
#define __max(a, b) (((a) > (b)) ? (a) : (b))
#define _rotl(v, s) mfcwx_rotl((v), (s))
#define _rotr(v, s) mfcwx_rotr((v), (s))
#define _lrotl(v, s) mfcwx_rotl((v), (s))
#define _lrotr(v, s) mfcwx_rotr((v), (s))
#define _byteswap_ulong(x) __builtin_bswap32(x)
#define _byteswap_ushort(x) __builtin_bswap16(x)
#define _byteswap_uint64(x) __builtin_bswap64(x)
#define _getch getchar
#define _getche getchar
#define getch getchar
#define _putch putchar
#define _kbhit() 0
#define kbhit() 0
#define _flushall() fflush(NULL)
#define _fcloseall() 0
#define _fsopen(name, mode, sh) mfcwx_fopen((name), (mode))
#define _wfopen(name, mode) NULL
#define _set_errno(e) (errno = (e), 0)
#define _get_errno(p) (*(p) = errno, 0)
#define _beginthread mfcwx_beginthread
#define _beginthreadex mfcwx_beginthreadex
#define _endthread() mfcwx_endthread(0)
#define _endthreadex(c) mfcwx_endthread(c)
#define _sleep(ms) mfcwx_sleep_ms(ms)
#define _clearfp() 0
#define _controlfp(a, b) 0
#define _control87(a, b) 0
#define _MCW_EM 0x0008001f
#define _EM_INVALID 0x00000010
#define _EM_ZERODIVIDE 0x00000008
#define _EM_OVERFLOW 0x00000004
#define _EM_UNDERFLOW 0x00000002
#define _EM_INEXACT 0x00000001
#define _EM_DENORMAL 0x00080000
#define _PC_24 0x00020000
#define _PC_53 0x00010000
#define _PC_64 0x00000000
#define _MCW_PC 0x00030000

#define _tcslen strlen
#define _tcscpy strcpy
#define _tcsncpy strncpy
#define _tcscat strcat
#define _tcsncat strncat
#define _tcscmp strcmp
#define _tcsncmp strncmp
#define _tcsicmp strcasecmp
#define _tcsnicmp strncasecmp
#define _tcschr strchr
#define _tcsrchr strrchr
#define _tcsstr strstr
#define _tcsspn strspn
#define _tcscspn strcspn
#define _tcspbrk strpbrk
#define _tcstok strtok
#define _tcsdup strdup
#define _tcsupr _strupr
#define _tcslwr _strlwr
#define _tcsrev _strrev
#define _tcstol strtol
#define _tcstoul strtoul
#define _tcstod strtod
#define _tcsftime strftime
#define _tcsinc(p) ((p) + 1)
#define _tcsdec(s, p) ((p) - 1)
#define _tcsnextc(p) ((unsigned int)(unsigned char)*(p))
#define _tclen(p) 1
#define _tccpy(d, s) (*(d) = *(s))
#define _ttoi atoi
#define _ttol atol
#define _ttof atof
#define _tstoi atoi
#define _tstof atof
#define _ttoi64 atoll
#define _itot _itoa
#define _ltot _ltoa
#define _ultot _ultoa
#define _stprintf sprintf
#define _sntprintf snprintf
#define _vstprintf vsprintf
#define _vsntprintf vsnprintf
#define _tprintf printf
#define _ftprintf fprintf
#define _stscanf sscanf
#define _tfopen mfcwx_fopen
#define _tremove mfcwx_unlink
#define _trename mfcwx_rename
#define _tunlink mfcwx_unlink
#define _taccess mfcwx_access
#define _tmkdir _mkdir
#define _tgetcwd getcwd
#define _tchdir mfcwx_chdir
#define _tsplitpath _splitpath
#define _tmakepath _makepath
#define _tfullpath _fullpath
#define _tsetlocale setlocale
#define _tgetenv getenv
#define _tsystem system
#define _istalpha isalpha
#define _istdigit isdigit
#define _istxdigit isxdigit
#define _istspace isspace
#define _istalnum isalnum
#define _istupper isupper
#define _istlower islower
#define _istprint isprint
#define _istpunct ispunct
#define _istcntrl iscntrl
#define _istgraph isgraph
#define _totupper toupper
#define _totlower tolower
#define _tcsclen strlen
#define _tcsnccpy strncpy
#define _tcsncpy_s(d, n, s, c) strncpy_s((d), (n), (s), (c))
#define _tcscpy_s strcpy_s
#define _tcscat_s strcat_s
#define _stprintf_s sprintf_s
#define _tmain main
#define _tWinMain WinMain

int mfcwx_open(const char* name, int flags, ...);
int mfcwx_memicmp(const void* a, const void* b, size_t n);
int mfcwx_unlink(const char* name);
int mfcwx_rmdir(const char* name);
int mfcwx_chdir(const char* name);
int mfcwx_access(const char* name, int mode);
int mfcwx_chmod(const char* name, int mode);
int mfcwx_rename(const char* from, const char* to);
FILE* mfcwx_fopen(const char* name, const char* mode);
size_t mfcwx_msize(void* p);
unsigned int mfcwx_rotl(unsigned int v, int s);
unsigned int mfcwx_rotr(unsigned int v, int s);
void mfcwx_sleep_ms(unsigned long ms);
uintptr_t mfcwx_beginthread(void (*fn)(void*), unsigned stack, void* arg);
uintptr_t mfcwx_beginthreadex(void* security, unsigned stack, unsigned (*fn)(void*), void* arg, unsigned flags,
                              unsigned* threadId);
void mfcwx_endthread(unsigned code);
void mfcwx_normalize_path(char* path);

int _mkdir(const char* path);

static inline int _close(int fd) { return close(fd); }
static inline int _read(int fd, void* buf, unsigned int n) { return (int)read(fd, buf, n); }
static inline int _write(int fd, const void* buf, unsigned int n) { return (int)write(fd, buf, n); }
static inline long _lseek(int fd, long off, int whence) { return (long)lseek(fd, off, whence); }
static inline int _dup(int fd) { return dup(fd); }
static inline int _dup2(int a, int b) { return dup2(a, b); }
static inline int _fileno(FILE* f) { return fileno(f); }
static inline FILE* _fdopen(int fd, const char* mode) { return fdopen(fd, mode); }
static inline int _unlink(const char* name) { return mfcwx_unlink(name); }
static inline int _rmdir(const char* name) { return mfcwx_rmdir(name); }
static inline int _chdir(const char* name) { return mfcwx_chdir(name); }
static inline char* _getcwd(char* buf, int size) { return getcwd(buf, (size_t)size); }
static inline int _access(const char* name, int mode) { return mfcwx_access(name, mode); }
static inline int _chmod(const char* name, int mode) { return mfcwx_chmod(name, mode); }
static inline int _getpid(void) { return (int)getpid(); }
static inline int _isatty(int fd) { return isatty(fd); }
static inline int _umask(int mask) { return (int)umask((mode_t)mask); }
static inline char* _strdup(const char* s) { return strdup(s); }

char* _strupr(char* s);
char* _strlwr(char* s);
char* _strrev(char* s);
char* strupr(char* s);
char* strlwr(char* s);
char* strrev(char* s);
char* _strset(char* s, int c);
char* _strnset(char* s, int c, size_t n);
char* _itoa(int value, char* buf, int radix);
char* _ltoa(long value, char* buf, int radix);
char* _ultoa(unsigned long value, char* buf, int radix);
char* _i64toa(long long value, char* buf, int radix);
char* _ui64toa(unsigned long long value, char* buf, int radix);
char* itoa(int value, char* buf, int radix);
char* ltoa(long value, char* buf, int radix);
char* ultoa(unsigned long value, char* buf, int radix);
char* _gcvt(double value, int digits, char* buf);
char* gcvt(double value, int digits, char* buf);
char* _ecvt(double value, int digits, int* dec, int* sign);
char* _fcvt(double value, int digits, int* dec, int* sign);
long _filelength(int fd);
long filelength(int fd);
long long _filelengthi64(int fd);
int _chsize(int fd, long size);
int _eof(int fd);
int _setmode(int fd, int mode);
void _splitpath(const char* path, char* drive, char* dir, char* fname, char* ext);
void _makepath(char* path, const char* drive, const char* dir, const char* fname, const char* ext);
char* _fullpath(char* abs, const char* rel, size_t size);
int _getdrive(void);
int _chdrive(int drive);
char* _mktemp(char* templ);
char* _tempnam(const char* dir, const char* prefix);
int _vsnprintf_s(char* buf, size_t size, size_t count, const char* fmt, va_list args);
int sprintf_s(char* buf, size_t size, const char* fmt, ...);
int _snprintf_s(char* buf, size_t size, size_t count, const char* fmt, ...);
int vsprintf_s(char* buf, size_t size, const char* fmt, va_list args);
int strcpy_s(char* dst, size_t size, const char* src);
int strncpy_s(char* dst, size_t size, const char* src, size_t count);
int strcat_s(char* dst, size_t size, const char* src);
int strncat_s(char* dst, size_t size, const char* src, size_t count);
char* strtok_s(char* s, const char* delim, char** ctx);
int _strupr_s(char* s, size_t size);
int _strlwr_s(char* s, size_t size);
int _itoa_s(int value, char* buf, size_t size, int radix);
int _ltoa_s(long value, char* buf, size_t size, int radix);
int _i64toa_s(long long value, char* buf, size_t size, int radix);
int fopen_s(FILE** f, const char* name, const char* mode);
int memcpy_s(void* dst, size_t dstSize, const void* src, size_t count);
int memmove_s(void* dst, size_t dstSize, const void* src, size_t count);
int localtime_s(struct tm* out, const time_t* t);
int gmtime_s(struct tm* out, const time_t* t);
int ctime_s(char* buf, size_t size, const time_t* t);
int asctime_s(char* buf, size_t size, const struct tm* t);
int _dupenv_s(char** buf, size_t* len, const char* name);
char* _strdate(char* buf);
char* _strtime(char* buf);
void _tzset(void);

struct _finddata_t {
    unsigned attrib;
    time_t time_create;
    time_t time_access;
    time_t time_write;
    unsigned long size;
    char name[260];
};
#define _finddatai64_t _finddata_t
#define _finddata32_t _finddata_t
#define _A_NORMAL 0x00
#define _A_RDONLY 0x01
#define _A_HIDDEN 0x02
#define _A_SYSTEM 0x04
#define _A_SUBDIR 0x10
#define _A_ARCH 0x20
intptr_t _findfirst(const char* pattern, struct _finddata_t* data);
int _findnext(intptr_t handle, struct _finddata_t* data);
int _findclose(intptr_t handle);
#define _findfirsti64 _findfirst
#define _findnexti64 _findnext

#endif /* _MFCWX_CRTCOMPAT */

#ifdef __cplusplus
}
#endif
