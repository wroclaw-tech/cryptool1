#pragma once
#define __AFXEXT_H__

#include "afxwin.h"
#include "afxdlgs.h"

class CBitmapButton : public CButton {
    DECLARE_DYNAMIC(CBitmapButton)
public:
    CBitmapButton() = default;
    CBitmapButton(const CBitmapButton&) = delete;
    BOOL LoadBitmaps(const char* lpszBitmapResource, const char* lpszBitmapResourceSel = nullptr,
                     const char* lpszBitmapResourceFocus = nullptr, const char* lpszBitmapResourceDisabled = nullptr);
    BOOL LoadBitmaps(UINT nIDBitmapResource, UINT nIDBitmapResourceSel = 0, UINT nIDBitmapResourceFocus = 0,
                     UINT nIDBitmapResourceDisabled = 0);
    BOOL AutoLoad(UINT nID, CWnd* pParent);
    void SizeToContent();
    void DrawItem(LPDRAWITEMSTRUCT lpDIS) override;

    CBitmap m_bitmap;
    CBitmap m_bitmapSel;
    CBitmap m_bitmapFocus;
    CBitmap m_bitmapDisabled;
};

class CFormView : public CScrollView {
    DECLARE_DYNAMIC(CFormView)
public:
    explicit CFormView(UINT nIDTemplate);
    explicit CFormView(const char* lpszTemplateName);
    void OnDraw(CDC*) override {}
    void OnInitialUpdate() override;
    UINT m_nIDTemplate;
};

class CSplitterWnd : public CWnd {
public:
    BOOL CreateStatic(CWnd*, int, int, DWORD = WS_CHILD | WS_VISIBLE, UINT = AFX_IDW_PANE_FIRST) { return FALSE; }
    BOOL CreateView(int, int, CRuntimeClass*, SIZE, CCreateContext*) { return FALSE; }
    CWnd* GetPane(int, int) const { return nullptr; }
};
