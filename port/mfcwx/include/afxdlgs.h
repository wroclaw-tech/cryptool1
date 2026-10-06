#pragma once
#define __AFXDLGS_H__

#include "afxwin.h"

#include <vector>

class CCommonDialog : public CDialog {
    DECLARE_DYNAMIC(CCommonDialog)
public:
    explicit CCommonDialog(CWnd* pParentWnd) : CDialog(static_cast<UINT>(0), pParentWnd) {}
};

class CFileDialog : public CCommonDialog {
    DECLARE_DYNAMIC(CFileDialog)
public:
    explicit CFileDialog(BOOL bOpenFileDialog, const char* lpszDefExt = nullptr, const char* lpszFileName = nullptr,
                         DWORD dwFlags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, const char* lpszFilter = nullptr,
                         CWnd* pParentWnd = nullptr, DWORD dwSize = 0, BOOL bVistaStyle = TRUE);
    INT_PTR DoModal() override;
    CString GetPathName() const;
    CString GetFileName() const;
    CString GetFileExt() const;
    CString GetFileTitle() const;
    CString GetFolderPath() const;
    BOOL GetReadOnlyPref() const { return FALSE; }
    POSITION GetStartPosition() const;
    CString GetNextPathName(POSITION& pos) const;
    OPENFILENAME& GetOFN() { return m_ofn; }

    OPENFILENAME m_ofn;
    BOOL m_bOpenFileDialog;
    CString m_strFilter;
    CString m_strDefExt;
    CString m_strTitle;
    CString m_strInitialDir;
    char m_szFileName[4096];
    char m_szFileTitle[260];
    std::vector<CString> m_paths;
};

class CColorDialog : public CCommonDialog {
    DECLARE_DYNAMIC(CColorDialog)
public:
    explicit CColorDialog(COLORREF clrInit = 0, DWORD dwFlags = 0, CWnd* pParentWnd = nullptr);
    INT_PTR DoModal() override;
    COLORREF GetColor() const { return m_cc.rgbResult; }
    void SetCurrentColor(COLORREF clr) { m_cc.rgbResult = clr; }
    CHOOSECOLOR m_cc;
};

typedef struct tagCHOOSEFONTA {
    DWORD lStructSize;
    HWND hwndOwner;
    HDC hDC;
    LPLOGFONT lpLogFont;
    INT iPointSize;
    DWORD Flags;
    COLORREF rgbColors;
    LPARAM lCustData;
    LPVOID lpfnHook;
    LPCSTR lpTemplateName;
    HINSTANCE hInstance;
    LPSTR lpszStyle;
    WORD nFontType;
    INT nSizeMin;
    INT nSizeMax;
} CHOOSEFONT, *LPCHOOSEFONT;
#define CF_SCREENFONTS 0x00000001
#define CF_PRINTERFONTS 0x00000002
#define CF_BOTH (CF_SCREENFONTS | CF_PRINTERFONTS)
#define CF_EFFECTS 0x00000100L
#define CF_INITTOLOGFONTSTRUCT 0x00000040L
#define CF_FIXEDPITCHONLY 0x00004000L
#define CF_NOVERTFONTS 0x01000000L

class CFontDialog : public CCommonDialog {
    DECLARE_DYNAMIC(CFontDialog)
public:
    explicit CFontDialog(LPLOGFONT lplfInitial = nullptr, DWORD dwFlags = CF_EFFECTS | CF_SCREENFONTS,
                         CDC* pdcPrinter = nullptr, CWnd* pParentWnd = nullptr);
    INT_PTR DoModal() override;
    void GetCurrentFont(LPLOGFONT lplf);
    CString GetFaceName() const;
    int GetSize() const;
    COLORREF GetColor() const { return m_cf.rgbColors; }
    int GetWeight() const { return m_lf.lfWeight; }
    BOOL IsItalic() const { return m_lf.lfItalic; }
    BOOL IsUnderline() const { return m_lf.lfUnderline; }
    BOOL IsStrikeOut() const { return m_lf.lfStrikeOut; }
    BOOL IsBold() const { return m_lf.lfWeight >= FW_SEMIBOLD; }
    CHOOSEFONT m_cf;
    LOGFONT m_lf;
};

typedef struct tagPSDA {
    DWORD lStructSize;
    HWND hwndOwner;
    HGLOBAL hDevMode, hDevNames;
    DWORD Flags;
    POINT ptPaperSize;
    RECT rtMinMargin, rtMargin;
} PAGESETUPDLG;
#define PSD_INTHOUSANDTHSOFINCHES 0x00000004
#define PSD_INHUNDREDTHSOFMILLIMETERS 0x00000008
#define PSD_MARGINS 0x00000002

class CPageSetupDialog : public CCommonDialog {
    DECLARE_DYNAMIC(CPageSetupDialog)
public:
    explicit CPageSetupDialog(DWORD dwFlags = 0, CWnd* pParentWnd = nullptr);
    INT_PTR DoModal() override;
    CSize GetPaperSize() const;
    void GetMargins(LPRECT lpRectMargins, LPRECT lpRectMinMargins) const;
    PAGESETUPDLG m_psd;
};

class CPrintDialog : public CCommonDialog {
    DECLARE_DYNAMIC(CPrintDialog)
public:
    explicit CPrintDialog(BOOL bPrintSetupOnly, DWORD dwFlags = 0, CWnd* pParentWnd = nullptr);
    INT_PTR DoModal() override;
    PRINTDLG m_pd;
    HDC GetPrinterDC() const { return nullptr; }
    HDC CreatePrinterDC() { return nullptr; }
    CString GetDeviceName() const { return CString(); }
};

#define FR_DOWN 0x00000001
#define FR_WHOLEWORD 0x00000002
#define FR_MATCHCASE 0x00000004
#define FR_FINDNEXT 0x00000008
#define FR_REPLACE 0x00000010
#define FR_REPLACEALL 0x00000020
#define FR_DIALOGTERM 0x00000040
#define FINDMSGSTRING "commdlg_FindReplace"

class CFindReplaceDialog : public CCommonDialog {
    DECLARE_DYNAMIC(CFindReplaceDialog)
public:
    CFindReplaceDialog();
    BOOL Create(BOOL bFindDialogOnly, const char* lpszFindWhat, const char* lpszReplaceWith = nullptr,
                DWORD dwFlags = FR_DOWN, CWnd* pParentWnd = nullptr);
    static CFindReplaceDialog* GetNotifier(LPARAM lParam);
    CString GetReplaceString() const { return m_replace; }
    CString GetFindString() const { return m_find; }
    BOOL SearchDown() const { return (m_flags & FR_DOWN) != 0; }
    BOOL FindNext() const { return (m_flags & FR_FINDNEXT) != 0; }
    BOOL MatchCase() const { return (m_flags & FR_MATCHCASE) != 0; }
    BOOL MatchWholeWord() const { return (m_flags & FR_WHOLEWORD) != 0; }
    BOOL ReplaceCurrent() const { return (m_flags & FR_REPLACE) != 0; }
    BOOL ReplaceAll() const { return (m_flags & FR_REPLACEALL) != 0; }
    BOOL IsTerminating() const { return (m_flags & FR_DIALOGTERM) != 0; }
    CString m_find;
    CString m_replace;
    DWORD m_flags;
};

class CPropertyPage : public CDialog {
    DECLARE_DYNAMIC(CPropertyPage)
public:
    CPropertyPage();
    explicit CPropertyPage(UINT nIDTemplate, UINT nIDCaption = 0, DWORD dwSize = 0);
    void SetModified(BOOL = TRUE) {}
    virtual BOOL OnSetActive() { return TRUE; }
    virtual BOOL OnKillActive() { return UpdateData(TRUE); }
    virtual BOOL OnApply() { return TRUE; }
    virtual void OnReset() {}
    void OnOK() override {}
    void OnCancel() override {}
    CString m_strCaption;
};

class CPropertySheet : public CWnd {
    DECLARE_DYNAMIC(CPropertySheet)
public:
    CPropertySheet();
    explicit CPropertySheet(UINT nIDCaption, CWnd* pParentWnd = nullptr, UINT iSelectPage = 0);
    explicit CPropertySheet(const char* pszCaption, CWnd* pParentWnd = nullptr, UINT iSelectPage = 0);
    void AddPage(CPropertyPage* pPage);
    void RemovePage(CPropertyPage* pPage);
    int GetPageCount() const { return static_cast<int>(m_pages.size()); }
    CPropertyPage* GetPage(int nPage) const { return m_pages[static_cast<size_t>(nPage)]; }
    int GetActiveIndex() const { return m_active; }
    BOOL SetActivePage(int nPage);
    void SetTitle(const char* lpszText, UINT = 0) { m_strCaption = lpszText; }
    virtual INT_PTR DoModal();
    std::vector<CPropertyPage*> m_pages;
    CString m_strCaption;
    CWnd* m_pParentWnd;
    int m_active;
};
