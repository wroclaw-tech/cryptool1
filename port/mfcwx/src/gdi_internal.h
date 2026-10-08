#pragma once

// Shared implementation details of the GDI emulation (gdi*.cpp).

#include <wx/wx.h>
#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include <wx/dcmemory.h>
#include <wx/fontenum.h>
#include <wx/graphics.h>
#include <wx/image.h>
#include <wx/rawbmp.h>
#include <wx/region.h>
#include <wx/settings.h>

#include "internal.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace mfcwx {

std::recursive_mutex& GdiMutex();

class GdiLock {
public:
    GdiLock() { GdiMutex().lock(); }
    ~GdiLock() { GdiMutex().unlock(); }
    GdiLock(const GdiLock&) = delete;
    GdiLock& operator=(const GdiLock&) = delete;
};

enum class GdiKind { Pen, Brush, Font, Bitmap, Region, Palette };

struct DCState;

struct GdiObjectImpl {
    explicit GdiObjectImpl(GdiKind k) : kind(k) {}
    GdiKind kind;
    int refs = 1;
    bool stock = false;
    bool deleted = false;
    CGdiObject* permanent = nullptr;

    LOGPEN logPen{};
    DWORD penStyle = 0;
    std::vector<wxDash> dashes;

    LOGBRUSH logBrush{};
    int sysColor = -1;
    wxBrush brush;

    LOGFONT logFont{};
    bool fontPending = false;
    wxFont font;
    double emPx = 0;
    double scaledFor = 1.0;
    wxFont scaledFont;

    wxBitmap bitmap;
    int bitsPixel = 32;
    SIZE dimension{0, 0};
    DCState* selectedIn = nullptr;

    wxRegion region;
};

// Handles (gdi.cpp)
GdiObjectImpl* GdiImpl(HGDIOBJ h);
GdiObjectImpl* GdiImpl(HGDIOBJ h, GdiKind kind);
HGDIOBJ RegisterGdiObject(GdiObjectImpl* impl);
void GdiAddRef(HGDIOBJ h);
void GdiRelease(HGDIOBJ h);
// (HBRUSH)(COLOR_xxx + 1) values accepted by FillRect and window classes; -1 otherwise.
int SysColorBrushIndex(HGDIOBJ h);
HGDIOBJ DefaultBitmapHandle();
bool IsStockFont(HFONT h);

wxPen DevicePen(HPEN h, double widthScale);
wxBrush DeviceBrush(HBRUSH h);
wxFont DeviceFont(HFONT h, double scale);
double FontEmPixels(HFONT h);
bool BrushIsHatched(HBRUSH h);
COLORREF BrushColor(HBRUSH h);
bool PenIsNull(HPEN h);
int PenStyle(HPEN h);
int PenWidth(HPEN h);
COLORREF PenColor(HPEN h);

wxFont MakeFont(const LOGFONT& lf, double* emPx);
double PixelsToPoints(double px);
wxBitmap NewBitmap(int width, int height, double scale, const wxColour& fill);
HBITMAP NewBitmapHandle(const wxBitmap& bmp, int bitsPixel);

// DCs (gdi_dc.cpp)
enum class DCKind { Info, Borrowed, Window, Memory };
enum class WinTarget { None, Paint, Client, OverlayPending, Overlay };

struct DCAttrs {
    HPEN pen = nullptr;
    HBRUSH brush = nullptr;
    HFONT font = nullptr;
    HBITMAP bitmap = nullptr;
    HPALETTE palette = nullptr;
    COLORREF textColor = 0;
    COLORREF bkColor = 0xFFFFFF;
    int bkMode = OPAQUE;
    int rop2 = R2_COPYPEN;
    int stretchMode = BLACKONWHITE;
    int polyFillMode = ALTERNATE;
    UINT textAlign = 0;
    int mapMode = MM_TEXT;
    POINT winOrg{0, 0};
    POINT vpOrg{0, 0};
    SIZE winExt{1, 1};
    SIZE vpExt{1, 1};
    POINT pos{0, 0};
    bool hasClip = false;
    wxRegion clip;
};

struct DCState {
    DCKind kind = DCKind::Info;
    DCAttrs a;
    std::vector<DCAttrs> saved;
    wxDC* dc = nullptr;
    std::unique_ptr<wxDC> owned;
    wxWindow* window = nullptr;
    CDC* owner = nullptr;
    WinTarget target = WinTarget::None;
    bool fromGetDC = false;
    bool paintDC = false;
    bool sessionOwner = false;
    bool wholeWindow = false;
    bool clipDirty = true;
    bool bitmapOut = false;
    double scale = 1.0;
    wxBitmap defaultBitmap;
    std::unique_ptr<wxImage> pixels;

    wxDC* Target(bool drawing);
    wxDC* Measure();
    void Finish();

    void Scales(double& sx, double& sy) const;
    wxPoint ToDevice(double x, double y) const;
    void ToLogical(double& x, double& y) const;
    wxRect DeviceRect(int l, int t, int r, int b) const;
    int LogicalWidth(int deviceCx) const;
    int LogicalHeight(int deviceCy) const;
    int DeviceWidth(int logicalCx) const;
    int DeviceHeight(int logicalCy) const;
    int Dpi() const;

    void ApplyPen(wxDC* d);
    void ApplyBrush(wxDC* d, bool fill);
    void ApplyFont(wxDC* d);
    void ApplyClip(wxDC* d, const wxRect* extra = nullptr);
    bool SupportsLogicalOps(wxDC* d) const;
    GdiObjectImpl* SelectedBitmap() const;
};

DCState* DCFromHandle(HDC h);
inline HDC ToHdc(DCState* s) { return reinterpret_cast<HDC>(s); }
wxDC& MeasureDC();
void SyncBitmap(GdiObjectImpl* bmp);
// Pixels of the bitmap selected into a memory DC (physical resolution); null for other DCs.
const wxImage* DCPixels(DCState* s);
// Scale factor of the bitmap a memory DC draws into (the DC's content scale otherwise).
double DCBitmapScale(DCState* s);

class DrawScope {
public:
    explicit DrawScope(DCState* s) : m_s(s), m_dc(s ? s->Target(true) : nullptr) {}
    ~DrawScope() {
        if (m_dc)
            m_s->Finish();
    }
    DrawScope(const DrawScope&) = delete;
    DrawScope& operator=(const DrawScope&) = delete;
    wxDC* dc() const { return m_dc; }
    explicit operator bool() const { return m_dc != nullptr; }

private:
    DCState* m_s;
    wxDC* m_dc;
};

// Fills a device rectangle with a brush, honouring hatch backgrounds and R2 modes.
void FillDeviceRect(DCState* s, wxDC* dc, const wxRect& r, HBRUSH brush);
void FillDeviceRectColor(wxDC* dc, const wxRect& r, const wxColour& c);

// Text (gdi_text.cpp)
struct DeviceMetrics {
    int height = 0;
    int ascent = 0;
    int descent = 0;
    int internalLeading = 0;
    int externalLeading = 0;
    int aveWidth = 0;
    int maxWidth = 0;
};
DeviceMetrics GetDeviceMetrics(DCState* s, wxDC* dc);
int DeviceTextWidth(wxDC* dc, const wxString& text);

// Bits (gdi_bits.cpp)
bool BlitImpl(DCState* dst, int x, int y, int w, int h, DCState* src, int xs, int ys, int ws, int hs, DWORD rop);
bool DrawImageImpl(DCState* dst, int x, int y, int w, int h, const wxImage& img, int xs, int ys, int ws, int hs,
                   DWORD rop);
bool DibToImage(const BITMAPINFO* bmi, const void* bits, UINT usage, wxImage& out, int startScan = 0,
                int lines = -1);
bool DdbBitsToImage(int width, int height, int bpp, int stride, const void* bits, wxImage& out);
bool ImageToDdbBits(const wxImage& img, int bpp, std::vector<unsigned char>& out, int& stride);
// Applies a raster operation to a device rectangle of a memory DC in software.
bool SoftwareRop(DCState* dst, const wxRect& r, const wxImage* src, DWORD rop, COLORREF pattern);

} // namespace mfcwx
