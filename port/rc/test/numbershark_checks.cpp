#include "rc_test.h"

#include "afxres.h"
#include "../../../NumberShark/resource.h"

namespace rctest {

void runNumberSharkChecks() {
    const Module& m = g_rcModule_NumberShark;
    RC_CHECK(m.languageCount == 7);

    const Language* el = language(m, "el");
    RC_CHECK(el && el->codepage == 1253);
    if (el)
        RC_CHECK(startsWith(string(*el, IDS_RULES), "Περιγραφή:\r\n"));

    const Language* pl = language(m, "pl");
    RC_CHECK(pl != nullptr);
    if (!pl)
        return;
    RC_CHECK(startsWith(string(*pl, IDS_RULES), "Opis gry:\n - Na początku gry Number Shark możesz wybrać rozmiar \"shell square\""));

    const Dialog* about = dialog(*pl, IDD_ABOUTBOX);
    RC_CHECK(about && about->fontWeight == 0 && equals(about->fontName, "MS Shell Dlg"));
    const Control* icon = about ? controlByText(*about, "#128") : nullptr;
    RC_CHECK(icon && equals(icon->cls, "Static") && icon->id == -1);
    RC_CHECK(icon && icon->style == (WS_CHILD | WS_VISIBLE | SS_ICON) && icon->cx == 20 && icon->cy == 20);

    const Dialog* options = dialog(*pl, IDD_DIALOG1);
    const Control* hidden = options ? control(*options, IDC_BUTTON_MAX) : nullptr;
    RC_CHECK(hidden && hidden->style == (WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON | BS_MULTILINE));

    const Dialog* main = dialog(*pl, IDD_MFCZAHLENHAI_DIALOG);
    RC_CHECK(main && main->exStyle == WS_EX_APPWINDOW && (main->style & WS_VISIBLE));
    const Control* shark = main ? control(*main, IDC_STATIC_HAI_LISTE) : nullptr;
    RC_CHECK(shark && equals(shark->text, "#169") && (shark->style & SS_TYPEMASK) == SS_BITMAP);
    const Control* group = main ? controlByText(*main, "Kurs gry - [\"prime\" oznacza liczbę pierwszą]") : nullptr;
    RC_CHECK(group != nullptr);

    const FileResource* bmp = file(*pl, "BITMAP", IDB_BITMAP_NUMBERSHARK_01, nullptr);
    RC_CHECK(bmp && equals(bmp->path, "Bilder/Zahlenhai01.bmp"));

    const AccelTable* acc = accelTable(*pl, IDR_ACCELERATOR1);
    const Accel* rest = acc ? accel(*acc, ID_GIVE_REST, 'N') : nullptr;
    RC_CHECK(rest && rest->flags == (kFVirtKey | kFShift | kFControl | kFNoInvert));
}

} // namespace rctest
