#pragma once
#define __AFXOLE_H__

#include "afxwin.h"

#define DROPEFFECT_NONE 0
#define DROPEFFECT_COPY 1
#define DROPEFFECT_MOVE 2
#define DROPEFFECT_LINK 4
#define DROPEFFECT_SCROLL 0x80000000
typedef DWORD DROPEFFECT;
typedef WORD CLIPFORMAT;

class COleDataObject {
public:
    COleDataObject() = default;
    BOOL IsDataAvailable(CLIPFORMAT cfFormat, void* = nullptr) const;
    HGLOBAL GetGlobalData(CLIPFORMAT cfFormat, void* = nullptr) const;
    BOOL AttachClipboard();
    void Release() {}
};

class COleDataSource : public CCmdTarget {
public:
    void CacheGlobalData(CLIPFORMAT cfFormat, HGLOBAL hGlobal, void* = nullptr);
    DROPEFFECT DoDragDrop(DWORD = DROPEFFECT_COPY | DROPEFFECT_MOVE, LPCRECT = nullptr, void* = nullptr);
    void SetClipboard();
    HGLOBAL m_data = nullptr;
    CLIPFORMAT m_format = 0;
};

class COleDropTarget : public CCmdTarget {
public:
    BOOL Register(CWnd*) { return TRUE; }
    void Revoke() {}
};

class COleDateTime {
public:
    COleDateTime() : m_dt(0), m_status(valid) {}
    COleDateTime(int nYear, int nMonth, int nDay, int nHour, int nMin, int nSec);
    static COleDateTime GetCurrentTime();
    int GetYear() const;
    int GetMonth() const;
    int GetDay() const;
    int GetHour() const;
    int GetMinute() const;
    int GetSecond() const;
    int GetDayOfWeek() const;
    CString Format(const char* pFormat) const;
    CString Format(DWORD dwFlags = 0, LCID lcid = 0) const;
    enum DateTimeStatus { error = -1, valid = 0, invalid = 1, null = 2 };
    double m_dt;
    DateTimeStatus m_status;
};

BOOL AFXAPI AfxOleInit();
