#include "afxole.h"
#include "afxdlgs.h"

#include "globalmem.h"

BOOL COleDataObject::AttachClipboard() { return TRUE; }

BOOL COleDataObject::IsDataAvailable(CLIPFORMAT cfFormat, void*) const {
    return ::IsClipboardFormatAvailable(cfFormat);
}

HGLOBAL COleDataObject::GetGlobalData(CLIPFORMAT cfFormat, void*) const {
    if (!::OpenClipboard(nullptr))
        return nullptr;
    HGLOBAL copy = nullptr;
    if (HANDLE h = ::GetClipboardData(cfFormat))
        copy = mfcwx::GlobalFromData(mfcwx::GlobalData(h), mfcwx::GlobalDataSize(h));
    ::CloseClipboard();
    return copy;
}

void COleDataSource::CacheGlobalData(CLIPFORMAT cfFormat, HGLOBAL hGlobal, void*) {
    if (m_data && m_data != hGlobal)
        ::GlobalFree(m_data);
    m_data = hGlobal;
    m_format = cfFormat;
}

DROPEFFECT COleDataSource::DoDragDrop(DWORD, LPCRECT, void*) { return DROPEFFECT_NONE; }

void COleDataSource::SetClipboard() {
    if (!m_data || !::OpenClipboard(nullptr))
        return;
    ::EmptyClipboard();
    if (::SetClipboardData(m_format, m_data))
        m_data = nullptr;
    ::CloseClipboard();
}

BOOL CPrintDialog::PrintSelection() const { return (m_pd.Flags & PD_SELECTION) != 0; }
BOOL CPrintDialog::PrintRange() const { return (m_pd.Flags & PD_PAGENUMS) != 0; }
BOOL CPrintDialog::PrintAll() const { return !PrintRange() && !PrintSelection(); }
int CPrintDialog::GetFromPage() const { return PrintRange() ? m_pd.nFromPage : -1; }
int CPrintDialog::GetToPage() const { return PrintRange() ? m_pd.nToPage : -1; }
