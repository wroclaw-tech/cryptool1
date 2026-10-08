# CrypTool 1 on macOS/Linux — porting status

Work in progress on branch `xplat-port`. CrypTool builds, links and starts on macOS (arm64) and
Linux (Ubuntu 24.04, GTK 3): the tip-of-the-day dialog and the starting example in the editor
come up. Most features have not been exercised yet.

**Automatic status:** [STATUS.md on the port-status branch](https://github.com/wroclaw-tech/cryptool1/blob/port-status/STATUS.md)
(`python3 port/tools/port_status.py` locally). It only measures compiling, linking and the unit
tests, which all pass, so it shows 100%; the table below estimates the remaining work by area.

| Area | Weight | Done |
|---|---|---|
| Resource compiler | 10% | 100% |
| Third-party libraries | 15% | 100% |
| SECUDE replacement | 10% | 100% |
| mfcwx (MFC/Win32 on wxWidgets) | 30% | 85% |
| App sources compile and link | 10% | 100% |
| Run, feature testing, UI tuning | 20% | 15% |
| Packaging (DMG, deb/tar.gz) | 5% | 60% |

## Approach
The original MFC sources are compiled almost unchanged against **mfcwx**, an MFC/Win32
compatibility layer implemented on wxWidgets 3.2. Windows resources (`.rc`) are converted to
C++ tables at build time. SECUDE (proprietary, Windows-only) is replaced by an OpenSSL 3 based
implementation of the same API.

Build and run:
```
cmake -S . -B build -G Ninja
ninja -C build
build/port/app/CrypTool.app/Contents/MacOS/CrypTool   # macOS
build/port/app/CrypTool                               # Linux
```
macOS: Homebrew `wxwidgets@3.2 openssl@3 gmp ninja cmake`. Linux (Ubuntu 24.04):
`libwxgtk3.2-dev libssl-dev libgmp-dev libglu1-mesa-dev ninja-build cmake`.
Tests: configure with `-DMFCWX_TESTS=ON -DCRYPTOOL_THIRDPARTY_TESTS=ON`, then `ctest`.
Packages: `cmake --install` / `cpack` (DMG on macOS, tar.gz and deb on Linux).

## Layout
| Path | Content |
|---|---|
| `port/rc/` | `rc2cpp.py` resource compiler and the `mfcwx_add_rc` CMake function |
| `port/thirdparty/` | libanalyse, MIRACL, NTL, libec, cracklib, apfloat→GMP, cv act→OpenSSL, AES candidates |
| `port/secude/` | SECUDE API on OpenSSL 3 (key stores, CA, certificates, PKCS#12) |
| `port/mfcwx/` | MFC/Win32 headers and implementation, plus runtime/GDI/Scintilla tests |
| `port/app/` | CrypTool target, data directory assembly, bundle/install/CPack rules, icons |
| `port/tools/` | port status, help index generator, dialog gallery, source fix scripts |

## Key decisions
- HWND = `wxWindow*`; Windows ids are offset by `kIdOffset`, ids that would exceed wx's 0x7fff
  limit get compact ids; controls have no data members (temporary `CWnd` casts work as in MFC).
- `CString` is a byte string in the active ANSI code page (1252/1250/1253 by UI language);
  conversion happens at the wx boundary. The Scintilla proxy keeps the editor in UTF-8 and
  translates text and positions.
- Paths inside the app keep Windows form (`\dir\file`); every file API converts them.
- The registry is an INI file (`~/Library/Preferences/CrypTool1.ini`, `~/.config/cryptool1.ini`);
  `APPDATA` points to `~/Library/Application Support` or `~/.local/share`.
- HTML Help is replaced by an in-app wxHtmlHelpController built from the `.hhp/.hhc/.hhk` sources.
- The app is compiled as C++14 with hidden symbol visibility.
- `MFCWX_SNAPSHOT_DIR` makes the app save PNGs of its windows (used by the Linux CI job).

## Remaining work
1. Exercise the menus: encryption/analysis dialogs, hex editor, plots, OpenGL views, key store,
   and fix what breaks (start with the dialog gallery, `-DCRYPTOOL_DIALOG_GALLERY=ON`).
2. UI tuning: dialog sizes and fonts, toolbar icons in dark mode, F1 on submenus
   (`WM_MENUSELECT`), printing (currently a stub).
3. Data: ship the PDFs referenced by the help, check the external tools (AES animation, ANIMAL, …).
4. Packaging: sign/notarize the macOS bundle, test the deb on a clean system.

Known approximations are listed in the commit messages of the respective mfcwx parts.
