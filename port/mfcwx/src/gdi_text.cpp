#include "gdi_internal.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace mfcwx {

namespace {

struct Run {
    wxString text;
    int x;
    size_t start;
};

struct Line {
    wxString text;
    int mnemonic = -1;
};

wxString StripLineBreaks(const wxString& s) {
    wxString out;
    out.reserve(s.length());
    for (wxUniChar c : s)
        if (c != '\r' && c != '\n')
            out += c;
    return out;
}

wxString ProcessPrefix(const wxString& s, int& mnemonic) {
    wxString out;
    out.reserve(s.length());
    mnemonic = -1;
    for (size_t i = 0; i < s.length(); ++i) {
        wxUniChar c = s[i];
        if (c == '&') {
            if (i + 1 < s.length() && s[i + 1] == '&') {
                out += '&';
                ++i;
            } else if (i + 1 < s.length()) {
                if (mnemonic < 0)
                    mnemonic = static_cast<int>(out.length());
            }
            continue;
        }
        out += c;
    }
    return out;
}

std::vector<Run> LayoutRuns(wxDC* dc, const wxString& line, int tabWidth, int* width) {
    std::vector<Run> runs;
    int x = 0;
    size_t start = 0;
    for (size_t i = 0; i <= line.length(); ++i) {
        if (i < line.length() && line[i] != '\t')
            continue;
        wxString seg = line.Mid(start, i - start);
        if (!seg.empty()) {
            runs.push_back(Run{seg, x, start});
            x += DeviceTextWidth(dc, seg);
        }
        if (i < line.length()) {
            if (tabWidth > 0)
                x = (x / tabWidth + 1) * tabWidth;
            else
                x += DeviceTextWidth(dc, " ");
        }
        start = i + 1;
    }
    if (width)
        *width = x;
    return runs;
}

int LineWidth(wxDC* dc, const wxString& line, int tabWidth) {
    if (line.Find('\t') == wxNOT_FOUND)
        return DeviceTextWidth(dc, line);
    int w = 0;
    LayoutRuns(dc, line, tabWidth, &w);
    return w;
}

std::vector<std::pair<size_t, size_t>> WrapParagraph(wxDC* dc, const wxString& p, int maxWidth, int tabWidth) {
    std::vector<std::pair<size_t, size_t>> ranges;
    size_t len = p.length(), i = 0;
    if (len == 0) {
        ranges.emplace_back(0, 0);
        return ranges;
    }
    while (i < len) {
        size_t lineStart = i, end = i, j = i;
        while (j < len) {
            size_t k = j;
            while (k < len && p[k] == ' ')
                ++k;
            while (k < len && p[k] != ' ')
                ++k;
            int w = LineWidth(dc, p.Mid(lineStart, k - lineStart), tabWidth);
            if (w > maxWidth && end > lineStart)
                break;
            end = k;
            j = k;
            if (w > maxWidth)
                break;
        }
        if (end == lineStart)
            end = std::min(len, lineStart + 1);
        ranges.emplace_back(lineStart, end);
        i = end;
        while (i < len && p[i] == ' ')
            ++i;
    }
    return ranges;
}

wxString Ellipsize(wxDC* dc, const wxString& line, int maxWidth, UINT fmt) {
    const wxString dots = "...";
    if (fmt & DT_PATH_ELLIPSIS) {
        int sep = std::max(line.Find('\\', true), line.Find('/', true));
        if (sep > 0) {
            wxString head = line.Left(static_cast<size_t>(sep)), tail = line.Mid(static_cast<size_t>(sep));
            if (DeviceTextWidth(dc, dots + tail) <= maxWidth) {
                size_t lo = 0, hi = head.length();
                while (lo < hi) {
                    size_t mid = (lo + hi + 1) / 2;
                    if (DeviceTextWidth(dc, head.Left(mid) + dots + tail) <= maxWidth)
                        lo = mid;
                    else
                        hi = mid - 1;
                }
                return head.Left(lo) + dots + tail;
            }
        }
    }
    size_t lo = 0, hi = line.length();
    while (lo < hi) {
        size_t mid = (lo + hi + 1) / 2;
        if (DeviceTextWidth(dc, line.Left(mid) + dots) <= maxWidth)
            lo = mid;
        else
            hi = mid - 1;
    }
    return line.Left(lo) + dots;
}

void DrawRuns(wxDC* d, const std::vector<Run>& runs, int x, int y) {
    for (const Run& r : runs)
        d->DrawText(r.text, x + r.x, y);
}

void FillTextBackground(DCState* s, wxDC* d, const wxRect& r) {
    if (s->a.bkMode == OPAQUE && r.width > 0 && r.height > 0) {
        FillDeviceRectColor(d, r, ToWxColour(s->a.bkColor));
        s->ApplyFont(d);
    }
}

int AlignLeft(UINT align, int refX, int width) {
    if ((align & TA_CENTER) == TA_CENTER)
        return refX - width / 2;
    if (align & TA_RIGHT)
        return refX - width;
    return refX;
}

int AlignTop(UINT align, int refY, const DeviceMetrics& m) {
    if ((align & TA_BASELINE) == TA_BASELINE)
        return refY - m.ascent;
    if (align & TA_BOTTOM)
        return refY - m.height;
    return refY;
}

void UpdateCurrentPosition(DCState* s, int deviceWidth) {
    UINT align = s->a.textAlign;
    if (!(align & TA_UPDATECP) || (align & TA_CENTER) == TA_CENTER)
        return;
    double sx, sy;
    s->Scales(sx, sy);
    int lw = s->LogicalWidth(deviceWidth);
    int dir = sx < 0 ? -1 : 1;
    s->a.pos.x += (align & TA_RIGHT) ? -dir * lw : dir * lw;
}

int Escapement(DCState* s) {
    GdiObjectImpl* f = GdiImpl(s->a.font, GdiKind::Font);
    if (!f)
        return 0;
    LONG e = f->logFont.lfEscapement;
    return e > -3600 && e < 3600 ? static_cast<int>(e) : 0;
}

BOOL TextOutImpl(DCState* s, int x, int y, const char* str, int n) {
    if (!s || !str)
        return FALSE;
    if (n < 0)
        n = static_cast<int>(strlen(str));
    wxString text = StripLineBreaks(ToWx(str, n));
    DrawScope ds(s);
    wxDC* d = ds ? ds.dc() : s->Measure();
    s->ApplyFont(d);
    int width = DeviceTextWidth(d, text);
    DeviceMetrics m = GetDeviceMetrics(s, d);
    POINT ref = (s->a.textAlign & TA_UPDATECP) ? s->a.pos : POINT{x, y};
    wxPoint dp = s->ToDevice(ref.x, ref.y);
    int left = AlignLeft(s->a.textAlign, dp.x, width);
    int top = AlignTop(s->a.textAlign, dp.y, m);
    if (ds) {
        int esc = Escapement(s);
        if (esc) {
            d->DrawRotatedText(text, dp.x, dp.y, esc / 10.0);
        } else {
            FillTextBackground(s, d, wxRect(left, top, width, m.height));
            d->DrawText(text, left, top);
        }
    }
    UpdateCurrentPosition(s, width);
    return TRUE;
}

int NextTabStop(int x, int origin, int nTabPositions, const int* stops, int defaultTab) {
    int rel = x - origin;
    if (nTabPositions <= 0 || !stops) {
        int t = std::max(1, defaultTab);
        return origin + (rel / t + 1) * t;
    }
    if (nTabPositions == 1) {
        int t = std::max(1, std::abs(stops[0]));
        return origin + (rel / t + 1) * t;
    }
    for (int i = 0; i < nTabPositions; ++i)
        if (std::abs(stops[i]) > rel)
            return origin + std::abs(stops[i]);
    return x;
}

CSize TabbedImpl(DCState* s, int x, int y, const char* str, int n, int nTabPositions, const int* stops,
                 int nTabOrigin, bool draw) {
    if (!s || !str)
        return CSize();
    if (n < 0)
        n = static_cast<int>(strlen(str));
    wxString text = StripLineBreaks(ToWx(str, n));
    DrawScope scope(draw ? s : nullptr);
    wxDC* d = scope ? scope.dc() : s->Measure();
    s->ApplyFont(d);
    DeviceMetrics m = GetDeviceMetrics(s, d);
    int defaultTab = s->LogicalWidth(8 * m.aveWidth);
    POINT ref = (s->a.textAlign & TA_UPDATECP) ? s->a.pos : POINT{x, y};
    int lx = ref.x;
    size_t start = 0;
    for (size_t i = 0; i <= text.length(); ++i) {
        if (i < text.length() && text[i] != '\t')
            continue;
        wxString seg = text.Mid(start, i - start);
        int w = DeviceTextWidth(d, seg);
        if (scope && !seg.empty()) {
            wxPoint p = s->ToDevice(lx, ref.y);
            int top = AlignTop(s->a.textAlign, p.y, m);
            FillTextBackground(s, d, wxRect(p.x, top, w, m.height));
            d->DrawText(seg, p.x, top);
        }
        lx += s->LogicalWidth(w);
        if (i < text.length())
            lx = NextTabStop(lx, nTabOrigin, nTabPositions, stops, defaultTab);
        start = i + 1;
    }
    return CSize(lx - ref.x, s->LogicalHeight(m.height));
}

int DrawTextImpl(DCState* s, const char* str, int n, LPRECT rc, UINT fmt) {
    if (!s || !rc || !str)
        return 0;
    if (n < 0)
        n = static_cast<int>(strlen(str));
    int tabChars = 8;
    if (fmt & DT_TABSTOP) {
        tabChars = static_cast<int>((fmt >> 8) & 0xFF);
        if (!tabChars)
            tabChars = 8;
        fmt &= ~0xFF00u;
    }
    bool calc = (fmt & DT_CALCRECT) != 0;
    bool single = (fmt & DT_SINGLELINE) != 0;
    DrawScope scope(calc ? nullptr : s);
    wxDC* d = scope ? scope.dc() : s->Measure();
    s->ApplyFont(d);
    DeviceMetrics m = GetDeviceMetrics(s, d);
    int lineH = m.height + ((fmt & DT_EXTERNALLEADING) ? m.externalLeading : 0);
    int tabW = (fmt & DT_EXPANDTABS) ? tabChars * m.aveWidth : 0;
    wxRect R = s->DeviceRect(rc->left, rc->top, rc->right, rc->bottom);

    wxString text = ToWx(str, n);
    std::vector<wxString> paragraphs;
    if (single) {
        paragraphs.push_back(StripLineBreaks(text));
    } else {
        wxString cur;
        for (size_t i = 0; i < text.length(); ++i) {
            wxUniChar c = text[i];
            if (c == '\r' || c == '\n') {
                paragraphs.push_back(cur);
                cur.clear();
                if (c == '\r' && i + 1 < text.length() && text[i + 1] == '\n')
                    ++i;
                continue;
            }
            cur += c;
        }
        paragraphs.push_back(cur);
    }

    std::vector<Line> lines;
    for (const wxString& para : paragraphs) {
        int mnemonic = -1;
        wxString p = (fmt & DT_NOPREFIX) ? para : ProcessPrefix(para, mnemonic);
        if ((fmt & DT_WORDBREAK) && !single) {
            for (const auto& range : WrapParagraph(d, p, R.width, tabW)) {
                Line l;
                l.text = p.Mid(range.first, range.second - range.first);
                if (mnemonic >= static_cast<int>(range.first) && mnemonic < static_cast<int>(range.second))
                    l.mnemonic = mnemonic - static_cast<int>(range.first);
                lines.push_back(l);
            }
        } else {
            Line l;
            l.text = p;
            l.mnemonic = mnemonic;
            lines.push_back(l);
        }
    }

    std::vector<int> widths;
    int maxW = 0;
    for (const Line& l : lines) {
        widths.push_back(LineWidth(d, l.text, tabW));
        maxW = std::max(maxW, widths.back());
    }
    int total = lineH * static_cast<int>(lines.size());
    double sx, sy;
    s->Scales(sx, sy);
    if (calc) {
        rc->right = rc->left + (sx < 0 ? -1 : 1) * s->LogicalWidth(maxW);
        rc->bottom = rc->top + (sy < 0 ? -1 : 1) * s->LogicalHeight(total);
        return s->LogicalHeight(total);
    }

    int y0 = R.y;
    if (single && (fmt & DT_VCENTER))
        y0 = R.y + (R.height - lineH) / 2;
    else if (single && (fmt & DT_BOTTOM))
        y0 = R.y + R.height - lineH;

    if (fmt & (DT_END_ELLIPSIS | DT_PATH_ELLIPSIS | DT_WORD_ELLIPSIS)) {
        for (size_t i = 0; i < lines.size(); ++i) {
            if (widths[i] > R.width) {
                lines[i].text = Ellipsize(d, lines[i].text, R.width, fmt);
                lines[i].mnemonic = -1;
                widths[i] = LineWidth(d, lines[i].text, tabW);
            }
        }
    }

    if (scope) {
        bool clip = false;
        std::vector<int> xs;
        for (size_t i = 0; i < lines.size(); ++i) {
            int x = R.x;
            if (fmt & DT_CENTER)
                x = R.x + (R.width - widths[i]) / 2;
            else if (fmt & DT_RIGHT)
                x = R.x + R.width - widths[i];
            xs.push_back(x);
            int y = y0 + lineH * static_cast<int>(i);
            if (x < R.x || x + widths[i] > R.x + R.width || y < R.y || y + lineH > R.y + R.height)
                clip = true;
        }
        if (fmt & DT_NOCLIP)
            clip = false;
        if (clip)
            s->ApplyClip(d, &R);
        for (size_t i = 0; i < lines.size(); ++i) {
            int y = y0 + lineH * static_cast<int>(i);
            FillTextBackground(s, d, wxRect(xs[i], y, widths[i], lineH));
            int w = 0;
            std::vector<Run> runs = LayoutRuns(d, lines[i].text, tabW, &w);
            DrawRuns(d, runs, xs[i], y);
            int mn = lines[i].mnemonic;
            if (mn >= 0 && mn < static_cast<int>(lines[i].text.length())) {
                for (const Run& r : runs) {
                    if (static_cast<size_t>(mn) < r.start || static_cast<size_t>(mn) >= r.start + r.text.length())
                        continue;
                    size_t col = static_cast<size_t>(mn) - r.start;
                    int ux = xs[i] + r.x + DeviceTextWidth(d, r.text.Left(col));
                    int uw = DeviceTextWidth(d, r.text.Mid(col, 1));
                    FillDeviceRectColor(d, wxRect(ux, y + m.ascent + 1, uw, 1), ToWxColour(s->a.textColor));
                    s->ApplyFont(d);
                }
            }
        }
        if (clip)
            s->ApplyClip(d);
    }
    if (single && (fmt & (DT_VCENTER | DT_BOTTOM)))
        return s->LogicalHeight(y0 - R.y + total);
    return s->LogicalHeight(total);
}

DCState* LockedState(HDC h) { return DCFromHandle(h); }

} // namespace

DeviceMetrics GetDeviceMetrics(DCState* s, wxDC* dc) {
    DeviceMetrics m;
    wxFontMetrics fm = dc->GetFontMetrics();
    m.externalLeading = std::max(0, fm.externalLeading);
    m.descent = fm.descent;
    m.height = fm.height - m.externalLeading;
    m.ascent = m.height - m.descent;
    double sx, sy;
    s->Scales(sx, sy);
    double em = FontEmPixels(s->a.font) * (IsStockFont(s->a.font) ? 1.0 : std::fabs(sy));
    m.internalLeading = std::max(0, m.height - static_cast<int>(std::lround(em)));
    m.aveWidth = fm.averageWidth > 0 ? fm.averageWidth : DeviceTextWidth(dc, "x");
    if (dc->GetFont().IsFixedWidth())
        m.maxWidth = m.aveWidth;
    else
        m.maxWidth = std::max(DeviceTextWidth(dc, "W"), DeviceTextWidth(dc, "M"));
    return m;
}

int DeviceTextWidth(wxDC* dc, const wxString& text) {
    if (text.empty())
        return 0;
    wxCoord w = 0, h = 0;
    dc->GetTextExtent(text, &w, &h);
    return w;
}

} // namespace mfcwx

using namespace mfcwx;

BOOL CDC::TextOut(int x, int y, const char* lpszString, int nCount) { return ::TextOut(m_hDC, x, y, lpszString, nCount); }

BOOL CDC::TextOut(int x, int y, const CString& str) {
    return ::TextOut(m_hDC, x, y, str.GetString(), str.GetLength());
}

BOOL CDC::ExtTextOut(int x, int y, UINT nOptions, LPCRECT lpRect, const char* lpszString, UINT nCount,
                     LPINT lpDxWidths) {
    GdiLock lock;
    DCState* s = LockedState(m_hDC);
    if (!s)
        return FALSE;
    const char* str = lpszString ? lpszString : "";
    int n = lpszString ? static_cast<int>(nCount) : 0;
    DrawScope ds(s);
    wxDC* d = ds ? ds.dc() : s->Measure();
    s->ApplyFont(d);
    wxRect R;
    if (lpRect)
        R = s->DeviceRect(lpRect->left, lpRect->top, lpRect->right, lpRect->bottom);
    if (ds && lpRect && (nOptions & ETO_OPAQUE)) {
        FillDeviceRectColor(d, R, ToWxColour(s->a.bkColor));
        s->ApplyFont(d);
    }
    wxString text = StripLineBreaks(ToWx(str, n));
    DeviceMetrics m = GetDeviceMetrics(s, d);
    std::vector<int> offsets;
    int width = 0;
    if (lpDxWidths && !text.empty()) {
        int acc = 0;
        for (size_t i = 0; i < text.length() && i < static_cast<size_t>(n); ++i) {
            offsets.push_back(s->DeviceWidth(acc));
            acc += lpDxWidths[i];
        }
        width = s->DeviceWidth(acc);
    } else {
        width = DeviceTextWidth(d, text);
    }
    POINT ref = (s->a.textAlign & TA_UPDATECP) ? s->a.pos : POINT{x, y};
    wxPoint dp = s->ToDevice(ref.x, ref.y);
    int left = AlignLeft(s->a.textAlign, dp.x, width);
    int top = AlignTop(s->a.textAlign, dp.y, m);
    if (ds) {
        bool clip = lpRect && (nOptions & ETO_CLIPPED);
        if (clip)
            s->ApplyClip(d, &R);
        if (!(lpRect && (nOptions & ETO_OPAQUE)))
            FillTextBackground(s, d, wxRect(left, top, width, m.height));
        if (!offsets.empty()) {
            for (size_t i = 0; i < offsets.size(); ++i)
                d->DrawText(text.Mid(i, 1), left + offsets[i], top);
        } else {
            d->DrawText(text, left, top);
        }
        if (clip)
            s->ApplyClip(d);
    }
    UpdateCurrentPosition(s, width);
    return TRUE;
}

BOOL CDC::ExtTextOut(int x, int y, UINT nOptions, LPCRECT lpRect, const CString& str, LPINT lpDxWidths) {
    return ExtTextOut(x, y, nOptions, lpRect, str.GetString(), static_cast<UINT>(str.GetLength()), lpDxWidths);
}

CSize CDC::TabbedTextOut(int x, int y, const char* lpszString, int nCount, int nTabPositions,
                         LPINT lpnTabStopPositions, int nTabOrigin) {
    GdiLock lock;
    DCState* s = LockedState(m_hDC);
    CSize size = TabbedImpl(s, x, y, lpszString, nCount, nTabPositions, lpnTabStopPositions, nTabOrigin, true);
    if (s && (s->a.textAlign & TA_UPDATECP))
        s->a.pos.x += size.cx;
    return size;
}

int CDC::DrawText(const char* lpszString, int nCount, LPRECT lpRect, UINT nFormat) {
    return ::DrawText(m_hDC, lpszString, nCount, lpRect, nFormat);
}

int CDC::DrawText(const CString& str, LPRECT lpRect, UINT nFormat) {
    return ::DrawText(m_hDC, str.GetString(), str.GetLength(), lpRect, nFormat);
}

CSize CDC::GetTextExtent(const char* lpszString, int nCount) const {
    SIZE sz{0, 0};
    ::GetTextExtentPoint32(m_hDC, lpszString, nCount, &sz);
    return CSize(sz);
}

CSize CDC::GetTextExtent(const CString& str) const { return GetTextExtent(str.GetString(), str.GetLength()); }

CSize CDC::GetTabbedTextExtent(const char* lpszString, int nCount, int nTabPositions, LPINT lpnTabStopPositions) const {
    GdiLock lock;
    return TabbedImpl(LockedState(m_hDC), 0, 0, lpszString, nCount, nTabPositions, lpnTabStopPositions, 0, false);
}

BOOL CDC::GetTextExtentExPoint(const char* lpszString, int nCount, int nMaxExtent, LPINT lpnFit, LPINT alpDx,
                               LPSIZE lpSize) const {
    GdiLock lock;
    DCState* s = LockedState(m_hDC);
    if (!s || !lpszString)
        return FALSE;
    if (nCount < 0)
        nCount = static_cast<int>(strlen(lpszString));
    wxDC* d = s->Measure();
    wxString text = ToWx(lpszString, nCount);
    wxArrayInt widths;
    if (!text.empty())
        d->GetPartialTextExtents(text, widths);
    int fit = 0;
    for (size_t i = 0; i < widths.size(); ++i) {
        int lw = s->LogicalWidth(widths[i]);
        if (lw <= nMaxExtent || !lpnFit)
            fit = static_cast<int>(i) + 1;
        if (alpDx && (static_cast<int>(i) < fit || !lpnFit))
            alpDx[i] = lw;
    }
    if (lpnFit)
        *lpnFit = fit;
    if (lpSize) {
        DeviceMetrics m = GetDeviceMetrics(s, d);
        lpSize->cx = widths.empty() ? 0 : s->LogicalWidth(widths.back());
        lpSize->cy = s->LogicalHeight(m.height);
    }
    return TRUE;
}

BOOL CDC::GetTextMetrics(LPTEXTMETRIC lpMetrics) const { return ::GetTextMetrics(m_hDC, lpMetrics); }

int CDC::GetTextFace(int nCount, char* lpszFacename) const {
    GdiLock lock;
    DCState* s = LockedState(m_hDC);
    if (!s)
        return 0;
    LOGFONT lf;
    memset(&lf, 0, sizeof lf);
    ::GetObject(s->a.font, sizeof lf, &lf);
    int len = static_cast<int>(strnlen(lf.lfFaceName, LF_FACESIZE));
    if (!lpszFacename)
        return len + 1;
    if (nCount <= 0)
        return 0;
    int n = std::min(len, nCount - 1);
    memcpy(lpszFacename, lf.lfFaceName, static_cast<size_t>(n));
    lpszFacename[n] = 0;
    return n;
}

int CDC::GetTextFace(CString& rString) const {
    char buf[LF_FACESIZE + 1];
    int n = GetTextFace(sizeof buf, buf);
    rString = buf;
    return n;
}

BOOL CDC::GetCharWidth(UINT nFirstChar, UINT nLastChar, LPINT lpBuffer) const {
    GdiLock lock;
    DCState* s = LockedState(m_hDC);
    if (!s || !lpBuffer || nLastChar < nFirstChar)
        return FALSE;
    wxDC* d = s->Measure();
    for (UINT c = nFirstChar; c <= nLastChar; ++c) {
        char ch = static_cast<char>(c & 0xFF);
        lpBuffer[c - nFirstChar] = s->LogicalWidth(DeviceTextWidth(d, ToWx(&ch, 1)));
    }
    return TRUE;
}

BOOL TextOut(HDC hdc, int x, int y, const char* lpString, int c) {
    GdiLock lock;
    return TextOutImpl(LockedState(hdc), x, y, lpString, c);
}

BOOL TextOutA(HDC hdc, int x, int y, const char* lpString, int c) { return TextOut(hdc, x, y, lpString, c); }

int DrawText(HDC hdc, const char* lpchText, int cchText, LPRECT lprc, UINT format) {
    GdiLock lock;
    return DrawTextImpl(LockedState(hdc), lpchText, cchText, lprc, format);
}

int DrawTextA(HDC hdc, const char* lpchText, int cchText, LPRECT lprc, UINT format) {
    return DrawText(hdc, lpchText, cchText, lprc, format);
}

BOOL GetTextExtentPoint32(HDC hdc, const char* lpString, int c, LPSIZE psizl) {
    GdiLock lock;
    DCState* s = LockedState(hdc);
    if (!s || !psizl)
        return FALSE;
    if (!lpString)
        c = 0;
    if (c < 0)
        c = static_cast<int>(strlen(lpString));
    wxDC* d = s->Measure();
    DeviceMetrics m = GetDeviceMetrics(s, d);
    psizl->cx = c ? s->LogicalWidth(DeviceTextWidth(d, StripLineBreaks(ToWx(lpString, c)))) : 0;
    psizl->cy = s->LogicalHeight(m.height);
    return TRUE;
}

BOOL GetTextExtentPoint32A(HDC hdc, const char* lpString, int c, LPSIZE psizl) {
    return GetTextExtentPoint32(hdc, lpString, c, psizl);
}

BOOL GetTextMetrics(HDC hdc, LPTEXTMETRIC lptm) {
    GdiLock lock;
    DCState* s = LockedState(hdc);
    if (!s || !lptm)
        return FALSE;
    wxDC* d = s->Measure();
    DeviceMetrics m = GetDeviceMetrics(s, d);
    LOGFONT lf;
    memset(&lf, 0, sizeof lf);
    ::GetObject(s->a.font, sizeof lf, &lf);
    memset(lptm, 0, sizeof *lptm);
    lptm->tmHeight = s->LogicalHeight(m.height);
    lptm->tmAscent = s->LogicalHeight(m.ascent);
    lptm->tmDescent = lptm->tmHeight - lptm->tmAscent;
    lptm->tmInternalLeading = s->LogicalHeight(m.internalLeading);
    lptm->tmExternalLeading = s->LogicalHeight(m.externalLeading);
    lptm->tmAveCharWidth = s->LogicalWidth(m.aveWidth);
    lptm->tmMaxCharWidth = s->LogicalWidth(m.maxWidth);
    lptm->tmWeight = lf.lfWeight ? lf.lfWeight : FW_NORMAL;
    lptm->tmOverhang = 0;
    lptm->tmDigitizedAspectX = 96;
    lptm->tmDigitizedAspectY = 96;
    lptm->tmFirstChar = 0x20;
    lptm->tmLastChar = 0xFF;
    lptm->tmDefaultChar = 0x1F;
    lptm->tmBreakChar = 0x20;
    lptm->tmItalic = lf.lfItalic ? 1 : 0;
    lptm->tmUnderlined = lf.lfUnderline ? 1 : 0;
    lptm->tmStruckOut = lf.lfStrikeOut ? 1 : 0;
    bool fixed = d->GetFont().IsFixedWidth();
    BYTE family = static_cast<BYTE>(lf.lfPitchAndFamily & 0xF0);
    if (!family)
        family = static_cast<BYTE>(fixed ? FF_MODERN : FF_SWISS);
    lptm->tmPitchAndFamily = static_cast<BYTE>(family | (fixed ? 0 : 0x01) | 0x02 | 0x04);
    lptm->tmCharSet = lf.lfCharSet;
    return TRUE;
}

BOOL GetTextMetricsA(HDC hdc, LPTEXTMETRIC lptm) { return GetTextMetrics(hdc, lptm); }
