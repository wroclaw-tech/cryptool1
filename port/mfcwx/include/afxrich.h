#pragma once
#define __AFXRICH_H__

#include "afxwin.h"
#include "afxcmn.h"
#include "afxole.h"

class CRichEditCntrItem : public CObject {
public:
    CRichEditCntrItem() = default;
};

class CRichEditDoc : public CDocument {
    DECLARE_DYNAMIC(CRichEditDoc)
public:
    CRichEditDoc();
    virtual CRichEditCntrItem* CreateClientItem(void* preo = nullptr) const;
    class CRichEditView* GetView() const;
    BOOL m_bRTF;
    void Serialize(CArchive& ar) override;
};

class CRichEditView : public CView {
    DECLARE_DYNCREATE(CRichEditView)
public:
    CRichEditView();
    CRichEditCtrl& GetRichEditCtrl() const { return *reinterpret_cast<CRichEditCtrl*>(const_cast<CRichEditView*>(this)); }
    CRichEditDoc* GetDocument() const { return reinterpret_cast<CRichEditDoc*>(m_pDocument); }
    void OnDraw(CDC*) override {}
    void OnInitialUpdate() override;
    void Serialize(CArchive& ar) override;
    long GetTextLength() const;
    void SetCharFormat(CHARFORMAT2 cf);
    CHARFORMAT2& GetCharFormatSelection();
    void WrapChanged() {}
    afx_msg int OnCreate(LPCREATESTRUCT lpcs);
    int m_nWordWrap;
    enum WordWrapType { WrapNone = 0, WrapToWindow = 1, WrapToTargetDevice = 2 };

protected:
    DECLARE_MESSAGE_MAP()
};
