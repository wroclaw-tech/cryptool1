#include "gdi_internal.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

IMPLEMENT_DYNCREATE(CDC, CObject)
IMPLEMENT_DYNAMIC(CPaintDC, CDC)
IMPLEMENT_DYNAMIC(CClientDC, CDC)
IMPLEMENT_DYNAMIC(CWindowDC, CDC)

namespace mfcwx {

namespace {

struct PaintSession {
    wxDC* dc = nullptr;
    std::unique_ptr<wxDC> own;
    std::vector<DCState*> users;
};

struct Overlay {
    wxBitmap bitmap;
    std::unique_ptr<wxMemoryDC> dc;
    std::vector<DCState*> users;
};

struct ClipInfo {
    DCState* owner = nullptr;
    bool clipped = false;
};

std::unordered_set<DCState*>& States() {
    static auto* states = new std::unordered_set<DCState*>;
    return *states;
}

std::unordered_map<HDC, CDC*>& TempDCs() {
    static auto* temps = new std::unordered_map<HDC, CDC*>;
    return *temps;
}

std::vector<CDC*>& DetachedTempDCs() {
    static auto* detached = new std::vector<CDC*>;
    return *detached;
}

std::unordered_map<wxWindow*, PaintSession>& Sessions() {
    static auto* sessions = new std::unordered_map<wxWindow*, PaintSession>;
    return *sessions;
}

std::unordered_map<wxWindow*, Overlay>& Overlays() {
    static auto* overlays = new std::unordered_map<wxWindow*, Overlay>;
    return *overlays;
}

std::unordered_map<wxDC*, ClipInfo>& ClipOwners() {
    static auto* owners = new std::unordered_map<wxDC*, ClipInfo>;
    return *owners;
}

std::unordered_set<wxWindow*>& HookedWindows() {
    static auto* hooked = new std::unordered_set<wxWindow*>;
    return *hooked;
}

void Remove(std::vector<DCState*>& v, DCState* s) { v.erase(std::remove(v.begin(), v.end(), s), v.end()); }

void ForgetDC(wxDC* d) {
    if (d)
        ClipOwners().erase(d);
}

bool NativePainting(wxWindow* w) {
#if defined(__WXOSX__)
    return w && w->MacGetCGContextRef() != nullptr;
#elif defined(__WXGTK3__)
    return w && w->GTKPaintContext() != nullptr;
#else
    (void)w;
    return false;
#endif
}

bool ClientDrawsDirectly() {
#if defined(__WXOSX__)
    return false;
#elif defined(__WXGTK__)
    static bool wayland = wxGetDisplayInfo().type == wxDisplayWayland;
    return !wayland;
#else
    return true;
#endif
}

void OnWindowDestroyed(wxWindow* w);

void HookDestroy(wxWindow* w) {
    if (!w || !HookedWindows().insert(w).second)
        return;
    w->Bind(wxEVT_DESTROY, [w](wxWindowDestroyEvent& e) {
        e.Skip();
        if (e.GetEventObject() == w)
            OnWindowDestroyed(w);
    });
}

void EndSession(wxWindow* w) {
    auto it = Sessions().find(w);
    if (it == Sessions().end())
        return;
    PaintSession& ps = it->second;
    for (DCState* u : ps.users) {
        u->dc = nullptr;
        u->target = WinTarget::None;
    }
    ForgetDC(ps.dc);
    std::unique_ptr<wxDC> own = std::move(ps.own);
    Sessions().erase(it);
    own.reset();
}

void ReleaseOverlayDC(Overlay& ov) {
    for (DCState* u : ov.users) {
        u->dc = nullptr;
        u->target = WinTarget::None;
    }
    ov.users.clear();
    if (ov.dc) {
        ForgetDC(ov.dc.get());
        ov.dc->SelectObject(wxNullBitmap);
        ov.dc.reset();
    }
}

void ReleaseTarget(DCState* s) {
    if (s->kind != DCKind::Window)
        return;
    if (s->dc) {
        auto c = ClipOwners().find(s->dc);
        if (c != ClipOwners().end() && c->second.owner == s) {
            if (c->second.clipped && s->target != WinTarget::Client && s->target != WinTarget::OverlayPending)
                s->dc->DestroyClippingRegion();
            ClipOwners().erase(c);
        }
    }
    if (s->target == WinTarget::Paint && s->window) {
        auto it = Sessions().find(s->window);
        if (it != Sessions().end()) {
            Remove(it->second.users, s);
            if (it->second.own && it->second.users.empty())
                EndSession(s->window);
        }
    } else if (s->target == WinTarget::Overlay && s->window) {
        auto it = Overlays().find(s->window);
        if (it != Overlays().end()) {
            Remove(it->second.users, s);
            if (it->second.users.empty() && it->second.dc) {
                ForgetDC(it->second.dc.get());
                it->second.dc->SelectObject(wxNullBitmap);
                it->second.dc.reset();
            }
        }
    } else if (s->owned) {
        ForgetDC(s->owned.get());
        s->owned.reset();
    }
    s->dc = nullptr;
    s->target = WinTarget::None;
}

wxBitmap TransparentBitmap(wxSize size, double scale) {
    int pw = std::max(1, static_cast<int>(std::lround(size.x * scale)));
    int ph = std::max(1, static_cast<int>(std::lround(size.y * scale)));
    wxImage img(pw, ph, true);
    img.InitAlpha();
    memset(img.GetAlpha(), 0, static_cast<size_t>(pw) * ph);
    return wxBitmap(img, 32, scale);
}

void AcquireWindowTarget(DCState* s) {
    wxWindow* w = s->window;
    auto it = Sessions().find(w);
    if (it == Sessions().end() && NativePainting(w)) {
        PaintSession ps;
        ps.own.reset(new wxPaintDC(w));
        ps.dc = ps.own.get();
        it = Sessions().emplace(w, std::move(ps)).first;
        HookDestroy(w);
    }
    if (it != Sessions().end()) {
        s->dc = it->second.dc;
        s->target = WinTarget::Paint;
        it->second.users.push_back(s);
    } else if (ClientDrawsDirectly()) {
        if (s->wholeWindow)
            s->owned.reset(new wxWindowDC(w));
        else
            s->owned.reset(new wxClientDC(w));
        s->dc = s->owned.get();
        s->target = WinTarget::Client;
    } else {
        s->owned.reset(new wxClientDC(w));
        s->dc = s->owned.get();
        s->target = WinTarget::OverlayPending;
    }
    s->clipDirty = true;
}

void SwitchToOverlay(DCState* s) {
    wxWindow* w = s->window;
    if (s->owned) {
        ForgetDC(s->owned.get());
        s->owned.reset();
    }
    s->dc = nullptr;
    wxSize size = w->GetClientSize();
    size.x = std::max(1, size.x);
    size.y = std::max(1, size.y);
    double scale = w->GetContentScaleFactor();
    Overlay& ov = Overlays()[w];
    if (ov.bitmap.IsOk()) {
        wxSize cur(static_cast<int>(std::lround(ov.bitmap.GetLogicalWidth())),
                   static_cast<int>(std::lround(ov.bitmap.GetLogicalHeight())));
        if (cur != size || std::fabs(ov.bitmap.GetScaleFactor() - scale) > 1e-6) {
            ReleaseOverlayDC(ov);
            ov.bitmap = wxBitmap();
        }
    }
    if (!ov.bitmap.IsOk())
        ov.bitmap = TransparentBitmap(size, scale);
    if (!ov.dc) {
        ov.dc.reset(new wxMemoryDC);
        ov.dc->SelectObject(ov.bitmap);
    }
    ov.users.push_back(s);
    s->dc = ov.dc.get();
    s->target = WinTarget::Overlay;
    s->clipDirty = true;
    HookDestroy(w);
}

void OnWindowDestroyed(wxWindow* w) {
    GdiLock lock;
    for (DCState* s : States()) {
        if (s->window != w)
            continue;
        if (s->kind == DCKind::Window)
            ReleaseTarget(s);
        s->window = nullptr;
    }
    EndSession(w);
    auto it = Overlays().find(w);
    if (it != Overlays().end()) {
        ReleaseOverlayDC(it->second);
        Overlays().erase(it);
    }
    HookedWindows().erase(w);
}

double ScreenScale() {
    wxWindow* top = wxTheApp ? wxTheApp->GetTopWindow() : nullptr;
    return top ? top->GetContentScaleFactor() : 1.0;
}

wxBitmap& CurrentBitmap(DCState* s) {
    GdiObjectImpl* impl = s->SelectedBitmap();
    if (impl && impl->bitmap.IsOk())
        return impl->bitmap;
    return s->defaultBitmap;
}

void DeselectBitmap(DCState* s) {
    if (s->kind != DCKind::Memory || s->bitmapOut || !s->dc)
        return;
    static_cast<wxMemoryDC*>(s->dc)->SelectObject(wxNullBitmap);
    s->bitmapOut = true;
}

void ReselectBitmap(DCState* s) {
    if (s->kind != DCKind::Memory || !s->dc)
        return;
    static_cast<wxMemoryDC*>(s->dc)->SelectObject(CurrentBitmap(s));
    s->bitmapOut = false;
    s->clipDirty = true;
}

DCAttrs DefaultAttrs(bool memory) {
    DCAttrs a;
    a.pen = ::GetStockObject(BLACK_PEN);
    a.brush = ::GetStockObject(WHITE_BRUSH);
    a.font = ::GetStockObject(SYSTEM_FONT);
    a.palette = ::GetStockObject(DEFAULT_PALETTE);
    a.bitmap = memory ? DefaultBitmapHandle() : nullptr;
    return a;
}

void AddRefs(const DCAttrs& a) {
    GdiAddRef(a.pen);
    GdiAddRef(a.brush);
    GdiAddRef(a.font);
    GdiAddRef(a.bitmap);
    GdiAddRef(a.palette);
}

void ReleaseRefs(const DCAttrs& a) {
    GdiRelease(a.pen);
    GdiRelease(a.brush);
    GdiRelease(a.font);
    GdiRelease(a.bitmap);
    GdiRelease(a.palette);
}

DCState* NewState(DCKind kind) {
    auto* s = new DCState;
    s->kind = kind;
    s->a = DefaultAttrs(kind == DCKind::Memory);
    States().insert(s);
    return s;
}

DCState* NewWindowState(wxWindow* w, bool whole) {
    if (!w)
        return NewState(DCKind::Info);
    DCState* s = NewState(DCKind::Window);
    s->window = w;
    s->wholeWindow = whole;
    s->scale = w->GetContentScaleFactor();
    HookDestroy(w);
    return s;
}

DCState* NewMemoryState(DCState* src) {
    DCState* s = NewState(DCKind::Memory);
    if (src)
        s->scale = src->kind == DCKind::Memory ? CurrentBitmap(src).GetScaleFactor() : src->scale;
    else
        s->scale = ScreenScale();
    if (s->scale <= 0)
        s->scale = 1;
    s->defaultBitmap = NewBitmap(1, 1, s->scale, *wxWHITE);
    auto* mdc = new wxMemoryDC;
    mdc->SelectObject(s->defaultBitmap);
    s->owned.reset(mdc);
    s->dc = mdc;
    return s;
}

void FreeState(DCState* s) {
    if (!s || !States().count(s))
        return;
    if (s->owner) {
        s->owner->m_hDC = nullptr;
        s->owner->m_hAttribDC = nullptr;
        s->owner = nullptr;
    }
    auto it = TempDCs().find(ToHdc(s));
    if (it != TempDCs().end()) {
        it->second->m_hDC = nullptr;
        it->second->m_hAttribDC = nullptr;
        DetachedTempDCs().push_back(it->second);
        TempDCs().erase(it);
    }
    if (s->kind == DCKind::Window) {
        ReleaseTarget(s);
    } else if (s->kind == DCKind::Memory) {
        if (GdiObjectImpl* b = s->SelectedBitmap())
            b->selectedIn = nullptr;
        DeselectBitmap(s);
    } else if (s->kind == DCKind::Borrowed && s->dc) {
        auto c = ClipOwners().find(s->dc);
        if (c != ClipOwners().end() && c->second.clipped)
            s->dc->DestroyClippingRegion();
    }
    if (s->sessionOwner && s->window)
        EndSession(s->window);
    for (auto c = ClipOwners().begin(); c != ClipOwners().end();) {
        if (c->second.owner == s)
            c = ClipOwners().erase(c);
        else
            ++c;
    }
    if (s->owned) {
        ForgetDC(s->owned.get());
        s->owned.reset();
    }
    ReleaseRefs(s->a);
    for (const DCAttrs& saved : s->saved)
        ReleaseRefs(saved);
    States().erase(s);
    delete s;
}

int RegionType(const wxRegion& r) {
    if (!r.IsOk() || r.IsEmpty())
        return NULLREGION;
    int n = 0;
    for (wxRegionIterator it(r); it && n < 2; ++it)
        ++n;
    return n <= 1 ? SIMPLEREGION : COMPLEXREGION;
}

wxRasterOperationMode Rop2ToWx(int rop2) {
    switch (rop2) {
    case R2_BLACK:
        return wxCLEAR;
    case R2_NOTMERGEPEN:
        return wxNOR;
    case R2_MASKNOTPEN:
        return wxAND_INVERT;
    case R2_NOTCOPYPEN:
        return wxSRC_INVERT;
    case R2_MASKPENNOT:
        return wxAND_REVERSE;
    case R2_NOT:
        return wxINVERT;
    case R2_XORPEN:
        return wxXOR;
    case R2_NOTMASKPEN:
        return wxNAND;
    case R2_MASKPEN:
        return wxAND;
    case R2_NOTXORPEN:
        return wxEQUIV;
    case R2_NOP:
        return wxNO_OP;
    case R2_MERGENOTPEN:
        return wxOR_INVERT;
    case R2_MERGEPENNOT:
        return wxOR_REVERSE;
    case R2_MERGEPEN:
        return wxOR;
    case R2_WHITE:
        return wxSET;
    default:
        return wxCOPY;
    }
}

wxColour Rop2Colour(int rop2, const wxColour& c) {
    switch (rop2) {
    case R2_BLACK:
        return *wxBLACK;
    case R2_WHITE:
        return *wxWHITE;
    case R2_NOTCOPYPEN:
        return wxColour(255 - c.Red(), 255 - c.Green(), 255 - c.Blue());
    default:
        return c;
    }
}

void SetRop(DCState* s, wxDC* d, int rop2) {
    if (!s->SupportsLogicalOps(d))
        return;
    wxRasterOperationMode m = Rop2ToWx(rop2);
    if (d->GetLogicalFunction() != m)
        d->SetLogicalFunction(m);
}

HGDIOBJ SelectBitmapImpl(DCState* s, HGDIOBJ h, GdiObjectImpl* impl) {
    if (s->kind != DCKind::Memory)
        return nullptr;
    if (!impl->stock && impl->selectedIn && impl->selectedIn != s)
        return nullptr;
    HGDIOBJ old = s->a.bitmap;
    if (old == h)
        return old;
    if (GdiObjectImpl* cur = s->SelectedBitmap())
        cur->selectedIn = nullptr;
    DeselectBitmap(s);
    GdiAddRef(h);
    s->a.bitmap = h;
    if (!impl->stock)
        impl->selectedIn = s;
    ReselectBitmap(s);
    s->pixels.reset();
    GdiRelease(old);
    return old;
}

int SelectClipImpl(DCState* s, const wxRegion* rgn) {
    if (!rgn) {
        s->a.hasClip = false;
        s->a.clip = wxRegion();
    } else {
        s->a.hasClip = true;
        s->a.clip = *rgn;
    }
    s->clipDirty = true;
    return s->a.hasClip ? RegionType(s->a.clip) : SIMPLEREGION;
}

HGDIOBJ SelectImpl(DCState* s, HGDIOBJ h) {
    if (!s || !h)
        return nullptr;
    HGDIOBJ old = nullptr;
    if (SysColorBrushIndex(h) >= 0) {
        old = s->a.brush;
        s->a.brush = h;
        GdiRelease(old);
        return old;
    }
    GdiObjectImpl* impl = GdiImpl(h);
    if (!impl)
        return nullptr;
    HGDIOBJ* slot = nullptr;
    switch (impl->kind) {
    case GdiKind::Pen:
        slot = &s->a.pen;
        break;
    case GdiKind::Brush:
        slot = &s->a.brush;
        break;
    case GdiKind::Font:
        slot = &s->a.font;
        break;
    case GdiKind::Palette:
        slot = &s->a.palette;
        break;
    case GdiKind::Bitmap:
        return SelectBitmapImpl(s, h, impl);
    case GdiKind::Region:
        return reinterpret_cast<HGDIOBJ>(static_cast<intptr_t>(SelectClipImpl(s, &impl->region)));
    }
    old = *slot;
    if (old == h)
        return old;
    GdiAddRef(h);
    *slot = h;
    GdiRelease(old);
    return old;
}

void RestoreAttrs(DCState* s, const DCAttrs& saved) {
    SelectImpl(s, saved.pen);
    SelectImpl(s, saved.brush);
    SelectImpl(s, saved.font);
    SelectImpl(s, saved.palette);
    if (s->kind == DCKind::Memory && saved.bitmap)
        SelectImpl(s, saved.bitmap);
    DCAttrs keep = s->a;
    s->a = saved;
    s->a.pen = keep.pen;
    s->a.brush = keep.brush;
    s->a.font = keep.font;
    s->a.palette = keep.palette;
    s->a.bitmap = keep.bitmap;
    s->clipDirty = true;
}

bool HasPen(DCState* s) { return !PenIsNull(s->a.pen) && s->a.rop2 != R2_NOP; }

int DevicePenWidth(DCState* s) {
    double sx, sy;
    s->Scales(sx, sy);
    return s->a.pen ? std::max(1, static_cast<int>(std::lround(PenWidth(s->a.pen) * std::fabs(sx)))) : 1;
}

void SetSolidBrush(wxDC* d, const wxColour& c) {
    wxBrush b(c);
    if (d->GetBrush() != b)
        d->SetBrush(b);
}

void SetNoPen(wxDC* d) {
    if (d->GetPen().GetStyle() != wxPENSTYLE_TRANSPARENT)
        d->SetPen(*wxTRANSPARENT_PEN);
}

bool NeedsHatchBackground(DCState* s) { return s->a.bkMode == OPAQUE && BrushIsHatched(s->a.brush); }

template <class F>
void DrawShape(DCState* s, wxDC* d, bool fill, F draw) {
    if (fill && NeedsHatchBackground(s)) {
        SetRop(s, d, R2_COPYPEN);
        SetNoPen(d);
        SetSolidBrush(d, ToWxColour(s->a.bkColor));
        draw();
    }
    s->ApplyPen(d);
    s->ApplyBrush(d, fill);
    draw();
    SetRop(s, d, R2_COPYPEN);
}

void DrawSegment(DCState* s, wxDC* d, wxPoint p1, wxPoint p2) {
    if (p1 == p2)
        return;
    int width = DevicePenWidth(s);
    bool solid = PenStyle(s->a.pen) == PS_SOLID || PenStyle(s->a.pen) == PS_INSIDEFRAME;
    if (width <= 1 && solid && (p1.x == p2.x || p1.y == p2.y)) {
        if (p1.x == p2.x)
            p2.y += p2.y > p1.y ? -1 : 1;
        else
            p2.x += p2.x > p1.x ? -1 : 1;
        if (p1 == p2) {
            SetRop(s, d, s->a.rop2);
            FillDeviceRectColor(d, wxRect(p1, wxSize(1, 1)), Rop2Colour(s->a.rop2, ToWxColour(PenColor(s->a.pen))));
            SetRop(s, d, R2_COPYPEN);
            return;
        }
    }
    s->ApplyPen(d);
    d->DrawLine(p1, p2);
    SetRop(s, d, R2_COPYPEN);
}

void DrawPolyline(DCState* s, wxDC* d, const std::vector<wxPoint>& pts) {
    if (pts.size() < 2 || !HasPen(s))
        return;
    bool solid = PenStyle(s->a.pen) == PS_SOLID || PenStyle(s->a.pen) == PS_INSIDEFRAME;
    if (DevicePenWidth(s) <= 1 && solid) {
        for (size_t i = 0; i + 1 < pts.size(); ++i)
            DrawSegment(s, d, pts[i], pts[i + 1]);
        return;
    }
    s->ApplyPen(d);
    d->DrawLines(static_cast<int>(pts.size()), pts.data());
    SetRop(s, d, R2_COPYPEN);
}

std::vector<wxPoint> DevicePoints(DCState* s, const POINT* p, int n) {
    std::vector<wxPoint> out;
    out.reserve(static_cast<size_t>(std::max(0, n)));
    for (int i = 0; i < n; ++i)
        out.push_back(s->ToDevice(p[i].x, p[i].y));
    return out;
}

std::vector<wxPoint> ArcPoints(const wxRect& r, wxPoint start, wxPoint end, bool full) {
    double rx = (r.width - 1) / 2.0, ry = (r.height - 1) / 2.0;
    double cx = r.x + rx, cy = r.y + ry;
    if (rx <= 0 || ry <= 0)
        return {};
    auto angle = [&](wxPoint p) { return std::atan2(-(p.y - cy) / ry, (p.x - cx) / rx); };
    double a0 = angle(start), a1 = angle(end);
    if (full || a1 <= a0 + 1e-9)
        a1 += 2 * M_PI;
    int n = std::max(8, static_cast<int>((rx + ry) * (a1 - a0) / 4));
    std::vector<wxPoint> pts;
    for (int i = 0; i <= n; ++i) {
        double t = a0 + (a1 - a0) * i / n;
        pts.emplace_back(static_cast<int>(std::lround(cx + rx * std::cos(t))),
                         static_cast<int>(std::lround(cy - ry * std::sin(t))));
    }
    return pts;
}

void FillRegionRects(DCState* s, wxDC* d, const wxRegion& logical, HBRUSH brush) {
    if (!logical.IsOk() || logical.IsEmpty())
        return;
    for (wxRegionIterator it(logical); it; ++it) {
        wxRect r = it.GetRect();
        FillDeviceRect(s, d, s->DeviceRect(r.x, r.y, r.x + r.width, r.y + r.height), brush);
    }
}

void EdgeRect(wxDC* d, const wxRect& r, UINT flags, COLORREF lt, COLORREF rb) {
    if (flags & BF_LEFT)
        FillDeviceRectColor(d, wxRect(r.x, r.y, 1, r.height), ToWxColour(lt));
    if (flags & BF_TOP)
        FillDeviceRectColor(d, wxRect(r.x, r.y, r.width, 1), ToWxColour(lt));
    if (flags & BF_RIGHT)
        FillDeviceRectColor(d, wxRect(r.x + r.width - 1, r.y, 1, r.height), ToWxColour(rb));
    if (flags & BF_BOTTOM)
        FillDeviceRectColor(d, wxRect(r.x, r.y + r.height - 1, r.width, 1), ToWxColour(rb));
}

wxRect ShrinkEdges(const wxRect& r, UINT flags) {
    wxRect o = r;
    if (flags & BF_LEFT) {
        o.x += 1;
        o.width -= 1;
    }
    if (flags & BF_TOP) {
        o.y += 1;
        o.height -= 1;
    }
    if (flags & BF_RIGHT)
        o.width -= 1;
    if (flags & BF_BOTTOM)
        o.height -= 1;
    return o;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// DCState

GdiObjectImpl* DCState::SelectedBitmap() const {
    GdiObjectImpl* impl = GdiImpl(a.bitmap, GdiKind::Bitmap);
    return impl && !impl->stock ? impl : nullptr;
}

wxDC* DCState::Target(bool drawing) {
    wxDC* d = nullptr;
    switch (kind) {
    case DCKind::Info:
        return nullptr;
    case DCKind::Borrowed:
        d = dc;
        break;
    case DCKind::Memory:
        if (bitmapOut)
            ReselectBitmap(this);
        d = dc;
        break;
    case DCKind::Window:
        if (!window)
            return nullptr;
        if (target == WinTarget::Paint) {
            auto it = Sessions().find(window);
            if (it == Sessions().end())
                ReleaseTarget(this);
            else if (it->second.own && !NativePainting(window))
                EndSession(window);
        }
        if (target == WinTarget::None)
            AcquireWindowTarget(this);
        if (drawing && target == WinTarget::OverlayPending)
            SwitchToOverlay(this);
        d = dc;
        break;
    }
    if (d && drawing) {
        auto it = ClipOwners().find(d);
        bool mine = it != ClipOwners().end() && it->second.owner == this;
        if (clipDirty || !mine)
            ApplyClip(d);
    }
    return d;
}

wxDC* DCState::Measure() {
    wxDC* d = Target(false);
    if (!d)
        d = &MeasureDC();
    ApplyFont(d);
    return d;
}

void DCState::Finish() {
    if (kind == DCKind::Memory)
        pixels.reset();
    else if (kind == DCKind::Window && target == WinTarget::Overlay && window)
        window->Refresh(false);
}

void DCState::Scales(double& sx, double& sy) const {
    if (a.mapMode == MM_TEXT) {
        sx = sy = 1.0;
        return;
    }
    sx = a.winExt.cx ? static_cast<double>(a.vpExt.cx) / a.winExt.cx : 1.0;
    sy = a.winExt.cy ? static_cast<double>(a.vpExt.cy) / a.winExt.cy : 1.0;
    if (sx == 0)
        sx = 1;
    if (sy == 0)
        sy = 1;
}

wxPoint DCState::ToDevice(double x, double y) const {
    double sx, sy;
    Scales(sx, sy);
    return wxPoint(static_cast<int>(std::lround((x - a.winOrg.x) * sx + a.vpOrg.x)),
                   static_cast<int>(std::lround((y - a.winOrg.y) * sy + a.vpOrg.y)));
}

void DCState::ToLogical(double& x, double& y) const {
    double sx, sy;
    Scales(sx, sy);
    x = (x - a.vpOrg.x) / sx + a.winOrg.x;
    y = (y - a.vpOrg.y) / sy + a.winOrg.y;
}

wxRect DCState::DeviceRect(int l, int t, int r, int b) const {
    wxPoint p1 = ToDevice(l, t), p2 = ToDevice(r, b);
    return wxRect(std::min(p1.x, p2.x), std::min(p1.y, p2.y), std::abs(p2.x - p1.x), std::abs(p2.y - p1.y));
}

int DCState::LogicalWidth(int deviceCx) const {
    double sx, sy;
    Scales(sx, sy);
    return static_cast<int>(std::lround(deviceCx / std::fabs(sx)));
}

int DCState::LogicalHeight(int deviceCy) const {
    double sx, sy;
    Scales(sx, sy);
    return static_cast<int>(std::lround(deviceCy / std::fabs(sy)));
}

int DCState::DeviceWidth(int logicalCx) const {
    double sx, sy;
    Scales(sx, sy);
    return static_cast<int>(std::lround(logicalCx * std::fabs(sx)));
}

int DCState::DeviceHeight(int logicalCy) const {
    double sx, sy;
    Scales(sx, sy);
    return static_cast<int>(std::lround(logicalCy * std::fabs(sy)));
}

int DCState::Dpi() const {
    if (owner && owner->m_bPrinting && dc) {
        int ppi = dc->GetPPI().y;
        if (ppi > 0)
            return ppi;
    }
    return 96;
}

bool DCState::SupportsLogicalOps(wxDC* d) const { return d && d->GetGraphicsContext() == nullptr; }

void DCState::ApplyPen(wxDC* d) {
    double sx, sy;
    Scales(sx, sy);
    wxPen p = DevicePen(a.pen, std::fabs(sx));
    if (a.rop2 == R2_NOP) {
        p = *wxTRANSPARENT_PEN;
    } else if (a.rop2 != R2_COPYPEN) {
        if (SupportsLogicalOps(d))
            SetRop(this, d, a.rop2);
        else if (p.GetStyle() != wxPENSTYLE_TRANSPARENT)
            p.SetColour(Rop2Colour(a.rop2, p.GetColour()));
    }
    if (d->GetPen() != p)
        d->SetPen(p);
}

void DCState::ApplyBrush(wxDC* d, bool fill) {
    wxBrush b = fill ? DeviceBrush(a.brush) : *wxTRANSPARENT_BRUSH;
    if (fill && a.rop2 != R2_COPYPEN && !SupportsLogicalOps(d) && b.GetStyle() == wxBRUSHSTYLE_SOLID)
        b = wxBrush(Rop2Colour(a.rop2, b.GetColour()));
    if (d->GetBrush() != b)
        d->SetBrush(b);
}

void DCState::ApplyFont(wxDC* d) {
    double sx, sy;
    Scales(sx, sy);
    wxFont f = DeviceFont(a.font, std::fabs(sy));
    if (f.IsOk() && d->GetFont() != f)
        d->SetFont(f);
    wxColour fg = ToWxColour(a.textColor);
    if (d->GetTextForeground() != fg)
        d->SetTextForeground(fg);
    if (d->GetBackgroundMode() != wxBRUSHSTYLE_TRANSPARENT)
        d->SetBackgroundMode(wxBRUSHSTYLE_TRANSPARENT);
}

void DCState::ApplyClip(wxDC* d, const wxRect* extra) {
    ClipInfo& info = ClipOwners()[d];
    wxRegion r;
    bool any = false;
    if (a.hasClip) {
        r = a.clip;
        any = true;
    }
    if (extra) {
        if (any) {
            if (r.IsOk() && !r.IsEmpty())
                r.Intersect(*extra);
        } else {
            r = wxRegion(*extra);
        }
        any = true;
    }
    if (!any) {
        if (info.clipped)
            d->DestroyClippingRegion();
        info.clipped = false;
    } else {
        d->DestroyClippingRegion();
        if (!r.IsOk() || r.IsEmpty()) {
            d->SetClippingRegion(wxRect(-32000, -32000, 1, 1));
        } else if (RegionType(r) == SIMPLEREGION) {
            d->SetClippingRegion(r.GetBox());
        } else {
            wxRegion dev = r;
            dev.Offset(d->LogicalToDeviceX(0), d->LogicalToDeviceY(0));
            d->SetDeviceClippingRegion(dev);
        }
        info.clipped = true;
    }
    info.owner = this;
    if (!extra)
        clipDirty = false;
    else
        clipDirty = true;
}

DCState* DCFromHandle(HDC h) {
    if (!h)
        return nullptr;
    GdiLock lock;
    auto* s = reinterpret_cast<DCState*>(h);
    return States().count(s) ? s : nullptr;
}

wxDC& MeasureDC() {
    static wxMemoryDC* dc = [] {
        auto* d = new wxMemoryDC;
        static wxBitmap bmp(1, 1, 24);
        d->SelectObject(bmp);
        return d;
    }();
    return *dc;
}

void SyncBitmap(GdiObjectImpl* bmp) {
    if (!bmp || !bmp->selectedIn)
        return;
    DeselectBitmap(bmp->selectedIn);
}

const wxImage* DCPixels(DCState* s) {
    if (!s || s->kind != DCKind::Memory)
        return nullptr;
    if (!s->pixels) {
        DeselectBitmap(s);
        wxBitmap& bmp = CurrentBitmap(s);
        s->pixels.reset(new wxImage(bmp.ConvertToImage()));
    }
    return s->pixels.get();
}

double DCBitmapScale(DCState* s) {
    if (!s)
        return 1.0;
    if (s->kind == DCKind::Memory) {
        double scale = CurrentBitmap(s).GetScaleFactor();
        return scale > 0 ? scale : 1.0;
    }
    if (s->dc)
        return s->dc->GetContentScaleFactor();
    return s->scale;
}

void FillDeviceRectColor(wxDC* dc, const wxRect& r, const wxColour& c) {
    if (r.width <= 0 || r.height <= 0)
        return;
    SetNoPen(dc);
    SetSolidBrush(dc, c);
    dc->DrawRectangle(r);
}

void FillDeviceRect(DCState* s, wxDC* dc, const wxRect& r, HBRUSH brush) {
    if (r.width <= 0 || r.height <= 0)
        return;
    if (s && s->a.bkMode == OPAQUE && BrushIsHatched(brush))
        FillDeviceRectColor(dc, r, ToWxColour(s->a.bkColor));
    wxBrush b = DeviceBrush(brush);
    if (!b.IsOk() || b.GetStyle() == wxBRUSHSTYLE_TRANSPARENT)
        return;
    SetNoPen(dc);
    if (dc->GetBrush() != b)
        dc->SetBrush(b);
    dc->DrawRectangle(r);
}

void PurgeTemporaryDCs() {
    std::vector<CDC*> doomed;
    {
        GdiLock lock;
        for (auto& kv : TempDCs()) {
            kv.second->m_hDC = nullptr;
            kv.second->m_hAttribDC = nullptr;
            doomed.push_back(kv.second);
        }
        TempDCs().clear();
        doomed.insert(doomed.end(), DetachedTempDCs().begin(), DetachedTempDCs().end());
        DetachedTempDCs().clear();
        for (DCState* s : States())
            if (s->fromGetDC && s->kind == DCKind::Window && s->target != WinTarget::None)
                ReleaseTarget(s);
    }
    for (CDC* d : doomed)
        delete d;
}

CDC* WrapDC(wxDC* dc, wxWindow* window) {
    GdiLock lock;
    DCState* s = NewState(DCKind::Borrowed);
    s->dc = dc;
    s->window = window;
    s->scale = dc ? dc->GetContentScaleFactor() : 1.0;
    if (window && dc && wxDynamicCast(dc, wxPaintDC) && !Sessions().count(window)) {
        PaintSession ps;
        ps.dc = dc;
        Sessions().emplace(window, std::move(ps));
        s->sessionOwner = true;
        HookDestroy(window);
    } else if (window) {
        HookDestroy(window);
    }
    auto* cdc = new CDC;
    cdc->m_hDC = ToHdc(s);
    cdc->m_hAttribDC = ToHdc(s);
    s->owner = cdc;
    return cdc;
}

void UnwrapDC(CDC* cdc) {
    if (!cdc)
        return;
    {
        GdiLock lock;
        FreeState(DCFromHandle(cdc->m_hDC));
        cdc->m_hDC = nullptr;
        cdc->m_hAttribDC = nullptr;
    }
    delete cdc;
}

void PaintClientDCOverlay(wxWindow* w, wxDC& dc) {
    GdiLock lock;
    auto s = Sessions().find(w);
    if (s != Sessions().end() && s->second.own)
        EndSession(w);
    auto it = Overlays().find(w);
    if (it == Overlays().end() || !it->second.bitmap.IsOk())
        return;
    ReleaseOverlayDC(it->second);
    ForgetDC(&dc);
    dc.DestroyClippingRegion();
    dc.DrawBitmap(it->second.bitmap, 0, 0, true);
}

bool HasClientDCOverlay(wxWindow* w) {
    GdiLock lock;
    return Overlays().count(w) != 0;
}

void ClearClientDCOverlay(wxWindow* w, const wxRect* rect) {
    GdiLock lock;
    auto it = Overlays().find(w);
    if (it == Overlays().end())
        return;
    Overlay& ov = it->second;
    ReleaseOverlayDC(ov);
    if (!rect) {
        Overlays().erase(it);
        return;
    }
    double scale = ov.bitmap.GetScaleFactor();
    wxRect pr(static_cast<int>(std::floor(rect->x * scale)), static_cast<int>(std::floor(rect->y * scale)),
              static_cast<int>(std::ceil(rect->width * scale)), static_cast<int>(std::ceil(rect->height * scale)));
    pr.Intersect(wxRect(0, 0, ov.bitmap.GetWidth(), ov.bitmap.GetHeight()));
    if (pr.IsEmpty())
        return;
    wxImage img = ov.bitmap.ConvertToImage();
    if (!img.HasAlpha())
        img.InitAlpha();
    unsigned char* alpha = img.GetAlpha();
    unsigned char* data = img.GetData();
    for (int y = pr.y; y < pr.GetBottom() + 1; ++y)
        for (int x = pr.x; x < pr.GetRight() + 1; ++x) {
            size_t i = static_cast<size_t>(y) * img.GetWidth() + x;
            alpha[i] = 0;
            data[i * 3] = data[i * 3 + 1] = data[i * 3 + 2] = 0;
        }
    ov.bitmap = wxBitmap(img, 32, scale);
}

} // namespace mfcwx

using namespace mfcwx;

// ---------------------------------------------------------------------------------------------
// CDC

CDC::CDC() : m_hDC(nullptr), m_hAttribDC(nullptr), m_bPrinting(FALSE) {}

CDC::~CDC() {
    if (m_hDC)
        ::DeleteDC(Detach());
}

CDC* CDC::FromHandle(HDC hDC) {
    GdiLock lock;
    DCState* s = DCFromHandle(hDC);
    if (!s)
        return nullptr;
    if (s->owner)
        return s->owner;
    auto it = TempDCs().find(hDC);
    if (it != TempDCs().end())
        return it->second;
    auto* cdc = new CDC;
    cdc->m_hDC = hDC;
    cdc->m_hAttribDC = hDC;
    TempDCs()[hDC] = cdc;
    return cdc;
}

void CDC::DeleteTempMap() { PurgeTemporaryDCs(); }

BOOL CDC::Attach(HDC hDC) {
    GdiLock lock;
    DCState* s = DCFromHandle(hDC);
    if (!s)
        return FALSE;
    if (m_hDC && m_hDC != hDC)
        ::DeleteDC(Detach());
    auto it = TempDCs().find(hDC);
    if (it != TempDCs().end()) {
        it->second->m_hDC = nullptr;
        it->second->m_hAttribDC = nullptr;
        DetachedTempDCs().push_back(it->second);
        TempDCs().erase(it);
    }
    s->owner = this;
    m_hDC = hDC;
    m_hAttribDC = hDC;
    return TRUE;
}

HDC CDC::Detach() {
    if (AfxIsNullThis(this))
        return nullptr;
    GdiLock lock;
    HDC h = m_hDC;
    if (DCState* s = DCFromHandle(h))
        if (s->owner == this)
            s->owner = nullptr;
    m_hDC = nullptr;
    m_hAttribDC = nullptr;
    return h;
}

BOOL CDC::CreateCompatibleDC(CDC* pDC) { return Attach(::CreateCompatibleDC(pDC ? pDC->m_hDC : nullptr)); }

BOOL CDC::DeleteDC() {
    if (AfxIsNullThis(this) || !m_hDC)
        return FALSE;
    return ::DeleteDC(Detach());
}

CWnd* CDC::GetWindow() const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    return s && s->window ? CWnd::FromHandle(ToHwnd(s->window)) : nullptr;
}

wxDC* CDC::GetWx() const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s)
        return nullptr;
    wxDC* d = s->Target(true);
    if (!d)
        return nullptr;
    s->ApplyPen(d);
    s->ApplyBrush(d, true);
    s->ApplyFont(d);
    s->Finish();
    return d;
}

int CDC::SaveDC() { return ::SaveDC(m_hDC); }

BOOL CDC::RestoreDC(int nSavedDC) { return ::RestoreDC(m_hDC, nSavedDC); }

CGdiObject* CDC::SelectStockObject(int nIndex) {
    return CGdiObject::FromHandle(::SelectObject(m_hDC, ::GetStockObject(nIndex)));
}

CPen* CDC::SelectObject(CPen* pPen) {
    return static_cast<CPen*>(CGdiObject::FromHandle(::SelectObject(m_hDC, pPen ? pPen->m_hObject : nullptr)));
}

CBrush* CDC::SelectObject(CBrush* pBrush) {
    return static_cast<CBrush*>(CGdiObject::FromHandle(::SelectObject(m_hDC, pBrush ? pBrush->m_hObject : nullptr)));
}

CFont* CDC::SelectObject(CFont* pFont) {
    return static_cast<CFont*>(CGdiObject::FromHandle(::SelectObject(m_hDC, pFont ? pFont->m_hObject : nullptr)));
}

CBitmap* CDC::SelectObject(CBitmap* pBitmap) {
    return static_cast<CBitmap*>(
        CGdiObject::FromHandle(::SelectObject(m_hDC, pBitmap ? pBitmap->m_hObject : nullptr)));
}

int CDC::SelectObject(CRgn* pRgn) { return ::SelectClipRgn(m_hDC, pRgn ? pRgn->m_hObject : nullptr); }

CGdiObject* CDC::SelectObject(CGdiObject* pObject) {
    if (!pObject)
        return nullptr;
    GdiLock lock;
    GdiObjectImpl* impl = GdiImpl(pObject->m_hObject);
    if (impl && impl->kind == GdiKind::Region) {
        ::SelectClipRgn(m_hDC, pObject->m_hObject);
        return nullptr;
    }
    return CGdiObject::FromHandle(::SelectObject(m_hDC, pObject->m_hObject));
}

HGDIOBJ CDC::SelectObject(HGDIOBJ hObject) { return ::SelectObject(m_hDC, hObject); }

CPalette* CDC::SelectPalette(CPalette* pPalette, BOOL) {
    return static_cast<CPalette*>(
        CGdiObject::FromHandle(::SelectObject(m_hDC, pPalette ? pPalette->m_hObject : nullptr)));
}

CPen* CDC::GetCurrentPen() const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    return s ? static_cast<CPen*>(CGdiObject::FromHandle(s->a.pen)) : nullptr;
}

CBrush* CDC::GetCurrentBrush() const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    return s ? static_cast<CBrush*>(CGdiObject::FromHandle(s->a.brush)) : nullptr;
}

CFont* CDC::GetCurrentFont() const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    return s ? static_cast<CFont*>(CGdiObject::FromHandle(s->a.font)) : nullptr;
}

CBitmap* CDC::GetCurrentBitmap() const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    return s ? static_cast<CBitmap*>(CGdiObject::FromHandle(s->a.bitmap)) : nullptr;
}

int CDC::GetDeviceCaps(int nIndex) const { return ::GetDeviceCaps(m_hDC, nIndex); }

COLORREF CDC::GetBkColor() const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    return s ? s->a.bkColor : CLR_INVALID;
}

COLORREF CDC::SetBkColor(COLORREF crColor) { return ::SetBkColor(m_hDC, crColor); }

int CDC::GetBkMode() const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    return s ? s->a.bkMode : 0;
}

int CDC::SetBkMode(int nBkMode) { return ::SetBkMode(m_hDC, nBkMode); }

COLORREF CDC::GetTextColor() const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    return s ? s->a.textColor : CLR_INVALID;
}

COLORREF CDC::SetTextColor(COLORREF crColor) { return ::SetTextColor(m_hDC, crColor); }

int CDC::GetROP2() const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    return s ? s->a.rop2 : 0;
}

int CDC::SetROP2(int nDrawMode) {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s || nDrawMode < R2_BLACK || nDrawMode > R2_WHITE)
        return 0;
    int old = s->a.rop2;
    s->a.rop2 = nDrawMode;
    return old;
}

int CDC::SetStretchBltMode(int nStretchMode) {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s)
        return 0;
    int old = s->a.stretchMode;
    s->a.stretchMode = nStretchMode;
    return old;
}

int CDC::GetStretchBltMode() const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    return s ? s->a.stretchMode : 0;
}

UINT CDC::SetTextAlign(UINT nFlags) {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s)
        return static_cast<UINT>(-1);
    UINT old = s->a.textAlign;
    s->a.textAlign = nFlags;
    return old;
}

UINT CDC::GetTextAlign() const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    return s ? s->a.textAlign : 0;
}

int CDC::SetMapMode(int nMapMode) {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s || nMapMode < MM_TEXT || nMapMode > MM_ANISOTROPIC)
        return 0;
    int old = s->a.mapMode;
    int dpi = s->Dpi();
    auto fixed = [&](int units) {
        s->a.winExt = SIZE{units, units};
        s->a.vpExt = SIZE{dpi, -dpi};
    };
    switch (nMapMode) {
    case MM_TEXT:
        s->a.winExt = SIZE{1, 1};
        s->a.vpExt = SIZE{1, 1};
        break;
    case MM_LOMETRIC:
        fixed(254);
        break;
    case MM_HIMETRIC:
        fixed(2540);
        break;
    case MM_LOENGLISH:
        fixed(100);
        break;
    case MM_HIENGLISH:
        fixed(1000);
        break;
    case MM_TWIPS:
        fixed(1440);
        break;
    default:
        break;
    }
    s->a.mapMode = nMapMode;
    return old;
}

int CDC::GetMapMode() const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    return s ? s->a.mapMode : 0;
}

CPoint CDC::SetViewportOrg(int x, int y) {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s)
        return CPoint();
    CPoint old(s->a.vpOrg);
    s->a.vpOrg = POINT{x, y};
    return old;
}

CPoint CDC::GetViewportOrg() const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    return s ? CPoint(s->a.vpOrg) : CPoint();
}

CPoint CDC::OffsetViewportOrg(int nWidth, int nHeight) {
    CPoint old = GetViewportOrg();
    SetViewportOrg(old.x + nWidth, old.y + nHeight);
    return old;
}

namespace {

void AdjustIsotropic(DCState* s) {
    if (s->a.mapMode != MM_ISOTROPIC || !s->a.winExt.cx || !s->a.winExt.cy)
        return;
    double sx = static_cast<double>(s->a.vpExt.cx) / s->a.winExt.cx;
    double sy = static_cast<double>(s->a.vpExt.cy) / s->a.winExt.cy;
    double m = std::min(std::fabs(sx), std::fabs(sy));
    if (m <= 0)
        return;
    s->a.vpExt.cx = static_cast<LONG>(std::lround(s->a.winExt.cx * std::copysign(m, sx)));
    s->a.vpExt.cy = static_cast<LONG>(std::lround(s->a.winExt.cy * std::copysign(m, sy)));
}

} // namespace

CSize CDC::SetViewportExt(int cx, int cy) {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s)
        return CSize();
    CSize old(s->a.vpExt);
    if ((s->a.mapMode == MM_ISOTROPIC || s->a.mapMode == MM_ANISOTROPIC) && cx && cy) {
        s->a.vpExt = SIZE{cx, cy};
        AdjustIsotropic(s);
    }
    return old;
}

CSize CDC::GetViewportExt() const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    return s ? CSize(s->a.vpExt) : CSize();
}

CPoint CDC::SetWindowOrg(int x, int y) {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s)
        return CPoint();
    CPoint old(s->a.winOrg);
    s->a.winOrg = POINT{x, y};
    return old;
}

CPoint CDC::GetWindowOrg() const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    return s ? CPoint(s->a.winOrg) : CPoint();
}

CPoint CDC::OffsetWindowOrg(int nWidth, int nHeight) {
    CPoint old = GetWindowOrg();
    SetWindowOrg(old.x + nWidth, old.y + nHeight);
    return old;
}

CSize CDC::SetWindowExt(int cx, int cy) {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s)
        return CSize();
    CSize old(s->a.winExt);
    if ((s->a.mapMode == MM_ISOTROPIC || s->a.mapMode == MM_ANISOTROPIC) && cx && cy) {
        s->a.winExt = SIZE{cx, cy};
        AdjustIsotropic(s);
    }
    return old;
}

CSize CDC::GetWindowExt() const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    return s ? CSize(s->a.winExt) : CSize(1, 1);
}

void CDC::DPtoLP(LPPOINT lpPoints, int nCount) const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s || !lpPoints)
        return;
    for (int i = 0; i < nCount; ++i) {
        double x = lpPoints[i].x, y = lpPoints[i].y;
        s->ToLogical(x, y);
        lpPoints[i].x = static_cast<LONG>(std::lround(x));
        lpPoints[i].y = static_cast<LONG>(std::lround(y));
    }
}

void CDC::DPtoLP(LPRECT lpRect) const { DPtoLP(reinterpret_cast<LPPOINT>(lpRect), 2); }

void CDC::DPtoLP(LPSIZE lpSize) const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s || !lpSize)
        return;
    double sx, sy;
    s->Scales(sx, sy);
    lpSize->cx = static_cast<LONG>(std::lround(lpSize->cx / std::fabs(sx)));
    lpSize->cy = static_cast<LONG>(std::lround(lpSize->cy / std::fabs(sy)));
}

void CDC::LPtoDP(LPPOINT lpPoints, int nCount) const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s || !lpPoints)
        return;
    for (int i = 0; i < nCount; ++i) {
        wxPoint p = s->ToDevice(lpPoints[i].x, lpPoints[i].y);
        lpPoints[i].x = p.x;
        lpPoints[i].y = p.y;
    }
}

void CDC::LPtoDP(LPRECT lpRect) const { LPtoDP(reinterpret_cast<LPPOINT>(lpRect), 2); }

void CDC::LPtoDP(LPSIZE lpSize) const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s || !lpSize)
        return;
    double sx, sy;
    s->Scales(sx, sy);
    lpSize->cx = static_cast<LONG>(std::lround(lpSize->cx * std::fabs(sx)));
    lpSize->cy = static_cast<LONG>(std::lround(lpSize->cy * std::fabs(sy)));
}

int CDC::GetClipBox(LPRECT lpRect) const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s || !lpRect)
        return ERROR;
    wxRect box;
    int type = SIMPLEREGION;
    if (s->a.hasClip) {
        type = RegionType(s->a.clip);
        box = s->a.clip.IsOk() ? s->a.clip.GetBox() : wxRect();
    } else if (s->kind == DCKind::Window && s->window) {
        box = wxRect(wxPoint(0, 0), s->window->GetClientSize());
    } else if (s->kind == DCKind::Memory) {
        wxBitmap& bmp = CurrentBitmap(s);
        box = wxRect(0, 0, static_cast<int>(std::lround(bmp.GetLogicalWidth())),
                     static_cast<int>(std::lround(bmp.GetLogicalHeight())));
    } else if (s->dc) {
        box = wxRect(wxPoint(0, 0), s->dc->GetSize());
    } else if (s->window) {
        box = wxRect(wxPoint(0, 0), s->window->GetClientSize());
    }
    double x0 = box.x, y0 = box.y, x1 = box.x + box.width, y1 = box.y + box.height;
    s->ToLogical(x0, y0);
    s->ToLogical(x1, y1);
    lpRect->left = static_cast<LONG>(std::lround(std::min(x0, x1)));
    lpRect->top = static_cast<LONG>(std::lround(std::min(y0, y1)));
    lpRect->right = static_cast<LONG>(std::lround(std::max(x0, x1)));
    lpRect->bottom = static_cast<LONG>(std::lround(std::max(y0, y1)));
    return type;
}

int CDC::SelectClipRgn(CRgn* pRgn) { return ::SelectClipRgn(m_hDC, pRgn ? pRgn->m_hObject : nullptr); }

int CDC::IntersectClipRect(int x1, int y1, int x2, int y2) {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s)
        return ERROR;
    wxRect r = s->DeviceRect(x1, y1, x2, y2);
    if (s->a.hasClip) {
        if (s->a.clip.IsOk() && !s->a.clip.IsEmpty() && r.width > 0 && r.height > 0)
            s->a.clip.Intersect(r);
        else
            s->a.clip = wxRegion();
    } else {
        s->a.clip = r.width > 0 && r.height > 0 ? wxRegion(r) : wxRegion();
        s->a.hasClip = true;
    }
    s->clipDirty = true;
    return RegionType(s->a.clip);
}

int CDC::IntersectClipRect(LPCRECT lpRect) {
    return lpRect ? IntersectClipRect(lpRect->left, lpRect->top, lpRect->right, lpRect->bottom) : ERROR;
}

CPoint CDC::GetCurrentPosition() const {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    return s ? CPoint(s->a.pos) : CPoint();
}

CPoint CDC::MoveTo(int x, int y) {
    POINT old{0, 0};
    ::MoveToEx(m_hDC, x, y, &old);
    return CPoint(old);
}

BOOL CDC::LineTo(int x, int y) { return ::LineTo(m_hDC, x, y); }

namespace {

BOOL ArcLike(HDC hdc, int kind, int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4) {
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    if (!s)
        return FALSE;
    DrawScope ds(s);
    if (!ds)
        return TRUE;
    wxDC* d = ds.dc();
    wxRect r = s->DeviceRect(x1, y1, x2, y2);
    std::vector<wxPoint> pts = ArcPoints(r, s->ToDevice(x3, y3), s->ToDevice(x4, y4), x3 == x4 && y3 == y4);
    if (pts.size() < 2)
        return TRUE;
    if (kind == 0) {
        DrawPolyline(s, d, pts);
        return TRUE;
    }
    if (kind == 1)
        pts.insert(pts.begin(), wxPoint(r.x + (r.width - 1) / 2, r.y + (r.height - 1) / 2));
    DrawShape(s, d, true, [&] { d->DrawPolygon(static_cast<int>(pts.size()), pts.data()); });
    return TRUE;
}

} // namespace

BOOL CDC::Arc(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4) {
    return ArcLike(m_hDC, 0, x1, y1, x2, y2, x3, y3, x4, y4);
}

BOOL CDC::Arc(LPCRECT lpRect, POINT ptStart, POINT ptEnd) {
    return Arc(lpRect->left, lpRect->top, lpRect->right, lpRect->bottom, ptStart.x, ptStart.y, ptEnd.x, ptEnd.y);
}

BOOL CDC::Pie(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4) {
    return ArcLike(m_hDC, 1, x1, y1, x2, y2, x3, y3, x4, y4);
}

BOOL CDC::Pie(LPCRECT lpRect, POINT ptStart, POINT ptEnd) {
    return Pie(lpRect->left, lpRect->top, lpRect->right, lpRect->bottom, ptStart.x, ptStart.y, ptEnd.x, ptEnd.y);
}

BOOL CDC::Chord(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4) {
    return ArcLike(m_hDC, 2, x1, y1, x2, y2, x3, y3, x4, y4);
}

BOOL CDC::Polyline(const POINT* lpPoints, int nCount) { return ::Polyline(m_hDC, lpPoints, nCount); }

BOOL CDC::PolyBezier(const POINT* lpPoints, int nCount) {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s || !lpPoints || nCount < 4 || (nCount - 1) % 3)
        return FALSE;
    DrawScope ds(s);
    if (!ds)
        return TRUE;
    std::vector<wxPoint> pts;
    double x0 = lpPoints[0].x, y0 = lpPoints[0].y;
    pts.push_back(s->ToDevice(x0, y0));
    for (int i = 1; i + 2 < nCount; i += 3) {
        const POINT &c1 = lpPoints[i], &c2 = lpPoints[i + 1], &e = lpPoints[i + 2];
        for (int k = 1; k <= 24; ++k) {
            double t = k / 24.0, u = 1 - t;
            double x = u * u * u * x0 + 3 * u * u * t * c1.x + 3 * u * t * t * c2.x + t * t * t * e.x;
            double y = u * u * u * y0 + 3 * u * u * t * c1.y + 3 * u * t * t * c2.y + t * t * t * e.y;
            pts.push_back(s->ToDevice(x, y));
        }
        x0 = e.x;
        y0 = e.y;
    }
    DrawPolyline(s, ds.dc(), pts);
    return TRUE;
}

BOOL CDC::PolylineTo(const POINT* lpPoints, int nCount) {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s || !lpPoints || nCount < 1)
        return FALSE;
    std::vector<POINT> all;
    all.push_back(s->a.pos);
    all.insert(all.end(), lpPoints, lpPoints + nCount);
    BOOL r = ::Polyline(m_hDC, all.data(), static_cast<int>(all.size()));
    s->a.pos = lpPoints[nCount - 1];
    return r;
}

void CDC::FillRect(LPCRECT lpRect, CBrush* pBrush) { ::FillRect(m_hDC, lpRect, pBrush ? pBrush->m_hObject : nullptr); }

void CDC::FrameRect(LPCRECT lpRect, CBrush* pBrush) {
    ::FrameRect(m_hDC, lpRect, pBrush ? pBrush->m_hObject : nullptr);
}

void CDC::InvertRect(LPCRECT lpRect) {
    if (lpRect)
        PatBlt(lpRect->left, lpRect->top, lpRect->right - lpRect->left, lpRect->bottom - lpRect->top, DSTINVERT);
}

BOOL CDC::DrawIcon(int x, int y, HICON hIcon) { return ::DrawIcon(m_hDC, x, y, hIcon); }

BOOL CDC::DrawEdge(LPRECT lpRect, UINT nEdge, UINT nFlags) { return ::DrawEdge(m_hDC, lpRect, nEdge, nFlags); }

BOOL CDC::DrawFrameControl(LPRECT lpRect, UINT nType, UINT nState) {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s || !lpRect)
        return FALSE;
    RECT r = *lpRect;
    if (nType == DFC_BUTTON && (nState & 0xFF) == DFCS_BUTTONRADIO) {
        DrawScope ds(s);
        if (!ds)
            return TRUE;
        wxDC* d = ds.dc();
        wxRect dr = s->DeviceRect(r.left, r.top, r.right, r.bottom);
        int size = std::min(dr.width, dr.height);
        wxRect box(dr.x, dr.y + (dr.height - size) / 2, size, size);
        d->SetPen(wxPen(ToWxColour(SysColor(COLOR_BTNSHADOW))));
        d->SetBrush(wxBrush(ToWxColour(SysColor((nState & DFCS_INACTIVE) ? COLOR_BTNFACE : COLOR_WINDOW))));
        d->DrawEllipse(box.x, box.y, box.width - 1, box.height - 1);
        if (nState & DFCS_CHECKED) {
            int inner = std::max(2, size / 3);
            d->SetPen(*wxTRANSPARENT_PEN);
            d->SetBrush(wxBrush(ToWxColour(SysColor(COLOR_WINDOWTEXT))));
            d->DrawEllipse(box.x + (size - inner) / 2, box.y + (size - inner) / 2, inner, inner);
        }
        return TRUE;
    }
    if (nType == DFC_BUTTON && (nState & 0xFF) == DFCS_BUTTONCHECK) {
        ::DrawEdge(m_hDC, &r, EDGE_SUNKEN, BF_RECT | BF_ADJUST);
        HBRUSH bg = GetSysColorBrush((nState & DFCS_INACTIVE) ? COLOR_BTNFACE : COLOR_WINDOW);
        ::FillRect(m_hDC, &r, bg);
        if (nState & DFCS_CHECKED) {
            DrawScope ds(s);
            if (!ds)
                return TRUE;
            wxDC* d = ds.dc();
            wxRect dr = s->DeviceRect(r.left, r.top, r.right, r.bottom);
            d->SetPen(wxPen(ToWxColour(SysColor(COLOR_WINDOWTEXT)), std::max(1, dr.width / 6)));
            wxPoint pts[3] = {wxPoint(dr.x + dr.width / 5, dr.y + dr.height / 2),
                              wxPoint(dr.x + dr.width * 2 / 5, dr.y + dr.height * 3 / 4),
                              wxPoint(dr.x + dr.width * 4 / 5, dr.y + dr.height / 4)};
            d->DrawLines(3, pts);
        }
        return TRUE;
    }
    ::FillRect(m_hDC, &r, GetSysColorBrush(COLOR_BTNFACE));
    UINT edge = (nState & DFCS_PUSHED) ? EDGE_SUNKEN : EDGE_RAISED;
    if (nState & DFCS_FLAT)
        edge = (nState & DFCS_PUSHED) ? BDR_SUNKENOUTER : BDR_RAISEDINNER;
    return ::DrawEdge(m_hDC, &r, edge, BF_RECT);
}

void CDC::DrawFocusRect(LPCRECT lpRect) { ::DrawFocusRect(m_hDC, lpRect); }

BOOL CDC::Ellipse(int x1, int y1, int x2, int y2) { return ::Ellipse(m_hDC, x1, y1, x2, y2); }

BOOL CDC::Polygon(const POINT* lpPoints, int nCount) { return ::Polygon(m_hDC, lpPoints, nCount); }

BOOL CDC::PolyPolygon(const POINT* lpPoints, const INT* lpPolyCounts, int nCount) {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s || !lpPoints || !lpPolyCounts || nCount <= 0)
        return FALSE;
    DrawScope ds(s);
    if (!ds)
        return TRUE;
    int total = 0;
    std::vector<int> counts;
    for (int i = 0; i < nCount; ++i) {
        counts.push_back(lpPolyCounts[i]);
        total += lpPolyCounts[i];
    }
    std::vector<wxPoint> pts = DevicePoints(s, lpPoints, total);
    wxDC* d = ds.dc();
    wxPolygonFillMode mode = s->a.polyFillMode == WINDING ? wxWINDING_RULE : wxODDEVEN_RULE;
    DrawShape(s, d, true, [&] { d->DrawPolyPolygon(nCount, counts.data(), pts.data(), 0, 0, mode); });
    return TRUE;
}

BOOL CDC::Rectangle(int x1, int y1, int x2, int y2) { return ::Rectangle(m_hDC, x1, y1, x2, y2); }

BOOL CDC::RoundRect(int x1, int y1, int x2, int y2, int x3, int y3) {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s)
        return FALSE;
    DrawScope ds(s);
    if (!ds)
        return TRUE;
    wxDC* d = ds.dc();
    wxRect r = s->DeviceRect(x1, y1, x2, y2);
    if (r.width <= 1 || r.height <= 1)
        return TRUE;
    double radius = (s->DeviceWidth(std::abs(x3)) + s->DeviceHeight(std::abs(y3))) / 4.0;
    radius = std::min(radius, std::min(r.width, r.height) / 2.0);
    bool pen = HasPen(s);
    int w = r.width - (pen ? 0 : 1), h = r.height - (pen ? 0 : 1);
    DrawShape(s, d, true, [&] { d->DrawRoundedRectangle(r.x, r.y, w, h, radius); });
    return TRUE;
}

BOOL CDC::RoundRect(LPCRECT lpRect, POINT point) {
    return RoundRect(lpRect->left, lpRect->top, lpRect->right, lpRect->bottom, point.x, point.y);
}

void CDC::FillSolidRect(LPCRECT lpRect, COLORREF clr) {
    if (!lpRect)
        return;
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    if (!s)
        return;
    s->a.bkColor = clr;
    DrawScope ds(s);
    if (!ds)
        return;
    FillDeviceRectColor(ds.dc(), s->DeviceRect(lpRect->left, lpRect->top, lpRect->right, lpRect->bottom),
                        ToWxColour(clr));
}

void CDC::FillSolidRect(int x, int y, int cx, int cy, COLORREF clr) {
    RECT r{x, y, x + cx, y + cy};
    FillSolidRect(&r, clr);
}

void CDC::Draw3dRect(LPCRECT lpRect, COLORREF clrTopLeft, COLORREF clrBottomRight) {
    Draw3dRect(lpRect->left, lpRect->top, lpRect->right - lpRect->left, lpRect->bottom - lpRect->top, clrTopLeft,
               clrBottomRight);
}

void CDC::Draw3dRect(int x, int y, int cx, int cy, COLORREF clrTopLeft, COLORREF clrBottomRight) {
    FillSolidRect(x, y, cx - 1, 1, clrTopLeft);
    FillSolidRect(x, y, 1, cy - 1, clrTopLeft);
    FillSolidRect(x + cx, y, -1, cy, clrBottomRight);
    FillSolidRect(x, y + cy, cx, -1, clrBottomRight);
}

BOOL CDC::FillRgn(CRgn* pRgn, CBrush* pBrush) {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    GdiObjectImpl* rgn = pRgn ? GdiImpl(pRgn->m_hObject, GdiKind::Region) : nullptr;
    if (!s || !rgn || !pBrush)
        return FALSE;
    DrawScope ds(s);
    if (ds)
        FillRegionRects(s, ds.dc(), rgn->region, pBrush->m_hObject);
    return TRUE;
}

BOOL CDC::FrameRgn(CRgn* pRgn, CBrush* pBrush, int nWidth, int nHeight) {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    GdiObjectImpl* rgn = pRgn ? GdiImpl(pRgn->m_hObject, GdiKind::Region) : nullptr;
    if (!s || !rgn || !pBrush || !rgn->region.IsOk() || rgn->region.IsEmpty())
        return FALSE;
    wxRegion inner = rgn->region;
    const int offs[4][2] = {{nWidth, 0}, {-nWidth, 0}, {0, nHeight}, {0, -nHeight}};
    for (const auto& o : offs) {
        wxRegion moved = rgn->region;
        moved.Offset(o[0], o[1]);
        inner.Intersect(moved);
    }
    wxRegion frame = rgn->region;
    if (inner.IsOk() && !inner.IsEmpty())
        frame.Subtract(inner);
    DrawScope ds(s);
    if (ds)
        FillRegionRects(s, ds.dc(), frame, pBrush->m_hObject);
    return TRUE;
}

BOOL CDC::PaintRgn(CRgn* pRgn) {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    GdiObjectImpl* rgn = pRgn ? GdiImpl(pRgn->m_hObject, GdiKind::Region) : nullptr;
    if (!s || !rgn)
        return FALSE;
    DrawScope ds(s);
    if (ds)
        FillRegionRects(s, ds.dc(), rgn->region, s->a.brush);
    return TRUE;
}

// ---------------------------------------------------------------------------------------------
// Window DCs

CPaintDC::CPaintDC(CWnd* pWnd) : m_hWnd(pWnd ? pWnd->m_hWnd : nullptr) {
    memset(&m_ps, 0, sizeof m_ps);
    Attach(::BeginPaint(m_hWnd, &m_ps));
}

CPaintDC::~CPaintDC() {
    if (m_hDC)
        ::EndPaint(m_hWnd, &m_ps);
    Detach();
}

CClientDC::CClientDC(CWnd* pWnd) : m_hWnd(pWnd ? pWnd->m_hWnd : nullptr) { Attach(::GetDC(m_hWnd)); }

CClientDC::~CClientDC() {
    HDC h = Detach();
    if (h)
        ::ReleaseDC(m_hWnd, h);
}

CWindowDC::CWindowDC(CWnd* pWnd) : m_hWnd(pWnd ? pWnd->m_hWnd : nullptr) { Attach(::GetWindowDC(m_hWnd)); }

CWindowDC::~CWindowDC() {
    HDC h = Detach();
    if (h)
        ::ReleaseDC(m_hWnd, h);
}

CDC* CWnd::GetDC() { return CDC::FromHandle(::GetDC(m_hWnd)); }

CDC* CWnd::GetWindowDC() { return CDC::FromHandle(::GetWindowDC(m_hWnd)); }

int CWnd::ReleaseDC(CDC* pDC) { return pDC ? ::ReleaseDC(m_hWnd, pDC->m_hDC) : 0; }

CDC* CWnd::BeginPaint(LPPAINTSTRUCT lpPaint) { return CDC::FromHandle(::BeginPaint(m_hWnd, lpPaint)); }

void CWnd::EndPaint(LPPAINTSTRUCT lpPaint) { ::EndPaint(m_hWnd, lpPaint); }

HDC GetDC(HWND hWnd) {
    GdiLock lock;
    DCState* s = NewWindowState(ToWx(hWnd), false);
    s->fromGetDC = true;
    return ToHdc(s);
}

HDC GetWindowDC(HWND hWnd) {
    GdiLock lock;
    DCState* s = NewWindowState(ToWx(hWnd), true);
    s->fromGetDC = true;
    return ToHdc(s);
}

int ReleaseDC(HWND, HDC hDC) {
    GdiLock lock;
    DCState* s = DCFromHandle(hDC);
    if (!s)
        return 0;
    FreeState(s);
    return 1;
}

HDC BeginPaint(HWND hWnd, LPPAINTSTRUCT lpPaint) {
    GdiLock lock;
    wxWindow* w = ToWx(hWnd);
    if (!w)
        return nullptr;
    DCState* s = NewWindowState(w, false);
    s->paintDC = true;
    if (lpPaint) {
        memset(lpPaint, 0, sizeof *lpPaint);
        lpPaint->hdc = ToHdc(s);
        wxRect box;
        if (Sessions().count(w) || NativePainting(w))
            box = w->GetUpdateRegion().GetBox();
        if (box.IsEmpty())
            box = wxRect(wxPoint(0, 0), w->GetClientSize());
        lpPaint->rcPaint.left = box.x;
        lpPaint->rcPaint.top = box.y;
        lpPaint->rcPaint.right = box.x + box.width;
        lpPaint->rcPaint.bottom = box.y + box.height;
    }
    return ToHdc(s);
}

BOOL EndPaint(HWND, const PAINTSTRUCT* lpPaint) {
    if (!lpPaint)
        return FALSE;
    GdiLock lock;
    FreeState(DCFromHandle(lpPaint->hdc));
    return TRUE;
}

// ---------------------------------------------------------------------------------------------
// Win32 DC functions

HDC CreateCompatibleDC(HDC hdc) {
    GdiLock lock;
    return ToHdc(NewMemoryState(DCFromHandle(hdc)));
}

BOOL DeleteDC(HDC hdc) {
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    if (!s)
        return FALSE;
    FreeState(s);
    return TRUE;
}

HGDIOBJ SelectObject(HDC hdc, HGDIOBJ h) {
    GdiLock lock;
    return SelectImpl(DCFromHandle(hdc), h);
}

int SelectClipRgn(HDC hdc, HRGN hrgn) {
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    if (!s)
        return ERROR;
    GdiObjectImpl* rgn = GdiImpl(hrgn, GdiKind::Region);
    return SelectClipImpl(s, rgn ? &rgn->region : nullptr);
}

COLORREF SetTextColor(HDC hdc, COLORREF color) {
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    if (!s)
        return CLR_INVALID;
    COLORREF old = s->a.textColor;
    s->a.textColor = color;
    return old;
}

COLORREF SetBkColor(HDC hdc, COLORREF color) {
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    if (!s)
        return CLR_INVALID;
    COLORREF old = s->a.bkColor;
    s->a.bkColor = color;
    return old;
}

int SetBkMode(HDC hdc, int mode) {
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    if (!s || (mode != TRANSPARENT && mode != OPAQUE))
        return 0;
    int old = s->a.bkMode;
    s->a.bkMode = mode;
    return old;
}

int GetDeviceCaps(HDC hdc, int index) {
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    int dpi = s ? s->Dpi() : 96;
    bool printing = s && s->owner && s->owner->m_bPrinting && s->dc;
    wxSize res;
    switch (index) {
    case HORZSIZE:
    case VERTSIZE:
    case HORZRES:
    case VERTRES:
    case 110:
    case 111:
    case 117:
    case 118:
        res = printing ? s->dc->GetSize() : wxGetDisplaySize();
        break;
    default:
        break;
    }
    switch (index) {
    case 2:
        return printing ? 2 : 1;
    case HORZSIZE:
        return static_cast<int>(std::lround(res.x * 25.4 / dpi));
    case VERTSIZE:
        return static_cast<int>(std::lround(res.y * 25.4 / dpi));
    case HORZRES:
    case 118:
        return res.x;
    case VERTRES:
    case 117:
        return res.y;
    case BITSPIXEL:
        return 32;
    case PLANES:
        return 1;
    case NUMCOLORS:
        return -1;
    case 28:
    case 30:
    case 32:
    case 34:
        return 0xFF;
    case 36:
        return 1;
    case RASTERCAPS:
        return 0x2A89;
    case 40:
    case 42:
        return 36;
    case 44:
        return 51;
    case LOGPIXELSX:
    case LOGPIXELSY:
        return dpi;
    case 108:
        return 24;
    case 110:
        return printing ? res.x : 0;
    case 111:
        return printing ? res.y : 0;
    case 116:
        return 60;
    default:
        return 0;
    }
}

BOOL MoveToEx(HDC hdc, int x, int y, LPPOINT lppt) {
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    if (!s)
        return FALSE;
    if (lppt)
        *lppt = s->a.pos;
    s->a.pos = POINT{x, y};
    return TRUE;
}

BOOL LineTo(HDC hdc, int x, int y) {
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    if (!s)
        return FALSE;
    if (HasPen(s)) {
        DrawScope ds(s);
        if (ds)
            DrawSegment(s, ds.dc(), s->ToDevice(s->a.pos.x, s->a.pos.y), s->ToDevice(x, y));
    }
    s->a.pos = POINT{x, y};
    return TRUE;
}

BOOL Rectangle(HDC hdc, int left, int top, int right, int bottom) {
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    if (!s)
        return FALSE;
    DrawScope ds(s);
    if (!ds)
        return TRUE;
    wxDC* d = ds.dc();
    wxRect r = s->DeviceRect(left, top, right, bottom);
    if (!HasPen(s)) {
        FillDeviceRect(s, d, wxRect(r.x, r.y, r.width - 1, r.height - 1), s->a.brush);
        return TRUE;
    }
    if (r.width <= 0 || r.height <= 0)
        return TRUE;
    int pw = DevicePenWidth(s);
    if (PenStyle(s->a.pen) == PS_INSIDEFRAME && pw > 1)
        r.Deflate(pw / 2);
    if (r.width <= 0 || r.height <= 0)
        return TRUE;
    DrawShape(s, d, true, [&] { d->DrawRectangle(r); });
    return TRUE;
}

BOOL Ellipse(HDC hdc, int left, int top, int right, int bottom) {
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    if (!s)
        return FALSE;
    DrawScope ds(s);
    if (!ds)
        return TRUE;
    wxDC* d = ds.dc();
    wxRect r = s->DeviceRect(left, top, right, bottom);
    if (r.width <= 1 || r.height <= 1)
        return TRUE;
    int pw = HasPen(s) ? DevicePenWidth(s) : 0;
    if (pw > 1 && PenStyle(s->a.pen) == PS_INSIDEFRAME)
        r.Deflate(pw / 2);
    DrawShape(s, d, true, [&] { d->DrawEllipse(r.x, r.y, r.width - 1, r.height - 1); });
    return TRUE;
}

BOOL Polygon(HDC hdc, const POINT* apt, int cpt) {
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    if (!s || !apt || cpt < 2)
        return FALSE;
    DrawScope ds(s);
    if (!ds)
        return TRUE;
    wxDC* d = ds.dc();
    std::vector<wxPoint> pts = DevicePoints(s, apt, cpt);
    wxPolygonFillMode mode = s->a.polyFillMode == WINDING ? wxWINDING_RULE : wxODDEVEN_RULE;
    DrawShape(s, d, true, [&] { d->DrawPolygon(static_cast<int>(pts.size()), pts.data(), 0, 0, mode); });
    return TRUE;
}

BOOL Polyline(HDC hdc, const POINT* apt, int cpt) {
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    if (!s || !apt || cpt < 2)
        return FALSE;
    DrawScope ds(s);
    if (ds)
        DrawPolyline(s, ds.dc(), DevicePoints(s, apt, cpt));
    return TRUE;
}

int FillRect(HDC hDC, const RECT* lprc, HBRUSH hbr) {
    GdiLock lock;
    DCState* s = DCFromHandle(hDC);
    if (!s || !lprc)
        return 0;
    DrawScope ds(s);
    if (ds)
        FillDeviceRect(s, ds.dc(), s->DeviceRect(lprc->left, lprc->top, lprc->right, lprc->bottom), hbr);
    return 1;
}

int FrameRect(HDC hDC, const RECT* lprc, HBRUSH hbr) {
    GdiLock lock;
    DCState* s = DCFromHandle(hDC);
    if (!s || !lprc)
        return 0;
    DrawScope ds(s);
    if (!ds)
        return 1;
    wxDC* d = ds.dc();
    wxRect r = s->DeviceRect(lprc->left, lprc->top, lprc->right, lprc->bottom);
    if (r.width <= 0 || r.height <= 0)
        return 1;
    int tx = std::max(1, s->DeviceWidth(1)), ty = std::max(1, s->DeviceHeight(1));
    FillDeviceRect(s, d, wxRect(r.x, r.y, r.width, ty), hbr);
    FillDeviceRect(s, d, wxRect(r.x, r.y + r.height - ty, r.width, ty), hbr);
    FillDeviceRect(s, d, wxRect(r.x, r.y + ty, tx, r.height - 2 * ty), hbr);
    FillDeviceRect(s, d, wxRect(r.x + r.width - tx, r.y + ty, tx, r.height - 2 * ty), hbr);
    return 1;
}

BOOL DrawEdge(HDC hdc, LPRECT qrc, UINT edge, UINT grfFlags) {
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    if (!s || !qrc)
        return FALSE;
    UINT sides = grfFlags & BF_RECT;
    UINT outer = edge & (BDR_RAISEDOUTER | BDR_SUNKENOUTER);
    UINT inner = edge & (BDR_RAISEDINNER | BDR_SUNKENINNER);
    bool soft = (grfFlags & BF_SOFT) != 0;
    auto colors = [&](UINT bdr, COLORREF& lt, COLORREF& rb) {
        if (grfFlags & BF_MONO) {
            lt = rb = SysColor(COLOR_WINDOWFRAME);
        } else if (grfFlags & BF_FLAT) {
            lt = rb = SysColor((bdr & (BDR_RAISEDOUTER | BDR_SUNKENOUTER)) ? COLOR_BTNSHADOW : COLOR_BTNFACE);
        } else if (bdr & BDR_RAISEDOUTER) {
            lt = SysColor(soft ? COLOR_3DHILIGHT : COLOR_3DLIGHT);
            rb = SysColor(COLOR_3DDKSHADOW);
        } else if (bdr & BDR_SUNKENOUTER) {
            lt = SysColor(soft ? COLOR_3DDKSHADOW : COLOR_3DSHADOW);
            rb = SysColor(COLOR_3DHILIGHT);
        } else if (bdr & BDR_RAISEDINNER) {
            lt = SysColor(soft ? COLOR_3DLIGHT : COLOR_3DHILIGHT);
            rb = SysColor(COLOR_3DSHADOW);
        } else {
            lt = SysColor(soft ? COLOR_3DSHADOW : COLOR_3DDKSHADOW);
            rb = SysColor(COLOR_3DLIGHT);
        }
    };
    wxRect r = s->DeviceRect(qrc->left, qrc->top, qrc->right, qrc->bottom);
    {
        DrawScope ds(s);
        if (ds) {
            wxDC* d = ds.dc();
            wxRect cur = r;
            for (UINT bdr : {outer, inner}) {
                if (!bdr)
                    continue;
                COLORREF lt, rb;
                colors(bdr, lt, rb);
                EdgeRect(d, cur, sides, lt, rb);
                cur = ShrinkEdges(cur, sides);
            }
            if (grfFlags & BF_MIDDLE)
                FillDeviceRectColor(d, cur,
                                    ToWxColour(SysColor((grfFlags & BF_MONO) ? COLOR_WINDOW : COLOR_BTNFACE)));
        }
    }
    if (grfFlags & BF_ADJUST) {
        int n = (outer ? 1 : 0) + (inner ? 1 : 0);
        int lx = s->LogicalWidth(n), ly = s->LogicalHeight(n);
        if (sides & BF_LEFT)
            qrc->left += lx;
        if (sides & BF_TOP)
            qrc->top += ly;
        if (sides & BF_RIGHT)
            qrc->right -= lx;
        if (sides & BF_BOTTOM)
            qrc->bottom -= ly;
    }
    return TRUE;
}

BOOL DrawFocusRect(HDC hDC, const RECT* lprc) {
    GdiLock lock;
    DCState* s = DCFromHandle(hDC);
    if (!s || !lprc)
        return FALSE;
    DrawScope ds(s);
    if (!ds)
        return TRUE;
    wxDC* d = ds.dc();
    wxRect r = s->DeviceRect(lprc->left, lprc->top, lprc->right, lprc->bottom);
    if (r.width <= 0 || r.height <= 0)
        return TRUE;
    if (s->SupportsLogicalOps(d)) {
        d->SetLogicalFunction(wxINVERT);
        static const wxDash dots[] = {1, 1};
        wxPen p(*wxBLACK, 1, wxPENSTYLE_USER_DASH);
        p.SetDashes(2, dots);
        d->SetPen(p);
        d->SetBrush(*wxTRANSPARENT_BRUSH);
        d->DrawRectangle(r);
        d->SetLogicalFunction(wxCOPY);
        return TRUE;
    }
    wxColour c(96, 96, 96);
    SetNoPen(d);
    SetSolidBrush(d, c);
    for (int x = r.x; x < r.GetRight() + 1; x += 2) {
        d->DrawRectangle(x, r.y, 1, 1);
        d->DrawRectangle(x, r.GetBottom(), 1, 1);
    }
    for (int y = r.y; y < r.GetBottom() + 1; y += 2) {
        d->DrawRectangle(r.x, y, 1, 1);
        d->DrawRectangle(r.GetRight(), y, 1, 1);
    }
    return TRUE;
}

int SaveDC(HDC hdc) {
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    if (!s)
        return 0;
    s->saved.push_back(s->a);
    AddRefs(s->a);
    return static_cast<int>(s->saved.size());
}

BOOL RestoreDC(HDC hdc, int nSavedDC) {
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    if (!s || s->saved.empty())
        return FALSE;
    int depth = static_cast<int>(s->saved.size());
    int index = nSavedDC < 0 ? depth + nSavedDC : nSavedDC - 1;
    if (index < 0 || index >= depth)
        return FALSE;
    DCAttrs target = s->saved[static_cast<size_t>(index)];
    RestoreAttrs(s, target);
    for (int i = depth - 1; i >= index; --i) {
        ReleaseRefs(s->saved[static_cast<size_t>(i)]);
        s->saved.pop_back();
    }
    return TRUE;
}

BOOL DrawIcon(HDC hDC, int X, int Y, HICON hIcon) {
    GdiLock lock;
    DCState* s = DCFromHandle(hDC);
    wxIcon* icon = IconFromHandle(hIcon);
    if (!s || !icon || !icon->IsOk())
        return FALSE;
    DrawScope ds(s);
    if (ds)
        ds.dc()->DrawIcon(*icon, s->ToDevice(X, Y));
    return TRUE;
}
