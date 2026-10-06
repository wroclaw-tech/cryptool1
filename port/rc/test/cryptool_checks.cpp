#include "rc_test.h"

#include "afxres.h"
#include "../../../CrypTool/resource.h"

namespace rctest {

static void checkLanguages(const Module& m) {
    static const char* const codes[] = {"pl", "rs", "de", "fr", "de-CH", "en", "es", "el"};
    RC_CHECK(m.languageCount == 8);
    for (int i = 0; i < m.languageCount && i < 8; ++i)
        RC_CHECK(equals(m.languages[i]->code, codes[i]));
    int dialogs = 0, menus = 0, toolbars = 0, accels = 0;
    for (int i = 0; i < m.languageCount; ++i) {
        const Language& l = *m.languages[i];
        dialogs += l.dialogCount;
        menus += l.menuCount;
        toolbars += l.toolbarCount;
        accels += l.accelCount;
        if (!equals(l.code, "de-CH")) {
            RC_CHECK(l.dialogCount >= 150);
            RC_CHECK(l.stringCount >= 1600);
            RC_CHECK(l.version != nullptr);
        }
    }
    RC_CHECK(dialogs >= 1100);
    RC_CHECK(menus == 56);
    RC_CHECK(toolbars == 7);
    RC_CHECK(accels == 7);
}

static void checkPolish(const Language& pl) {
    RC_CHECK(pl.langId == 0x0415);
    RC_CHECK(pl.codepage == 1250);

    const Dialog* props = dialog(pl, IDD_FILE_PROPERTIES);
    RC_CHECK(props != nullptr);
    if (props) {
        RC_CHECK(equals(props->caption, "Właściwości dokumentu"));
        RC_CHECK(equals(props->fontName, "MS Sans Serif") && props->fontSize == 8);
        RC_CHECK(props->style == (DS_SETFONT | DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU));
        RC_CHECK(props->cx == 362 && props->cy == 159);

        const Control* edit = control(*props, IDC_EDIT1);
        RC_CHECK(edit && equals(edit->cls, "Edit") && edit->text == nullptr);
        RC_CHECK(edit && edit->style == (WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL | ES_READONLY));
        RC_CHECK(edit && !(edit->style & WS_BORDER));
        RC_CHECK(edit && edit->x == 103 && edit->y == 66 && edit->cx == 245 && edit->cy == 12);

        const Control* ok = control(*props, IDOK);
        RC_CHECK(ok && equals(ok->cls, "Button") && equals(ok->text, "Zamknij"));
        RC_CHECK(ok && ok->style == (WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON));

        const Control* group = controlByText(*props, "Informacje o aktualnie wybranym dokumencie");
        RC_CHECK(group && group->id == -1 && group->style == (WS_CHILD | WS_VISIBLE | BS_GROUPBOX));

        const Control* label = controlByText(*props, "Nazwa pliku:");
        RC_CHECK(label && equals(label->cls, "Static") && label->style == (WS_CHILD | WS_VISIBLE | WS_GROUP | SS_RIGHT));

        const Control* temp = controlByText(*props, "Nazwa pliku tymczasowego:");
        RC_CHECK(temp && temp->style == (WS_CHILD | WS_VISIBLE | WS_GROUP | SS_LEFT) && temp->exStyle == WS_EX_RIGHT);
    }

    const Dialog* dh = dialog(pl, IDD_DIFFIEHELLMANVISUALIZATION);
    RC_CHECK(dh != nullptr);
    if (dh) {
        RC_CHECK(equals(dh->caption, "Demonstracja protokołu Diffie'ego - Hellmana: wizualizacja wymiany kluczy."));
        const Control* bmpButton = controlByText(*dh, "IDB_DH_BUTTON1_R_");
        RC_CHECK(bmpButton && bmpButton->style == (WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW | BS_BITMAP));
        const Control* bmpStatic = controlByText(*dh, "#550");
        RC_CHECK(bmpStatic && equals(bmpStatic->cls, "Static") && (bmpStatic->style & SS_TYPEMASK) == SS_BITMAP);
    }

    const char* texts[8] = {};
    RC_CHECK(dlgInitTexts(pl, IDD_KEYASYM_GENERATION, IDC_COMBO1, texts, 8) == 4);
    RC_CHECK(equals(texts[0], "512") && equals(texts[1], "768") && equals(texts[2], "1024") && equals(texts[3], "2048"));

    const Dialog* keyasym = dialog(pl, IDD_KEYASYM_GENERATION);
    const Control* combo = keyasym ? control(*keyasym, IDC_COMBO1) : nullptr;
    RC_CHECK(combo && equals(combo->cls, "ComboBox"));
    RC_CHECK(combo && (combo->style & 3) == CBS_DROPDOWN);
}

static void checkEnglish(const Language& en) {
    RC_CHECK(en.langId == 0x0809);
    RC_CHECK(equals(string(en, IDS_CRYPTOOL_RUNTIME_LANGUAGE_STRING), "English"));

    const AccelTable* acc = accelTable(en, IDR_MAINFRAME);
    RC_CHECK(acc != nullptr);
    if (acc) {
        const Accel* copy = accel(*acc, ID_EDIT_COPY, 'C');
        RC_CHECK(copy && copy->flags == (kFVirtKey | kFControl | kFNoInvert));
        const Accel* help = accel(*acc, ID_CONTEXT_HELP, VK_F1);
        RC_CHECK(help && help->flags == (kFVirtKey | kFShift | kFNoInvert));
        const Accel* undo = accel(*acc, ID_EDIT_UNDO, VK_BACK);
        RC_CHECK(undo && undo->flags == (kFVirtKey | kFAlt | kFNoInvert));
    }

    const Menu* mainMenu = menu(en, IDR_MAINFRAME);
    RC_CHECK(mainMenu != nullptr);
    if (mainMenu && mainMenu->itemCount > 3) {
        RC_CHECK(mainMenu->items[0].depth == 0 && mainMenu->items[0].flags == kMfPopup);
        RC_CHECK(equals(mainMenu->items[0].text, "&File"));
        RC_CHECK(mainMenu->items[1].depth == 1 && mainMenu->items[1].id == ID_FILE_NEW);
        RC_CHECK(equals(mainMenu->items[1].text, "&New\tCtrl+N"));
        RC_CHECK(mainMenu->items[3].id == ID_FILE_CLOSE && mainMenu->items[3].flags == kMfGrayed);
    }

    const Toolbar* tb = toolbar(en, IDR_MAINFRAME);
    RC_CHECK(tb && tb->buttonWidth == 16 && tb->buttonHeight == 15 && tb->buttonCount == 15);
    RC_CHECK(tb && tb->buttons[0] == ID_FILE_NEW && tb->buttons[5] == 0);

    const FileResource* bmp = file(en, "BITMAP", IDR_MAINFRAME, nullptr);
    RC_CHECK(bmp && equals(bmp->path, "res/mainfram.bmp"));
    const FileResource* named = file(en, "BITMAP", 0, "PASTEU");
    RC_CHECK(named && named->id == 0 && equals(named->path, "res/EditPaste.bmp"));
    RC_CHECK(file(en, "ICON", IDR_MAINFRAME, nullptr) != nullptr);

    const Dialog* playfair = dialog(en, IDD_PLAYFAIR_ANALYSIS);
    const Control* scroll = playfair ? control(*playfair, IDC_SCROLLBAR1) : nullptr;
    RC_CHECK(scroll && equals(scroll->cls, "ScrollBar") && scroll->style == (WS_CHILD | WS_VISIBLE | SBS_HORZ));

    RC_CHECK(en.version && en.version->fileVersion[0] == 1 && en.version->fileVersion[1] == 4 &&
             en.version->fileVersion[2] == 31 && en.version->fileVersion[3] == 0);
    RC_CHECK(en.version && en.version->keyCount >= 1 && equals(en.version->keys[0], "CompanyName") &&
             equals(en.version->keys[1], "CrypTool Team"));
}

static void checkGreek(const Language& el) {
    RC_CHECK(el.langId == 0x0408);
    RC_CHECK(el.codepage == 1253);
    RC_CHECK(contains(string(el, IDS_RSA_LOG_FOUND_NO_SOLUTION), "ί"));
}

static void checkSwiss(const Language& ch) {
    RC_CHECK(ch.langId == 0x0807 && ch.dialogCount == 1);
    const Dialog* d = dialog(ch, AFX_IDD_NEWTYPEDLG);
    RC_CHECK(d != nullptr);
    if (d) {
        const Control* list = control(*d, AFX_IDC_LISTBOX);
        RC_CHECK(list && equals(list->cls, "ListBox") &&
                 list->style == (WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY | WS_VSCROLL | WS_TABSTOP));
        const Control* label = control(*d, IDC_STATIC);
        RC_CHECK(label && label->style == (WS_CHILD | WS_VISIBLE | SS_LEFT));
    }
}

void runCrypToolChecks() {
    const Module& m = g_rcModule_CrypTool;
    checkLanguages(m);
    if (const Language* pl = language(m, "pl"))
        checkPolish(*pl);
    else
        RC_CHECK(!"Polish language missing");
    if (const Language* en = language(m, "en"))
        checkEnglish(*en);
    else
        RC_CHECK(!"English language missing");
    if (const Language* el = language(m, "el"))
        checkGreek(*el);
    else
        RC_CHECK(!"Greek language missing");
    if (const Language* ch = language(m, "de-CH"))
        checkSwiss(*ch);
    else
        RC_CHECK(!"Swiss German language missing");
}

} // namespace rctest
