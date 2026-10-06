#pragma once

#include <cstdint>

// Resource tables generated from .rc files by port/rc/rc2cpp.py.
// All strings are UTF-8. A resource is identified either by a numeric id (name == nullptr)
// or by a string name (id == 0).
namespace mfcwx {
namespace rc {

struct Control {
    const char* cls;      // window class as written in the .rc ("Button", "Edit", "Static", "ComboBox",
                          // "ListBox", "ScrollBar", "msctls_progress32", "msctls_trackbar32",
                          // "msctls_updown32", "SysListView32", "SysTreeView32", "SysTabControl32",
                          // "RICHEDIT", "RichEdit20A", "SysDateTimePick32", "SysAnimate32", ...)
    const char* text;     // caption, or resource name for SS_BITMAP/SS_ICON/BS_BITMAP ("#123" for numeric ids)
    int id;               // control id, -1 for IDC_STATIC
    int16_t x, y, cx, cy; // dialog units
    uint32_t style;       // full style including the implied bits of the statement keyword
    uint32_t exStyle;
};

struct Dialog {
    int id;
    const char* name;
    int16_t x, y, cx, cy;
    uint32_t style;
    uint32_t exStyle;
    const char* caption;
    const char* fontName; // nullptr if DS_SETFONT not given
    int16_t fontSize;
    int16_t fontWeight;
    uint8_t fontItalic;
    const char* menuName; // nullptr if none
    int menuId;
    const Control* controls;
    int controlCount;
};

struct MenuItem {
    int16_t depth;        // 0 = top level of the menu resource
    int id;               // command id, 0 for popups and separators
    uint32_t flags;       // MF_POPUP, MF_SEPARATOR, MF_GRAYED, MF_DISABLED, MF_CHECKED, MF_MENUBARBREAK, MF_MENUBREAK, MF_HELP
    const char* text;     // with '&' mnemonics and "\t" accelerator hints as in the .rc
};

struct Menu {
    int id;
    const char* name;
    const MenuItem* items;
    int itemCount;
};

struct StringEntry {
    int id;
    const char* text;
};

struct Accel {
    int key;              // virtual key code if (flags & FVIRTKEY), else the character code
    int id;
    uint32_t flags;       // FVIRTKEY | FSHIFT | FCONTROL | FALT | FNOINVERT
};

struct AccelTable {
    int id;
    const char* name;
    const Accel* items;
    int itemCount;
};

struct Toolbar {
    int id;
    const char* name;
    int16_t buttonWidth, buttonHeight;
    const int* buttons;   // command ids, 0 = separator
    int buttonCount;
};

// BITMAP, ICON, CURSOR, GIF, AVI, HTML, WAVE and other file-backed resources.
struct FileResource {
    const char* type;     // "BITMAP", "ICON", "CURSOR", or the custom type name as in the .rc
    int id;
    const char* name;
    const char* path;     // relative to the directory of the .rc file, with '/' separators
};

// DLGINIT: initial list entries of combo boxes.
struct DlgInitEntry {
    int dialogId;
    int controlId;
    const char* text;
};

struct VersionInfo {
    uint16_t fileVersion[4];
    uint16_t productVersion[4];
    const char* const* keys;   // alternating key/value pairs from the first StringFileInfo block
    int keyCount;              // number of pairs
};

struct Language {
    const char* code;          // "de", "en", "fr", "es", "pl", "el", "rs", ...
    int langId;                // MAKELANGID(primary, sub)
    int codepage;              // code page of the original .rc section
    const Dialog* dialogs;
    int dialogCount;
    const Menu* menus;
    int menuCount;
    const StringEntry* strings; // sorted by id
    int stringCount;
    const AccelTable* accels;
    int accelCount;
    const Toolbar* toolbars;
    int toolbarCount;
    const FileResource* files;
    int fileCount;
    const DlgInitEntry* dlgInits;
    int dlgInitCount;
    const VersionInfo* version; // nullptr if none
};

struct Module {
    const char* name;          // e.g. "CrypTool"
    const Language* const* languages;
    int languageCount;
};

} // namespace rc
} // namespace mfcwx
