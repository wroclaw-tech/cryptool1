// Visual and pixel tests of the GDI emulation. Renders through CDC into bitmaps, checks key
// pixels and writes PNGs (argv[1] or ./gdi_test_output) for inspection.
// Pass --no-window to skip the tests that create a frame (e.g. on a headless CI machine).

#include "gdi_internal.h"

#include <wx/filename.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace mfcwx;

namespace {

int g_checks = 0;
int g_failures = 0;
wxString g_out = "gdi_test_output";
bool g_windowTests = true;

#define CHECK(cond, ...)                                                                                     \
    do {                                                                                                     \
        ++g_checks;                                                                                          \
        if (!(cond)) {                                                                                       \
            ++g_failures;                                                                                    \
            printf("FAIL %s:%d: %s -- ", __FILE__, __LINE__, #cond);                                         \
            printf(__VA_ARGS__);                                                                             \
            printf("\n");                                                                                    \
        }                                                                                                    \
    } while (0)

const COLORREF kWhite = RGB(255, 255, 255);
const COLORREF kBlack = RGB(0, 0, 0);
const COLORREF kRed = RGB(255, 0, 0);
const COLORREF kGreen = RGB(0, 255, 0);
const COLORREF kBlue = RGB(0, 0, 255);
const COLORREF kYellow = RGB(255, 255, 0);

bool Near(COLORREF a, COLORREF b, int tol = 10) {
    return std::abs(GetRValue(a) - GetRValue(b)) <= tol && std::abs(GetGValue(a) - GetGValue(b)) <= tol &&
           std::abs(GetBValue(a) - GetBValue(b)) <= tol;
}

bool Dark(COLORREF c) { return GetRValue(c) + GetGValue(c) + GetBValue(c) < 384; }

struct Box {
    int left = 1 << 30, top = 1 << 30, right = -1, bottom = -1;
    bool Empty() const { return right < 0; }
};

struct Canvas {
    CDC dc;
    CBitmap bmp;
    CBitmap* old = nullptr;
    int w, h;
    Canvas(int width, int height, CDC* compat = nullptr) : w(width), h(height) {
        dc.CreateCompatibleDC(compat);
        bmp.CreateCompatibleBitmap(compat ? compat : &dc, w, h);
        old = dc.SelectObject(&bmp);
        dc.FillSolidRect(0, 0, w, h, kWhite);
    }
    ~Canvas() {
        if (old)
            dc.SelectObject(old);
    }
    COLORREF Px(int x, int y) { return dc.GetPixel(x, y); }
    Box DarkBox(int l, int t, int r, int b) {
        Box box;
        for (int y = t; y < b; ++y)
            for (int x = l; x < r; ++x)
                if (Dark(Px(x, y))) {
                    box.left = std::min(box.left, x);
                    box.top = std::min(box.top, y);
                    box.right = std::max(box.right, x);
                    box.bottom = std::max(box.bottom, y);
                }
        return box;
    }
    void Save(const char* name) {
        wxBitmap* b = BitmapFromHandle(bmp);
        if (b)
            b->SaveFile(g_out + "/" + name + ".png", wxBITMAP_TYPE_PNG);
    }
};

void TestShapes() {
    Canvas c(240, 160);
    CPen pen(PS_SOLID, 1, kBlack);
    CBrush red(kRed);
    CPen* oldPen = c.dc.SelectObject(&pen);
    CBrush* oldBrush = c.dc.SelectObject(&red);
    CHECK(oldPen && oldBrush, "SelectObject returns the default objects");

    c.dc.Rectangle(10, 10, 20, 20);
    CHECK(Near(c.Px(10, 10), kBlack, 2) && c.Px(15, 10) == kBlack && Near(c.Px(19, 19), kBlack, 2) &&
              c.Px(10, 15) == kBlack && c.Px(19, 15) == kBlack && c.Px(15, 19) == kBlack,
          "rectangle outline covers [left,right-1] (%06x %06x)", c.Px(10, 10), c.Px(19, 19));
    CHECK(c.Px(20, 15) == kWhite && c.Px(15, 20) == kWhite, "rectangle excludes right/bottom (%06x %06x)",
          c.Px(20, 15), c.Px(15, 20));
    CHECK(c.Px(15, 15) == kRed && c.Px(11, 11) == kRed, "rectangle fill (%06x)", c.Px(15, 15));

    CBrush blue(kBlue);
    RECT fr{30, 10, 40, 20};
    c.dc.FillRect(&fr, &blue);
    CHECK(c.Px(30, 10) == kBlue && c.Px(39, 19) == kBlue, "FillRect covers [left,right) (%06x)", c.Px(39, 19));
    CHECK(c.Px(40, 15) == kWhite && c.Px(35, 20) == kWhite && c.Px(29, 15) == kWhite, "FillRect excludes right/bottom");

    CBrush green(kGreen);
    c.dc.SelectStockObject(NULL_PEN);
    c.dc.SelectObject(&green);
    c.dc.Rectangle(50, 10, 60, 20);
    CHECK(c.Px(50, 10) == kGreen && c.Px(58, 18) == kGreen, "null-pen rectangle fill (%06x)", c.Px(58, 18));
    CHECK(c.Px(59, 15) == kWhite && c.Px(55, 19) == kWhite, "null-pen rectangle is one pixel smaller (%06x)",
          c.Px(59, 15));
    c.dc.SelectObject(&pen);

    c.dc.MoveTo(10, 30);
    c.dc.LineTo(20, 30);
    CHECK(c.Px(10, 30) == kBlack && c.Px(19, 30) == kBlack, "LineTo draws the start (%06x %06x)", c.Px(10, 30),
          c.Px(19, 30));
    CHECK(c.Px(20, 30) == kWhite && c.Px(15, 29) == kWhite && c.Px(15, 31) == kWhite, "LineTo excludes the end (%06x)",
          c.Px(20, 30));
    CHECK(c.dc.GetCurrentPosition() == CPoint(20, 30), "LineTo updates the position");
    c.dc.MoveTo(25, 25);
    c.dc.LineTo(25, 35);
    CHECK(c.Px(25, 25) == kBlack && c.Px(25, 34) == kBlack && c.Px(25, 35) == kWhite, "vertical LineTo");

    POINT frame[] = {{30, 25}, {30, 40}, {45, 40}, {45, 25}, {30, 25}};
    c.dc.Polyline(frame, 5);
    CHECK(c.Px(30, 25) == kBlack && c.Px(30, 40) == kBlack && c.Px(45, 40) == kBlack && c.Px(45, 25) == kBlack,
          "polyline corners are covered");

    CBrush yellow(kYellow);
    c.dc.SelectObject(&yellow);
    c.dc.Ellipse(70, 10, 90, 30);
    CHECK(c.Px(80, 20) == kYellow, "ellipse fill (%06x)", c.Px(80, 20));
    CHECK(Dark(c.Px(70, 20)) && Dark(c.Px(89, 20)) && Dark(c.Px(80, 10)) && Dark(c.Px(80, 29)),
          "ellipse touches [left,right-1] (%06x %06x)", c.Px(70, 20), c.Px(89, 20));
    CHECK(c.Px(90, 20) == kWhite && c.Px(69, 20) == kWhite && c.Px(80, 30) == kWhite, "ellipse stays in its box (%06x)",
          c.Px(90, 20));

    CPen dash(PS_DASH, 1, kBlack);
    c.dc.SelectObject(&dash);
    c.dc.MoveTo(10, 50);
    c.dc.LineTo(110, 50);
    int on = 0, off = 0;
    for (int x = 10; x < 110; ++x)
        (Dark(c.Px(x, 50)) ? on : off)++;
    CHECK(on > 40 && off > 10, "dashed line has dashes and gaps (%d/%d)", on, off);
    CPen dot(PS_DOT, 1, kBlack);
    c.dc.SelectObject(&dot);
    c.dc.MoveTo(10, 55);
    c.dc.LineTo(110, 55);
    on = off = 0;
    for (int x = 10; x < 110; ++x)
        (Dark(c.Px(x, 55)) ? on : off)++;
    CHECK(on > 20 && off > 20, "dotted line (%d/%d)", on, off);

    CPen wide(PS_SOLID, 5, RGB(0, 0, 128));
    c.dc.SelectObject(&wide);
    c.dc.MoveTo(10, 65);
    c.dc.LineTo(110, 65);
    CHECK(Dark(c.Px(50, 63)) && Dark(c.Px(50, 67)) && !Dark(c.Px(50, 69)), "wide pen width");

    c.dc.SelectObject(&pen);
    c.dc.SelectObject(&red);
    POINT tri[] = {{130, 10}, {170, 10}, {150, 40}};
    c.dc.Polygon(tri, 3);
    CHECK(c.Px(150, 20) == kRed && c.Px(132, 35) == kWhite, "polygon fill");

    CBrush hatch(HS_DIAGCROSS, kBlue);
    c.dc.SetBkColor(kYellow);
    c.dc.SetBkMode(OPAQUE);
    RECT hr{130, 50, 170, 80};
    c.dc.FillRect(&hr, &hatch);
    int yellowCount = 0, blueCount = 0;
    for (int y = 50; y < 80; ++y)
        for (int x = 130; x < 170; ++x) {
            if (Near(c.Px(x, y), kYellow, 40))
                ++yellowCount;
            if (Near(c.Px(x, y), kBlue, 90))
                ++blueCount;
        }
    CHECK(yellowCount > 200 && blueCount > 50, "opaque hatch uses the bk colour (%d yellow %d blue)", yellowCount,
          blueCount);
    c.dc.SetBkMode(TRANSPARENT);
    RECT hr2{175, 50, 215, 80};
    c.dc.FillRect(&hr2, &hatch);
    int whiteCount = 0;
    for (int y = 50; y < 80; ++y)
        for (int x = 175; x < 215; ++x)
            if (c.Px(x, y) == kWhite)
                ++whiteCount;
    CHECK(whiteCount > 200, "transparent hatch keeps the background (%d)", whiteCount);

    c.dc.SetPixel(5, 5, RGB(12, 34, 56));
    CHECK(c.Px(5, 5) == RGB(12, 34, 56), "SetPixel/GetPixel (%06x)", c.Px(5, 5));
    CHECK(c.dc.GetPixel(-1, 5) == CLR_INVALID, "GetPixel outside");

    int saved = c.dc.SaveDC();
    CRgn clip;
    clip.CreateRectRgn(100, 90, 120, 110);
    c.dc.SelectClipRgn(&clip);
    c.dc.FillSolidRect(90, 85, 40, 30, kRed);
    CHECK(c.Px(105, 95) == kRed && c.Px(95, 95) == kWhite && c.Px(125, 95) == kWhite && c.Px(105, 110) == kWhite,
          "clip region limits drawing");
    CRect box;
    c.dc.GetClipBox(&box);
    CHECK(box == CRect(100, 90, 120, 110), "GetClipBox (%d,%d,%d,%d)", box.left, box.top, box.right, box.bottom);
    c.dc.IntersectClipRect(110, 80, 140, 100);
    c.dc.FillSolidRect(90, 85, 40, 30, kGreen);
    CHECK(c.Px(115, 95) == kGreen && c.Px(105, 95) == kRed && c.Px(115, 105) == kRed, "IntersectClipRect");
    c.dc.SelectObject(&yellow);
    CHECK(c.dc.RestoreDC(saved), "RestoreDC");
    CHECK(c.dc.GetCurrentBrush()->m_hObject == red.m_hObject, "RestoreDC restores the brush");
    c.dc.FillSolidRect(90, 120, 40, 10, kBlue);
    CHECK(c.Px(92, 125) == kBlue, "RestoreDC removes the clip region");

    CRgn er;
    er.CreateEllipticRgn(140, 100, 180, 140);
    c.dc.FillRgn(&er, &blue);
    CHECK(c.Px(160, 120) == kBlue && c.Px(141, 101) == kWhite, "elliptic region fill");
    CBrush black(kBlack);
    c.dc.FrameRgn(&er, &black, 1, 1);

    RECT pie{190, 100, 230, 140};
    c.dc.Pie(&pie, CPoint(230, 120), CPoint(210, 100));
    CHECK(c.Px(220, 110) == kRed && c.Px(200, 130) != kRed, "pie quadrant");
    c.dc.RoundRect(190, 10, 230, 40, 10, 10);
    CHECK(c.Px(210, 25) == kRed && c.Px(190, 10) == kWhite, "round rect corners");

    c.dc.SelectObject(oldPen);
    c.dc.SelectObject(oldBrush);
    c.Save("shapes");
}

void TestText() {
    Canvas c(320, 260);
    c.dc.SetBkMode(TRANSPARENT);
    TEXTMETRIC tm;
    CHECK(c.dc.GetTextMetrics(&tm), "GetTextMetrics");
    CHECK(tm.tmHeight > 8 && tm.tmAscent + tm.tmDescent == tm.tmHeight && tm.tmAveCharWidth > 2,
          "metrics h=%d a=%d d=%d ave=%d", tm.tmHeight, tm.tmAscent, tm.tmDescent, tm.tmAveCharWidth);
    CSize hello = c.dc.GetTextExtent("Hello");
    CSize hello2 = c.dc.GetTextExtent("HelloHello");
    CHECK(hello.cy == tm.tmHeight && hello.cx > 10, "extent %dx%d", hello.cx, hello.cy);
    CHECK(std::abs(hello2.cx - 2 * hello.cx) <= 2, "extent additivity %d vs %d", hello2.cx, hello.cx);
    CHECK(c.dc.GetTextExtent("", 0).cx == 0, "empty extent");

    c.dc.SetTextAlign(TA_LEFT | TA_BASELINE);
    c.dc.TextOut(10, 40, "HHH", 3);
    Box b = c.DarkBox(0, 0, 100, 60);
    CHECK(!b.Empty() && b.bottom == 39, "baseline: glyph bottom at y-1 (%d)", b.bottom);
    CHECK(b.left >= 10 && b.left <= 12, "TA_LEFT starts at x (%d)", b.left);

    c.dc.SetTextAlign(TA_LEFT | TA_TOP);
    c.dc.TextOut(110, 10, "HHH", 3);
    b = c.DarkBox(100, 0, 200, 60);
    CHECK(!b.Empty() && b.top >= 10 && b.top <= 10 + tm.tmInternalLeading + 4, "TA_TOP (%d)", b.top);
    int capTop = b.top - 10;

    c.dc.SetTextAlign(TA_RIGHT | TA_TOP);
    c.dc.TextOut(300, 10, "HHH", 3);
    b = c.DarkBox(200, 0, 320, 60);
    CHECK(!b.Empty() && b.right <= 299 && b.right >= 295, "TA_RIGHT ends at x (%d)", b.right);

    c.dc.SetTextAlign(TA_CENTER | TA_BOTTOM);
    c.dc.TextOut(160, 90, "HHHH", 4);
    b = c.DarkBox(100, 60, 220, 100);
    CHECK(!b.Empty() && std::abs((b.left + b.right) / 2 - 160) <= 2, "TA_CENTER (%d..%d)", b.left, b.right);
    CHECK(b.bottom < 90 && b.bottom >= 90 - tm.tmDescent - 2, "TA_BOTTOM (%d)", b.bottom);

    c.dc.SetTextAlign(TA_LEFT | TA_TOP);
    c.dc.SetBkMode(OPAQUE);
    c.dc.SetBkColor(kYellow);
    c.dc.TextOut(10, 60, "Opaque", 6);
    CSize op = c.dc.GetTextExtent("Opaque");
    CHECK(Near(c.Px(10, 60), kYellow) && Near(c.Px(10 + op.cx - 1, 60 + tm.tmHeight - 1), kYellow),
          "opaque text background box");
    CHECK(c.Px(10, 60 + tm.tmHeight) == kWhite && c.Px(10 + op.cx, 60) == kWhite, "opaque box size");
    c.dc.SetBkMode(TRANSPARENT);

    c.dc.MoveTo(10, 100);
    c.dc.SetTextAlign(TA_UPDATECP);
    c.dc.TextOut(0, 0, "ab", 2);
    CHECK(c.dc.GetCurrentPosition().x == 10 + c.dc.GetTextExtent("ab").cx, "TA_UPDATECP advances");
    c.dc.SetTextAlign(TA_LEFT | TA_TOP);

    CRect r(0, 0, 0, 0);
    int hgt = c.dc.DrawText("Hello World", -1, &r, DT_SINGLELINE | DT_CALCRECT);
    CHECK(hgt == tm.tmHeight && r.Height() == tm.tmHeight && r.Width() == c.dc.GetTextExtent("Hello World").cx,
          "DT_CALCRECT single line (%d, %dx%d)", hgt, r.Width(), r.Height());
    r.SetRect(0, 0, 60, 0);
    hgt = c.dc.DrawText("aaa bbb ccc ddd eee", -1, &r, DT_WORDBREAK | DT_CALCRECT);
    CHECK(hgt >= 2 * tm.tmHeight && hgt % tm.tmHeight == 0 && r.right <= 60, "DT_WORDBREAK wraps (%d, right %d)", hgt,
          r.right);
    r.SetRect(0, 0, 200, 0);
    hgt = c.dc.DrawText("a\nb\r\nc", -1, &r, DT_CALCRECT);
    CHECK(hgt == 3 * tm.tmHeight, "line breaks (%d)", hgt);
    r.SetRect(0, 0, 0, 0);
    c.dc.DrawText("&File", -1, &r, DT_SINGLELINE | DT_CALCRECT);
    CHECK(r.Width() == c.dc.GetTextExtent("File").cx, "prefix removed (%d)", r.Width());
    r.SetRect(0, 0, 0, 0);
    c.dc.DrawText("&File", -1, &r, DT_SINGLELINE | DT_CALCRECT | DT_NOPREFIX);
    CHECK(r.Width() == c.dc.GetTextExtent("&File").cx, "DT_NOPREFIX keeps '&'");
    r.SetRect(0, 0, 0, 0);
    c.dc.DrawText("a\tb", -1, &r, DT_SINGLELINE | DT_CALCRECT | DT_EXPANDTABS);
    CHECK(std::abs(r.Width() - (8 * tm.tmAveCharWidth + c.dc.GetTextExtent("b").cx)) <= 2, "DT_EXPANDTABS (%d)",
          r.Width());

    CRect cr(10, 120, 190, 160);
    c.dc.FrameRect(&cr, CBrush::FromHandle(::GetStockObject(GRAY_BRUSH)));
    int ret = c.dc.DrawText("Centered", -1, &cr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    b = c.DarkBox(11, 121, 189, 159);
    CHECK(!b.Empty() && std::abs((b.left + b.right) / 2 - 100) <= 2, "DT_CENTER (%d..%d)", b.left, b.right);
    CHECK(std::abs((b.top + b.bottom) / 2 - 140) <= 4, "DT_VCENTER (%d..%d)", b.top, b.bottom);
    CHECK(ret == (40 - tm.tmHeight) / 2 + tm.tmHeight, "DT_VCENTER return value (%d)", ret);

    CRect rr(200, 120, 300, 140);
    c.dc.DrawText("Right", -1, &rr, DT_RIGHT | DT_SINGLELINE);
    b = c.DarkBox(200, 120, 320, 140);
    CHECK(!b.Empty() && b.right >= 296 && b.right <= 299, "DT_RIGHT (%d)", b.right);

    CRect er(10, 170, 70, 190);
    c.dc.DrawText("abcdefghijklmnopqrstuvwxyz", -1, &er, DT_SINGLELINE | DT_END_ELLIPSIS);
    b = c.DarkBox(0, 170, 320, 190);
    CHECK(!b.Empty() && b.right < 70, "DT_END_ELLIPSIS fits (%d)", b.right);
    CRect clipR(80, 170, 120, 190);
    c.dc.DrawText("abcdefghijklmnopqrstuvwxyz", -1, &clipR, DT_SINGLELINE);
    b = c.DarkBox(75, 170, 320, 190);
    CHECK(!b.Empty() && b.right < 120, "DrawText clips to the rectangle (%d)", b.right);
    CRect noclip(130, 170, 150, 190);
    c.dc.DrawText("abcdefghij", -1, &noclip, DT_SINGLELINE | DT_NOCLIP);
    b = c.DarkBox(125, 170, 320, 190);
    CHECK(b.right > 150, "DT_NOCLIP draws outside (%d)", b.right);

    CRect pr(10, 200, 100, 220);
    c.dc.DrawText("&Underline", -1, &pr, DT_SINGLELINE);
    int under = 0;
    for (int x = 10; x < 10 + c.dc.GetTextExtent("U").cx; ++x)
        if (Dark(c.Px(x, 200 + tm.tmAscent + 1)))
            ++under;
    CHECK(under >= 3, "mnemonic underline (%d)", under);

    CRect wr(120, 200, 200, 260);
    c.dc.DrawText("word wrapped text in a box", -1, &wr, DT_WORDBREAK | DT_CENTER);
    b = c.DarkBox(115, 200, 320, 260);
    CHECK(b.right < 200 && b.bottom > 200 + tm.tmHeight, "word wrap drawing (%d,%d)", b.right, b.bottom);

    c.Save("text");
    (void)capTop;
}

void TestFonts() {
    Canvas c(420, 260);
    c.dc.SetBkMode(TRANSPARENT);
    CFont arial;
    arial.CreateFont(-20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                     CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
    CFont* old = c.dc.SelectObject(&arial);
    TEXTMETRIC tm;
    c.dc.GetTextMetrics(&tm);
    CHECK(std::abs(tm.tmHeight - tm.tmInternalLeading - 20) <= 1, "negative lfHeight is the em height (%d-%d)",
          tm.tmHeight, tm.tmInternalLeading);
    c.dc.TextOut(10, 10, "Arial -20: The quick brown fox", 30);

    CFont cell;
    cell.CreateFont(30, 0, 0, 0, FW_BOLD, TRUE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    DEFAULT_QUALITY, DEFAULT_PITCH | FF_ROMAN, "Times New Roman");
    c.dc.SelectObject(&cell);
    c.dc.GetTextMetrics(&tm);
    CHECK(std::abs(tm.tmHeight - 30) <= 1, "positive lfHeight is the cell height (%d)", tm.tmHeight);
    CHECK(tm.tmWeight == FW_BOLD && tm.tmItalic, "weight/italic in metrics");
    c.dc.TextOut(10, 40, "Times 30 bold italic", 20);

    CFont courier;
    courier.CreatePointFont(100, "Courier New");
    LOGFONT lf;
    courier.GetLogFont(&lf);
    CHECK(lf.lfHeight == -13, "CreatePointFont(100) -> lfHeight -13 (%d)", lf.lfHeight);
    c.dc.SelectObject(&courier);
    CHECK(c.dc.GetTextExtent("iiii").cx == c.dc.GetTextExtent("WWWW").cx, "Courier New maps to a monospace font");
    c.dc.GetTextMetrics(&tm);
    CHECK((tm.tmPitchAndFamily & 1) == 0, "TMPF_FIXED_PITCH clear for fixed fonts (%x)", tm.tmPitchAndFamily);
    c.dc.TextOut(10, 80, "Courier New 10pt: 0123456789 ABCDEF", 35);
    CString face;
    c.dc.GetTextFace(face);
    CHECK(face == "Courier New", "GetTextFace (%s)", face.GetString());

    CFont fixedStock;
    fixedStock.CreateStockObject(SYSTEM_FIXED_FONT);
    LOGFONT slf;
    fixedStock.GetLogFont(&slf);
    CHECK((slf.lfPitchAndFamily & 3) == FIXED_PITCH, "SYSTEM_FIXED_FONT is fixed pitch");
    CHECK(FontFromHandle(fixedStock).IsFixedWidth(), "SYSTEM_FIXED_FONT wx font is fixed");
    c.dc.SelectObject(&fixedStock);
    c.dc.TextOut(10, 100, "SYSTEM_FIXED_FONT", 17);

    LOGFONT gui;
    ::GetObject(DefaultGuiFont(), sizeof gui, &gui);
    CHECK(strcmp(gui.lfFaceName, "MS Shell Dlg") == 0 && gui.lfHeight < 0, "DEFAULT_GUI_FONT LOGFONT (%s %d)",
          gui.lfFaceName, gui.lfHeight);
    gui.lfWeight = FW_BOLD;
    CFont bold;
    bold.CreateFontIndirect(&gui);
    CHECK(FontFromHandle(bold).GetWeight() == wxFONTWEIGHT_BOLD, "bold copy of the GUI font");
    CHECK(FontFromHandle(bold).GetFaceName() == FontFromHandle(DefaultGuiFont()).GetFaceName(),
          "GUI font face round-trips (%s)", (const char*)FontFromHandle(bold).GetFaceName().utf8_str());
    c.dc.SelectObject(&bold);
    c.dc.TextOut(10, 120, "Bold GUI font", 13);

    LOGFONT garbage;
    memset(&garbage, 0xCC, sizeof garbage);
    garbage.lfWeight = FW_BOLD;
    CFont g;
    CHECK(g.CreateFontIndirect(&garbage) && FontFromHandle(g).IsOk(), "uninitialized LOGFONT is tolerated");

    CFont underline;
    underline.CreateFont(-16, 0, 0, 0, FW_NORMAL, FALSE, TRUE, TRUE, 0, 0, 0, 0, 0, "Tahoma");
    c.dc.SelectObject(&underline);
    c.dc.TextOut(10, 140, "Tahoma underline strikeout", 26);

    CFont unknown;
    unknown.CreateFont(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "ArialBold");
    c.dc.SelectObject(&unknown);
    c.dc.TextOut(10, 165, "ArialBold (unknown face) 15", 27);

    CFont fixedNoFace;
    fixedNoFace.CreateFont(0, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, OUT_RASTER_PRECIS, 0, DEFAULT_QUALITY,
                           FIXED_PITCH | FF_DONTCARE, nullptr);
    CHECK(FontFromHandle(fixedNoFace).IsFixedWidth(), "FIXED_PITCH without a face is monospace");
    c.dc.SelectObject(&fixedNoFace);
    c.dc.TextOut(10, 185, "FIXED_PITCH no face", 19);

    c.dc.SetTextColor(RGB(200, 0, 0));
    c.dc.SelectObject(&arial);
    c.dc.TextOut(10, 205, "\xC4\xD6\xDC \xE4\xF6\xFC \xDF \x80 (cp1252)", -1);
    c.dc.SelectObject(old);
    c.Save("fonts");
}

void TestBlit() {
    Canvas src(40, 40);
    src.dc.FillSolidRect(0, 0, 20, 40, kRed);
    src.dc.FillSolidRect(20, 0, 20, 40, kBlue);
    Canvas d(260, 140);

    d.dc.BitBlt(10, 10, 40, 40, &src.dc, 0, 0, SRCCOPY);
    CHECK(d.Px(15, 15) == kRed && d.Px(45, 15) == kBlue && d.Px(50, 15) == kWhite && d.Px(10, 50) == kWhite,
          "SRCCOPY (%06x %06x)", d.Px(15, 15), d.Px(45, 15));
    d.dc.BitBlt(60, 10, 40, 40, &src.dc, 0, 0, NOTSRCCOPY);
    CHECK(d.Px(65, 15) == RGB(0, 255, 255) && d.Px(95, 15) == kYellow, "NOTSRCCOPY (%06x)", d.Px(65, 15));

    const COLORREF gray = RGB(128, 128, 128);
    d.dc.FillSolidRect(110, 10, 40, 40, gray);
    d.dc.BitBlt(110, 10, 40, 40, &src.dc, 0, 0, SRCAND);
    CHECK(d.Px(115, 15) == RGB(128, 0, 0) && d.Px(145, 15) == RGB(0, 0, 128), "SRCAND (%06x)", d.Px(115, 15));
    d.dc.FillSolidRect(160, 10, 40, 40, gray);
    d.dc.BitBlt(160, 10, 40, 40, &src.dc, 0, 0, SRCPAINT);
    CHECK(d.Px(165, 15) == RGB(255, 128, 128), "SRCPAINT (%06x)", d.Px(165, 15));
    d.dc.FillSolidRect(210, 10, 40, 40, gray);
    d.dc.BitBlt(210, 10, 40, 40, &src.dc, 0, 0, SRCINVERT);
    CHECK(d.Px(215, 15) == RGB(127, 128, 128), "SRCINVERT (%06x)", d.Px(215, 15));

    CBrush green(kGreen);
    d.dc.SelectObject(&green);
    d.dc.PatBlt(10, 60, 20, 20, PATCOPY);
    d.dc.PatBlt(35, 60, 20, 20, BLACKNESS);
    d.dc.FillSolidRect(60, 60, 20, 20, kRed);
    d.dc.PatBlt(60, 60, 10, 20, DSTINVERT);
    CHECK(d.Px(15, 65) == kGreen && d.Px(40, 65) == kBlack && d.Px(62, 65) == RGB(0, 255, 255) &&
              d.Px(75, 65) == kRed,
          "PatBlt PATCOPY/BLACKNESS/DSTINVERT");
    d.dc.InvertRect(CRect(70, 60, 80, 70));
    CHECK(d.Px(75, 65) == RGB(0, 255, 255), "InvertRect");

    d.dc.StretchBlt(90, 60, 80, 20, &src.dc, 0, 0, 40, 10, SRCCOPY);
    CHECK(d.Px(92, 70) == kRed && d.Px(168, 70) == kBlue && d.Px(130, 79) != kWhite && d.Px(130, 80) == kWhite,
          "StretchBlt");
    d.dc.StretchBlt(250, 90, -60, 20, &src.dc, 0, 0, 40, 40, SRCCOPY);
    CHECK(d.Px(195, 95) == kBlue && d.Px(245, 95) == kRed, "StretchBlt mirrored (%06x %06x)", d.Px(195, 95),
          d.Px(245, 95));

    Canvas mask(40, 40);
    mask.dc.FillSolidRect(10, 10, 20, 20, kRed);
    d.dc.FillSolidRect(10, 90, 40, 40, kYellow);
    d.dc.TransparentBlt(10, 90, 40, 40, &mask.dc, 0, 0, 40, 40, kWhite);
    CHECK(d.Px(12, 92) == kYellow && d.Px(25, 105) == kRed, "TransparentBlt");

    unsigned char dib[] = {255, 0, 0, 0, 255, 0, 0, 0, 0, 0, 255, 255, 255, 255, 255, 0};
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof bmi);
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = 2;
    bmi.bmiHeader.biHeight = 2;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    ::StretchDIBits(d.dc, 60, 90, 2, 2, 0, 0, 2, 2, dib, &bmi, DIB_RGB_COLORS, SRCCOPY);
    CHECK(d.Px(60, 90) == kRed && d.Px(61, 90) == kWhite && d.Px(60, 91) == kBlue && d.Px(61, 91) == kGreen,
          "bottom-up DIB orientation (%06x %06x %06x %06x)", d.Px(60, 90), d.Px(61, 90), d.Px(60, 91), d.Px(61, 91));
    ::StretchDIBits(d.dc, 70, 90, 20, 20, 0, 0, 2, 2, dib, &bmi, DIB_RGB_COLORS, SRCCOPY);
    CHECK(d.Px(75, 95) == kRed && d.Px(85, 105) == kGreen, "StretchDIBits scaled");
    ::SetDIBitsToDevice(d.dc, 95, 90, 2, 2, 0, 0, 0, 2, dib, &bmi, DIB_RGB_COLORS);
    CHECK(d.Px(95, 90) == kRed && d.Px(96, 91) == kGreen, "SetDIBitsToDevice");
    d.Save("blit");
}

void TestBitmaps() {
    const unsigned char bits[] = {0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 0, 255, 255, 255, 0,
                                  1, 2, 3, 0,  4, 5,  6, 0, 7, 8,   9, 0, 10, 11, 12, 0};
    CBitmap b;
    CHECK(b.CreateBitmap(4, 2, 1, 32, bits), "CreateBitmap 32bpp");
    BITMAP bm;
    b.GetBitmap(&bm);
    CHECK(bm.bmWidth == 4 && bm.bmHeight == 2 && bm.bmBitsPixel == 32 && bm.bmWidthBytes == 16 && bm.bmPlanes == 1,
          "GetBitmap %dx%d %dbpp %d", bm.bmWidth, bm.bmHeight, bm.bmBitsPixel, bm.bmWidthBytes);
    unsigned char back[32];
    CHECK(b.GetBitmapBits(sizeof back, back) == 32 && memcmp(back, bits, 32) == 0, "GetBitmapBits round trip");
    CDC dc;
    dc.CreateCompatibleDC(nullptr);
    CBitmap* old = dc.SelectObject(&b);
    CHECK(dc.GetPixel(0, 0) == kRed && dc.GetPixel(1, 0) == kGreen && dc.GetPixel(3, 0) == kWhite &&
              dc.GetPixel(0, 1) == RGB(3, 2, 1),
          "pixels of a 32bpp DDB (%06x)", dc.GetPixel(0, 0));
    dc.SetPixel(2, 1, kYellow);
    unsigned char after[32];
    b.GetBitmapBits(sizeof after, after);
    CHECK(after[24] == 0 && after[25] == 255 && after[26] == 255, "GetBitmapBits sees drawing in a selected bitmap");
    unsigned char set[32];
    memset(set, 0x40, sizeof set);
    b.SetBitmapBits(sizeof set, set);
    CHECK(dc.GetPixel(1, 1) == RGB(0x40, 0x40, 0x40), "SetBitmapBits on a selected bitmap (%06x)", dc.GetPixel(1, 1));
    dc.SelectObject(old);

    const unsigned char mono[] = {0xAA, 0x55};
    CBitmap m;
    m.CreateBitmap(16, 1, 1, 1, mono);
    m.GetBitmap(&bm);
    CHECK(bm.bmBitsPixel == 1 && bm.bmWidthBytes == 2, "monochrome GetBitmap");
    old = dc.SelectObject(&m);
    CHECK(dc.GetPixel(0, 0) == kWhite && dc.GetPixel(1, 0) == kBlack && dc.GetPixel(9, 0) == kWhite,
          "monochrome bits (1 = white)");
    dc.SelectObject(old);

    CBitmap res;
    CHECK(res.LoadBitmap(1), "LoadBitmap through LoadBitmapResource");
    res.GetBitmap(&bm);
    CHECK(bm.bmWidth == 4 && bm.bmHeight == 2, "loaded bitmap size");
    old = dc.SelectObject(&res);
    CHECK(dc.GetPixel(0, 0) == kRed && dc.GetPixel(3, 1) == kBlue, "loaded bitmap pixels");
    dc.SelectObject(old);
    CHECK(!CBitmap().LoadBitmap(2), "missing bitmap");

    CBitmap other;
    other.CreateCompatibleBitmap(&dc, 2, 2);
    CDC dc2;
    dc2.CreateCompatibleDC(nullptr);
    dc.SelectObject(&other);
    CHECK(dc2.SelectObject(&other) == nullptr, "a bitmap can be selected into one DC only");

    HBITMAP h = CreateBitmapHandle(wxBitmap(3, 5, 24));
    CHECK(GetObject(h, sizeof bm, &bm) && bm.bmWidth == 3 && bm.bmHeight == 5, "CreateBitmapHandle");
    CHECK(DeleteObject(h) && !DeleteObject(h), "DeleteObject once");
    CBrush pattern(&res);
    CHECK(pattern.m_hObject && BrushFromHandle(pattern).GetStyle() == wxBRUSHSTYLE_STIPPLE, "pattern brush");
}

void TestMapping() {
    Canvas c(200, 200);
    c.dc.SetMapMode(MM_ANISOTROPIC);
    c.dc.SetWindowExt(1280, 1280);
    c.dc.SetViewportExt(200, -200);
    c.dc.SetViewportOrg(0, 200);
    CPoint p[2] = {CPoint(0, 0), CPoint(1280, 1280)};
    c.dc.LPtoDP(p, 2);
    CHECK(p[0] == CPoint(0, 200) && p[1] == CPoint(200, 0), "LPtoDP (%d,%d) (%d,%d)", p[0].x, p[0].y, p[1].x, p[1].y);
    CPoint q(100, 100);
    c.dc.DPtoLP(&q);
    CHECK(q == CPoint(640, 640), "DPtoLP (%d,%d)", q.x, q.y);
    CPen pen(PS_SOLID, 1, kBlack);
    CBrush red(kRed);
    c.dc.SelectObject(&pen);
    c.dc.SelectObject(&red);
    c.dc.Rectangle(0, 0, 640, 640);
    CSize ext = c.dc.GetTextExtent("Plot");
    CSize dev = ext;
    c.dc.LPtoDP(&dev);
    CHECK(ext.cx > dev.cx * 5, "text extent in logical units (%d vs %d)", ext.cx, dev.cx);
    TEXTMETRIC tm;
    c.dc.GetTextMetrics(&tm);
    CHECK(tm.tmHeight > 40, "metrics in logical units (%d)", tm.tmHeight);
    c.dc.SetBkMode(TRANSPARENT);
    c.dc.SetTextAlign(TA_CENTER | TA_BASELINE);
    c.dc.TextOut(640, 1100, "Headline", 8);
    CPen wide(PS_SOLID, 13, kBlue);
    c.dc.SelectObject(&wide);
    c.dc.MoveTo(700, 300);
    c.dc.LineTo(1200, 300);
    c.dc.SelectObject(&pen);
    c.dc.SetMapMode(MM_TEXT);
    c.dc.SetViewportOrg(0, 0);
    CHECK(c.Px(50, 150) == kRed && c.Px(150, 50) == kWhite, "y-up rectangle placement");
    CHECK(c.Px(0, 150) == kBlack && c.Px(99, 150) == kBlack && c.Px(100, 150) == kWhite, "y-up rectangle edges");
    Box b = c.DarkBox(0, 0, 200, 60);
    CHECK(!b.Empty() && b.bottom == 27 && std::abs((b.left + b.right) / 2 - 100) <= 2,
          "text at a mapped point (%d %d..%d)", b.bottom, b.left, b.right);
    int thick = 0;
    for (int y = 145; y < 160; ++y)
        if (Near(c.Px(150, y), kBlue, 60))
            ++thick;
    CHECK(thick == 2, "pen width scales with the mapping (%d)", thick);
    c.Save("mapping");

    CDC m;
    m.CreateCompatibleDC(nullptr);
    m.SetMapMode(MM_LOMETRIC);
    CPoint lm(254, -254);
    m.LPtoDP(&lm);
    CHECK(lm == CPoint(96, 96), "MM_LOMETRIC (%d,%d)", lm.x, lm.y);
    m.SetMapMode(MM_ISOTROPIC);
    m.SetWindowExt(100, 100);
    m.SetViewportExt(200, 100);
    CHECK(m.GetViewportExt() == CSize(100, 100), "MM_ISOTROPIC keeps the aspect ratio");
    m.SetMapMode(MM_TEXT);
    m.SetWindowOrg(-10, -10);
    CPoint o(0, 0);
    m.LPtoDP(&o);
    CHECK(o == CPoint(10, 10), "window origin in MM_TEXT");
    CHECK(m.GetDeviceCaps(LOGPIXELSY) == 96, "LOGPIXELSY");
}

void TestRegionsAndHandles() {
    CRgn a, b, c;
    a.CreateRectRgn(0, 0, 10, 10);
    b.CreateRectRgn(5, 5, 15, 15);
    c.CreateRectRgn(0, 0, 0, 0);
    CHECK(c.CombineRgn(&a, &b, RGN_OR) == COMPLEXREGION, "RGN_OR");
    CHECK(c.PtInRegion(12, 12) && !c.PtInRegion(12, 2), "PtInRegion");
    CRect box;
    c.GetRgnBox(&box);
    CHECK(box == CRect(0, 0, 15, 15), "GetRgnBox");
    CHECK(c.CombineRgn(&a, &b, RGN_AND) == SIMPLEREGION, "RGN_AND");
    c.GetRgnBox(&box);
    CHECK(box == CRect(5, 5, 10, 10), "intersection box");
    CHECK(c.CombineRgn(&a, &b, RGN_DIFF) == COMPLEXREGION && !c.PtInRegion(7, 7), "RGN_DIFF");
    c.OffsetRgn(100, 0);
    CHECK(c.PtInRegion(101, 1), "OffsetRgn");

    CGdiObject* t1 = CGdiObject::FromHandle(GetStockObject(BLACK_PEN));
    CGdiObject* t2 = CGdiObject::FromHandle(GetStockObject(BLACK_PEN));
    CHECK(t1 && t1 == t2 && t1->IsKindOf(RUNTIME_CLASS(CPen)), "temporary wrappers are reused and typed");
    CPen perm(PS_SOLID, 1, kRed);
    CHECK(CGdiObject::FromHandle(perm.m_hObject) == &perm, "FromHandle returns the permanent object");
    PurgeTemporaryGdiWrappers();
    CHECK(DeleteObject(GetStockObject(WHITE_BRUSH)), "deleting a stock object succeeds");

    {
        CBitmap bmp;
        bmp.CreateCompatibleBitmap(nullptr, 10, 10);
        CDC memdc;
        memdc.CreateCompatibleDC(nullptr);
        memdc.SelectObject(&bmp);
        ::DeleteDC(memdc);
        ::DeleteObject(bmp);
        CHECK(memdc.m_hDC == nullptr && bmp.m_hObject == nullptr, "Win32 deletes clear the MFC wrappers");
    }

    Canvas cv(40, 20);
    {
        CPen temp(PS_SOLID, 1, kRed);
        cv.dc.SelectObject(&temp);
    }
    cv.dc.MoveTo(0, 5);
    cv.dc.LineTo(30, 5);
    CHECK(cv.Px(10, 5) == kRed, "a pen destroyed while selected stays usable");
    cv.dc.SelectStockObject(BLACK_PEN);

    HDC raw = ::CreateCompatibleDC(nullptr);
    CDC* tdc = CDC::FromHandle(raw);
    CHECK(tdc && tdc->m_hDC == raw && CDC::FromHandle(raw) == tdc, "temporary CDC");
    ::DeleteDC(raw);
    CHECK(tdc->m_hDC == nullptr, "DeleteDC detaches the temporary CDC");
    CDC::DeleteTempMap();

    CHECK(GetObject(GetStockObject(BLACK_PEN), 0, nullptr) == sizeof(LOGPEN), "GetObject size query");
    LOGBRUSH lb;
    CHECK(GetObject(reinterpret_cast<HGDIOBJ>(COLOR_WINDOW + 1), sizeof lb, &lb) == sizeof lb &&
              lb.lbColor == GetSysColor(COLOR_WINDOW),
          "system colour brush handle");
}

void TestWrapAndColors() {
    wxBitmap target(120, 60, 24);
    {
        wxMemoryDC mdc(target);
        mdc.SetBackground(*wxWHITE_BRUSH);
        mdc.Clear();
        CDC* cdc = WrapDC(&mdc, nullptr);
        cdc->SetTextColor(0xFFFFFFFF);
        CHECK(cdc->GetTextColor() == 0xFFFFFFFF, "CTLCOLOR sentinel text colour is preserved");
        cdc->SetBkMode(TRANSPARENT);
        CHECK(cdc->GetBkMode() == TRANSPARENT, "bk mode");
        CHECK(CDC::FromHandle(cdc->m_hDC) == cdc, "FromHandle of a wrapped DC");
        CPen pen(PS_SOLID, 1, kBlack);
        CBrush br(kRed);
        cdc->SelectObject(&pen);
        cdc->SelectObject(&br);
        cdc->Rectangle(10, 10, 20, 20);
        RECT fr{30, 10, 40, 20};
        ::FillRect(cdc->m_hDC, &fr, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
        RECT er{50, 10, 70, 30};
        cdc->DrawEdge(&er, EDGE_RAISED, BF_RECT | BF_ADJUST);
        CHECK(er.left == 52 && er.top == 12 && er.right == 68 && er.bottom == 28, "BF_ADJUST");
        cdc->Draw3dRect(80, 10, 20, 20, kBlue, kGreen);
        UnwrapDC(cdc);
    }
    wxImage img = target.ConvertToImage();
    auto px = [&](int x, int y) { return RGB(img.GetRed(x, y), img.GetGreen(x, y), img.GetBlue(x, y)); };
    CHECK(Near(px(10, 10), kBlack, 2) && Near(px(19, 19), kBlack, 2) && px(20, 20) == kWhite && px(15, 15) == kRed,
          "rectangle on a borrowed wx DC");
    CHECK(Near(px(35, 15), GetSysColor(COLOR_WINDOW), 2), "FillRect with (HBRUSH)(COLOR_WINDOW+1)");
    CHECK(Near(px(50, 10), GetSysColor(COLOR_3DLIGHT), 2) && Near(px(69, 29), GetSysColor(COLOR_3DDKSHADOW), 2) &&
              Near(px(51, 11), GetSysColor(COLOR_3DHILIGHT), 2) && Near(px(68, 28), GetSysColor(COLOR_3DSHADOW), 2),
          "DrawEdge colours");
    CHECK(px(80, 10) == kBlue && px(99, 29) == kGreen && px(81, 11) == kWhite, "Draw3dRect");
    target.SaveFile(g_out + "/wrapped.png", wxBITMAP_TYPE_PNG);

    CHECK(BrushFromHandle(GetStockObject(NULL_BRUSH)).GetStyle() == wxBRUSHSTYLE_TRANSPARENT, "NULL_BRUSH");
    CHECK(!BrushFromHandle(nullptr).IsOk(), "null brush handle");
    CHECK(BrushFromHandle(GetSysColorBrush(COLOR_BTNFACE)).GetColour() == ToWxColour(GetSysColor(COLOR_BTNFACE)),
          "GetSysColorBrush");
    CHECK(FromWxColour(ToWxColour(RGB(1, 2, 3))) == RGB(1, 2, 3), "colour round trip");
    CHECK(PenFromHandle(GetStockObject(NULL_PEN)).GetStyle() == wxPENSTYLE_TRANSPARENT, "NULL_PEN");
    HFONT fh = FontHandleFor(*wxNORMAL_FONT);
    CHECK(fh == FontHandleFor(*wxNORMAL_FONT) && FontFromHandle(fh).IsOk(), "FontHandleFor caches");
}

void TestHiDpi() {
    wxBitmap hb;
    hb.CreateWithDIPSize(wxSize(120, 40), 2.0, 24);
    wxMemoryDC hm(hb);
    hm.SetBackground(*wxWHITE_BRUSH);
    hm.Clear();
    CDC* wrapped = WrapDC(&hm, nullptr);
    {
        CDC mem;
        mem.CreateCompatibleDC(wrapped);
        CBitmap cb;
        cb.CreateCompatibleBitmap(&mem, 120, 40);
        CHECK(BitmapFromHandle(cb)->GetScaleFactor() == 2.0, "compatible bitmap inherits the scale factor");
        BITMAP bm;
        cb.GetBitmap(&bm);
        CHECK(bm.bmWidth == 120 && bm.bmHeight == 40, "BITMAP reports logical size");
        CBitmap* old = mem.SelectObject(&cb);
        mem.FillSolidRect(0, 0, 120, 40, RGB(230, 240, 255));
        mem.FillSolidRect(100, 10, 10, 10, kRed);
        CHECK(mem.GetPixel(105, 15) == kRed && mem.GetPixel(110, 15) == RGB(230, 240, 255),
              "GetPixel in logical coordinates on a 2x bitmap");
        mem.SetBkMode(TRANSPARENT);
        mem.TextOut(4, 10, "HiDPI text", 10);
        wrapped->BitBlt(0, 0, 120, 40, &mem, 0, 0, SRCCOPY);
        mem.SelectObject(old);
    }
    UnwrapDC(wrapped);
    hm.SelectObject(wxNullBitmap);
    CHECK(hb.GetWidth() == 240, "physical size %d", hb.GetWidth());
    wxImage img = hb.ConvertToImage();
    CHECK(img.GetRed(210, 30) == 255 && img.GetGreen(210, 30) == 0, "blit of a 2x bitmap to a 2x DC");
    hb.SaveFile(g_out + "/hidpi.png", wxBITMAP_TYPE_PNG);
}

bool WaitFor(const std::function<bool()>& done) {
    for (int i = 0; i < 200 && !done(); ++i) {
        wxYield();
        wxMilliSleep(10);
    }
    return done();
}

void TestWindows() {
    auto* frame = new wxFrame(nullptr, wxID_ANY, "mfcwx gdi test", wxPoint(50, 50), wxSize(320, 240));
    auto* panel = new wxPanel(frame, wxID_ANY, wxPoint(0, 0), wxSize(300, 200));
    HWND hwnd = ToHwnd(panel);

    HDC h = ::GetDC(hwnd);
    CDC* cdc = CDC::FromHandle(h);
    CHECK(cdc && cdc->GetTextExtent("Measure").cx > 10, "measuring through GetDC outside paint");
    TEXTMETRIC tm;
    CHECK(cdc->GetTextMetrics(&tm) && tm.tmHeight > 8, "metrics through GetDC");
    cdc->FillSolidRect(10, 10, 30, 20, kRed);
    ::ReleaseDC(hwnd, h);
#ifdef __WXOSX__
    CHECK(HasClientDCOverlay(panel), "client DC drawing goes to the overlay on macOS");
#endif
    if (HasClientDCOverlay(panel)) {
        wxBitmap out(panel->GetClientSize().x, panel->GetClientSize().y, 24);
        {
            wxMemoryDC md(out);
            md.SetBackground(*wxWHITE_BRUSH);
            md.Clear();
            PaintClientDCOverlay(panel, md);
        }
        wxImage img = out.ConvertToImage();
        CHECK(img.GetRed(15, 15) == 255 && img.GetGreen(15, 15) == 0 && img.GetRed(45, 15) == 255 &&
                  img.GetGreen(45, 15) == 255,
              "overlay content");
        ClearClientDCOverlay(panel);
        CHECK(!HasClientDCOverlay(panel), "overlay cleared");
    }

    HDC leaked = ::GetDC(hwnd);
    bool bridged = false, borrowed = false, ownSession = false, painted = false;
    int pass = 0;
    panel->Bind(wxEVT_PAINT, [&](wxPaintEvent&) {
        ++pass;
        if (pass == 1) {
            wxPaintDC pdc(panel);
            CDC* w = WrapDC(&pdc, panel);
            PAINTSTRUCT ps;
            HDC p = ::BeginPaint(hwnd, &ps);
            HDC g = ::GetDC(hwnd);
            bridged = DCFromHandle(p)->Target(true) == &pdc;
            borrowed = DCFromHandle(g)->Target(true) == &pdc;
            CDC::FromHandle(g)->FillSolidRect(0, 0, 20, 20, kBlue);
            ::EndPaint(hwnd, &ps);
            HDC inPaintLeak = ::GetDC(hwnd);
            CDC::FromHandle(inPaintLeak)->FillSolidRect(20, 0, 20, 20, kGreen);
            PaintClientDCOverlay(panel, pdc);
            UnwrapDC(w);
            CHECK(DCFromHandle(inPaintLeak)->dc == nullptr, "leaked paint DC detached at UnwrapDC");
            ::ReleaseDC(hwnd, g);
        } else {
            PAINTSTRUCT ps;
            HDC p = ::BeginPaint(hwnd, &ps);
            HDC g = ::GetDC(hwnd);
            wxDC* pd = DCFromHandle(p)->Target(true);
            ownSession = pd && DCFromHandle(g)->Target(true) == pd;
            CDC::FromHandle(g)->FillSolidRect(0, 0, 20, 20, kRed);
            ::ReleaseDC(hwnd, g);
            ::EndPaint(hwnd, &ps);
            painted = true;
        }
    });
    {
        frame->Show();
        panel->Refresh();
        bool got = WaitFor([&] { return pass >= 1; });
        if (got) {
            CHECK(bridged, "CPaintDC draws through the bridge's wxPaintDC");
            CHECK(borrowed, "GetDC inside paint borrows the paint DC");
            panel->Refresh();
            WaitFor([&] { return painted; });
            CHECK(painted && ownSession, "BeginPaint without bridge shares one wxPaintDC with GetDC");
        } else {
            printf("SKIP paint tests: no paint event was delivered\n");
        }
    }
    frame->Destroy();
    wxTheApp->ProcessIdle();
    WaitFor([] { return false; });
    CDC* l = CDC::FromHandle(leaked);
    CHECK(l && l->GetTextExtent("after destroy").cx > 10, "a GetDC handle survives its window");
    l->FillSolidRect(0, 0, 10, 10, kRed);
    ::ReleaseDC(nullptr, leaked);
    PurgeTemporaryGdiWrappers();
}

class TestApp : public wxApp {
public:
    bool OnInit() override {
        for (int i = 1; i < argc; ++i) {
            wxString a = argv[i];
            if (a == "--no-window")
                g_windowTests = false;
            else
                g_out = a;
        }
        wxInitAllImageHandlers();
        wxFileName::Mkdir(g_out, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
        CallAfter([this] {
            TestShapes();
            TestText();
            TestFonts();
            TestBlit();
            TestBitmaps();
            TestMapping();
            TestRegionsAndHandles();
            TestWrapAndColors();
            TestHiDpi();
            if (g_windowTests)
                TestWindows();
            PurgeTemporaryGdiWrappers();
            printf("%d checks, %d failures; images in %s\n", g_checks, g_failures, (const char*)g_out.utf8_str());
            ExitMainLoop();
        });
        return true;
    }
    int OnRun() override {
        wxApp::OnRun();
        return g_failures ? 1 : 0;
    }
};

} // namespace

wxIMPLEMENT_APP_NO_MAIN(TestApp);

int main(int argc, char** argv) {
    int rc = wxEntry(argc, argv);
    return g_failures ? 1 : rc;
}
