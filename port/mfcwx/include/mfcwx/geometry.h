#pragma once

#include "mfcwx/wintypes.h"

class CPoint;
class CRect;

class CSize : public tagSIZE {
public:
    CSize() { cx = cy = 0; }
    CSize(int initCX, int initCY) { cx = initCX; cy = initCY; }
    CSize(SIZE initSize) { cx = initSize.cx; cy = initSize.cy; }
    CSize(POINT initPt) { cx = initPt.x; cy = initPt.y; }
    CSize(DWORD dwSize) { cx = static_cast<short>(dwSize & 0xffff); cy = static_cast<short>(dwSize >> 16); }
    bool operator==(SIZE s) const { return cx == s.cx && cy == s.cy; }
    bool operator!=(SIZE s) const { return !(*this == s); }
    void operator+=(SIZE s) { cx += s.cx; cy += s.cy; }
    void operator-=(SIZE s) { cx -= s.cx; cy -= s.cy; }
    void SetSize(int CX, int CY) { cx = CX; cy = CY; }
    CSize operator+(SIZE s) const { return CSize(cx + s.cx, cy + s.cy); }
    CSize operator-(SIZE s) const { return CSize(cx - s.cx, cy - s.cy); }
    CSize operator-() const { return CSize(-cx, -cy); }
    CPoint operator+(POINT p) const;
    CPoint operator-(POINT p) const;
};

class CPoint : public tagPOINT {
public:
    CPoint() { x = y = 0; }
    CPoint(int initX, int initY) { x = initX; y = initY; }
    CPoint(POINT initPt) { x = initPt.x; y = initPt.y; }
    CPoint(SIZE initSize) { x = initSize.cx; y = initSize.cy; }
    CPoint(LPARAM dwPoint) { x = static_cast<short>(dwPoint & 0xffff); y = static_cast<short>((dwPoint >> 16) & 0xffff); }
    void Offset(int xOffset, int yOffset) { x += xOffset; y += yOffset; }
    void Offset(POINT point) { x += point.x; y += point.y; }
    void Offset(SIZE size) { x += size.cx; y += size.cy; }
    void SetPoint(int X, int Y) { x = X; y = Y; }
    bool operator==(POINT p) const { return x == p.x && y == p.y; }
    bool operator!=(POINT p) const { return !(*this == p); }
    void operator+=(SIZE s) { x += s.cx; y += s.cy; }
    void operator-=(SIZE s) { x -= s.cx; y -= s.cy; }
    void operator+=(POINT p) { x += p.x; y += p.y; }
    void operator-=(POINT p) { x -= p.x; y -= p.y; }
    CPoint operator+(SIZE s) const { return CPoint(x + s.cx, y + s.cy); }
    CPoint operator-(SIZE s) const { return CPoint(x - s.cx, y - s.cy); }
    CPoint operator-() const { return CPoint(-x, -y); }
    CPoint operator+(POINT p) const { return CPoint(x + p.x, y + p.y); }
    CSize operator-(POINT p) const { return CSize(x - p.x, y - p.y); }
};

inline CPoint CSize::operator+(POINT p) const { return CPoint(cx + p.x, cy + p.y); }
inline CPoint CSize::operator-(POINT p) const { return CPoint(cx - p.x, cy - p.y); }

class CRect : public tagRECT {
public:
    CRect() { left = top = right = bottom = 0; }
    CRect(int l, int t, int r, int b) { left = l; top = t; right = r; bottom = b; }
    CRect(const RECT& srcRect) { left = srcRect.left; top = srcRect.top; right = srcRect.right; bottom = srcRect.bottom; }
    CRect(LPCRECT lpSrcRect) { *this = *lpSrcRect; }
    CRect(POINT point, SIZE size) { left = point.x; top = point.y; right = left + size.cx; bottom = top + size.cy; }
    CRect(POINT topLeft, POINT bottomRight) { left = topLeft.x; top = topLeft.y; right = bottomRight.x; bottom = bottomRight.y; }
    CRect& operator=(const RECT& r) { left = r.left; top = r.top; right = r.right; bottom = r.bottom; return *this; }

    int Width() const { return right - left; }
    int Height() const { return bottom - top; }
    CSize Size() const { return CSize(Width(), Height()); }
    CPoint& TopLeft() { return *reinterpret_cast<CPoint*>(this); }
    CPoint& BottomRight() { return *(reinterpret_cast<CPoint*>(this) + 1); }
    const CPoint& TopLeft() const { return *reinterpret_cast<const CPoint*>(this); }
    const CPoint& BottomRight() const { return *(reinterpret_cast<const CPoint*>(this) + 1); }
    CPoint CenterPoint() const { return CPoint((left + right) / 2, (top + bottom) / 2); }
    operator LPRECT() { return this; }
    operator LPCRECT() const { return this; }
    BOOL IsRectEmpty() const { return left >= right || top >= bottom; }
    BOOL IsRectNull() const { return left == 0 && right == 0 && top == 0 && bottom == 0; }
    BOOL PtInRect(POINT point) const { return point.x >= left && point.x < right && point.y >= top && point.y < bottom; }
    void SetRect(int x1, int y1, int x2, int y2) { left = x1; top = y1; right = x2; bottom = y2; }
    void SetRect(POINT topLeft, POINT bottomRight) { SetRect(topLeft.x, topLeft.y, bottomRight.x, bottomRight.y); }
    void SetRectEmpty() { left = top = right = bottom = 0; }
    void CopyRect(LPCRECT lpSrcRect) { *this = *lpSrcRect; }
    BOOL EqualRect(LPCRECT r) const { return left == r->left && top == r->top && right == r->right && bottom == r->bottom; }
    void InflateRect(int x, int y) { left -= x; top -= y; right += x; bottom += y; }
    void InflateRect(SIZE size) { InflateRect(size.cx, size.cy); }
    void InflateRect(LPCRECT r) { left -= r->left; top -= r->top; right += r->right; bottom += r->bottom; }
    void InflateRect(int l, int t, int r, int b) { left -= l; top -= t; right += r; bottom += b; }
    void DeflateRect(int x, int y) { InflateRect(-x, -y); }
    void DeflateRect(SIZE size) { InflateRect(-size.cx, -size.cy); }
    void DeflateRect(LPCRECT r) { left += r->left; top += r->top; right -= r->right; bottom -= r->bottom; }
    void DeflateRect(int l, int t, int r, int b) { left += l; top += t; right -= r; bottom -= b; }
    void OffsetRect(int x, int y) { left += x; right += x; top += y; bottom += y; }
    void OffsetRect(POINT point) { OffsetRect(point.x, point.y); }
    void OffsetRect(SIZE size) { OffsetRect(size.cx, size.cy); }
    void MoveToY(int y) { bottom = Height() + y; top = y; }
    void MoveToX(int x) { right = Width() + x; left = x; }
    void MoveToXY(int x, int y) { MoveToX(x); MoveToY(y); }
    void MoveToXY(POINT p) { MoveToXY(p.x, p.y); }
    void NormalizeRect() {
        if (left > right) { LONG t = left; left = right; right = t; }
        if (top > bottom) { LONG t = top; top = bottom; bottom = t; }
    }
    BOOL IntersectRect(LPCRECT r1, LPCRECT r2) {
        left = r1->left > r2->left ? r1->left : r2->left;
        top = r1->top > r2->top ? r1->top : r2->top;
        right = r1->right < r2->right ? r1->right : r2->right;
        bottom = r1->bottom < r2->bottom ? r1->bottom : r2->bottom;
        if (IsRectEmpty()) { SetRectEmpty(); return FALSE; }
        return TRUE;
    }
    BOOL UnionRect(LPCRECT r1, LPCRECT r2) {
        CRect a(r1), b(r2);
        if (a.IsRectEmpty()) { *this = b; return !IsRectEmpty(); }
        if (b.IsRectEmpty()) { *this = a; return TRUE; }
        left = a.left < b.left ? a.left : b.left;
        top = a.top < b.top ? a.top : b.top;
        right = a.right > b.right ? a.right : b.right;
        bottom = a.bottom > b.bottom ? a.bottom : b.bottom;
        return TRUE;
    }
    bool operator==(const RECT& r) const { return EqualRect(&r); }
    bool operator!=(const RECT& r) const { return !EqualRect(&r); }
    void operator+=(POINT p) { OffsetRect(p); }
    void operator+=(SIZE s) { OffsetRect(s); }
    void operator-=(POINT p) { OffsetRect(-p.x, -p.y); }
    void operator-=(SIZE s) { OffsetRect(-s.cx, -s.cy); }
    void operator&=(const RECT& r) { IntersectRect(this, &r); }
    void operator|=(const RECT& r) { UnionRect(this, &r); }
    CRect operator+(POINT p) const { CRect r(*this); r.OffsetRect(p); return r; }
    CRect operator-(POINT p) const { CRect r(*this); r.OffsetRect(-p.x, -p.y); return r; }
    CRect operator+(SIZE s) const { CRect r(*this); r.OffsetRect(s); return r; }
    CRect operator-(SIZE s) const { CRect r(*this); r.OffsetRect(-s.cx, -s.cy); return r; }
    CRect operator&(const RECT& r2) const { CRect r; r.IntersectRect(this, &r2); return r; }
    CRect operator|(const RECT& r2) const { CRect r; r.UnionRect(this, &r2); return r; }
};
