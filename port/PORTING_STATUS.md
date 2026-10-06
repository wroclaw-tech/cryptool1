# CrypTool 1 on macOS/Linux — porting status

Work in progress on branch `xplat-port`. Nothing builds into a runnable app yet.

**Overall progress:** measured automatically, see [status/STATUS.md](status/STATUS.md).
The table below is a manual estimate by area.

| Area | Weight | Done |
|---|---|---|
| Resource compiler | 10% | 100% |
| Third-party libraries | 15% | 100% |
| mfcwx headers | 10% | 90% |
| mfcwx implementation | 30% | 25% |
| SECUDE replacement | 10% | 40% |
| App sources compile | 10% | 80% |
| Link, run, UI tuning | 10% | 0% |
| Packaging (DMG, AppImage) | 5% | 0% |

## Approach
The original MFC sources are compiled almost unchanged against **mfcwx**, an MFC/Win32
compatibility layer implemented on wxWidgets 3.2. Windows resources (`.rc`) are converted to
C++ tables at build time. SECUDE (proprietary, Windows-only) is replaced by an OpenSSL 3 based
implementation of the same API.

Build: `cmake -S . -B build -G Ninja && ninja -C build -k 0 CrypTool`
(macOS: Homebrew `wxwidgets@3.2`, `openssl@3`, `gmp`, `cmake`).

## Layout
| Path | Content | State |
|---|---|---|
| `CMakeLists.txt` | top-level build | done |
| `port/rc/` | `rc2cpp.py` resource compiler + CMake function | done, tested on all 4 .rc files |
| `port/thirdparty/` | libanalyse, MIRACL (32-bit digits), NTL 5.5.2, libec, cracklib, apfloat→GMP, cv act→OpenSSL, AES candidates, OpenSSL hash shims | done, tests pass on macOS + Linux (Docker) |
| `port/secude/` | SECUDE API on OpenSSL 3 | partial (agent stopped mid-work); `secude_compat` must also export `global_add_error` and `rabinstest` for libec |
| `port/mfcwx/include/` | MFC/Win32 headers (afx*.h, windows.h, …) | written; app compiles against them |
| `port/mfcwx/src/` | implementation | partial: CString, code pages, resources, CCmdTarget/message maps, CWnd + event bridge (`wnd.cpp`), controls (`controls.cpp`); other files may be partial from stopped agents |
| `port/tools/` | `fix_includes.py`, `fix_msgmaps.py`, `ccheck.py` (single-file syntax check) | done |

## Key decisions
- HWND = `wxWindow*`; control ids are offset by `mfcwx::kIdOffset`; controls have no data members (temporary `CWnd` casts work as in MFC).
- `CString` is a byte string in the active ANSI code page (1252/1250/1253 by UI language); conversion happens at the wx boundary.
- Paths inside the app keep Windows form (`\\dir\\file`); every file API converts with `mfcwx::NativePath`.
- App is compiled as C++14 (C++17 `std::byte` clashes with the Windows `byte` typedef).
- Cross-thread `SendMessage` is marshalled to the main thread like on Windows.

## Remaining work (in order)
1. mfcwx implementation still missing: non-GUI runtime (files, archives, threads, registry/INI, CRT compat), GDI (CDC on wxDC), `DefaultWindowProc` + Win32 HWND functions (winapi.cpp), dialogs + DDX (dialog.cpp), menus, CWinApp/main loop/help, doc/view + MDI frames, Scintilla proxy (ANSI↔UTF-8 positions), WGL on wxGLCanvas.
2. Finish SECUDE replacement and its tests; recreate sample PSE keys.
3. Remaining ~680 compile errors in app sources (MSVC-isms: CString through varargs, temporaries bound to non-const refs, PictureEx/IPicture, OpenGL/libVolRen NV extensions, ActiveX editor).
4. Link, run, visually tune dialog units/fonts, then packaging (.app/DMG, AppImage/.deb).
