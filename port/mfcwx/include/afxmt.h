#pragma once
#define __AFXMT_H__

#include "afxwin.h"

class CSyncObject : public CObject {
    DECLARE_DYNAMIC(CSyncObject)
public:
    explicit CSyncObject(const char* pstrName = nullptr);
    ~CSyncObject() override;
    virtual BOOL Lock(DWORD dwTimeout = INFINITE);
    virtual BOOL Unlock() = 0;
    virtual BOOL Unlock(LONG, LPLONG = nullptr) { return TRUE; }
    operator HANDLE() const { return m_hObject; }
    HANDLE m_hObject;
};

class CCriticalSection : public CSyncObject {
    DECLARE_DYNAMIC(CCriticalSection)
public:
    CCriticalSection();
    ~CCriticalSection() override;
    BOOL Lock() { return Lock(INFINITE); }
    BOOL Lock(DWORD dwTimeout) override;
    BOOL Unlock() override;
    operator CRITICAL_SECTION*() { return &m_sect; }
    CRITICAL_SECTION m_sect;
};

class CMutex : public CSyncObject {
    DECLARE_DYNAMIC(CMutex)
public:
    explicit CMutex(BOOL bInitiallyOwn = FALSE, const char* lpszName = nullptr, LPSECURITY_ATTRIBUTES lpsaAttribute = nullptr);
    BOOL Unlock() override;
};

class CSemaphore : public CSyncObject {
    DECLARE_DYNAMIC(CSemaphore)
public:
    explicit CSemaphore(LONG lInitialCount = 1, LONG lMaxCount = 1, const char* pstrName = nullptr,
                        LPSECURITY_ATTRIBUTES lpsaAttributes = nullptr);
    BOOL Unlock() override;
    BOOL Unlock(LONG lCount, LPLONG lprevCount = nullptr) override;
};

class CEvent : public CSyncObject {
    DECLARE_DYNAMIC(CEvent)
public:
    explicit CEvent(BOOL bInitiallyOwn = FALSE, BOOL bManualReset = FALSE, const char* lpszName = nullptr,
                    LPSECURITY_ATTRIBUTES lpsaAttribute = nullptr);
    BOOL SetEvent();
    BOOL PulseEvent();
    BOOL ResetEvent();
    BOOL Unlock() override { return TRUE; }
};

class CSingleLock {
public:
    explicit CSingleLock(CSyncObject* pObject, BOOL bInitialLock = FALSE);
    ~CSingleLock();
    BOOL Lock(DWORD dwTimeOut = INFINITE);
    BOOL Unlock();
    BOOL Unlock(LONG lCount, LPLONG lPrevCount = nullptr);
    BOOL IsLocked() { return m_bAcquired; }

protected:
    CSyncObject* m_pObject;
    BOOL m_bAcquired;
};

class CMultiLock {
public:
    CMultiLock(CSyncObject* ppObjects[], DWORD dwCount, BOOL bInitialLock = FALSE);
    ~CMultiLock();
    DWORD Lock(DWORD dwTimeOut = INFINITE, BOOL bWaitForAll = TRUE, DWORD dwWakeMask = 0);
    BOOL Unlock();

protected:
    std::vector<CSyncObject*> m_objects;
    std::vector<bool> m_locked;
};
