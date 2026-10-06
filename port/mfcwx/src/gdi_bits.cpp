#include "gdi_internal.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace mfcwx {

namespace {

unsigned char Rop3Channel(unsigned rop3, unsigned char p, unsigned char s, unsigned char d) {
    unsigned out = 0;
    for (unsigned k = 0; k < 8; ++k) {
        if (!((rop3 >> k) & 1))
            continue;
        unsigned pm = (k & 4) ? p : static_cast<unsigned char>(~p);
        unsigned sm = (k & 2) ? s : static_cast<unsigned char>(~s);
        unsigned dm = (k & 1) ? d : static_cast<unsigned char>(~d);
        out |= pm & sm & dm & 0xFF;
    }
    return static_cast<unsigned char>(out);
}

bool RopUsesSource(DWORD rop) {
    unsigned r = (rop >> 16) & 0xFF;
    return ((r >> 2) & 0x33) != (r & 0x33);
}

wxImage Opaque(wxImage img) {
    if (img.HasAlpha())
        img.ClearAlpha();
    return img;
}

wxRect PhysicalRect(const wxRect& r, double scale) {
    return wxRect(static_cast<int>(std::lround(r.x * scale)), static_cast<int>(std::lround(r.y * scale)),
                  static_cast<int>(std::lround(r.width * scale)), static_cast<int>(std::lround(r.height * scale)));
}

double TargetScale(DCState* s, wxDC* d) {
    if (s->kind == DCKind::Memory)
        return DCBitmapScale(s);
    double scale = d ? d->GetContentScaleFactor() : 1.0;
    return scale > 0 ? scale : 1.0;
}

bool SourceImage(DCState* src, const wxRect& S, wxImage& out) {
    if (!src || S.width <= 0 || S.height <= 0)
        return false;
    if (const wxImage* px = DCPixels(src)) {
        double scale = DCBitmapScale(src);
        wxRect P = PhysicalRect(S, scale);
        wxRect clipped = P;
        clipped.Intersect(wxRect(0, 0, px->GetWidth(), px->GetHeight()));
        out = wxImage(std::max(1, P.width), std::max(1, P.height), true);
        if (!clipped.IsEmpty())
            out.Paste(px->GetSubImage(clipped), clipped.x - P.x, clipped.y - P.y);
        return true;
    }
    wxDC* d = src->Target(false);
    if (!d)
        return false;
    wxBitmap b = d->GetAsBitmap(&S);
    if (!b.IsOk())
        return false;
    out = b.ConvertToImage();
    return out.IsOk();
}

void FitImage(wxImage& img, int w, int h, bool mirrorX, bool mirrorY, bool smooth) {
    if (mirrorX)
        img = img.Mirror(true);
    if (mirrorY)
        img = img.Mirror(false);
    if (img.GetWidth() != w || img.GetHeight() != h)
        img.Rescale(std::max(1, w), std::max(1, h), smooth ? wxIMAGE_QUALITY_BILINEAR : wxIMAGE_QUALITY_NEAREST);
}

void Invert(wxImage& img) {
    unsigned char* p = img.GetData();
    size_t n = static_cast<size_t>(img.GetWidth()) * img.GetHeight() * 3;
    for (size_t i = 0; i < n; ++i)
        p[i] = static_cast<unsigned char>(255 - p[i]);
}

void DrawImageAt(wxDC* d, wxImage img, const wxRect& D, double scale) {
    d->DrawBitmap(wxBitmap(Opaque(img), -1, scale), D.x, D.y, false);
}

void DrawMasked(wxDC* d, wxImage img, const wxRect& D, double scale, unsigned char key) {
    img = Opaque(img);
    img.SetMaskColour(key, key, key);
    d->DrawBitmap(wxBitmap(img, -1, scale), D.x, D.y, true);
}

bool BlitImage(DCState* dst, wxDC* d, const wxRect& D, wxImage img, DWORD rop) {
    double scale = TargetScale(dst, d);
    switch (rop) {
    case SRCCOPY:
        DrawImageAt(d, img, D, scale);
        return true;
    case NOTSRCCOPY:
        Invert(img);
        DrawImageAt(d, img, D, scale);
        return true;
    default:
        break;
    }
    if (dst->kind == DCKind::Memory)
        return SoftwareRop(dst, D, &img, rop, BrushColor(dst->a.brush));
    switch (rop) {
    case SRCAND:
        DrawMasked(d, img, D, scale, 255);
        return true;
    case SRCPAINT:
    case SRCINVERT:
        DrawMasked(d, img, D, scale, 0);
        return true;
    case MERGEPAINT:
        Invert(img);
        DrawMasked(d, img, D, scale, 0);
        return true;
    case NOTSRCERASE:
        Invert(img);
        DrawImageAt(d, img, D, scale);
        return true;
    default:
        DrawImageAt(d, img, D, scale);
        return true;
    }
}

bool PatternOnly(DCState* dst, wxDC* d, const wxRect& D, DWORD rop) {
    switch (rop) {
    case BLACKNESS:
        FillDeviceRectColor(d, D, *wxBLACK);
        return true;
    case WHITENESS:
        FillDeviceRectColor(d, D, *wxWHITE);
        return true;
    case PATCOPY:
        FillDeviceRect(dst, d, D, dst->a.brush);
        return true;
    default:
        break;
    }
    if (rop == 0x00AA0029UL)
        return true;
    if (dst->kind == DCKind::Memory)
        return SoftwareRop(dst, D, nullptr, rop, BrushColor(dst->a.brush));
    if (dst->SupportsLogicalOps(d)) {
        wxRasterOperationMode mode = rop == DSTINVERT ? wxINVERT : wxXOR;
        d->SetLogicalFunction(mode);
        d->SetPen(*wxTRANSPARENT_PEN);
        d->SetBrush(rop == DSTINVERT ? *wxBLACK_BRUSH : DeviceBrush(dst->a.brush));
        d->DrawRectangle(D);
        d->SetLogicalFunction(wxCOPY);
        return true;
    }
    return false;
}

int MaskShift(DWORD mask) {
    int s = 0;
    if (!mask)
        return 0;
    while (!(mask & 1)) {
        mask >>= 1;
        ++s;
    }
    return s;
}

unsigned char MaskValue(DWORD v, DWORD mask) {
    if (!mask)
        return 0;
    int shift = MaskShift(mask);
    DWORD m = mask >> shift;
    DWORD x = (v & mask) >> shift;
    return static_cast<unsigned char>(m ? x * 255 / m : 0);
}

int FloodImpl(DCState* s, int x, int y, COLORREF crColor, UINT type) {
    const wxImage* px = DCPixels(s);
    if (!px)
        return FALSE;
    double scale = DCBitmapScale(s);
    wxPoint dp = s->ToDevice(x, y);
    int sx = static_cast<int>(std::lround(dp.x * scale)), sy = static_cast<int>(std::lround(dp.y * scale));
    int w = px->GetWidth(), h = px->GetHeight();
    if (sx < 0 || sy < 0 || sx >= w || sy >= h)
        return FALSE;
    const unsigned char* data = px->GetData();
    auto colorAt = [&](int ix, int iy) {
        const unsigned char* p = data + (static_cast<size_t>(iy) * w + ix) * 3;
        return RGB(p[0], p[1], p[2]);
    };
    COLORREF target = crColor & 0xFFFFFF;
    auto inside = [&](int ix, int iy) {
        COLORREF c = colorAt(ix, iy);
        return type == 1 ? c == target : c != target;
    };
    if (!inside(sx, sy))
        return FALSE;
    std::vector<unsigned char> mask(static_cast<size_t>(w) * h, 0);
    std::vector<wxPoint> stack{wxPoint(sx, sy)};
    int minX = sx, maxX = sx, minY = sy, maxY = sy;
    while (!stack.empty()) {
        wxPoint p = stack.back();
        stack.pop_back();
        if (p.x < 0 || p.y < 0 || p.x >= w || p.y >= h)
            continue;
        size_t i = static_cast<size_t>(p.y) * w + p.x;
        if (mask[i] || !inside(p.x, p.y))
            continue;
        mask[i] = 1;
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
        stack.emplace_back(p.x + 1, p.y);
        stack.emplace_back(p.x - 1, p.y);
        stack.emplace_back(p.x, p.y + 1);
        stack.emplace_back(p.x, p.y - 1);
    }
    wxRect box(minX, minY, maxX - minX + 1, maxY - minY + 1);
    wxImage sub = Opaque(px->GetSubImage(box));
    wxColour fill = DeviceBrush(s->a.brush).GetColour();
    unsigned char* out = sub.GetData();
    for (int iy = 0; iy < box.height; ++iy)
        for (int ix = 0; ix < box.width; ++ix)
            if (mask[static_cast<size_t>(iy + box.y) * w + ix + box.x]) {
                unsigned char* q = out + (static_cast<size_t>(iy) * box.width + ix) * 3;
                q[0] = fill.Red();
                q[1] = fill.Green();
                q[2] = fill.Blue();
            }
    DrawScope ds(s);
    if (!ds)
        return FALSE;
    ds.dc()->DestroyClippingRegion();
    s->ApplyClip(ds.dc());
    ds.dc()->DrawBitmap(wxBitmap(sub, -1, scale), static_cast<int>(std::lround(box.x / scale)),
                        static_cast<int>(std::lround(box.y / scale)), false);
    return TRUE;
}

GdiObjectImpl* BitmapImpl(HGDIOBJ h) { return GdiImpl(h, GdiKind::Bitmap); }

} // namespace

bool SoftwareRop(DCState* dst, const wxRect& r, const wxImage* src, DWORD rop, COLORREF pattern) {
    const wxImage* px = DCPixels(dst);
    if (!px)
        return false;
    double scale = DCBitmapScale(dst);
    wxRect P = PhysicalRect(r, scale);
    wxRect clipped = P;
    clipped.Intersect(wxRect(0, 0, px->GetWidth(), px->GetHeight()));
    if (clipped.IsEmpty())
        return true;
    wxImage out = Opaque(px->GetSubImage(clipped));
    unsigned rop3 = (rop >> 16) & 0xFF;
    unsigned char pr = GetRValue(pattern), pg = GetGValue(pattern), pb = GetBValue(pattern);
    unsigned char* o = out.GetData();
    const unsigned char* sp = src ? src->GetData() : nullptr;
    int sw = src ? src->GetWidth() : 0, sh = src ? src->GetHeight() : 0;
    for (int y = 0; y < clipped.height; ++y) {
        for (int x = 0; x < clipped.width; ++x) {
            unsigned char* q = o + (static_cast<size_t>(y) * clipped.width + x) * 3;
            unsigned char s0 = 0, s1 = 0, s2 = 0;
            int ix = x + clipped.x - P.x, iy = y + clipped.y - P.y;
            if (sp && ix < sw && iy < sh) {
                const unsigned char* t = sp + (static_cast<size_t>(iy) * sw + ix) * 3;
                s0 = t[0];
                s1 = t[1];
                s2 = t[2];
            }
            q[0] = Rop3Channel(rop3, pr, s0, q[0]);
            q[1] = Rop3Channel(rop3, pg, s1, q[1]);
            q[2] = Rop3Channel(rop3, pb, s2, q[2]);
        }
    }
    DrawScope ds(dst);
    if (!ds)
        return false;
    ds.dc()->DrawBitmap(wxBitmap(out, -1, scale), static_cast<int>(std::lround(clipped.x / scale)),
                        static_cast<int>(std::lround(clipped.y / scale)), false);
    return true;
}

bool BlitImpl(DCState* dst, int x, int y, int w, int h, DCState* src, int xs, int ys, int ws, int hs, DWORD rop) {
    if (!dst)
        return false;
    wxPoint p1 = dst->ToDevice(x, y), p2 = dst->ToDevice(x + w, y + h);
    wxRect D(std::min(p1.x, p2.x), std::min(p1.y, p2.y), std::abs(p2.x - p1.x), std::abs(p2.y - p1.y));
    if (D.width <= 0 || D.height <= 0)
        return true;
    DrawScope ds(dst);
    if (!ds)
        return true;
    wxDC* d = ds.dc();
    if (!RopUsesSource(rop))
        return PatternOnly(dst, d, D, rop);
    if (!src)
        return false;
    wxPoint q1 = src->ToDevice(xs, ys), q2 = src->ToDevice(xs + ws, ys + hs);
    wxRect S(std::min(q1.x, q2.x), std::min(q1.y, q2.y), std::abs(q2.x - q1.x), std::abs(q2.y - q1.y));
    if (S.width <= 0 || S.height <= 0)
        return true;
    bool mirrorX = (p2.x < p1.x) != (q2.x < q1.x);
    bool mirrorY = (p2.y < p1.y) != (q2.y < q1.y);
    if (rop == SRCCOPY && !mirrorX && !mirrorY && src->kind != DCKind::Memory) {
        wxDC* sd = src->Target(false);
        if (sd && d->StretchBlit(D.x, D.y, D.width, D.height, sd, S.x, S.y, S.width, S.height, wxCOPY))
            return true;
    }
    wxImage img;
    if (!SourceImage(src, S, img))
        return false;
    double scale = TargetScale(dst, d);
    wxRect PD = PhysicalRect(D, scale);
    bool smooth = dst->a.stretchMode == HALFTONE && (S.width != D.width || S.height != D.height);
    FitImage(img, PD.width, PD.height, mirrorX, mirrorY, smooth);
    return BlitImage(dst, d, D, img, rop);
}

bool DrawImageImpl(DCState* dst, int x, int y, int w, int h, const wxImage& src, int xs, int ys, int ws, int hs,
                   DWORD rop) {
    if (!dst || !src.IsOk() || ws == 0 || hs == 0)
        return false;
    wxPoint p1 = dst->ToDevice(x, y), p2 = dst->ToDevice(x + w, y + h);
    wxRect D(std::min(p1.x, p2.x), std::min(p1.y, p2.y), std::abs(p2.x - p1.x), std::abs(p2.y - p1.y));
    if (D.width <= 0 || D.height <= 0)
        return true;
    DrawScope ds(dst);
    if (!ds)
        return true;
    wxDC* d = ds.dc();
    if (!RopUsesSource(rop))
        return PatternOnly(dst, d, D, rop);
    wxRect S(std::min(xs, xs + ws), std::min(ys, ys + hs), std::abs(ws), std::abs(hs));
    wxRect clipped = S;
    clipped.Intersect(wxRect(0, 0, src.GetWidth(), src.GetHeight()));
    wxImage img(S.width, S.height, true);
    if (!clipped.IsEmpty())
        img.Paste(src.GetSubImage(clipped), clipped.x - S.x, clipped.y - S.y);
    bool mirrorX = (p2.x < p1.x) != (ws < 0);
    bool mirrorY = (p2.y < p1.y) != (hs < 0);
    double scale = TargetScale(dst, d);
    wxRect PD = PhysicalRect(D, scale);
    bool smooth = dst->a.stretchMode == HALFTONE;
    FitImage(img, PD.width, PD.height, mirrorX, mirrorY, smooth);
    return BlitImage(dst, d, D, img, rop);
}

bool DibToImage(const BITMAPINFO* bmi, const void* bits, UINT usage, wxImage& out, int startScan, int lines) {
    if (!bmi || !bits)
        return false;
    const BITMAPINFOHEADER& hdr = bmi->bmiHeader;
    int w = hdr.biWidth, h = std::abs(hdr.biHeight), bpp = hdr.biBitCount;
    bool bottomUp = hdr.biHeight > 0;
    if (w <= 0 || h <= 0 || (hdr.biCompression != BI_RGB && hdr.biCompression != 3))
        return false;
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32)
        return false;
    const unsigned char* base = reinterpret_cast<const unsigned char*>(bmi) + hdr.biSize;
    DWORD masks[3] = {0, 0, 0};
    if (hdr.biCompression == 3) {
        memcpy(masks, base, sizeof masks);
    } else if (bpp == 16) {
        masks[0] = 0x7C00;
        masks[1] = 0x03E0;
        masks[2] = 0x001F;
    } else if (bpp == 32) {
        masks[0] = 0x00FF0000;
        masks[1] = 0x0000FF00;
        masks[2] = 0x000000FF;
    }
    const RGBQUAD* palette = reinterpret_cast<const RGBQUAD*>(base);
    int paletteSize = bpp <= 8 ? static_cast<int>(hdr.biClrUsed ? hdr.biClrUsed : (1u << bpp)) : 0;
    int stride = ((w * bpp + 31) / 32) * 4;
    if (lines < 0)
        lines = h;
    out.Create(w, h, true);
    unsigned char* data = out.GetData();
    const unsigned char* src = static_cast<const unsigned char*>(bits);
    for (int i = 0; i < lines; ++i) {
        int dibRow = startScan + i;
        if (dibRow < 0 || dibRow >= h)
            continue;
        int iy = bottomUp ? h - 1 - dibRow : dibRow;
        const unsigned char* row = src + static_cast<size_t>(i) * stride;
        unsigned char* dstRow = data + static_cast<size_t>(iy) * w * 3;
        for (int x = 0; x < w; ++x) {
            unsigned char r = 0, g = 0, b = 0;
            if (bpp <= 8) {
                int index = 0;
                if (bpp == 1)
                    index = (row[x >> 3] >> (7 - (x & 7))) & 1;
                else if (bpp == 4)
                    index = (row[x >> 1] >> ((x & 1) ? 0 : 4)) & 0xF;
                else
                    index = row[x];
                if (usage == DIB_RGB_COLORS && index < paletteSize) {
                    r = palette[index].rgbRed;
                    g = palette[index].rgbGreen;
                    b = palette[index].rgbBlue;
                } else {
                    r = g = b = static_cast<unsigned char>(bpp == 1 ? index * 255 : bpp == 4 ? index * 17 : index);
                }
            } else if (bpp == 24) {
                b = row[x * 3];
                g = row[x * 3 + 1];
                r = row[x * 3 + 2];
            } else {
                DWORD v = bpp == 16 ? static_cast<DWORD>(row[x * 2] | (row[x * 2 + 1] << 8))
                                    : static_cast<DWORD>(row[x * 4] | (row[x * 4 + 1] << 8) |
                                                         (row[x * 4 + 2] << 16) |
                                                         (static_cast<DWORD>(row[x * 4 + 3]) << 24));
                r = MaskValue(v, masks[0]);
                g = MaskValue(v, masks[1]);
                b = MaskValue(v, masks[2]);
            }
            dstRow[x * 3] = r;
            dstRow[x * 3 + 1] = g;
            dstRow[x * 3 + 2] = b;
        }
    }
    return true;
}

bool DdbBitsToImage(int width, int height, int bpp, int stride, const void* bits, wxImage& out) {
    if (width <= 0 || height <= 0)
        return false;
    out.Create(width, height, true);
    if (!bits)
        return true;
    const unsigned char* src = static_cast<const unsigned char*>(bits);
    unsigned char* data = out.GetData();
    for (int y = 0; y < height; ++y) {
        const unsigned char* row = src + static_cast<size_t>(y) * stride;
        unsigned char* d = data + static_cast<size_t>(y) * width * 3;
        for (int x = 0; x < width; ++x) {
            unsigned char r = 0, g = 0, b = 0;
            switch (bpp) {
            case 1:
                r = g = b = ((row[x >> 3] >> (7 - (x & 7))) & 1) ? 255 : 0;
                break;
            case 4:
                r = g = b = static_cast<unsigned char>(((row[x >> 1] >> ((x & 1) ? 0 : 4)) & 0xF) * 17);
                break;
            case 8:
                r = g = b = row[x];
                break;
            case 16: {
                unsigned v = row[x * 2] | (row[x * 2 + 1] << 8);
                r = static_cast<unsigned char>(((v >> 10) & 31) * 255 / 31);
                g = static_cast<unsigned char>(((v >> 5) & 31) * 255 / 31);
                b = static_cast<unsigned char>((v & 31) * 255 / 31);
                break;
            }
            case 24:
                b = row[x * 3];
                g = row[x * 3 + 1];
                r = row[x * 3 + 2];
                break;
            default:
                b = row[x * 4];
                g = row[x * 4 + 1];
                r = row[x * 4 + 2];
                break;
            }
            d[x * 3] = r;
            d[x * 3 + 1] = g;
            d[x * 3 + 2] = b;
        }
    }
    return true;
}

bool ImageToDdbBits(const wxImage& img, int bpp, std::vector<unsigned char>& out, int& stride) {
    int w = img.GetWidth(), h = img.GetHeight();
    if (w <= 0 || h <= 0)
        return false;
    stride = ((w * bpp + 15) / 16) * 2;
    out.assign(static_cast<size_t>(stride) * h, 0);
    const unsigned char* data = img.GetData();
    for (int y = 0; y < h; ++y) {
        unsigned char* row = out.data() + static_cast<size_t>(y) * stride;
        const unsigned char* s = data + static_cast<size_t>(y) * w * 3;
        for (int x = 0; x < w; ++x) {
            unsigned char r = s[x * 3], g = s[x * 3 + 1], b = s[x * 3 + 2];
            unsigned gray = (r * 30u + g * 59u + b * 11u) / 100u;
            switch (bpp) {
            case 1:
                if (gray >= 128)
                    row[x >> 3] |= static_cast<unsigned char>(0x80 >> (x & 7));
                break;
            case 4:
                row[x >> 1] |= static_cast<unsigned char>((gray / 17) << ((x & 1) ? 0 : 4));
                break;
            case 8:
                row[x] = static_cast<unsigned char>(gray);
                break;
            case 16: {
                unsigned v = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
                row[x * 2] = static_cast<unsigned char>(v & 0xFF);
                row[x * 2 + 1] = static_cast<unsigned char>(v >> 8);
                break;
            }
            case 24:
                row[x * 3] = b;
                row[x * 3 + 1] = g;
                row[x * 3 + 2] = r;
                break;
            default:
                row[x * 4] = b;
                row[x * 4 + 1] = g;
                row[x * 4 + 2] = r;
                row[x * 4 + 3] = 0;
                break;
            }
        }
    }
    return true;
}

} // namespace mfcwx

using namespace mfcwx;

// ---------------------------------------------------------------------------------------------
// CBitmap

BOOL CBitmap::LoadBitmap(const char* lpszResourceName) {
    wxBitmap bmp = LoadBitmapResource(ResRef::From(lpszResourceName));
    if (!bmp.IsOk())
        return FALSE;
    GdiLock lock;
    return Attach(NewBitmapHandle(bmp, 32));
}

BOOL CBitmap::LoadBitmap(UINT nIDResource) {
    wxBitmap bmp = LoadBitmapResource(ResRef::FromId(nIDResource));
    if (!bmp.IsOk())
        return FALSE;
    GdiLock lock;
    return Attach(NewBitmapHandle(bmp, 32));
}

BOOL CBitmap::LoadOEMBitmap(UINT) { return FALSE; }

BOOL CBitmap::LoadMappedBitmap(UINT nIDBitmap, UINT, void*, int) { return LoadBitmap(nIDBitmap); }

BOOL CBitmap::CreateBitmap(int nWidth, int nHeight, UINT nPlanes, UINT nBitcount, const void* lpBits) {
    return Attach(::CreateBitmap(nWidth, nHeight, nPlanes, nBitcount, lpBits));
}

BOOL CBitmap::CreateBitmapIndirect(LPBITMAP lpBitmap) {
    if (!lpBitmap)
        return FALSE;
    int bpp = std::max(1, lpBitmap->bmBitsPixel * std::max<int>(1, lpBitmap->bmPlanes));
    wxImage img;
    int stride = lpBitmap->bmWidthBytes ? lpBitmap->bmWidthBytes : ((lpBitmap->bmWidth * bpp + 15) / 16) * 2;
    if (!DdbBitsToImage(lpBitmap->bmWidth, lpBitmap->bmHeight, bpp, stride, lpBitmap->bmBits, img))
        return FALSE;
    GdiLock lock;
    return Attach(NewBitmapHandle(wxBitmap(img), bpp == 1 ? 1 : bpp));
}

BOOL CBitmap::CreateCompatibleBitmap(CDC* pDC, int nWidth, int nHeight) {
    return Attach(::CreateCompatibleBitmap(pDC ? pDC->m_hDC : nullptr, nWidth, nHeight));
}

BOOL CBitmap::CreateDiscardableBitmap(CDC* pDC, int nWidth, int nHeight) {
    return CreateCompatibleBitmap(pDC, nWidth, nHeight);
}

int CBitmap::GetBitmap(BITMAP* pBitMap) { return GetObject(sizeof(BITMAP), pBitMap); }

DWORD CBitmap::SetBitmapBits(DWORD dwCount, const void* lpBits) {
    GdiLock lock;
    GdiObjectImpl* impl = BitmapImpl(m_hObject);
    if (!impl || !impl->bitmap.IsOk() || !lpBits)
        return 0;
    SyncBitmap(impl);
    double scale = impl->bitmap.GetScaleFactor();
    int w = static_cast<int>(std::lround(impl->bitmap.GetLogicalWidth()));
    int h = static_cast<int>(std::lround(impl->bitmap.GetLogicalHeight()));
    int stride = ((w * impl->bitsPixel + 15) / 16) * 2;
    std::vector<unsigned char> buf(static_cast<size_t>(stride) * h, 0);
    DWORD n = std::min<DWORD>(dwCount, static_cast<DWORD>(buf.size()));
    if (n < buf.size()) {
        std::vector<unsigned char> current;
        int st = 0;
        wxImage img = impl->bitmap.ConvertToImage();
        if (scale != 1.0)
            img.Rescale(w, h, wxIMAGE_QUALITY_NEAREST);
        if (ImageToDdbBits(img, impl->bitsPixel, current, st) && current.size() == buf.size())
            buf = current;
    }
    memcpy(buf.data(), lpBits, n);
    wxImage img;
    if (!DdbBitsToImage(w, h, impl->bitsPixel, stride, buf.data(), img))
        return 0;
    if (scale != 1.0)
        img.Rescale(static_cast<int>(std::lround(w * scale)), static_cast<int>(std::lround(h * scale)),
                    wxIMAGE_QUALITY_NEAREST);
    impl->bitmap = wxBitmap(img, -1, scale);
    if (impl->selectedIn)
        impl->selectedIn->pixels.reset();
    return n;
}

DWORD CBitmap::GetBitmapBits(DWORD dwCount, LPVOID lpBits) const {
    GdiLock lock;
    GdiObjectImpl* impl = BitmapImpl(m_hObject);
    if (!impl || !impl->bitmap.IsOk() || !lpBits)
        return 0;
    SyncBitmap(impl);
    wxImage img = impl->bitmap.ConvertToImage();
    int w = static_cast<int>(std::lround(impl->bitmap.GetLogicalWidth()));
    int h = static_cast<int>(std::lround(impl->bitmap.GetLogicalHeight()));
    if (img.GetWidth() != w || img.GetHeight() != h)
        img.Rescale(w, h, wxIMAGE_QUALITY_NEAREST);
    std::vector<unsigned char> buf;
    int stride = 0;
    if (!ImageToDdbBits(img, impl->bitsPixel, buf, stride))
        return 0;
    DWORD n = std::min<DWORD>(dwCount, static_cast<DWORD>(buf.size()));
    memcpy(lpBits, buf.data(), n);
    return n;
}

CSize CBitmap::SetBitmapDimension(int nWidth, int nHeight) {
    GdiLock lock;
    GdiObjectImpl* impl = BitmapImpl(m_hObject);
    if (!impl)
        return CSize();
    CSize old(impl->dimension);
    impl->dimension = SIZE{nWidth, nHeight};
    return old;
}

CSize CBitmap::GetBitmapDimension() const {
    GdiLock lock;
    GdiObjectImpl* impl = BitmapImpl(m_hObject);
    return impl ? CSize(impl->dimension) : CSize();
}

// ---------------------------------------------------------------------------------------------
// CDC raster operations

BOOL CDC::PatBlt(int x, int y, int nWidth, int nHeight, DWORD dwRop) {
    GdiLock lock;
    return BlitImpl(DCFromHandle(m_hDC), x, y, nWidth, nHeight, nullptr, 0, 0, 0, 0, dwRop);
}

BOOL CDC::BitBlt(int x, int y, int nWidth, int nHeight, CDC* pSrcDC, int xSrc, int ySrc, DWORD dwRop) {
    return ::BitBlt(m_hDC, x, y, nWidth, nHeight, pSrcDC ? pSrcDC->m_hDC : nullptr, xSrc, ySrc, dwRop);
}

BOOL CDC::StretchBlt(int x, int y, int nWidth, int nHeight, CDC* pSrcDC, int xSrc, int ySrc, int nSrcWidth,
                     int nSrcHeight, DWORD dwRop) {
    return ::StretchBlt(m_hDC, x, y, nWidth, nHeight, pSrcDC ? pSrcDC->m_hDC : nullptr, xSrc, ySrc, nSrcWidth,
                        nSrcHeight, dwRop);
}

BOOL CDC::TransparentBlt(int xDest, int yDest, int nDestWidth, int nDestHeight, CDC* pSrcDC, int xSrc, int ySrc,
                         int nSrcWidth, int nSrcHeight, UINT clrTransparent) {
    GdiLock lock;
    DCState* dst = DCFromHandle(m_hDC);
    DCState* src = pSrcDC ? DCFromHandle(pSrcDC->m_hDC) : nullptr;
    if (!dst || !src)
        return FALSE;
    wxRect D = dst->DeviceRect(xDest, yDest, xDest + nDestWidth, yDest + nDestHeight);
    wxRect S = src->DeviceRect(xSrc, ySrc, xSrc + nSrcWidth, ySrc + nSrcHeight);
    if (D.width <= 0 || D.height <= 0)
        return TRUE;
    wxImage img;
    if (!SourceImage(src, S, img))
        return FALSE;
    DrawScope ds(dst);
    if (!ds)
        return TRUE;
    double scale = TargetScale(dst, ds.dc());
    wxRect PD = PhysicalRect(D, scale);
    FitImage(img, PD.width, PD.height, false, false, false);
    img = Opaque(img);
    img.SetMaskColour(GetRValue(clrTransparent), GetGValue(clrTransparent), GetBValue(clrTransparent));
    ds.dc()->DrawBitmap(wxBitmap(img, -1, scale), D.x, D.y, true);
    return TRUE;
}

COLORREF CDC::GetPixel(int x, int y) const { return ::GetPixel(m_hDC, x, y); }

COLORREF CDC::SetPixel(int x, int y, COLORREF crColor) { return ::SetPixel(m_hDC, x, y, crColor); }

BOOL CDC::FloodFill(int x, int y, COLORREF crColor) { return ExtFloodFill(x, y, crColor, 0); }

BOOL CDC::ExtFloodFill(int x, int y, COLORREF crColor, UINT nFillType) {
    GdiLock lock;
    DCState* s = DCFromHandle(m_hDC);
    return s ? FloodImpl(s, x, y, crColor, nFillType) : FALSE;
}

// ---------------------------------------------------------------------------------------------
// Win32 bitmap functions

HBITMAP CreateCompatibleBitmap(HDC hdc, int cx, int cy) {
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    double scale = s ? DCBitmapScale(s) : 1.0;
    if (s && s->kind == DCKind::Window && s->window)
        scale = s->window->GetContentScaleFactor();
    return NewBitmapHandle(NewBitmap(std::max(1, cx), std::max(1, cy), scale, *wxBLACK), 32);
}

HBITMAP CreateBitmap(int nWidth, int nHeight, UINT nPlanes, UINT nBitCount, const void* lpBits) {
    int bpp = static_cast<int>(std::max<UINT>(1, nBitCount) * std::max<UINT>(1, nPlanes));
    wxImage img;
    if (!DdbBitsToImage(std::max(1, nWidth), std::max(1, nHeight), bpp, ((std::max(1, nWidth) * bpp + 15) / 16) * 2,
                        lpBits, img))
        return nullptr;
    GdiLock lock;
    return NewBitmapHandle(wxBitmap(img), bpp);
}

BOOL BitBlt(HDC hdc, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1, DWORD rop) {
    GdiLock lock;
    return BlitImpl(DCFromHandle(hdc), x, y, cx, cy, DCFromHandle(hdcSrc), x1, y1, cx, cy, rop);
}

BOOL StretchBlt(HDC hdcDest, int xDest, int yDest, int wDest, int hDest, HDC hdcSrc, int xSrc, int ySrc, int wSrc,
                int hSrc, DWORD rop) {
    GdiLock lock;
    return BlitImpl(DCFromHandle(hdcDest), xDest, yDest, wDest, hDest, DCFromHandle(hdcSrc), xSrc, ySrc, wSrc, hSrc,
                    rop);
}

COLORREF SetPixel(HDC hdc, int x, int y, COLORREF color) {
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    if (!s)
        return CLR_INVALID;
    DrawScope ds(s);
    if (ds)
        FillDeviceRectColor(ds.dc(), wxRect(s->ToDevice(x, y), wxSize(1, 1)), ToWxColour(color));
    return color & 0xFFFFFF;
}

COLORREF GetPixel(HDC hdc, int x, int y) {
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    const wxImage* px = DCPixels(s);
    if (!px)
        return CLR_INVALID;
    double scale = DCBitmapScale(s);
    wxPoint p = s->ToDevice(x, y);
    int ix = static_cast<int>(std::floor(p.x * scale)), iy = static_cast<int>(std::floor(p.y * scale));
    if (ix < 0 || iy < 0 || ix >= px->GetWidth() || iy >= px->GetHeight())
        return CLR_INVALID;
    const unsigned char* d = px->GetData() + (static_cast<size_t>(iy) * px->GetWidth() + ix) * 3;
    return RGB(d[0], d[1], d[2]);
}

int SetDIBitsToDevice(HDC hdc, int xDest, int yDest, DWORD w, DWORD h, int xSrc, int ySrc, UINT StartScan,
                      UINT cLines, const void* lpvBits, const BITMAPINFO* lpbmi, UINT ColorUse) {
    wxImage img;
    if (!DibToImage(lpbmi, lpvBits, ColorUse, img, static_cast<int>(StartScan), static_cast<int>(cLines)))
        return 0;
    int iw = static_cast<int>(w), ih = static_cast<int>(h);
    int top = lpbmi->bmiHeader.biHeight > 0 ? img.GetHeight() - ySrc - ih : ySrc;
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    if (!s || !DrawImageImpl(s, xDest, yDest, iw, ih, img, xSrc, top, iw, ih, SRCCOPY))
        return 0;
    return static_cast<int>(cLines);
}

int StretchDIBits(HDC hdc, int xDest, int yDest, int DestWidth, int DestHeight, int xSrc, int ySrc, int SrcWidth,
                  int SrcHeight, const void* lpBits, const BITMAPINFO* lpbmi, UINT iUsage, DWORD rop) {
    wxImage img;
    if (!DibToImage(lpbmi, lpBits, iUsage, img))
        return 0;
    int ys = lpbmi->bmiHeader.biHeight > 0 ? img.GetHeight() - ySrc - SrcHeight : ySrc;
    GdiLock lock;
    DCState* s = DCFromHandle(hdc);
    if (!s || !DrawImageImpl(s, xDest, yDest, DestWidth, DestHeight, img, xSrc, ys, SrcWidth, SrcHeight, rop))
        return 0;
    return std::abs(SrcHeight);
}
