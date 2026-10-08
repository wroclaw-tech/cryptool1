#include "rc_test.h"

#include <cstdio>
#include <cstring>

namespace rctest {

static int g_checks = 0;
static int g_failures = 0;

void check(bool ok, const char* what, const char* file, int line) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::printf("FAIL %s:%d: %s\n", file, line, what);
    }
}

bool equals(const char* a, const char* b) {
    return a && b && std::strcmp(a, b) == 0;
}

bool startsWith(const char* s, const char* prefix) {
    return s && prefix && std::strncmp(s, prefix, std::strlen(prefix)) == 0;
}

bool contains(const char* s, const char* part) {
    return s && part && std::strstr(s, part) != nullptr;
}

const Language* language(const Module& m, const char* code) {
    for (int i = 0; i < m.languageCount; ++i)
        if (equals(m.languages[i]->code, code))
            return m.languages[i];
    return nullptr;
}

const Dialog* dialog(const Language& l, int id) {
    for (int i = 0; i < l.dialogCount; ++i)
        if (l.dialogs[i].id == id && !l.dialogs[i].name)
            return &l.dialogs[i];
    return nullptr;
}

const Control* control(const Dialog& d, int id) {
    for (int i = 0; i < d.controlCount; ++i)
        if (d.controls[i].id == id)
            return &d.controls[i];
    return nullptr;
}

const Control* controlByText(const Dialog& d, const char* text) {
    for (int i = 0; i < d.controlCount; ++i)
        if (equals(d.controls[i].text, text))
            return &d.controls[i];
    return nullptr;
}

const char* string(const Language& l, int id) {
    int lo = 0, hi = l.stringCount - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (l.strings[mid].id == id)
            return l.strings[mid].text;
        if (l.strings[mid].id < id)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return nullptr;
}

const Menu* menu(const Language& l, int id) {
    for (int i = 0; i < l.menuCount; ++i)
        if (l.menus[i].id == id && !l.menus[i].name)
            return &l.menus[i];
    return nullptr;
}

const AccelTable* accelTable(const Language& l, int id) {
    for (int i = 0; i < l.accelCount; ++i)
        if (l.accels[i].id == id && !l.accels[i].name)
            return &l.accels[i];
    return nullptr;
}

const Accel* accel(const AccelTable& t, int command, int key) {
    for (int i = 0; i < t.itemCount; ++i)
        if (t.items[i].id == command && t.items[i].key == key)
            return &t.items[i];
    return nullptr;
}

const Toolbar* toolbar(const Language& l, int id) {
    for (int i = 0; i < l.toolbarCount; ++i)
        if (l.toolbars[i].id == id && !l.toolbars[i].name)
            return &l.toolbars[i];
    return nullptr;
}

const FileResource* file(const Language& l, const char* type, int id, const char* name) {
    for (int i = 0; i < l.fileCount; ++i) {
        const FileResource& f = l.files[i];
        if (!equals(f.type, type))
            continue;
        if (name ? equals(f.name, name) : (f.id == id && !f.name))
            return &f;
    }
    return nullptr;
}

int dlgInitTexts(const Language& l, int dialogId, int controlId, const char** out, int max) {
    int n = 0;
    for (int i = 0; i < l.dlgInitCount; ++i)
        if (l.dlgInits[i].dialogId == dialogId && l.dlgInits[i].controlId == controlId && n < max)
            out[n++] = l.dlgInits[i].text;
    return n;
}

} // namespace rctest

using namespace rctest;

static void printStats(const Module& m) {
    std::printf("%s: %d language(s)\n", m.name, m.languageCount);
    std::printf("  %-7s %-6s %5s %7s %8s %5s %6s %7s %6s %8s %5s %7s %7s\n", "lang", "langid", "cp", "dialogs",
                "controls", "menus", "items", "strings", "accels", "toolbars", "files", "dlginit", "version");
    for (int i = 0; i < m.languageCount; ++i) {
        const Language& l = *m.languages[i];
        int controls = 0, items = 0;
        for (int d = 0; d < l.dialogCount; ++d)
            controls += l.dialogs[d].controlCount;
        for (int k = 0; k < l.menuCount; ++k)
            items += l.menus[k].itemCount;
        std::printf("  %-7s 0x%04X %5d %7d %8d %5d %6d %7d %6d %8d %5d %7d %7s\n", l.code, l.langId, l.codepage,
                    l.dialogCount, controls, l.menuCount, items, l.stringCount, l.accelCount, l.toolbarCount,
                    l.fileCount, l.dlgInitCount, l.version ? "yes" : "no");
    }
}

static void structuralChecks(const Module& m) {
    RC_CHECK(m.name != nullptr);
    RC_CHECK(m.languageCount > 0);
    for (int i = 0; i < m.languageCount; ++i) {
        const Language& l = *m.languages[i];
        RC_CHECK(l.code != nullptr);
        for (int k = 1; k < l.stringCount; ++k)
            RC_CHECK(l.strings[k - 1].id < l.strings[k].id);
        for (int k = 0; k < l.stringCount; ++k)
            RC_CHECK(l.strings[k].text != nullptr);
        for (int d = 0; d < l.dialogCount; ++d) {
            const Dialog& dlg = l.dialogs[d];
            RC_CHECK((dlg.controls != nullptr) == (dlg.controlCount > 0));
            RC_CHECK((dlg.id != 0) != (dlg.name != nullptr));
            for (int c = 0; c < dlg.controlCount; ++c)
                RC_CHECK(dlg.controls[c].cls != nullptr);
        }
        for (int k = 0; k < l.menuCount; ++k) {
            const Menu& mn = l.menus[k];
            RC_CHECK(mn.itemCount > 0 && mn.items[0].depth == 0);
            for (int j = 1; j < mn.itemCount; ++j)
                RC_CHECK(mn.items[j].depth <= mn.items[j - 1].depth + 1);
            for (int j = 0; j < mn.itemCount; ++j)
                RC_CHECK((mn.items[j].text == nullptr) == ((mn.items[j].flags & kMfSeparator) != 0));
        }
        for (int k = 0; k < l.fileCount; ++k) {
            const FileResource& f = l.files[k];
            RC_CHECK(f.type != nullptr && f.path != nullptr);
            RC_CHECK((f.id != 0) != (f.name != nullptr));
            RC_CHECK(std::strchr(f.path, '\\') == nullptr);
        }
    }
}

int main() {
    const Module* modules[] = {&g_rcModule_CrypTool, &g_rcModule_aestool, &g_rcModule_NumberShark, &g_rcModule_VolRen};
    for (const Module* m : modules) {
        printStats(*m);
        structuralChecks(*m);
    }
    runCrypToolChecks();
    runNumberSharkChecks();
    runAesToolVolRenChecks();
    std::printf("%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
