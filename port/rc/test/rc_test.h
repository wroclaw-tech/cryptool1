#pragma once

#include "mfcwx/rcdata.h"

extern const mfcwx::rc::Module g_rcModule_CrypTool;
extern const mfcwx::rc::Module g_rcModule_aestool;
extern const mfcwx::rc::Module g_rcModule_NumberShark;
extern const mfcwx::rc::Module g_rcModule_VolRen;

namespace rctest {

using namespace mfcwx::rc;

constexpr uint32_t kFVirtKey = 0x01;
constexpr uint32_t kFNoInvert = 0x02;
constexpr uint32_t kFShift = 0x04;
constexpr uint32_t kFControl = 0x08;
constexpr uint32_t kFAlt = 0x10;
constexpr uint32_t kMfGrayed = 0x01;
constexpr uint32_t kMfPopup = 0x10;
constexpr uint32_t kMfSeparator = 0x800;

void check(bool ok, const char* what, const char* file, int line);
bool equals(const char* a, const char* b);
bool startsWith(const char* s, const char* prefix);
bool contains(const char* s, const char* part);

const Language* language(const Module& m, const char* code);
const Dialog* dialog(const Language& l, int id);
const Control* control(const Dialog& d, int id);
const Control* controlByText(const Dialog& d, const char* text);
const char* string(const Language& l, int id);
const Menu* menu(const Language& l, int id);
const AccelTable* accelTable(const Language& l, int id);
const Accel* accel(const AccelTable& t, int command, int key);
const Toolbar* toolbar(const Language& l, int id);
const FileResource* file(const Language& l, const char* type, int id, const char* name);
int dlgInitTexts(const Language& l, int dialogId, int controlId, const char** out, int max);

void runCrypToolChecks();
void runNumberSharkChecks();
void runAesToolVolRenChecks();

} // namespace rctest

#define RC_CHECK(cond) ::rctest::check((cond), #cond, __FILE__, __LINE__)
