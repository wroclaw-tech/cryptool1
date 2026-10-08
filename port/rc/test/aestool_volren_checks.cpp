#include "rc_test.h"

#include "afxres.h"
#include "../../../aestool/Resource.h"
#include "../../../libVolRen/resource.h"

namespace rctest {

void runAesToolVolRenChecks() {
    const Module& aes = g_rcModule_aestool;
    RC_CHECK(aes.languageCount == 7);
    const Language* el = language(aes, "el");
    RC_CHECK(el && startsWith(string(*el, IDS_STRING_DECOK), "Το αποκρυπτογραφημένο αρχείο %s"));
    const Language* de = language(aes, "de");
    RC_CHECK(de && dialog(*de, IDD_AESTOOL_DIALOG) != nullptr);
    RC_CHECK(de && file(*de, "ICON", IDR_MAINFRAME, nullptr) != nullptr);
    const Language* pl = language(aes, "pl");
    RC_CHECK(pl && file(*pl, "CURSOR", 0, "AFX_IDC_CONTEXTHELP") != nullptr);

    const Module& volren = g_rcModule_VolRen;
    RC_CHECK(volren.languageCount == 1);
    const Language* en = language(volren, "en");
    RC_CHECK(en && en->langId == 0x0409 && en->dlgInitCount == 0);
    if (!en)
        return;
    const Dialog* editor = dialog(*en, IDD_EDITOR);
    RC_CHECK(editor && equals(editor->caption, "Transfer Function Dialog") && editor->controlCount == 1);
    RC_CHECK(editor && editor->style == (DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_SETFONT));
    const Control* ctrl = editor ? control(*editor, IDC_GTTRAPEZEDITORCTRL1) : nullptr;
    RC_CHECK(ctrl && equals(ctrl->cls, "{90393A34-2F31-48A4-9323-8CEC42C73954}"));
    RC_CHECK(ctrl && ctrl->style == (WS_CHILD | WS_VISIBLE | WS_TABSTOP) && ctrl->cx == 405 && ctrl->cy == 97);
    RC_CHECK(en->version && en->version->keyCount == 9);
    RC_CHECK(en->version && equals(en->version->keys[2], "FileDescription") && equals(en->version->keys[3], "VolRen DLL"));
}

} // namespace rctest
