#include "gdi_internal.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

IMPLEMENT_DYNCREATE(CGdiObject, CObject)
IMPLEMENT_DYNAMIC(CPen, CGdiObject)
IMPLEMENT_DYNAMIC(CBrush, CGdiObject)
IMPLEMENT_DYNAMIC(CFont, CGdiObject)
IMPLEMENT_DYNAMIC(CBitmap, CGdiObject)
IMPLEMENT_DYNAMIC(CRgn, CGdiObject)
IMPLEMENT_DYNAMIC(CPalette, CGdiObject)

namespace mfcwx {

void PurgeTemporaryDCs();

namespace {

std::unordered_set<GdiObjectImpl*>& Objects() {
    static auto* objects = new std::unordered_set<GdiObjectImpl*>;
    return *objects;
}

std::unordered_map<HGDIOBJ, CGdiObject*>& TempObjects() {
    static auto* temps = new std::unordered_map<HGDIOBJ, CGdiObject*>;
    return *temps;
}

std::vector<CGdiObject*>& DetachedTemps() {
    static auto* detached = new std::vector<CGdiObject*>;
    return *detached;
}

void DetachWrappers(GdiObjectImpl* impl) {
    HGDIOBJ h = reinterpret_cast<HGDIOBJ>(impl);
    if (impl->permanent) {
        impl->permanent->m_hObject = nullptr;
        impl->permanent = nullptr;
    }
    auto it = TempObjects().find(h);
    if (it != TempObjects().end()) {
        it->second->m_hObject = nullptr;
        DetachedTemps().push_back(it->second);
        TempObjects().erase(it);
    }
}

void FreeImpl(GdiObjectImpl* impl) {
    DetachWrappers(impl);
    Objects().erase(impl);
    delete impl;
}

double ScreenDpi() {
#ifdef __WXOSX__
    return 72.0;
#else
    static double dpi = [] {
        wxSize ppi = wxGetDisplayPPI();
        return ppi.y > 0 ? static_cast<double>(ppi.y) : 96.0;
    }();
    return dpi;
#endif
}

double PointsToPixels(double pt) { return pt * ScreenDpi() / 72.0; }

void SetFontPixels(wxFont& f, double px) {
    if (px < 1)
        px = 1;
#ifdef __WXOSX__
    f.SetFractionalPointSize(PixelsToPoints(px));
#else
    f.SetPixelSize(wxSize(0, static_cast<int>(std::lround(px))));
#endif
}

std::string Lower(std::string s) {
    for (char& c : s)
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return s;
}

enum class FaceBase { Gui, Mono, Serif, Named };

bool IsValidFace(const wxString& name) {
    static std::map<wxString, bool> cache;
    auto it = cache.find(name);
    if (it != cache.end())
        return it->second;
    bool ok = wxFontEnumerator::IsValidFacename(name);
    cache[name] = ok;
    return ok;
}

FaceBase ResolveFace(const std::string& face, BYTE pitchAndFamily, wxString& named) {
    static const char* const gui[] = {"ms sans serif", "microsoft sans serif", "ms shell dlg", "ms shell dlg 2",
                                      "tahoma", "arial", "helvetica", "system", "segoe ui", "small fonts"};
    static const char* const mono[] = {"courier new", "courier", "fixedsys", "terminal", "lucida console",
                                       "consolas", "lucida sans typewriter", "monospace"};
    static const char* const serif[] = {"times new roman", "times", "ms serif"};
    std::string l = Lower(face);
    while (!l.empty() && l.back() == ' ')
        l.pop_back();
    if (!l.empty()) {
        for (const char* g : gui)
            if (l == g)
                return FaceBase::Gui;
        for (const char* m : mono)
            if (l == m)
                return FaceBase::Mono;
        for (const char* s : serif)
            if (l == s)
                return FaceBase::Serif;
        named = ToWx(face.c_str());
        if (l == "symbol" || l == "wingdings" || l == "webdings" || IsValidFace(named))
            return FaceBase::Named;
    }
    int family = pitchAndFamily & 0xF0;
    if ((pitchAndFamily & 3) == FIXED_PITCH || family == FF_MODERN)
        return FaceBase::Mono;
    if (family == FF_ROMAN && l.empty())
        return FaceBase::Serif;
    return FaceBase::Gui;
}

wxFont GuiFont() {
    wxFont f = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    if (!f.IsOk())
        f = *wxNORMAL_FONT;
    return f;
}

wxFont BaseFont(FaceBase base, const wxString& named) {
    switch (base) {
    case FaceBase::Mono:
#ifdef __WXOSX__
        return wxFont(wxFontInfo(12).FaceName("Menlo"));
#else
        return wxFont(wxFontInfo(10).Family(wxFONTFAMILY_TELETYPE));
#endif
    case FaceBase::Serif:
#ifdef __WXOSX__
        if (IsValidFace("Times New Roman"))
            return wxFont(wxFontInfo(12).FaceName("Times New Roman"));
#endif
        return wxFont(wxFontInfo(12).Family(wxFONTFAMILY_ROMAN));
    case FaceBase::Named:
        return wxFont(wxFontInfo(12).FaceName(named));
    case FaceBase::Gui:
        break;
    }
    return GuiFont();
}

double GuiEmPixels() {
    static double em = [] {
        wxFont f = GuiFont();
#ifdef __WXGTK__
        if (f.GetPixelSize().y > 0 && f.GetPointSize() <= 0)
            return static_cast<double>(f.GetPixelSize().y);
#endif
        return PointsToPixels(f.GetFractionalPointSize());
    }();
    return em;
}

std::string FaceOf(const LOGFONT& lf) { return std::string(lf.lfFaceName, strnlen(lf.lfFaceName, LF_FACESIZE)); }

void SetFace(LOGFONT& lf, const char* face) {
    memset(lf.lfFaceName, 0, sizeof lf.lfFaceName);
    if (face)
        strncpy(lf.lfFaceName, face, LF_FACESIZE - 1);
}

LOGFONT LogFontFromWx(const wxFont& f, double em) {
    LOGFONT lf;
    memset(&lf, 0, sizeof lf);
    lf.lfHeight = -static_cast<LONG>(std::lround(em));
    lf.lfWeight = f.GetNumericWeight();
    lf.lfItalic = f.GetStyle() != wxFONTSTYLE_NORMAL;
    lf.lfUnderline = f.GetUnderlined();
    lf.lfStrikeOut = f.GetStrikethrough();
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = DEFAULT_QUALITY;
    bool fixed = f.IsFixedWidth();
    lf.lfPitchAndFamily = static_cast<BYTE>(fixed ? (FIXED_PITCH | FF_MODERN) : (VARIABLE_PITCH | FF_SWISS));
    wxString face = f.GetFaceName();
    if (face.empty() || face.StartsWith(".") || face == GuiFont().GetFaceName())
        SetFace(lf, fixed ? "Courier New" : "MS Shell Dlg");
    else
        SetFace(lf, FromWx(face).c_str());
    return lf;
}

GdiObjectImpl* NewImpl(GdiKind kind) { return new GdiObjectImpl(kind); }

GdiObjectImpl* MakeStock(int index) {
    auto brush = [](BYTE v, bool null) {
        auto* b = NewImpl(GdiKind::Brush);
        if (null) {
            b->logBrush.lbStyle = BS_NULL;
            b->brush = *wxTRANSPARENT_BRUSH;
        } else {
            b->logBrush.lbStyle = BS_SOLID;
            b->logBrush.lbColor = RGB(v, v, v);
            b->brush = wxBrush(wxColour(v, v, v));
        }
        return b;
    };
    auto pen = [](int style, BYTE v) {
        auto* p = NewImpl(GdiKind::Pen);
        p->penStyle = static_cast<DWORD>(style);
        p->logPen.lopnStyle = static_cast<UINT>(style);
        p->logPen.lopnWidth.x = 0;
        p->logPen.lopnColor = RGB(v, v, v);
        return p;
    };
    auto font = [](bool fixed, const char* face, int weight) {
        auto* f = NewImpl(GdiKind::Font);
        wxString named;
        f->font = BaseFont(fixed ? FaceBase::Mono : FaceBase::Gui, named);
        double em = GuiEmPixels();
        SetFontPixels(f->font, em);
        f->emPx = em;
        f->logFont = LogFontFromWx(f->font, em);
        f->logFont.lfWeight = weight;
        SetFace(f->logFont, face);
        return f;
    };
    switch (index) {
    case WHITE_BRUSH:
        return brush(255, false);
    case LTGRAY_BRUSH:
        return brush(192, false);
    case GRAY_BRUSH:
        return brush(128, false);
    case DKGRAY_BRUSH:
        return brush(64, false);
    case BLACK_BRUSH:
        return brush(0, false);
    case NULL_BRUSH:
        return brush(0, true);
    case DC_BRUSH:
        return brush(255, false);
    case WHITE_PEN:
        return pen(PS_SOLID, 255);
    case BLACK_PEN:
    case DC_PEN:
        return pen(PS_SOLID, 0);
    case NULL_PEN:
        return pen(PS_NULL, 0);
    case OEM_FIXED_FONT:
        return font(true, "Terminal", FW_NORMAL);
    case ANSI_FIXED_FONT:
        return font(true, "Courier", FW_NORMAL);
    case SYSTEM_FIXED_FONT:
        return font(true, "Fixedsys", FW_NORMAL);
    case ANSI_VAR_FONT:
        return font(false, "MS Sans Serif", FW_NORMAL);
    case SYSTEM_FONT:
        return font(false, "System", FW_NORMAL);
    case DEVICE_DEFAULT_FONT:
        return font(false, "System", FW_NORMAL);
    case DEFAULT_GUI_FONT:
        return font(false, "MS Shell Dlg", FW_NORMAL);
    case DEFAULT_PALETTE:
        return NewImpl(GdiKind::Palette);
    default:
        return nullptr;
    }
}

constexpr int kStockCount = 21;
constexpr int kDefaultBitmapIndex = 20;

GdiObjectImpl* Stock(int index) {
    static GdiObjectImpl* stock[kStockCount] = {};
    if (index < 0 || index >= kStockCount)
        return nullptr;
    if (!stock[index]) {
        GdiObjectImpl* impl = nullptr;
        if (index == kDefaultBitmapIndex) {
            impl = NewImpl(GdiKind::Bitmap);
            impl->bitmap = wxBitmap(1, 1, 24);
            impl->bitsPixel = 1;
        } else {
            impl = MakeStock(index);
        }
        if (!impl)
            return nullptr;
        impl->stock = true;
        Objects().insert(impl);
        stock[index] = impl;
    }
    return stock[index];
}

wxColour ClassicSysColor(int index) {
    static const COLORREF table[] = {
        RGB(212, 208, 200), RGB(58, 110, 165),  RGB(10, 36, 106),   RGB(128, 128, 128), RGB(212, 208, 200),
        RGB(255, 255, 255), RGB(0, 0, 0),       RGB(0, 0, 0),       RGB(0, 0, 0),       RGB(255, 255, 255),
        RGB(212, 208, 200), RGB(212, 208, 200), RGB(128, 128, 128), RGB(10, 36, 106),   RGB(255, 255, 255),
        RGB(212, 208, 200), RGB(128, 128, 128), RGB(128, 128, 128), RGB(0, 0, 0),       RGB(212, 208, 200),
        RGB(255, 255, 255), RGB(64, 64, 64),    RGB(212, 208, 200), RGB(0, 0, 0),       RGB(255, 255, 225),
        RGB(181, 181, 181), RGB(0, 0, 128),     RGB(166, 202, 240), RGB(192, 192, 192), RGB(49, 106, 197),
        RGB(236, 233, 216)};
    if (index < 0 || index >= static_cast<int>(sizeof table / sizeof table[0]))
        return *wxBLACK;
    return ToWxColour(table[index]);
}

bool WxSysColorIndex(int index, wxSystemColour& out) {
    static const int map[] = {wxSYS_COLOUR_SCROLLBAR,
                              wxSYS_COLOUR_DESKTOP,
                              wxSYS_COLOUR_ACTIVECAPTION,
                              wxSYS_COLOUR_INACTIVECAPTION,
                              wxSYS_COLOUR_MENU,
                              wxSYS_COLOUR_WINDOW,
                              wxSYS_COLOUR_WINDOWFRAME,
                              wxSYS_COLOUR_MENUTEXT,
                              wxSYS_COLOUR_WINDOWTEXT,
                              wxSYS_COLOUR_CAPTIONTEXT,
                              wxSYS_COLOUR_ACTIVEBORDER,
                              wxSYS_COLOUR_INACTIVEBORDER,
                              wxSYS_COLOUR_APPWORKSPACE,
                              wxSYS_COLOUR_HIGHLIGHT,
                              wxSYS_COLOUR_HIGHLIGHTTEXT,
                              wxSYS_COLOUR_BTNFACE,
                              wxSYS_COLOUR_BTNSHADOW,
                              wxSYS_COLOUR_GRAYTEXT,
                              wxSYS_COLOUR_BTNTEXT,
                              wxSYS_COLOUR_INACTIVECAPTIONTEXT,
                              wxSYS_COLOUR_BTNHIGHLIGHT,
                              wxSYS_COLOUR_3DDKSHADOW,
                              wxSYS_COLOUR_3DLIGHT,
                              wxSYS_COLOUR_INFOTEXT,
                              wxSYS_COLOUR_INFOBK,
                              -1,
                              wxSYS_COLOUR_HOTLIGHT,
                              wxSYS_COLOUR_GRADIENTACTIVECAPTION,
                              wxSYS_COLOUR_GRADIENTINACTIVECAPTION,
                              wxSYS_COLOUR_MENUHILIGHT,
                              wxSYS_COLOUR_MENUBAR};
    if (index < 0 || index >= static_cast<int>(sizeof map / sizeof map[0]) || map[index] < 0)
        return false;
    out = static_cast<wxSystemColour>(map[index]);
    return true;
}

const wxDash* InternDashes(const std::vector<wxDash>& dashes) {
    static auto* pool = new std::set<std::vector<wxDash>>;
    return pool->insert(dashes).first->data();
}

CGdiObject* NewWrapper(GdiKind kind) {
    switch (kind) {
    case GdiKind::Pen:
        return new CPen;
    case GdiKind::Brush:
        return new CBrush;
    case GdiKind::Font:
        return new CFont;
    case GdiKind::Bitmap:
        return new CBitmap;
    case GdiKind::Region:
        return new CRgn;
    case GdiKind::Palette:
        return new CPalette;
    }
    return new CGdiObject;
}

HGDIOBJ NewPen(int style, int width, COLORREF color, const LOGBRUSH* lb, int styleCount, const DWORD* userStyle) {
    auto* impl = NewImpl(GdiKind::Pen);
    impl->penStyle = static_cast<DWORD>(style);
    impl->logPen.lopnStyle = static_cast<UINT>(style);
    impl->logPen.lopnWidth.x = width;
    impl->logPen.lopnColor = lb ? lb->lbColor : color;
    if ((style & PS_STYLE_MASK) == PS_USERSTYLE && userStyle) {
        for (int i = 0; i < styleCount; ++i)
            impl->dashes.push_back(static_cast<wxDash>(std::max<DWORD>(1, std::min<DWORD>(userStyle[i], 255))));
    }
    return RegisterGdiObject(impl);
}

wxBrushStyle HatchStyle(int index) {
    switch (index) {
    case HS_HORIZONTAL:
        return wxBRUSHSTYLE_HORIZONTAL_HATCH;
    case HS_VERTICAL:
        return wxBRUSHSTYLE_VERTICAL_HATCH;
    case HS_FDIAGONAL:
        return wxBRUSHSTYLE_FDIAGONAL_HATCH;
    case HS_BDIAGONAL:
        return wxBRUSHSTYLE_BDIAGONAL_HATCH;
    case HS_CROSS:
        return wxBRUSHSTYLE_CROSS_HATCH;
    default:
        return wxBRUSHSTYLE_CROSSDIAG_HATCH;
    }
}

HGDIOBJ NewSolidBrush(COLORREF c) {
    auto* impl = NewImpl(GdiKind::Brush);
    impl->logBrush.lbStyle = BS_SOLID;
    impl->logBrush.lbColor = c;
    impl->brush = wxBrush(ToWxColour(c));
    return RegisterGdiObject(impl);
}

HGDIOBJ NewHatchBrush(int index, COLORREF c) {
    auto* impl = NewImpl(GdiKind::Brush);
    impl->logBrush.lbStyle = BS_HATCHED;
    impl->logBrush.lbColor = c;
    impl->logBrush.lbHatch = static_cast<ULONG_PTR>(index);
    impl->brush = wxBrush(ToWxColour(c), HatchStyle(index));
    return RegisterGdiObject(impl);
}

HGDIOBJ NewPatternBrush(HBITMAP hbm) {
    GdiObjectImpl* bmp = GdiImpl(hbm, GdiKind::Bitmap);
    if (!bmp || !bmp->bitmap.IsOk())
        return nullptr;
    SyncBitmap(bmp);
    auto* impl = NewImpl(GdiKind::Brush);
    impl->logBrush.lbStyle = BS_PATTERN;
    impl->logBrush.lbHatch = reinterpret_cast<ULONG_PTR>(hbm);
    wxBitmap copy = bmp->bitmap.GetSubBitmap(wxRect(0, 0, bmp->bitmap.GetLogicalWidth(), bmp->bitmap.GetLogicalHeight()));
    impl->brush = wxBrush(copy.IsOk() ? copy : bmp->bitmap);
    return RegisterGdiObject(impl);
}

HGDIOBJ NewBrushIndirect(const LOGBRUSH* lb) {
    if (!lb)
        return nullptr;
    switch (lb->lbStyle) {
    case BS_SOLID:
        return NewSolidBrush(lb->lbColor);
    case BS_HATCHED:
        return NewHatchBrush(static_cast<int>(lb->lbHatch), lb->lbColor);
    case BS_PATTERN:
        return NewPatternBrush(reinterpret_cast<HBITMAP>(lb->lbHatch));
    case BS_NULL: {
        auto* impl = NewImpl(GdiKind::Brush);
        impl->logBrush.lbStyle = BS_NULL;
        impl->brush = *wxTRANSPARENT_BRUSH;
        return RegisterGdiObject(impl);
    }
    default:
        return NewSolidBrush(lb->lbColor);
    }
}

HGDIOBJ NewFont(const LOGFONT* lf) {
    if (!lf)
        return nullptr;
    auto* impl = NewImpl(GdiKind::Font);
    impl->logFont = *lf;
    impl->logFont.lfFaceName[LF_FACESIZE - 1] = 0;
    impl->font = MakeFont(impl->logFont, &impl->emPx);
    return RegisterGdiObject(impl);
}

HGDIOBJ NewRegion(const wxRegion& r) {
    auto* impl = NewImpl(GdiKind::Region);
    impl->region = r;
    return RegisterGdiObject(impl);
}

wxRegion RectRegion(int x1, int y1, int x2, int y2) {
    int l = std::min(x1, x2), r = std::max(x1, x2), t = std::min(y1, y2), b = std::max(y1, y2);
    if (r <= l || b <= t)
        return wxRegion();
    return wxRegion(l, t, r - l, b - t);
}

wxRegion EllipseRegion(int x1, int y1, int x2, int y2) {
    int l = std::min(x1, x2), r = std::max(x1, x2), t = std::min(y1, y2), b = std::max(y1, y2);
    if (r <= l || b <= t)
        return wxRegion();
    double cx = (l + r) / 2.0, cy = (t + b) / 2.0, rx = (r - l) / 2.0, ry = (b - t) / 2.0;
    int n = std::max(16, static_cast<int>(rx + ry));
    std::vector<wxPoint> pts(n);
    for (int i = 0; i < n; ++i) {
        double a = 2 * M_PI * i / n;
        pts[i] = wxPoint(static_cast<int>(std::lround(cx + rx * cos(a))), static_cast<int>(std::lround(cy + ry * sin(a))));
    }
    return wxRegion(pts.size(), pts.data());
}

wxRegion RoundRectRegion(int x1, int y1, int x2, int y2, int w, int h) {
    int l = std::min(x1, x2), r = std::max(x1, x2), t = std::min(y1, y2), b = std::max(y1, y2);
    if (r <= l || b <= t)
        return wxRegion();
    double rx = std::min(std::abs(w) / 2.0, (r - l) / 2.0), ry = std::min(std::abs(h) / 2.0, (b - t) / 2.0);
    if (rx < 1 || ry < 1)
        return wxRegion(l, t, r - l, b - t);
    std::vector<wxPoint> pts;
    const double cxs[4] = {r - rx, l + rx, l + rx, r - rx};
    const double cys[4] = {t + ry, t + ry, b - ry, b - ry};
    int seg = std::max(4, static_cast<int>((rx + ry) / 2));
    for (int q = 0; q < 4; ++q) {
        for (int i = 0; i <= seg; ++i) {
            double a = -M_PI / 2 * (q + static_cast<double>(i) / seg);
            pts.emplace_back(static_cast<int>(std::lround(cxs[q] + rx * cos(a))),
                             static_cast<int>(std::lround(cys[q] + ry * sin(a))));
        }
    }
    return wxRegion(pts.size(), pts.data());
}

int RegionType(const wxRegion& r) {
    if (!r.IsOk() || r.IsEmpty())
        return NULLREGION;
    int n = 0;
    for (wxRegionIterator it(r); it && n < 2; ++it)
        ++n;
    return n <= 1 ? SIMPLEREGION : COMPLEXREGION;
}

int GetObjectImpl(HGDIOBJ h, int c, void* pv) {
    GdiLock lock;
    int sys = SysColorBrushIndex(h);
    if (sys >= 0) {
        LOGBRUSH lb{BS_SOLID, SysColor(sys), 0};
        if (!pv)
            return sizeof lb;
        int n = std::min<int>(c, sizeof lb);
        memcpy(pv, &lb, static_cast<size_t>(n));
        return n;
    }
    GdiObjectImpl* impl = GdiImpl(h);
    if (!impl)
        return 0;
    const void* src = nullptr;
    size_t size = 0;
    BITMAP bm;
    WORD entries = 0;
    switch (impl->kind) {
    case GdiKind::Pen:
        src = &impl->logPen;
        size = sizeof impl->logPen;
        break;
    case GdiKind::Brush:
        src = &impl->logBrush;
        size = sizeof impl->logBrush;
        break;
    case GdiKind::Font:
        src = &impl->logFont;
        size = sizeof impl->logFont;
        break;
    case GdiKind::Bitmap: {
        memset(&bm, 0, sizeof bm);
        if (impl->bitmap.IsOk()) {
            bm.bmWidth = static_cast<LONG>(std::lround(impl->bitmap.GetLogicalWidth()));
            bm.bmHeight = static_cast<LONG>(std::lround(impl->bitmap.GetLogicalHeight()));
        }
        bm.bmBitsPixel = static_cast<WORD>(impl->bitsPixel);
        bm.bmPlanes = 1;
        bm.bmWidthBytes = ((bm.bmWidth * impl->bitsPixel + 15) / 16) * 2;
        src = &bm;
        size = sizeof bm;
        break;
    }
    case GdiKind::Palette:
        entries = 256;
        src = &entries;
        size = sizeof entries;
        break;
    case GdiKind::Region:
        return 0;
    }
    if (!pv)
        return static_cast<int>(size);
    int n = std::min<int>(c, static_cast<int>(size));
    if (n <= 0)
        return 0;
    memcpy(pv, src, static_cast<size_t>(n));
    return n;
}

} // namespace

std::recursive_mutex& GdiMutex() {
    static auto* m = new std::recursive_mutex;
    return *m;
}

double PixelsToPoints(double px) { return px * 72.0 / ScreenDpi(); }

GdiObjectImpl* GdiImpl(HGDIOBJ h) {
    if (!h || SysColorBrushIndex(h) >= 0)
        return nullptr;
    GdiLock lock;
    auto* impl = reinterpret_cast<GdiObjectImpl*>(h);
    return Objects().count(impl) ? impl : nullptr;
}

GdiObjectImpl* GdiImpl(HGDIOBJ h, GdiKind kind) {
    GdiObjectImpl* impl = GdiImpl(h);
    return impl && impl->kind == kind ? impl : nullptr;
}

HGDIOBJ RegisterGdiObject(GdiObjectImpl* impl) {
    GdiLock lock;
    Objects().insert(impl);
    return reinterpret_cast<HGDIOBJ>(impl);
}

void GdiAddRef(HGDIOBJ h) {
    GdiLock lock;
    if (GdiObjectImpl* impl = GdiImpl(h))
        if (!impl->stock)
            ++impl->refs;
}

void GdiRelease(HGDIOBJ h) {
    GdiLock lock;
    GdiObjectImpl* impl = GdiImpl(h);
    if (!impl || impl->stock)
        return;
    if (--impl->refs <= 0)
        FreeImpl(impl);
}

int SysColorBrushIndex(HGDIOBJ h) {
    auto v = reinterpret_cast<uintptr_t>(h);
    return v >= 1 && v <= 31 ? static_cast<int>(v) - 1 : -1;
}

HGDIOBJ DefaultBitmapHandle() { return reinterpret_cast<HGDIOBJ>(Stock(kDefaultBitmapIndex)); }

bool IsStockFont(HFONT h) {
    GdiObjectImpl* impl = GdiImpl(h, GdiKind::Font);
    return !impl || impl->stock;
}

wxFont MakeFont(const LOGFONT& lfIn, double* emPx) {
    LOGFONT lf = lfIn;
    lf.lfFaceName[LF_FACESIZE - 1] = 0;
    LONG height = lf.lfHeight;
    if (height > 2000 || height < -2000)
        height = 0;
    wxString named;
    FaceBase base = ResolveFace(FaceOf(lf), lf.lfPitchAndFamily, named);
    wxFont f = BaseFont(base, named);
    double em = height < 0 ? -static_cast<double>(height) : (height > 0 ? height : GuiEmPixels());
    SetFontPixels(f, em);
    LONG weight = lf.lfWeight;
    if (weight > 0 && weight <= 1000)
        f.SetNumericWeight(static_cast<int>(weight));
    else
        f.SetNumericWeight(wxFONTWEIGHT_NORMAL);
    f.SetStyle(lf.lfItalic ? wxFONTSTYLE_ITALIC : wxFONTSTYLE_NORMAL);
    f.SetUnderlined(lf.lfUnderline != 0);
    f.SetStrikethrough(lf.lfStrikeOut != 0);
    if (height > 0) {
        GdiLock lock;
        wxDC& dc = MeasureDC();
        dc.SetFont(f);
        wxFontMetrics fm = dc.GetFontMetrics();
        int cell = fm.height - fm.externalLeading;
        if (cell > 0) {
            em = em * height / cell;
            SetFontPixels(f, em);
        }
    }
    if (emPx)
        *emPx = em;
    return f;
}

wxPen DevicePen(HPEN h, double widthScale) {
    GdiObjectImpl* impl = GdiImpl(h, GdiKind::Pen);
    if (!impl)
        return *wxBLACK_PEN;
    int style = static_cast<int>(impl->penStyle & PS_STYLE_MASK);
    if (style == PS_NULL)
        return *wxTRANSPARENT_PEN;
    bool geometric = (impl->penStyle & PS_GEOMETRIC) != 0;
    int w = impl->logPen.lopnWidth.x;
    int dw = w <= 0 ? 1 : std::max(1, static_cast<int>(std::lround(w * widthScale)));
    if (!geometric && dw > 1 && style >= PS_DASH && style <= PS_DASHDOTDOT)
        style = PS_SOLID;
    wxPen pen(ToWxColour(impl->logPen.lopnColor), dw, wxPENSTYLE_SOLID);
    static const wxDash dash[] = {18, 6}, dot[] = {3, 3}, dashDot[] = {9, 6, 3, 6}, dashDotDot[] = {9, 3, 3, 3, 3, 3},
                        alternate[] = {1, 1};
    bool dashed = true;
    switch (style) {
    case PS_DASH:
        pen.SetDashes(2, dash);
        break;
    case PS_DOT:
        pen.SetDashes(2, dot);
        break;
    case PS_DASHDOT:
        pen.SetDashes(4, dashDot);
        break;
    case PS_DASHDOTDOT:
        pen.SetDashes(6, dashDotDot);
        break;
    case PS_ALTERNATE:
        pen.SetDashes(2, alternate);
        break;
    case PS_USERSTYLE:
        if (!impl->dashes.empty()) {
            pen.SetDashes(static_cast<int>(impl->dashes.size()), InternDashes(impl->dashes));
            break;
        }
        dashed = false;
        break;
    default:
        dashed = false;
        break;
    }
    if (dashed)
        pen.SetStyle(wxPENSTYLE_USER_DASH);
    if (geometric) {
        DWORD cap = impl->penStyle & 0x00000F00;
        pen.SetCap(cap == PS_ENDCAP_SQUARE ? wxCAP_PROJECTING : cap == PS_ENDCAP_FLAT ? wxCAP_BUTT : wxCAP_ROUND);
        DWORD join = impl->penStyle & 0x0000F000;
        pen.SetJoin(join == PS_JOIN_BEVEL ? wxJOIN_BEVEL : join == PS_JOIN_MITER ? wxJOIN_MITER : wxJOIN_ROUND);
    } else if (dw == 1) {
        pen.SetCap(dashed ? wxCAP_BUTT : wxCAP_PROJECTING);
        pen.SetJoin(wxJOIN_MITER);
    }
    return pen;
}

wxBrush DeviceBrush(HBRUSH h) {
    int sys = SysColorBrushIndex(h);
    if (sys >= 0)
        return wxBrush(ToWxColour(SysColor(sys)));
    GdiObjectImpl* impl = GdiImpl(h, GdiKind::Brush);
    if (!impl)
        return *wxTRANSPARENT_BRUSH;
    if (impl->sysColor >= 0) {
        COLORREF c = SysColor(impl->sysColor);
        if (c != impl->logBrush.lbColor) {
            impl->logBrush.lbColor = c;
            impl->brush = wxBrush(ToWxColour(c));
        }
    }
    return impl->brush;
}

wxFont DeviceFont(HFONT h, double scale) {
    GdiObjectImpl* impl = GdiImpl(h, GdiKind::Font);
    if (!impl)
        impl = Stock(SYSTEM_FONT);
    if (impl->stock || std::fabs(scale - 1.0) < 1e-6 || scale <= 0)
        return impl->font;
    if (!impl->scaledFont.IsOk() || std::fabs(impl->scaledFor - scale) > 1e-6) {
        impl->scaledFont = impl->font;
        SetFontPixels(impl->scaledFont, impl->emPx * scale);
        impl->scaledFor = scale;
    }
    return impl->scaledFont;
}

double FontEmPixels(HFONT h) {
    GdiObjectImpl* impl = GdiImpl(h, GdiKind::Font);
    if (!impl)
        impl = Stock(SYSTEM_FONT);
    return impl->emPx;
}

bool BrushIsHatched(HBRUSH h) {
    GdiObjectImpl* impl = GdiImpl(h, GdiKind::Brush);
    return impl && impl->logBrush.lbStyle == BS_HATCHED;
}

COLORREF BrushColor(HBRUSH h) {
    int sys = SysColorBrushIndex(h);
    if (sys >= 0)
        return SysColor(sys);
    GdiObjectImpl* impl = GdiImpl(h, GdiKind::Brush);
    if (!impl)
        return 0;
    if (impl->sysColor >= 0)
        return SysColor(impl->sysColor);
    return impl->logBrush.lbColor;
}

bool PenIsNull(HPEN h) {
    GdiObjectImpl* impl = GdiImpl(h, GdiKind::Pen);
    return !impl || (impl->penStyle & PS_STYLE_MASK) == PS_NULL;
}

int PenStyle(HPEN h) {
    GdiObjectImpl* impl = GdiImpl(h, GdiKind::Pen);
    return impl ? static_cast<int>(impl->penStyle & PS_STYLE_MASK) : PS_SOLID;
}

int PenWidth(HPEN h) {
    GdiObjectImpl* impl = GdiImpl(h, GdiKind::Pen);
    return impl ? std::max<int>(1, impl->logPen.lopnWidth.x) : 1;
}

COLORREF PenColor(HPEN h) {
    GdiObjectImpl* impl = GdiImpl(h, GdiKind::Pen);
    return impl ? impl->logPen.lopnColor : 0;
}

wxBitmap NewBitmap(int width, int height, double scale, const wxColour& fill) {
    width = std::max(1, width);
    height = std::max(1, height);
    if (scale <= 0)
        scale = 1;
    int pw = std::max(1, static_cast<int>(std::lround(width * scale)));
    int ph = std::max(1, static_cast<int>(std::lround(height * scale)));
    wxImage img(pw, ph, false);
    unsigned char* data = img.GetData();
    for (int i = 0; i < pw * ph; ++i) {
        data[i * 3] = fill.Red();
        data[i * 3 + 1] = fill.Green();
        data[i * 3 + 2] = fill.Blue();
    }
    return wxBitmap(img, -1, scale);
}

HBITMAP NewBitmapHandle(const wxBitmap& bmp, int bitsPixel) {
    if (!bmp.IsOk())
        return nullptr;
    auto* impl = NewImpl(GdiKind::Bitmap);
    impl->bitmap = bmp;
    impl->bitsPixel = bitsPixel;
    return RegisterGdiObject(impl);
}

wxPen PenFromHandle(HPEN h) {
    GdiLock lock;
    return DevicePen(h, 1.0);
}

wxBrush BrushFromHandle(HBRUSH h) {
    GdiLock lock;
    if (!h)
        return wxNullBrush;
    return DeviceBrush(h);
}

wxFont FontFromHandle(HFONT h) {
    GdiLock lock;
    GdiObjectImpl* impl = GdiImpl(h, GdiKind::Font);
    return impl ? impl->font : wxNullFont;
}

wxBitmap* BitmapFromHandle(HBITMAP h) {
    GdiLock lock;
    GdiObjectImpl* impl = GdiImpl(h, GdiKind::Bitmap);
    if (!impl || !impl->bitmap.IsOk())
        return nullptr;
    SyncBitmap(impl);
    return &impl->bitmap;
}

HFONT DefaultGuiFont() {
    GdiLock lock;
    return reinterpret_cast<HFONT>(Stock(DEFAULT_GUI_FONT));
}

HBITMAP CreateBitmapHandle(const wxBitmap& bmp) {
    GdiLock lock;
    return NewBitmapHandle(bmp, bmp.IsOk() && bmp.GetDepth() == 1 ? 1 : 32);
}

HFONT FontHandleFor(const wxFont& font) {
    GdiLock lock;
    if (!font.IsOk())
        return DefaultGuiFont();
    static auto* cache = new std::map<wxString, GdiObjectImpl*>;
    wxString key = font.GetNativeFontInfoDesc();
    auto it = cache->find(key);
    if (it != cache->end())
        return reinterpret_cast<HFONT>(it->second);
    auto* impl = NewImpl(GdiKind::Font);
    impl->font = font;
#ifdef __WXGTK__
    impl->emPx = font.GetPixelSize().y > 0 ? font.GetPixelSize().y : PointsToPixels(font.GetFractionalPointSize());
#else
    impl->emPx = PointsToPixels(font.GetFractionalPointSize());
#endif
    impl->logFont = LogFontFromWx(font, impl->emPx);
    impl->stock = true;
    Objects().insert(impl);
    (*cache)[key] = impl;
    return reinterpret_cast<HFONT>(impl);
}

wxColour ToWxColour(COLORREF c) {
    if (c == CLR_INVALID)
        return wxTransparentColour;
    if ((c & 0xFF000000) == 0x01000000)
        return *wxBLACK;
    return wxColour(GetRValue(c), GetGValue(c), GetBValue(c));
}

COLORREF FromWxColour(const wxColour& c) {
    if (!c.IsOk())
        return 0;
    return RGB(c.Red(), c.Green(), c.Blue());
}

COLORREF SysColor(int index) {
    wxSystemColour sc;
    if (WxSysColorIndex(index, sc)) {
        wxColour c = wxSystemSettings::GetColour(sc);
        if (c.IsOk() && c.IsSolid())
            return FromWxColour(c);
    }
    return FromWxColour(ClassicSysColor(index));
}

void PurgeTemporaryGdiWrappers() {
    std::vector<CGdiObject*> doomed;
    {
        GdiLock lock;
        for (auto& kv : TempObjects()) {
            kv.second->m_hObject = nullptr;
            doomed.push_back(kv.second);
        }
        TempObjects().clear();
        doomed.insert(doomed.end(), DetachedTemps().begin(), DetachedTemps().end());
        DetachedTemps().clear();
    }
    for (CGdiObject* o : doomed)
        delete o;
    PurgeTemporaryDCs();
}

} // namespace mfcwx

using namespace mfcwx;

// ---------------------------------------------------------------------------------------------
// CGdiObject and derived classes

CGdiObject::CGdiObject() : m_hObject(nullptr) {}

CGdiObject::~CGdiObject() { DeleteObject(); }

CGdiObject* CGdiObject::FromHandle(HGDIOBJ hObject) {
    if (!hObject)
        return nullptr;
    GdiLock lock;
    GdiObjectImpl* impl = GdiImpl(hObject);
    if (impl && impl->permanent)
        return impl->permanent;
    auto it = TempObjects().find(hObject);
    if (it != TempObjects().end())
        return it->second;
    CGdiObject* wrapper = nullptr;
    if (impl)
        wrapper = NewWrapper(impl->kind);
    else if (SysColorBrushIndex(hObject) >= 0)
        wrapper = new CBrush;
    else
        return nullptr;
    wrapper->m_hObject = hObject;
    TempObjects()[hObject] = wrapper;
    return wrapper;
}

BOOL CGdiObject::Attach(HGDIOBJ hObject) {
    if (!hObject)
        return FALSE;
    GdiLock lock;
    if (m_hObject && m_hObject != hObject)
        DeleteObject();
    GdiObjectImpl* impl = GdiImpl(hObject);
    if (!impl && SysColorBrushIndex(hObject) < 0)
        return FALSE;
    auto it = TempObjects().find(hObject);
    if (it != TempObjects().end()) {
        it->second->m_hObject = nullptr;
        DetachedTemps().push_back(it->second);
        TempObjects().erase(it);
    }
    if (impl)
        impl->permanent = this;
    m_hObject = hObject;
    return TRUE;
}

HGDIOBJ CGdiObject::Detach() {
    if (AfxIsNullThis(this))
        return nullptr;
    GdiLock lock;
    HGDIOBJ h = m_hObject;
    if (GdiObjectImpl* impl = GdiImpl(h))
        if (impl->permanent == this)
            impl->permanent = nullptr;
    m_hObject = nullptr;
    return h;
}

BOOL CGdiObject::DeleteObject() {
    if (AfxIsNullThis(this) || !m_hObject)
        return FALSE;
    return ::DeleteObject(Detach());
}

int CGdiObject::GetObject(int nCount, LPVOID lpObject) const { return GetObjectImpl(m_hObject, nCount, lpObject); }

BOOL CGdiObject::CreateStockObject(int nIndex) { return Attach(::GetStockObject(nIndex)); }

CPen::CPen(int nPenStyle, int nWidth, COLORREF crColor) { CreatePen(nPenStyle, nWidth, crColor); }

CPen::CPen(int nPenStyle, int nWidth, const LOGBRUSH* pLogBrush, int nStyleCount, const DWORD* lpStyle) {
    CreatePen(nPenStyle, nWidth, pLogBrush, nStyleCount, lpStyle);
}

BOOL CPen::CreatePen(int nPenStyle, int nWidth, COLORREF crColor) {
    return Attach(::CreatePen(nPenStyle, nWidth, crColor));
}

BOOL CPen::CreatePen(int nPenStyle, int nWidth, const LOGBRUSH* pLogBrush, int nStyleCount, const DWORD* lpStyle) {
    GdiLock lock;
    return Attach(NewPen(nPenStyle, nWidth, 0, pLogBrush, nStyleCount, lpStyle));
}

BOOL CPen::CreatePenIndirect(LPLOGPEN lpLogPen) {
    if (!lpLogPen)
        return FALSE;
    return Attach(::CreatePen(static_cast<int>(lpLogPen->lopnStyle), lpLogPen->lopnWidth.x, lpLogPen->lopnColor));
}

int CPen::GetLogPen(LOGPEN* pLogPen) { return GetObject(sizeof(LOGPEN), pLogPen); }

CBrush::CBrush(COLORREF crColor) { CreateSolidBrush(crColor); }

CBrush::CBrush(int nIndex, COLORREF crColor) { CreateHatchBrush(nIndex, crColor); }

CBrush::CBrush(CBitmap* pBitmap) { CreatePatternBrush(pBitmap); }

BOOL CBrush::CreateSolidBrush(COLORREF crColor) { return Attach(::CreateSolidBrush(crColor)); }

BOOL CBrush::CreateHatchBrush(int nIndex, COLORREF crColor) { return Attach(::CreateHatchBrush(nIndex, crColor)); }

BOOL CBrush::CreatePatternBrush(CBitmap* pBitmap) {
    return pBitmap ? Attach(::CreatePatternBrush(pBitmap->m_hObject)) : FALSE;
}

BOOL CBrush::CreateSysColorBrush(int nIndex) {
    GdiLock lock;
    auto* impl = new GdiObjectImpl(GdiKind::Brush);
    impl->logBrush.lbStyle = BS_SOLID;
    impl->logBrush.lbColor = SysColor(nIndex);
    impl->sysColor = nIndex;
    impl->brush = wxBrush(ToWxColour(impl->logBrush.lbColor));
    return Attach(RegisterGdiObject(impl));
}

BOOL CBrush::CreateBrushIndirect(const LOGBRUSH* lpLogBrush) {
    GdiLock lock;
    return Attach(NewBrushIndirect(lpLogBrush));
}

int CBrush::GetLogBrush(LOGBRUSH* pLogBrush) { return GetObject(sizeof(LOGBRUSH), pLogBrush); }

BOOL CFont::CreateFontIndirect(const LOGFONT* lpLogFont) { return Attach(::CreateFontIndirect(lpLogFont)); }

BOOL CFont::CreateFont(int nHeight, int nWidth, int nEscapement, int nOrientation, int nWeight, BYTE bItalic,
                       BYTE bUnderline, BYTE cStrikeOut, BYTE nCharSet, BYTE nOutPrecision, BYTE nClipPrecision,
                       BYTE nQuality, BYTE nPitchAndFamily, const char* lpszFacename) {
    return Attach(::CreateFont(nHeight, nWidth, nEscapement, nOrientation, nWeight, bItalic, bUnderline, cStrikeOut,
                               nCharSet, nOutPrecision, nClipPrecision, nQuality, nPitchAndFamily, lpszFacename));
}

BOOL CFont::CreatePointFont(int nPointSize, const char* lpszFaceName, CDC* pDC) {
    LOGFONT lf;
    memset(&lf, 0, sizeof lf);
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfHeight = nPointSize;
    SetFace(lf, lpszFaceName);
    return CreatePointFontIndirect(&lf, pDC);
}

BOOL CFont::CreatePointFontIndirect(const LOGFONT* lpLogFont, CDC* pDC) {
    if (!lpLogFont)
        return FALSE;
    LOGFONT lf = *lpLogFont;
    GdiLock lock;
    DCState* s = pDC ? DCFromHandle(pDC->m_hAttribDC ? pDC->m_hAttribDC : pDC->m_hDC) : nullptr;
    int dpi = s ? s->Dpi() : 96;
    double py = static_cast<double>(dpi) * lf.lfHeight / 720.0;
    if (s) {
        double x0 = 0, y0 = 0, x1 = 0, y1 = py;
        s->ToLogical(x0, y0);
        s->ToLogical(x1, y1);
        py = y1 - y0;
    }
    lf.lfHeight = -static_cast<LONG>(std::lround(std::fabs(py)));
    return CreateFontIndirect(&lf);
}

int CFont::GetLogFont(LOGFONT* pLogFont) { return GetObject(sizeof(LOGFONT), pLogFont); }

BOOL CRgn::CreateRectRgn(int x1, int y1, int x2, int y2) { return Attach(::CreateRectRgn(x1, y1, x2, y2)); }

BOOL CRgn::CreateRectRgnIndirect(LPCRECT lpRect) {
    return lpRect ? CreateRectRgn(lpRect->left, lpRect->top, lpRect->right, lpRect->bottom) : FALSE;
}

BOOL CRgn::CreateEllipticRgn(int x1, int y1, int x2, int y2) { return Attach(::CreateEllipticRgn(x1, y1, x2, y2)); }

BOOL CRgn::CreateEllipticRgnIndirect(LPCRECT lpRect) {
    return lpRect ? CreateEllipticRgn(lpRect->left, lpRect->top, lpRect->right, lpRect->bottom) : FALSE;
}

BOOL CRgn::CreatePolygonRgn(LPPOINT lpPoints, int nCount, int nMode) {
    if (!lpPoints || nCount < 3)
        return FALSE;
    std::vector<wxPoint> pts;
    for (int i = 0; i < nCount; ++i)
        pts.emplace_back(lpPoints[i].x, lpPoints[i].y);
    GdiLock lock;
    return Attach(NewRegion(wxRegion(pts.size(), pts.data(), nMode == WINDING ? wxWINDING_RULE : wxODDEVEN_RULE)));
}

BOOL CRgn::CreateRoundRectRgn(int x1, int y1, int x2, int y2, int x3, int y3) {
    GdiLock lock;
    return Attach(NewRegion(RoundRectRegion(x1, y1, x2, y2, x3, y3)));
}

int CRgn::CombineRgn(CRgn* pRgn1, CRgn* pRgn2, int nCombineMode) {
    GdiLock lock;
    GdiObjectImpl* self = GdiImpl(m_hObject, GdiKind::Region);
    GdiObjectImpl* a = pRgn1 ? GdiImpl(pRgn1->m_hObject, GdiKind::Region) : nullptr;
    GdiObjectImpl* b = pRgn2 ? GdiImpl(pRgn2->m_hObject, GdiKind::Region) : nullptr;
    if (!self || !a)
        return ERROR;
    wxRegion r = a->region;
    wxRegion other = b ? b->region : wxRegion();
    bool aEmpty = !r.IsOk() || r.IsEmpty();
    bool bEmpty = !other.IsOk() || other.IsEmpty();
    switch (nCombineMode) {
    case RGN_AND:
        if (aEmpty || bEmpty)
            r = wxRegion();
        else
            r.Intersect(other);
        break;
    case RGN_OR:
        if (aEmpty)
            r = other;
        else if (!bEmpty)
            r.Union(other);
        break;
    case RGN_XOR:
        if (aEmpty)
            r = other;
        else if (!bEmpty)
            r.Xor(other);
        break;
    case RGN_DIFF:
        if (!aEmpty && !bEmpty)
            r.Subtract(other);
        break;
    case RGN_COPY:
        break;
    default:
        return ERROR;
    }
    self->region = r;
    return RegionType(self->region);
}

int CRgn::CopyRgn(CRgn* pRgnSrc) { return CombineRgn(pRgnSrc, nullptr, RGN_COPY); }

BOOL CRgn::PtInRegion(int x, int y) const {
    GdiLock lock;
    GdiObjectImpl* self = GdiImpl(m_hObject, GdiKind::Region);
    return self && self->region.IsOk() && !self->region.IsEmpty() && self->region.Contains(x, y) == wxInRegion;
}

BOOL CRgn::RectInRegion(LPCRECT lpRect) const {
    GdiLock lock;
    GdiObjectImpl* self = GdiImpl(m_hObject, GdiKind::Region);
    if (!self || !lpRect || !self->region.IsOk() || self->region.IsEmpty())
        return FALSE;
    wxRect r(lpRect->left, lpRect->top, lpRect->right - lpRect->left, lpRect->bottom - lpRect->top);
    return r.width > 0 && r.height > 0 && self->region.Contains(r) != wxOutRegion;
}

int CRgn::GetRgnBox(LPRECT lpRect) const {
    GdiLock lock;
    GdiObjectImpl* self = GdiImpl(m_hObject, GdiKind::Region);
    if (!self || !lpRect)
        return ERROR;
    wxRect b = self->region.IsOk() ? self->region.GetBox() : wxRect();
    lpRect->left = b.x;
    lpRect->top = b.y;
    lpRect->right = b.x + b.width;
    lpRect->bottom = b.y + b.height;
    return RegionType(self->region);
}

void CRgn::SetRectRgn(int x1, int y1, int x2, int y2) {
    GdiLock lock;
    if (GdiObjectImpl* self = GdiImpl(m_hObject, GdiKind::Region))
        self->region = RectRegion(x1, y1, x2, y2);
}

int CRgn::OffsetRgn(int x, int y) {
    GdiLock lock;
    GdiObjectImpl* self = GdiImpl(m_hObject, GdiKind::Region);
    if (!self)
        return ERROR;
    if (self->region.IsOk() && !self->region.IsEmpty())
        self->region.Offset(x, y);
    return RegionType(self->region);
}

// ---------------------------------------------------------------------------------------------
// Win32 object functions

HGDIOBJ GetStockObject(int i) {
    GdiLock lock;
    if (i == kDefaultBitmapIndex || i == 9)
        return nullptr;
    return reinterpret_cast<HGDIOBJ>(Stock(i));
}

BOOL DeleteObject(HGDIOBJ ho) {
    GdiLock lock;
    GdiObjectImpl* impl = GdiImpl(ho);
    if (!impl)
        return SysColorBrushIndex(ho) >= 0;
    if (impl->stock)
        return TRUE;
    if (impl->deleted)
        return FALSE;
    impl->deleted = true;
    DetachWrappers(impl);
    GdiRelease(ho);
    return TRUE;
}

int GetObject(HANDLE h, int c, LPVOID pv) { return GetObjectImpl(reinterpret_cast<HGDIOBJ>(h), c, pv); }

int GetObjectA(HANDLE h, int c, LPVOID pv) { return GetObjectImpl(reinterpret_cast<HGDIOBJ>(h), c, pv); }

HBRUSH CreateSolidBrush(COLORREF color) {
    GdiLock lock;
    return NewSolidBrush(color);
}

HBRUSH CreateHatchBrush(int iHatch, COLORREF color) {
    GdiLock lock;
    return NewHatchBrush(iHatch, color);
}

HBRUSH CreatePatternBrush(HBITMAP hbm) {
    GdiLock lock;
    return NewPatternBrush(hbm);
}

HPEN CreatePen(int iStyle, int cWidth, COLORREF color) {
    GdiLock lock;
    return NewPen(iStyle, cWidth, color, nullptr, 0, nullptr);
}

HFONT CreateFont(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight, DWORD bItalic,
                 DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet, DWORD iOutPrecision, DWORD iClipPrecision,
                 DWORD iQuality, DWORD iPitchAndFamily, const char* pszFaceName) {
    LOGFONT lf;
    memset(&lf, 0, sizeof lf);
    lf.lfHeight = cHeight;
    lf.lfWidth = cWidth;
    lf.lfEscapement = cEscapement;
    lf.lfOrientation = cOrientation;
    lf.lfWeight = cWeight;
    lf.lfItalic = static_cast<BYTE>(bItalic != 0);
    lf.lfUnderline = static_cast<BYTE>(bUnderline != 0);
    lf.lfStrikeOut = static_cast<BYTE>(bStrikeOut != 0);
    lf.lfCharSet = static_cast<BYTE>(iCharSet);
    lf.lfOutPrecision = static_cast<BYTE>(iOutPrecision);
    lf.lfClipPrecision = static_cast<BYTE>(iClipPrecision);
    lf.lfQuality = static_cast<BYTE>(iQuality);
    lf.lfPitchAndFamily = static_cast<BYTE>(iPitchAndFamily);
    SetFace(lf, pszFaceName);
    return CreateFontIndirect(&lf);
}

HFONT CreateFontA(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight, DWORD bItalic,
                  DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet, DWORD iOutPrecision, DWORD iClipPrecision,
                  DWORD iQuality, DWORD iPitchAndFamily, const char* pszFaceName) {
    return CreateFont(cHeight, cWidth, cEscapement, cOrientation, cWeight, bItalic, bUnderline, bStrikeOut, iCharSet,
                      iOutPrecision, iClipPrecision, iQuality, iPitchAndFamily, pszFaceName);
}

HFONT CreateFontIndirect(const LOGFONT* lplf) {
    GdiLock lock;
    return NewFont(lplf);
}

HFONT CreateFontIndirectA(const LOGFONT* lplf) { return CreateFontIndirect(lplf); }

HRGN CreateRectRgn(int x1, int y1, int x2, int y2) {
    GdiLock lock;
    return NewRegion(RectRegion(x1, y1, x2, y2));
}

HRGN CreateEllipticRgn(int x1, int y1, int x2, int y2) {
    GdiLock lock;
    return NewRegion(EllipseRegion(x1, y1, x2, y2));
}

DWORD GetSysColor(int nIndex) { return SysColor(nIndex); }

HBRUSH GetSysColorBrush(int nIndex) {
    GdiLock lock;
    static std::map<int, GdiObjectImpl*> brushes;
    GdiObjectImpl*& impl = brushes[nIndex];
    if (!impl) {
        impl = new GdiObjectImpl(GdiKind::Brush);
        impl->stock = true;
        impl->sysColor = nIndex;
        impl->logBrush.lbStyle = BS_SOLID;
        impl->logBrush.lbColor = SysColor(nIndex);
        impl->brush = wxBrush(ToWxColour(impl->logBrush.lbColor));
        Objects().insert(impl);
    }
    DeviceBrush(reinterpret_cast<HBRUSH>(impl));
    return reinterpret_cast<HBRUSH>(impl);
}

// ---------------------------------------------------------------------------------------------
// Rectangles

BOOL SetRect(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom) {
    if (!lprc)
        return FALSE;
    lprc->left = xLeft;
    lprc->top = yTop;
    lprc->right = xRight;
    lprc->bottom = yBottom;
    return TRUE;
}

BOOL SetRectEmpty(LPRECT lprc) { return SetRect(lprc, 0, 0, 0, 0); }

BOOL CopyRect(LPRECT lprcDst, const RECT* lprcSrc) {
    if (!lprcDst || !lprcSrc)
        return FALSE;
    *lprcDst = *lprcSrc;
    return TRUE;
}

BOOL InflateRect(LPRECT lprc, int dx, int dy) {
    if (!lprc)
        return FALSE;
    lprc->left -= dx;
    lprc->right += dx;
    lprc->top -= dy;
    lprc->bottom += dy;
    return TRUE;
}

BOOL OffsetRect(LPRECT lprc, int dx, int dy) {
    if (!lprc)
        return FALSE;
    lprc->left += dx;
    lprc->right += dx;
    lprc->top += dy;
    lprc->bottom += dy;
    return TRUE;
}

BOOL IsRectEmpty(const RECT* lprc) { return !lprc || lprc->left >= lprc->right || lprc->top >= lprc->bottom; }

BOOL IntersectRect(LPRECT lprcDst, const RECT* lprcSrc1, const RECT* lprcSrc2) {
    if (!lprcDst || !lprcSrc1 || !lprcSrc2)
        return FALSE;
    RECT r;
    r.left = std::max(lprcSrc1->left, lprcSrc2->left);
    r.top = std::max(lprcSrc1->top, lprcSrc2->top);
    r.right = std::min(lprcSrc1->right, lprcSrc2->right);
    r.bottom = std::min(lprcSrc1->bottom, lprcSrc2->bottom);
    if (IsRectEmpty(&r)) {
        SetRectEmpty(lprcDst);
        return FALSE;
    }
    *lprcDst = r;
    return TRUE;
}

BOOL UnionRect(LPRECT lprcDst, const RECT* lprcSrc1, const RECT* lprcSrc2) {
    if (!lprcDst || !lprcSrc1 || !lprcSrc2)
        return FALSE;
    bool e1 = IsRectEmpty(lprcSrc1), e2 = IsRectEmpty(lprcSrc2);
    if (e1 && e2) {
        SetRectEmpty(lprcDst);
        return FALSE;
    }
    if (e1) {
        *lprcDst = *lprcSrc2;
        return TRUE;
    }
    if (e2) {
        *lprcDst = *lprcSrc1;
        return TRUE;
    }
    RECT r;
    r.left = std::min(lprcSrc1->left, lprcSrc2->left);
    r.top = std::min(lprcSrc1->top, lprcSrc2->top);
    r.right = std::max(lprcSrc1->right, lprcSrc2->right);
    r.bottom = std::max(lprcSrc1->bottom, lprcSrc2->bottom);
    *lprcDst = r;
    return TRUE;
}

BOOL PtInRect(const RECT* lprc, POINT pt) {
    return lprc && pt.x >= lprc->left && pt.x < lprc->right && pt.y >= lprc->top && pt.y < lprc->bottom;
}

BOOL EqualRect(const RECT* lprc1, const RECT* lprc2) {
    return lprc1 && lprc2 && lprc1->left == lprc2->left && lprc1->top == lprc2->top &&
           lprc1->right == lprc2->right && lprc1->bottom == lprc2->bottom;
}
