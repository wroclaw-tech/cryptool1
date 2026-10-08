// Tests of the non-GUI runtime of mfcwx (paths, files, archives, profiles, registry, threads,
// processes, CRT and Win32 helpers).

#include <wx/init.h>

#include "afxmt.h"
#include "afxole.h"
#include "afxwin.h"
#include "process.h"
#include "runtime.h"

#include <atomic>
#include <string>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond)                                                                                  \
    do {                                                                                             \
        ++g_checks;                                                                                  \
        if (!(cond)) {                                                                               \
            ++g_failures;                                                                            \
            fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);                        \
        }                                                                                            \
    } while (0)

#define CHECK_EQ_STR(a, b)                                                                           \
    do {                                                                                             \
        ++g_checks;                                                                                  \
        std::string va_ = (a), vb_ = (b);                                                            \
        if (va_ != vb_) {                                                                            \
            ++g_failures;                                                                            \
            fprintf(stderr, "FAILED %s:%d: %s == %s (\"%s\" vs \"%s\")\n", __FILE__, __LINE__, #a, #b, \
                    va_.c_str(), vb_.c_str());                                                       \
        }                                                                                            \
    } while (0)

std::string g_root;    // native scratch directory
std::string g_appRoot; // the same in application form

std::string Native(const std::string& name) { return g_root + "/" + name; }
std::string App(const std::string& name) { return g_appRoot + "\\" + name; }

void WriteRaw(const std::string& native, const std::string& data) {
    FILE* f = ::mfcwx_fopen(native.c_str(), "wb");
    if (f) {
        fwrite(data.data(), 1, data.size(), f);
        fclose(f);
    }
}

std::string ReadRaw(const std::string& native) {
    std::string data;
    FILE* f = ::mfcwx_fopen(native.c_str(), "rb");
    if (!f)
        return data;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0)
        data.append(buf, n);
    fclose(f);
    return data;
}

void TestPaths() {
    CHECK_EQ_STR(mfcwx::NativePath("C:\\dir\\sub\\file.txt"), "/dir/sub/file.txt");
    CHECK_EQ_STR(mfcwx::NativePath("rel\\file.txt"), "rel/file.txt");
    CHECK_EQ_STR(mfcwx::AppPath("/Users/x/a.txt"), "\\Users\\x\\a.txt");
    mfcwx::SetAnsiCodePage(1250);
    std::string utf8 = "/tmp/za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87";
    std::string app = mfcwx::AppPath(utf8.c_str());
    CHECK_EQ_STR(app, "\\tmp\\za\xBF\xF3\xB3\xE6");
    CHECK_EQ_STR(mfcwx::NativePath(app.c_str()), utf8);
    mfcwx::SetAnsiCodePage(1252);

    char drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
    _splitpath("C:\\dir\\sub\\file.tar.gz", drive, dir, fname, ext);
    CHECK_EQ_STR(drive, "C:");
    CHECK_EQ_STR(dir, "\\dir\\sub\\");
    CHECK_EQ_STR(fname, "file.tar");
    CHECK_EQ_STR(ext, ".gz");
    _splitpath("\\Users\\x\\noext", drive, dir, fname, ext);
    CHECK_EQ_STR(drive, "");
    CHECK_EQ_STR(dir, "\\Users\\x\\");
    CHECK_EQ_STR(fname, "noext");
    CHECK_EQ_STR(ext, "");
    _splitpath("plain.txt", drive, dir, fname, ext);
    CHECK_EQ_STR(dir, "");
    CHECK_EQ_STR(fname, "plain");
    char path[_MAX_PATH];
    _makepath(path, "", "\\Users\\x", "name", "txt");
    CHECK_EQ_STR(path, "\\Users\\x\\name.txt");

    char cwd[1024];
    CHECK(_getcwd(cwd, sizeof cwd) != nullptr);
    CHECK(cwd[0] == '\\');
    CHECK(strchr(cwd, '/') == nullptr);
    DWORD n = GetCurrentDirectory(sizeof cwd, cwd);
    CHECK(n == strlen(cwd));
    CHECK(GetCurrentDirectory(2, cwd) > 2);

    char full[1024];
    char* filePart = nullptr;
    GetFullPathName("a\\..\\b\\c.txt", sizeof full, full, &filePart);
    char expected[1024];
    _getcwd(expected, sizeof expected);
    CHECK_EQ_STR(full, std::string(expected) + "\\b\\c.txt");
    CHECK(filePart && strcmp(filePart, "c.txt") == 0);
    char* fp = _fullpath(nullptr, "x\\y", 0);
    CHECK_EQ_STR(fp, std::string(expected) + "\\x\\y");
    free(fp);

    char temp[1024];
    DWORD t = GetTempPath(sizeof temp, temp);
    CHECK(t > 0 && temp[t - 1] == '\\');
    struct stat st;
    CHECK(stat(mfcwx::NativePath(temp).c_str(), &st) == 0 && S_ISDIR(st.st_mode));

    mkdir(Native("CaseDir").c_str(), 0755);
    WriteRaw(Native("CaseDir/MixedCase.TXT"), "x");
    CHECK(GetFileAttributes(App("casedir\\mixedcase.txt").c_str()) != INVALID_FILE_ATTRIBUTES);
    CHECK(_access(App("CASEDIR\\MIXEDCASE.txt").c_str(), 0) == 0);
    CHECK((GetFileAttributes(App("CaseDir").c_str()) & FILE_ATTRIBUTE_DIRECTORY) != 0);

    char exe[1024];
    DWORD len = GetModuleFileName(nullptr, exe, sizeof exe);
    CHECK(len > 0 && exe[0] == '\\');
    CHECK(strstr(exe, "mfcwx_runtime_tests") != nullptr);
    CHECK(strncmp(GetCommandLine(), "\\", 1) == 0 || GetCommandLine()[0] == '"');
}

void TestFiles() {
    std::string name = App("data.bin");
    {
        CFile f;
        CFileException e;
        CHECK(f.Open(name.c_str(), CFile::modeCreate | CFile::modeWrite, &e));
        const char data[] = "0123456789";
        f.Write(data, 10);
        CHECK(f.GetLength() == 10);
        CHECK(f.GetFileName() == "data.bin");
        CHECK(f.GetFileTitle() == "data");
        CHECK(f.GetFilePath() == name.c_str());
        f.Close();
    }
    {
        CFile f(name.c_str(), CFile::modeRead);
        char buf[16] = {};
        f.Seek(3, CFile::begin);
        CHECK(f.Read(buf, 4) == 4);
        CHECK_EQ_STR(buf, "3456");
        CHECK(f.GetPosition() == 7);
        CHECK(f.Read(buf, 16) == 3);
        CFileStatus status;
        CHECK(f.GetStatus(status));
        CHECK(status.m_size == 10);
    }
    {
        CFile f(name.c_str(), CFile::modeReadWrite | CFile::modeCreate | CFile::modeNoTruncate);
        CHECK(f.GetLength() == 10);
        f.SeekToEnd();
        f.Write("AB", 2);
        f.SetLength(11);
    }
    CHECK_EQ_STR(ReadRaw(Native("data.bin")), "0123456789A");

    CFile missing;
    CFileException e;
    CHECK(!missing.Open(App("missing.bin").c_str(), CFile::modeRead, &e));
    CHECK(e.m_cause == CFileException::fileNotFound);
    bool thrown = false;
    try {
        CFile f(App("missing.bin").c_str(), CFile::modeRead);
    } catch (CFileException* ex) {
        thrown = ex->m_cause == CFileException::fileNotFound;
        char msg[256];
        ex->GetErrorMessage(msg, sizeof msg);
        CHECK(strstr(msg, "was not found") != nullptr);
        ex->Delete();
    }
    CHECK(thrown);

    CFileStatus st;
    CHECK(CFile::GetStatus(name.c_str(), st));
    CHECK(st.m_size == 11);
    CFile::Rename(name.c_str(), App("renamed.bin").c_str());
    CHECK(!CFile::GetStatus(name.c_str(), st));
    CFile::Remove(App("renamed.bin").c_str());
    CHECK(!CFile::GetStatus(App("renamed.bin").c_str(), st));
    thrown = false;
    try {
        CFile::Remove(App("renamed.bin").c_str());
    } catch (CFileException* ex) {
        thrown = true;
        ex->Delete();
    }
    CHECK(thrown);

    WriteRaw(Native("crlf.txt"), "first line\r\nsecond\r\n\r\nlast");
    {
        CStdioFile f;
        CHECK(f.Open(App("crlf.txt").c_str(), CFile::modeRead));
        CString line;
        CHECK(f.ReadString(line) && line == "first line");
        CHECK(f.ReadString(line) && line == "second");
        CHECK(f.ReadString(line) && line.IsEmpty());
        CHECK(f.ReadString(line) && line == "last");
        CHECK(!f.ReadString(line));
        f.SeekToBegin();
        char buf[64];
        CHECK(f.ReadString(buf, sizeof buf) != nullptr);
        CHECK_EQ_STR(buf, "first line\n");
    }
    {
        CStdioFile f(App("out.txt").c_str(), CFile::modeCreate | CFile::modeWrite | CFile::typeText);
        f.WriteString("alpha\n");
        f.WriteString("beta");
    }
    CHECK_EQ_STR(ReadRaw(Native("out.txt")), "alpha\nbeta");

    CMemFile mem;
    mem.Write("hello", 5);
    mem.Write(" world", 6);
    CHECK(mem.GetLength() == 11);
    mem.SeekToBegin();
    char buf[16] = {};
    CHECK(mem.Read(buf, 5) == 5);
    CHECK_EQ_STR(buf, "hello");
    mem.SetLength(3);
    CHECK(mem.GetLength() == 3 && mem.GetPosition() == 3);

    HANDLE h = CreateFile(App("api.bin").c_str(), GENERIC_WRITE | GENERIC_READ, 0, nullptr, CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL, nullptr);
    CHECK(h != INVALID_HANDLE_VALUE);
    DWORD written = 0;
    CHECK(WriteFile(h, "abcdef", 6, &written, nullptr) && written == 6);
    CHECK(SetFilePointer(h, 2, nullptr, FILE_BEGIN) == 2);
    char rb[8] = {};
    DWORD read = 0;
    CHECK(ReadFile(h, rb, 3, &read, nullptr) && read == 3);
    CHECK_EQ_STR(rb, "cde");
    CHECK(GetFileSize(h, nullptr) == 6);
    FILETIME ftWrite;
    CHECK(GetFileTime(h, nullptr, nullptr, &ftWrite));
    CTime written_at(ftWrite);
    CHECK(llabs(static_cast<long long>(written_at.GetTime() - time(nullptr))) < 60);
    CHECK(CloseHandle(h));
    CHECK(!CloseHandle(h));
    CHECK(CreateFile(App("api.bin").c_str(), GENERIC_READ, 0, nullptr, CREATE_NEW, 0, nullptr) == INVALID_HANDLE_VALUE);
    CHECK(GetLastError() == 80);

    CHECK(CopyFile(App("api.bin").c_str(), App("copy.bin").c_str(), TRUE));
    CHECK(!CopyFile(App("api.bin").c_str(), App("copy.bin").c_str(), TRUE));
    CHECK(!MoveFile(App("api.bin").c_str(), App("copy.bin").c_str()));
    CHECK(MoveFileEx(App("api.bin").c_str(), App("copy.bin").c_str(), MOVEFILE_REPLACE_EXISTING));
    CHECK(GetFileAttributes(App("api.bin").c_str()) == INVALID_FILE_ATTRIBUTES);
    CHECK(GetLastError() == ERROR_FILE_NOT_FOUND);
    CHECK(SetFileAttributes(App("copy.bin").c_str(), FILE_ATTRIBUTE_READONLY));
    CHECK(GetFileAttributes(App("copy.bin").c_str()) & FILE_ATTRIBUTE_READONLY);
    CHECK(SetFileAttributes(App("copy.bin").c_str(), FILE_ATTRIBUTE_NORMAL));
    CHECK(DeleteFile(App("copy.bin").c_str()));
    CHECK(CreateDirectory(App("newdir").c_str(), nullptr));
    CHECK(!CreateDirectory(App("newdir").c_str(), nullptr) && GetLastError() == ERROR_ALREADY_EXISTS);
    CHECK(RemoveDirectory(App("newdir").c_str()));

    char tmpName[MAX_PATH];
    UINT u = GetTempFileName(g_appRoot.c_str(), "tst", 0, tmpName);
    CHECK(u != 0);
    CHECK(strncmp(tmpName, g_appRoot.c_str(), g_appRoot.size()) == 0);
    CHECK(GetFileAttributes(tmpName) != INVALID_FILE_ATTRIBUTES);
    DeleteFile(tmpName);

    FILE* f = fopen(App("fopen.txt").c_str(), "wt");
    CHECK(f != nullptr);
    if (f) {
        fputs("text", f);
        fclose(f);
    }
    CHECK(rename(App("fopen.txt").c_str(), App("fopen2.txt").c_str()) == 0);
    CHECK(remove(App("fopen2.txt").c_str()) == 0);
}

void TestFind() {
    mkdir(Native("find").c_str(), 0755);
    WriteRaw(Native("find/a.txt"), "1");
    WriteRaw(Native("find/b.TXT"), "22");
    WriteRaw(Native("find/c.dat"), "333");
    WriteRaw(Native("find/noext"), "4");
    struct _finddata_t fd;
    intptr_t h = _findfirst(App("find\\*.txt").c_str(), &fd);
    CHECK(h != -1);
    std::vector<std::string> names;
    if (h != -1) {
        do
            names.push_back(fd.name);
        while (_findnext(h, &fd) == 0);
        CHECK(_findclose(h) == 0);
    }
    CHECK(names.size() == 2);
    CHECK(names.size() == 2 && names[0] == "a.txt" && names[1] == "b.TXT");
    CHECK(_findfirst(App("find\\*.none").c_str(), &fd) == -1);

    WIN32_FIND_DATA wfd;
    HANDLE fh = FindFirstFile(App("find\\*.*").c_str(), &wfd);
    CHECK(fh != INVALID_HANDLE_VALUE);
    int count = 0;
    bool dots = false;
    if (fh != INVALID_HANDLE_VALUE) {
        do {
            ++count;
            if (strcmp(wfd.cFileName, ".") == 0)
                dots = (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            if (strcmp(wfd.cFileName, "c.dat") == 0)
                CHECK(wfd.nFileSizeLow == 3);
        } while (FindNextFile(fh, &wfd));
        CHECK(GetLastError() == ERROR_NO_MORE_FILES);
        CHECK(FindClose(fh));
    }
    CHECK(count == 6);
    CHECK(dots);
    fh = FindFirstFile(App("find\\*.").c_str(), &wfd);
    CHECK(fh != INVALID_HANDLE_VALUE);
    std::vector<std::string> noExt;
    if (fh != INVALID_HANDLE_VALUE) {
        do
            noExt.push_back(wfd.cFileName);
        while (FindNextFile(fh, &wfd));
        FindClose(fh);
    }
    CHECK(noExt.size() == 3 && noExt[0] == "." && noExt[1] == ".." && noExt[2] == "noext");

    CFileFind finder;
    int files = 0;
    BOOL more = finder.FindFile(App("find\\*.dat").c_str());
    while (more) {
        more = finder.FindNextFile();
        ++files;
        CHECK(finder.GetFileName() == "c.dat");
        CHECK(finder.GetFilePath() == App("find\\c.dat").c_str());
        CHECK(finder.GetRoot() == App("find\\").c_str());
        CHECK(finder.GetLength() == 3);
        CHECK(!finder.IsDirectory() && !finder.IsDots());
    }
    CHECK(files == 1);
}

void TestArchive() {
    CString shortStr = "abc";
    CString longStr(static_cast<char>('x'), 300);
    CString hugeStr(static_cast<char>('y'), 70000);
    {
        CFile f(App("archive.bin").c_str(), CFile::modeCreate | CFile::modeWrite);
        CArchive ar(&f, CArchive::store);
        ar << shortStr << longStr << hugeStr;
        ar << static_cast<BYTE>(7) << static_cast<WORD>(0x1234) << static_cast<LONG>(-5) << static_cast<DWORD>(0xDEADBEEF);
        ar << 3.25 << static_cast<LONGLONG>(-1234567890123LL) << (int)42;
        ar.WriteCount(5);
        ar.WriteCount(70000);
        CStringArray arr;
        arr.Add("one");
        arr.Add("two");
        arr.Serialize(ar);
        CStringArray* obj = new CStringArray;
        obj->Add("serialized");
        ar << obj;
        ar << obj;
        ar.WriteObject(nullptr);
        ar.WriteString("text line\r\n");
        ar.Close();
        delete obj;
    }
    std::string raw = ReadRaw(Native("archive.bin"));
    CHECK(raw.size() > 8 && raw.substr(0, 4) == "\x03" "abc");
    CHECK(static_cast<unsigned char>(raw[4]) == 0xFF && static_cast<unsigned char>(raw[5]) == 0x2C &&
          static_cast<unsigned char>(raw[6]) == 0x01);
    {
        CFile f(App("archive.bin").c_str(), CFile::modeRead);
        CArchive ar(&f, CArchive::load);
        CString a, b, c;
        ar >> a >> b >> c;
        CHECK(a == shortStr);
        CHECK(b == longStr);
        CHECK(c == hugeStr);
        BYTE by;
        WORD w;
        LONG l;
        DWORD dw;
        double d;
        LONGLONG ll;
        int i;
        ar >> by >> w >> l >> dw >> d >> ll >> i;
        CHECK(by == 7 && w == 0x1234 && l == -5 && dw == 0xDEADBEEF && d == 3.25 && ll == -1234567890123LL && i == 42);
        CHECK(ar.ReadCount() == 5);
        CHECK(ar.ReadCount() == 70000);
        CStringArray arr;
        arr.Serialize(ar);
        CHECK(arr.GetSize() == 2 && arr[0] == "one" && arr[1] == "two");
        CStringArray* first = nullptr;
        CStringArray* second = nullptr;
        ar >> first >> second;
        CHECK(first != nullptr && first == second);
        CHECK(first && first->GetSize() == 1 && first->GetAt(0) == "serialized");
        CHECK(first && first->IsKindOf(RUNTIME_CLASS(CStringArray)));
        CObject* none = ar.ReadObject(nullptr);
        CHECK(none == nullptr);
        CString line;
        CHECK(ar.ReadString(line) && line == "text line");
        CHECK(!ar.ReadString(line));
        bool eof = false;
        try {
            CString more;
            ar >> more;
        } catch (CArchiveException* e) {
            eof = e->m_cause == CArchiveException::endOfFile;
            e->Delete();
        }
        CHECK(eof);
        delete first;
    }
    WriteRaw(Native("unicode.bin"), std::string("\xFF\xFE\xFF\x02h\0i\0", 8));
    {
        CFile f(App("unicode.bin").c_str(), CFile::modeRead);
        CArchive ar(&f, CArchive::load);
        CString s;
        ar >> s;
        CHECK(s == "hi");
    }
    CHECK(CRuntimeClass::FromName("CStringArray") == RUNTIME_CLASS(CStringArray));
    CObject* created = CRuntimeClass::CreateObject("CDWordArray");
    CHECK(created && created->IsKindOf(RUNTIME_CLASS(CDWordArray)) && created->IsSerializable());
    delete created;
}

void TestIni() {
    std::string ini = App("test.ini");
    CHECK(WritePrivateProfileString("General", "Name", "Value one", ini.c_str()));
    CHECK(WritePrivateProfileString("General", "Count", "42", ini.c_str()));
    CHECK(WritePrivateProfileString("Other", "Key", "\"quoted\"", ini.c_str()));
    CHECK(WritePrivateProfileString("general", "name", "Value two", ini.c_str()));
    char buf[256];
    CHECK(GetPrivateProfileString("GENERAL", "NAME", "def", buf, sizeof buf, ini.c_str()) == 9);
    CHECK_EQ_STR(buf, "Value two");
    CHECK(GetPrivateProfileInt("General", "Count", 0, ini.c_str()) == 42);
    CHECK(GetPrivateProfileInt("General", "Missing", 7, ini.c_str()) == 7);
    GetPrivateProfileString("Other", "Key", "", buf, sizeof buf, ini.c_str());
    CHECK_EQ_STR(buf, "quoted");
    GetPrivateProfileString("Nope", "Key", "fallback  ", buf, sizeof buf, ini.c_str());
    CHECK_EQ_STR(buf, "fallback");
    DWORD n = GetPrivateProfileString(nullptr, nullptr, "", buf, sizeof buf, ini.c_str());
    CHECK(n == 14 && memcmp(buf, "General\0Other\0\0", 15) == 0);
    n = GetPrivateProfileString("General", nullptr, "", buf, sizeof buf, ini.c_str());
    CHECK(n == 11 && memcmp(buf, "Name\0Count\0\0", 12) == 0);
    n = GetPrivateProfileString("General", "Name", "", buf, 4, ini.c_str());
    CHECK(n == 3);
    CHECK_EQ_STR(buf, "Val");
    n = GetPrivateProfileSection("General", buf, sizeof buf, ini.c_str());
    CHECK(n > 0 && strcmp(buf, "Name=Value two") == 0);
    CHECK(WritePrivateProfileString("General", "Count", nullptr, ini.c_str()));
    CHECK(GetPrivateProfileInt("General", "Count", -1, ini.c_str()) == static_cast<UINT>(-1));
    CHECK(WritePrivateProfileString("Other", nullptr, nullptr, ini.c_str()));
    n = GetPrivateProfileString(nullptr, nullptr, "", buf, sizeof buf, ini.c_str());
    CHECK(n == 8 && memcmp(buf, "General\0\0", 9) == 0);
    std::string text = ReadRaw(Native("test.ini"));
    CHECK(text == "[General]\r\nName=Value two\r\n");
}

void TestRegistry() {
    CRegKey key;
    CHECK(key.Open(HKEY_CURRENT_USER, "Software\\MfcwxTest\\Missing") == ERROR_FILE_NOT_FOUND);
    DWORD disposition = 0;
    CHECK(key.Create(HKEY_CURRENT_USER, "Software\\MfcwxTest\\Settings", nullptr, REG_OPTION_NON_VOLATILE,
                     KEY_ALL_ACCESS, nullptr, &disposition) == ERROR_SUCCESS);
    CHECK(disposition == REG_CREATED_NEW_KEY);
    CHECK(key.SetDWORDValue("Number", 1234) == ERROR_SUCCESS);
    CHECK(key.SetValue(77u, "Other") == ERROR_SUCCESS);
    CHECK(key.SetStringValue("Text", "hello w\xF6rld") == ERROR_SUCCESS);
    CHECK(key.SetValue("default value") == ERROR_SUCCESS);
    BYTE bin[4] = {1, 2, 0xFE, 0};
    CHECK(key.SetBinaryValue("Blob", bin, 4) == ERROR_SUCCESS);
    CHECK(key.Close() == ERROR_SUCCESS);

    HKEY sub;
    CHECK(RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\MfcwxTest\\Empty", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &sub,
                         &disposition) == ERROR_SUCCESS);
    CHECK(RegCloseKey(sub) == ERROR_SUCCESS);
    CHECK(RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\MfcwxTest\\Empty", 0, KEY_READ, &sub) == ERROR_SUCCESS);
    RegCloseKey(sub);

    CHECK(key.Open(HKEY_CURRENT_USER, "SOFTWARE\\mfcwxtest\\settings", KEY_READ) == ERROR_SUCCESS);
    DWORD dw = 0;
    CHECK(key.QueryDWORDValue("number", dw) == ERROR_SUCCESS && dw == 1234);
    unsigned long other = 0;
    CHECK(key.QueryValue(dw, "Other") == ERROR_SUCCESS && dw == 77);
    (void)other;
    char text[64];
    DWORD count = sizeof text;
    CHECK(key.QueryValue(text, "Text", &count) == ERROR_SUCCESS);
    CHECK_EQ_STR(text, "hello w\xF6rld");
    CHECK(count == 12);
    ULONG chars = 4;
    CHECK(key.QueryStringValue("Text", text, &chars) == ERROR_MORE_DATA && chars == 12);
    CHECK(key.QueryDWORDValue("Text", dw) == 13);
    count = sizeof text;
    CHECK(key.QueryValue(text, nullptr, &count) == ERROR_SUCCESS);
    CHECK_EQ_STR(text, "default value");
    BYTE out[8] = {};
    ULONG bytes = sizeof out;
    CHECK(key.QueryBinaryValue("Blob", out, &bytes) == ERROR_SUCCESS && bytes == 4 && memcmp(out, bin, 4) == 0);
    DWORD type = 0, size = 0;
    CHECK(RegQueryValueEx(key, "Text", nullptr, &type, nullptr, &size) == ERROR_SUCCESS && type == REG_SZ && size == 12);
    CHECK(key.DeleteValue("Other") == ERROR_SUCCESS);
    CHECK(key.QueryDWORDValue("Other", dw) == ERROR_FILE_NOT_FOUND);
    key.Close();

    HKEY parent;
    CHECK(RegOpenKey(HKEY_CURRENT_USER, "Software\\MfcwxTest", &parent) == ERROR_SUCCESS);
    char name[64];
    std::vector<std::string> subkeys;
    for (DWORD i = 0;; ++i) {
        DWORD nameSize = sizeof name;
        if (RegEnumKeyEx(parent, i, name, &nameSize, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            break;
        subkeys.push_back(name);
    }
    CHECK(subkeys.size() == 2 && subkeys[0] == "Empty" && subkeys[1] == "Settings");
    CHECK(RegDeleteKey(parent, "Empty") == ERROR_SUCCESS);
    CHECK(RegOpenKeyEx(parent, "Empty", 0, KEY_READ, &sub) == ERROR_FILE_NOT_FOUND);
    RegCloseKey(parent);

    std::string file = ReadRaw(getenv("MFCWX_REGISTRY_FILE"));
    CHECK(file.find("Number=dword:000004d2") != std::string::npos);
    CHECK(file.find("HKEY_CURRENT_USER/Software/MfcwxTest/Settings") != std::string::npos);
}

std::atomic<int> g_counter{0};

UINT CountingProc(LPVOID param) {
    int n = *static_cast<int*>(param);
    for (int i = 0; i < n; ++i)
        ++g_counter;
    CHECK(AfxGetThread() != nullptr);
    return 17;
}

struct LockData {
    CCriticalSection* cs;
    long value;
};

UINT LockedIncrement(LPVOID param) {
    auto* d = static_cast<LockData*>(param);
    for (int i = 0; i < 10000; ++i) {
        CSingleLock lock(d->cs, TRUE);
        long v = d->value;
        d->value = v + 1;
    }
    return 0;
}

UINT EndEarly(LPVOID) {
    AfxEndThread(99, FALSE);
    return 1;
}

UINT WaitForEvent(LPVOID param) {
    auto* ev = static_cast<CEvent*>(param);
    return ::WaitForSingleObject(*ev, 5000) == WAIT_OBJECT_0 ? 5 : 6;
}

std::atomic<int> g_ticks{0};
std::atomic<bool> g_stop{false};

DWORD Ticker(LPVOID) {
    while (!g_stop) {
        ++g_ticks;
        Sleep(1);
    }
    return 3;
}

DWORD ExitEarly(LPVOID) {
    ExitThread(21);
    return 0;
}

unsigned BeginEx(void* arg) {
    ++*static_cast<std::atomic<int>*>(arg);
    return 8;
}

void TestThreads() {
    int iterations = 1000;
    CWinThread* t = AfxBeginThread(CountingProc, &iterations, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
    CHECK(t != nullptr);
    t->m_bAutoDelete = FALSE;
    Sleep(20);
    CHECK(g_counter == 0);
    DWORD code = 0;
    CHECK(GetExitCodeThread(t->m_hThread, &code) && code == STILL_ACTIVE);
    t->ResumeThread();
    CHECK(WaitForSingleObject(t->m_hThread, 5000) == WAIT_OBJECT_0);
    CHECK(g_counter == 1000);
    CHECK(GetExitCodeThread(t->m_hThread, &code) && code == 17);
    delete t;

    t = AfxBeginThread(CountingProc, &iterations);
    HANDLE stale = t->m_hThread;
    CHECK(WaitForSingleObject(stale, 5000) == WAIT_OBJECT_0);
    Sleep(20);
    CHECK(WaitForSingleObject(stale, 0) == WAIT_OBJECT_0);
    CHECK(g_counter == 2000);

    t = AfxBeginThread(EndEarly, nullptr, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
    t->m_bAutoDelete = FALSE;
    t->ResumeThread();
    CHECK(WaitForSingleObject(*t, 5000) == WAIT_OBJECT_0);
    CHECK(GetExitCodeThread(*t, &code) && code == 99);
    delete t;

    CCriticalSection cs;
    LockData data{&cs, 0};
    std::vector<CWinThread*> workers;
    std::vector<HANDLE> handles;
    for (int i = 0; i < 4; ++i) {
        CWinThread* w = AfxBeginThread(LockedIncrement, &data, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
        w->m_bAutoDelete = FALSE;
        workers.push_back(w);
        handles.push_back(w->m_hThread);
    }
    for (CWinThread* w : workers)
        w->ResumeThread();
    CHECK(WaitForMultipleObjects(4, handles.data(), TRUE, 10000) == WAIT_OBJECT_0);
    CHECK(data.value == 40000);
    for (CWinThread* w : workers)
        delete w;

    CEvent manual(FALSE, TRUE);
    CHECK(WaitForSingleObject(manual, 10) == WAIT_TIMEOUT);
    manual.SetEvent();
    CHECK(WaitForSingleObject(manual, 0) == WAIT_OBJECT_0);
    CHECK(WaitForSingleObject(manual, 0) == WAIT_OBJECT_0);
    manual.ResetEvent();
    CHECK(WaitForSingleObject(manual, 0) == WAIT_TIMEOUT);
    CEvent autoEvent;
    autoEvent.SetEvent();
    CHECK(WaitForSingleObject(autoEvent, 0) == WAIT_OBJECT_0);
    CHECK(WaitForSingleObject(autoEvent, 0) == WAIT_TIMEOUT);
    CEvent go(FALSE, TRUE);
    t = AfxBeginThread(WaitForEvent, &go, THREAD_PRIORITY_NORMAL, 0, CREATE_SUSPENDED);
    t->m_bAutoDelete = FALSE;
    t->ResumeThread();
    Sleep(20);
    CHECK(WaitForSingleObject(*t, 0) == WAIT_TIMEOUT);
    go.SetEvent();
    CHECK(WaitForSingleObject(*t, 5000) == WAIT_OBJECT_0);
    CHECK(GetExitCodeThread(*t, &code) && code == 5);
    delete t;

    HANDLE ev1 = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    HANDLE ev2 = CreateEvent(nullptr, FALSE, TRUE, nullptr);
    HANDLE pair[2] = {ev1, ev2};
    CHECK(WaitForMultipleObjects(2, pair, FALSE, 0) == WAIT_OBJECT_0 + 1);
    CHECK(WaitForMultipleObjects(2, pair, FALSE, 10) == WAIT_TIMEOUT);
    SetEvent(ev1);
    SetEvent(ev2);
    CHECK(WaitForMultipleObjects(2, pair, TRUE, 0) == WAIT_OBJECT_0);
    CHECK(WaitForSingleObject(ev1, 0) == WAIT_TIMEOUT);
    HANDLE named = CreateEvent(nullptr, TRUE, FALSE, "mfcwx-test-event");
    HANDLE again = CreateEvent(nullptr, TRUE, FALSE, "mfcwx-test-event");
    CHECK(again == named && GetLastError() == ERROR_ALREADY_EXISTS);
    CloseHandle(again);
    CloseHandle(named);
    CloseHandle(ev1);
    CloseHandle(ev2);
    CHECK(WaitForSingleObject(ev1, 0) == WAIT_FAILED);

    CMutex mutex;
    CSingleLock ml(&mutex);
    CHECK(ml.Lock(0) && ml.IsLocked());
    CHECK(mutex.Lock(0));
    mutex.Unlock();
    ml.Unlock();
    CSemaphore sem(1, 2);
    CHECK(sem.Lock(0));
    CHECK(!sem.Lock(0));
    LONG prev = -1;
    CHECK(sem.Unlock(2, &prev) && prev == 0);
    CHECK(!sem.Unlock(1));
    CSyncObject* objs[2] = {&manual, &sem};
    CMultiLock multi(objs, 2);
    manual.SetEvent();
    CHECK(multi.Lock(100, TRUE) == WAIT_OBJECT_0);
    multi.Unlock();

    DWORD tid = 0;
    HANDLE ticker = CreateThread(nullptr, 0, Ticker, nullptr, CREATE_SUSPENDED, &tid);
    CHECK(ticker != nullptr && tid != 0 && tid != GetCurrentThreadId());
    Sleep(20);
    CHECK(g_ticks == 0);
    CHECK(ResumeThread(ticker) == 1);
    Sleep(30);
    CHECK(g_ticks > 0);
    CHECK(SuspendThread(ticker) == 0);
    Sleep(20);
    int frozen = g_ticks;
    Sleep(40);
    CHECK(g_ticks == frozen);
    CHECK(ResumeThread(ticker) == 1);
    Sleep(30);
    CHECK(g_ticks > frozen);
    g_stop = true;
    CHECK(WaitForSingleObject(ticker, 5000) == WAIT_OBJECT_0);
    CHECK(GetExitCodeThread(ticker, &code) && code == 3);
    CHECK(!TerminateThread(ticker, 0));
    CloseHandle(ticker);

    HANDLE early = CreateThread(nullptr, 0, ExitEarly, nullptr, 0, nullptr);
    CHECK(WaitForSingleObject(early, 5000) == WAIT_OBJECT_0);
    CHECK(GetExitCodeThread(early, &code) && code == 21);
    CloseHandle(early);

    std::atomic<int> hits{0};
    unsigned exTid = 0;
    HANDLE ex = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, BeginEx, &hits, 0, &exTid));
    CHECK(WaitForSingleObject(ex, 5000) == WAIT_OBJECT_0 && hits == 1);
    CHECK(GetExitCodeThread(ex, &code) && code == 8);
    CloseHandle(ex);

    HANDLE never = CreateThread(nullptr, 0, Ticker, nullptr, CREATE_SUSPENDED, nullptr);
    CHECK(TerminateThread(never, 4));
    CHECK(WaitForSingleObject(never, 0) == WAIT_OBJECT_0);
    CloseHandle(never);

    LONG v = 5;
    CHECK(InterlockedIncrement(&v) == 6 && InterlockedDecrement(&v) == 5);
    CHECK(InterlockedExchange(&v, 9) == 5 && v == 9);
    CHECK(InterlockedCompareExchange(&v, 1, 9) == 9 && v == 1);
    CHECK(InterlockedExchangeAdd(&v, 4) == 1 && v == 5);
    CRITICAL_SECTION raw;
    InitializeCriticalSection(&raw);
    EnterCriticalSection(&raw);
    CHECK(TryEnterCriticalSection(&raw));
    LeaveCriticalSection(&raw);
    LeaveCriticalSection(&raw);
    DeleteCriticalSection(&raw);
    CHECK(AfxGetThread() == nullptr);
}

void TestProcesses() {
    std::string out = App("echo.txt");
    SECURITY_ATTRIBUTES sa = {sizeof sa, nullptr, TRUE};
    HANDLE file = CreateFile(out.c_str(), FILE_WRITE_DATA, FILE_SHARE_READ, &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    CHECK(file != INVALID_HANDLE_VALUE);
    STARTUPINFO si;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = file;
    si.hStdError = file;
    PROCESS_INFORMATION pi;
    char cmd[] = "/bin/echo hello \"big world\"";
    CHECK(CreateProcess(nullptr, cmd, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi));
    CHECK(WaitForSingleObject(pi.hProcess, 5000) == WAIT_OBJECT_0);
    DWORD code = 1234;
    CHECK(GetExitCodeProcess(pi.hProcess, &code) && code == 0);
    CHECK(WaitForSingleObject(pi.hThread, 0) == WAIT_OBJECT_0);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(file);
    CHECK_EQ_STR(ReadRaw(Native("echo.txt")), "hello big world\n");

    char sh[] = "sh -c \"exit 3\"";
    CHECK(CreateProcess(nullptr, sh, nullptr, nullptr, FALSE, 0, nullptr, g_appRoot.c_str(), nullptr, &pi));
    CHECK(WaitForSingleObject(pi.hProcess, 5000) == WAIT_OBJECT_0);
    CHECK(GetExitCodeProcess(pi.hProcess, &code) && code == 3);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    char sleeper[] = "sleep 5";
    CHECK(CreateProcess(nullptr, sleeper, nullptr, nullptr, FALSE, 0, nullptr, nullptr, nullptr, &pi));
    CHECK(WaitForSingleObject(pi.hProcess, 50) == WAIT_TIMEOUT);
    CHECK(GetExitCodeProcess(pi.hProcess, &code) && code == STILL_ACTIVE);
    CHECK(TerminateProcess(pi.hProcess, 1));
    CHECK(WaitForSingleObject(pi.hProcess, 5000) == WAIT_OBJECT_0);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    char missing[] = "definitely-not-a-program-xyz.exe --flag";
    CHECK(!CreateProcess(nullptr, missing, nullptr, nullptr, FALSE, 0, nullptr, nullptr, nullptr, &pi));
    CHECK(GetLastError() == ERROR_FILE_NOT_FOUND);
    CHECK(WinExec("/usr/bin/true", SW_HIDE) > 31);
    CHECK(_spawnl(_P_WAIT, "/bin/sh", "sh", "-c", "\"exit 4\"", nullptr) == 4);
    char found[MAX_PATH];
    char* part = nullptr;
    CHECK(SearchPath(nullptr, "sh", nullptr, sizeof found, found, &part) > 0 && part && strcmp(part, "sh") == 0);
    CHECK(found[0] == '\\');
    CHECK(SearchPath(nullptr, "AESTool.exe", nullptr, sizeof found, found, &part) == 0);
    CHECK(reinterpret_cast<intptr_t>(ShellExecute(nullptr, "open", "/no/such/file.xyz", nullptr, nullptr, SW_SHOW)) <= 32);
}

void TestCrt() {
    char buf[80];
    CHECK_EQ_STR(_itoa(-255, buf, 10), "-255");
    CHECK_EQ_STR(_itoa(-1, buf, 16), "ffffffff");
    CHECK_EQ_STR(_itoa(255, buf, 2), "11111111");
    CHECK_EQ_STR(_itoa(35, buf, 36), "z");
    CHECK_EQ_STR(itoa(0, buf, 10), "0");
    CHECK_EQ_STR(_ltoa(-1L, buf, 16), "ffffffff");
    CHECK_EQ_STR(_ltoa(-123456L, buf, 10), "-123456");
    CHECK_EQ_STR(_ultoa(4294967295UL, buf, 10), "4294967295");
    CHECK_EQ_STR(_i64toa(-1LL, buf, 16), "ffffffffffffffff");
    CHECK_EQ_STR(_i64toa(LLONG_MIN, buf, 10), "-9223372036854775808");
    CHECK_EQ_STR(_ui64toa(18446744073709551615ULL, buf, 10), "18446744073709551615");
    CHECK(_itoa_s(12345, buf, 4, 10) == ERANGE && buf[0] == 0);
    CHECK(_itoa_s(123, buf, 4, 10) == 0 && strcmp(buf, "123") == 0);
    CHECK_EQ_STR(_gcvt(1.0, 5, buf), "1.");
    CHECK_EQ_STR(_gcvt(3.14159, 3, buf), "3.14");
    int dec = 0, sign = 0;
    CHECK_EQ_STR(_ecvt(3.1415926535, 5, &dec, &sign), "31416");
    CHECK(dec == 1 && sign == 0);
    CHECK_EQ_STR(_fcvt(-3.1415926535, 7, &dec, &sign), "31415927");
    CHECK(dec == 1 && sign == 1);
    CHECK_EQ_STR(_fcvt(0.012, 4, &dec, &sign), "120");
    CHECK(dec == -1);
    char s[] = "Hello";
    CHECK_EQ_STR(_strupr(s), "HELLO");
    CHECK_EQ_STR(_strlwr(s), "hello");
    CHECK_EQ_STR(_strrev(s), "olleh");
    CHECK_EQ_STR(_strnset(s, '*', 2), "**leh");
    CHECK(sprintf_s(buf, 4, "%s", "toolong") == -1 && buf[0] == 0);
    CHECK(sprintf_s(buf, sizeof buf, "%I64d", 1234567890123LL) == 13 && strcmp(buf, "1234567890123") == 0);
    CHECK(_snprintf_s(buf, sizeof buf, _TRUNCATE, "%s", "abc") == 3);
    CHECK(_snprintf_s(buf, 4, static_cast<size_t>(-1), "%s", "abcdef") == -1 && strcmp(buf, "abc") == 0);
    CHECK(strcpy_s(buf, 3, "abc") == ERANGE && buf[0] == 0);
    CHECK(strcpy_s(buf, sizeof buf, "abc") == 0 && strcat_s(buf, sizeof buf, "def") == 0 && strcmp(buf, "abcdef") == 0);
    CHECK(strncpy_s(buf, sizeof buf, "abcdef", 3) == 0 && strcmp(buf, "abc") == 0);
    CHECK(mfcwx_rotl(0x80000001u, 1) == 3u && mfcwx_rotr(3u, 1) == 0x80000001u);
    time_t now = time(nullptr);
    struct tm tm;
    CHECK(localtime_s(&tm, &now) == 0);
    char* env = nullptr;
    size_t envLen = 0;
    CHECK(_dupenv_s(&env, &envLen, "HOME") == 0 && env && envLen == strlen(env) + 1);
    free(env);
    char templ[MAX_PATH];
    snprintf(templ, sizeof templ, "%s\\tmpXXXXXX", g_appRoot.c_str());
    CHECK(_mktemp(templ) != nullptr && strstr(templ, "XXXXXX") == nullptr);
    char* tn = _tempnam(g_appRoot.c_str(), "pse");
    CHECK(tn && strncmp(tn, (g_appRoot + "\\pse").c_str(), g_appRoot.size() + 4) == 0);
    free(tn);
    CHECK(_mkdir(App("crtdir").c_str()) == 0);
    CHECK(_rmdir(App("crtdir").c_str()) == 0);
    int fd = _open(App("open.bin").c_str(), _O_CREAT | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
    CHECK(fd >= 0);
    CHECK(_write(fd, "12345", 5) == 5);
    CHECK(_filelength(fd) == 5);
    CHECK(_chsize(fd, 2) == 0 && _filelength(fd) == 2);
    _close(fd);
}

void TestWin32() {
    mfcwx::SetAnsiCodePage(1250);
    const char polish[] = "\xB9\xE6\x9C\xBF\xA3";
    wchar_t wide[16];
    int n = MultiByteToWideChar(1250, 0, polish, -1, nullptr, 0);
    CHECK(n == 6);
    CHECK(MultiByteToWideChar(1250, 0, polish, -1, wide, 16) == 6);
    CHECK(wide[0] == 0x105 && wide[1] == 0x107 && wide[2] == 0x15B && wide[3] == 0x17C && wide[4] == 0x141 && wide[5] == 0);
    CHECK(MultiByteToWideChar(CP_ACP, 0, polish, 2, wide, 16) == 2 && wide[0] == 0x105);
    char back[16];
    BOOL usedDefault = TRUE;
    CHECK(WideCharToMultiByte(1250, 0, wide, 2, back, sizeof back, nullptr, &usedDefault) == 2);
    CHECK(memcmp(back, polish, 2) == 0 && !usedDefault);
    CHECK(WideCharToMultiByte(1252, 0, wide, 1, back, sizeof back, nullptr, &usedDefault) == 1 && back[0] == '?' &&
          usedDefault);
    CHECK(WideCharToMultiByte(CP_UTF8, 0, wide, 1, back, sizeof back, nullptr, nullptr) == 2);
    CHECK(memcmp(back, "\xC4\x85", 2) == 0);
    CHECK(MultiByteToWideChar(CP_UTF8, 0, "\xF0\x9F\x98\x80", 4, wide, 16) == 1 && wide[0] == 0x1F600);
    CHECK(MultiByteToWideChar(1250, 0, polish, 5, wide, 2) == 0 && GetLastError() == 122);

    char upper[] = "\xB9\xE6\x9F" "abc";
    CharUpper(upper);
    CHECK_EQ_STR(upper, "\xA5\xC6\x8F" "ABC");
    CharLowerBuff(upper, 2);
    CHECK_EQ_STR(upper, "\xB9\xE6\x8F" "ABC");
    CHECK(IsCharAlpha('\xB9') && IsCharLower('\xB9') && IsCharUpper('\xA5') && !IsCharAlpha('1'));
    CHECK(IsCharAlphaNumeric('7') && !IsCharAlpha('\xA7'));
    CHECK(GetACP() == 1250);
    mfcwx::SetAnsiCodePage(1252);
    CHECK(lstrcmpi("\xC4pfel", "\xE4PFEL") == 0);
    CHECK(lstrcmp("a", "B") < 0 && lstrcmp("a", "A") < 0 && lstrcmp("abc", "abc") == 0);
    char buf[256];
    CHECK(wsprintf(buf, "%d-%s-%I64u", 5, "x", 99ULL) == 6 && strcmp(buf, "5-x-99") == 0);
    lstrcpyn(buf, "abcdef", 4);
    CHECK_EQ_STR(buf, "abc");

    LPSTR msg = nullptr;
    DWORD len = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                              nullptr, ERROR_FILE_NOT_FOUND, 0, reinterpret_cast<LPSTR>(&msg), 0, nullptr);
    CHECK(len > 0 && msg && strstr(msg, "cannot find the file") != nullptr);
    CHECK(LocalFree(msg) == nullptr);
    DWORD_PTR args[2] = {reinterpret_cast<DWORD_PTR>("file.txt"), 7};
    len = FormatMessage(FORMAT_MESSAGE_FROM_STRING | 0x2000, "Open %1 failed (%2!d!)%n", 0, 0, buf, sizeof buf,
                        reinterpret_cast<va_list*>(args));
    CHECK_EQ_STR(buf, "Open file.txt failed (7)\r\n");

    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, 16);
    CHECK(g && GlobalSize(g) == 16);
    char* p = static_cast<char*>(GlobalLock(g));
    CHECK(p && p[15] == 0);
    strcpy(p, "clip");
    GlobalUnlock(g);
    g = GlobalReAlloc(g, 4096, GMEM_MOVEABLE);
    CHECK(g && GlobalSize(g) == 4096 && strcmp(static_cast<char*>(GlobalLock(g)), "clip") == 0);
    GlobalUnlock(g);
    CHECK(GlobalFree(g) == nullptr);
    char* fixed = static_cast<char*>(GlobalAlloc(GPTR, 8));
    CHECK(fixed && fixed[7] == 0 && GlobalLock(fixed) == fixed);
    GlobalFree(fixed);

    OSVERSIONINFO vi;
    vi.dwOSVersionInfoSize = sizeof vi;
    CHECK(GetVersionEx(&vi) && vi.dwMajorVersion == 5 && vi.dwMinorVersion == 1);
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    CHECK(si.dwNumberOfProcessors >= 1 && si.dwPageSize >= 4096);
    MEMORYSTATUS ms;
    GlobalMemoryStatus(&ms);
    CHECK(ms.dwTotalPhys > 0 && ms.dwAvailPhys <= ms.dwTotalPhys);
    LARGE_INTEGER f, c1, c2;
    CHECK(QueryPerformanceFrequency(&f) && f.QuadPart == 10000000);
    QueryPerformanceCounter(&c1);
    DWORD t0 = GetTickCount();
    Sleep(20);
    QueryPerformanceCounter(&c2);
    CHECK(c2.QuadPart - c1.QuadPart >= 150000);
    CHECK(GetTickCount() - t0 >= 15);
    SYSTEMTIME st;
    GetSystemTime(&st);
    FILETIME ft, ft2;
    CHECK(SystemTimeToFileTime(&st, &ft));
    SYSTEMTIME st2;
    CHECK(FileTimeToSystemTime(&ft, &st2));
    CHECK(st2.wYear == st.wYear && st2.wSecond == st.wSecond && st2.wMilliseconds == st.wMilliseconds);
    st.wYear = 1601;
    st.wMonth = 1;
    st.wDay = 1;
    st.wHour = st.wMinute = st.wSecond = st.wMilliseconds = 0;
    CHECK(SystemTimeToFileTime(&st, &ft2) && ft2.dwLowDateTime == 0 && ft2.dwHighDateTime == 0);
    CHECK(MulDiv(10, 3, 4) == 8 && MulDiv(-10, 3, 4) == -8 && MulDiv(1, 1, 0) == -1);
    WORD lang = static_cast<WORD>(GetUserDefaultLangID());
    CHECK(PRIMARYLANGID(lang) != 0);

    CHECK(SetEnvironmentVariable("MFCWX_TEST_VAR", "abc"));
    CHECK(GetEnvironmentVariable("MFCWX_TEST_VAR", buf, sizeof buf) == 3 && strcmp(buf, "abc") == 0);
    CHECK(ExpandEnvironmentStrings("x%MFCWX_TEST_VAR%y%UNDEFINED_MFCWX%", buf, sizeof buf) == 23);
    CHECK_EQ_STR(buf, "xabcy%UNDEFINED_MFCWX%");
    CHECK(SetEnvironmentVariable("MFCWX_TEST_VAR", nullptr));
    CHECK(GetEnvironmentVariable("MFCWX_TEST_VAR", buf, sizeof buf) == 0);
    DWORD size = sizeof buf;
    CHECK(GetUserName(buf, &size) && size == strlen(buf) + 1);
    size = sizeof buf;
    CHECK(GetComputerName(buf, &size) && size == strlen(buf));
    CHECK(LoadLibrary("NoSuchLib.dll") == nullptr && GetLastError() == 126);
    HMODULE sci = LoadLibrary("SciLexer.DLL");
    CHECK(sci != nullptr && GetModuleHandle("scilexer.dll") == sci);
    CHECK(GetProcAddress(sci, "Scintilla_DirectFunction") == nullptr);
    CHECK(FreeLibrary(sci));
    mfcwx::RegisterBuiltinModule("extra.dll");
    CHECK(LoadLibraryEx("C:\\x\\EXTRA.dll", nullptr, 0) != nullptr);
    CHECK(GetModuleHandle(nullptr) != nullptr);

    CTime ct(2024, 3, 5, 7, 8, 9);
    CHECK(ct.GetYear() == 2024 && ct.GetMonth() == 3 && ct.GetDay() == 5 && ct.GetHour() == 7);
    CHECK(ct.Format("%#d.%#m.%Y %H:%M:%S") == "5.3.2024 07:08:09");
    CHECK(ct.Format("%#x") == "Tuesday, March 5, 2024");
    CHECK(ct.GetDayOfWeek() == 3);
    CHECK((ct + CTimeSpan(1, 2, 3, 4)).Format("%d %H:%M:%S") == "06 09:11:13");
    CHECK(CTimeSpan(1, 2, 3, 4).Format("%D:%H:%M:%S") == "1:02:03:04");
    COleDateTime od(2024, 2, 29, 12, 30, 15);
    CHECK(od.m_status == COleDateTime::valid);
    CHECK(od.GetYear() == 2024 && od.GetMonth() == 2 && od.GetDay() == 29 && od.GetHour() == 12 &&
          od.GetMinute() == 30 && od.GetSecond() == 15);
    CHECK(od.GetDayOfWeek() == 5);
    CHECK(static_cast<long>(od.m_dt) == 45351);
    CHECK(od.Format("%Y-%m-%d") == "2024-02-29");
    COleDateTime bad(2023, 2, 29, 0, 0, 0);
    CHECK(bad.m_status == COleDateTime::invalid);
    CHECK(COleDateTime::GetCurrentTime().GetYear() >= 2024);
}

void TestExceptions() {
    bool caught = false;
    try {
        AfxThrowArchiveException(CArchiveException::badSchema, "doc.ct");
    } catch (CException* e) {
        char msg[256];
        caught = e->IsKindOf(RUNTIME_CLASS(CArchiveException)) && e->GetErrorMessage(msg, sizeof msg) &&
                 strcmp(msg, "doc.ct contains an incorrect schema.") == 0;
        e->Delete();
    }
    CHECK(caught);
    caught = false;
    try {
        AfxThrowMemoryException();
    } catch (CMemoryException* e) {
        caught = true;
        e->Delete();
    }
    CHECK(caught);
    CHECK(CFileException::ErrnoToException(ENOENT) == CFileException::fileNotFound);
    CHECK(CFileException::OsErrorToException(5) == CFileException::accessDenied);
}

} // namespace

int main(int argc, char** argv) {
    wxInitializer initializer(argc, argv);
    if (!initializer.IsOk()) {
        fprintf(stderr, "wxWidgets initialization failed\n");
        return 2;
    }
    const char* tmp = getenv("TMPDIR");
    std::string base = tmp && *tmp ? tmp : "/tmp";
    while (base.size() > 1 && base.back() == '/')
        base.pop_back();
    std::string templ = base + "/mfcwx-tests-XXXXXX";
    std::vector<char> buf(templ.begin(), templ.end());
    buf.push_back(0);
    if (!mkdtemp(buf.data())) {
        perror("mkdtemp");
        return 2;
    }
    g_root = buf.data();
    char real[PATH_MAX];
    if (realpath(g_root.c_str(), real))
        g_root = real;
    g_appRoot = mfcwx::AppPath(g_root.c_str());
    std::string registry = g_root + "/registry.ini";
    setenv("MFCWX_REGISTRY_FILE", registry.c_str(), 1);

    TestPaths();
    TestFiles();
    TestFind();
    TestArchive();
    TestIni();
    TestRegistry();
    TestThreads();
    TestProcesses();
    TestCrt();
    TestWin32();
    TestExceptions();

    std::string cleanup = "rm -rf '" + g_root + "'";
    if (g_failures == 0 && system(cleanup.c_str()) != 0)
        fprintf(stderr, "cleanup of %s failed\n", g_root.c_str());
    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
