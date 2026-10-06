#include "afxmt.h"

#include <vector>

IMPLEMENT_DYNAMIC(CSyncObject, CObject)
IMPLEMENT_DYNAMIC(CCriticalSection, CSyncObject)
IMPLEMENT_DYNAMIC(CMutex, CSyncObject)
IMPLEMENT_DYNAMIC(CSemaphore, CSyncObject)
IMPLEMENT_DYNAMIC(CEvent, CSyncObject)

CSyncObject::CSyncObject(const char*) : m_hObject(nullptr) {}

CSyncObject::~CSyncObject() {
    if (m_hObject)
        CloseHandle(m_hObject);
    m_hObject = nullptr;
}

BOOL CSyncObject::Lock(DWORD dwTimeout) {
    DWORD r = WaitForSingleObject(m_hObject, dwTimeout);
    return r == WAIT_OBJECT_0 || r == WAIT_ABANDONED;
}

CCriticalSection::CCriticalSection() : CSyncObject(nullptr) { InitializeCriticalSection(&m_sect); }

CCriticalSection::~CCriticalSection() { DeleteCriticalSection(&m_sect); }

BOOL CCriticalSection::Lock(DWORD) {
    EnterCriticalSection(&m_sect);
    return TRUE;
}

BOOL CCriticalSection::Unlock() {
    LeaveCriticalSection(&m_sect);
    return TRUE;
}

CMutex::CMutex(BOOL bInitiallyOwn, const char* lpszName, LPSECURITY_ATTRIBUTES lpsaAttribute) : CSyncObject(lpszName) {
    m_hObject = CreateMutexA(lpsaAttribute, bInitiallyOwn, lpszName);
    if (!m_hObject)
        AfxThrowResourceException();
}

BOOL CMutex::Unlock() { return ReleaseMutex(m_hObject); }

CSemaphore::CSemaphore(LONG lInitialCount, LONG lMaxCount, const char* pstrName, LPSECURITY_ATTRIBUTES lpsaAttributes)
    : CSyncObject(pstrName) {
    m_hObject = CreateSemaphoreA(lpsaAttributes, lInitialCount, lMaxCount, pstrName);
    if (!m_hObject)
        AfxThrowResourceException();
}

BOOL CSemaphore::Unlock() { return Unlock(1, nullptr); }

BOOL CSemaphore::Unlock(LONG lCount, LPLONG lprevCount) { return ReleaseSemaphore(m_hObject, lCount, lprevCount); }

CEvent::CEvent(BOOL bInitiallyOwn, BOOL bManualReset, const char* lpszName, LPSECURITY_ATTRIBUTES lpsaAttribute)
    : CSyncObject(lpszName) {
    m_hObject = CreateEventA(lpsaAttribute, bManualReset, bInitiallyOwn, lpszName);
    if (!m_hObject)
        AfxThrowResourceException();
}

BOOL CEvent::SetEvent() { return ::SetEvent(m_hObject); }

BOOL CEvent::PulseEvent() { return ::PulseEvent(m_hObject); }

BOOL CEvent::ResetEvent() { return ::ResetEvent(m_hObject); }

CSingleLock::CSingleLock(CSyncObject* pObject, BOOL bInitialLock) : m_pObject(pObject), m_bAcquired(FALSE) {
    if (bInitialLock)
        Lock();
}

CSingleLock::~CSingleLock() { Unlock(); }

BOOL CSingleLock::Lock(DWORD dwTimeOut) {
    m_bAcquired = m_pObject->Lock(dwTimeOut);
    return m_bAcquired;
}

BOOL CSingleLock::Unlock() {
    if (m_bAcquired)
        m_bAcquired = !m_pObject->Unlock();
    return !m_bAcquired;
}

BOOL CSingleLock::Unlock(LONG lCount, LPLONG lPrevCount) {
    if (m_bAcquired)
        m_bAcquired = !m_pObject->Unlock(lCount, lPrevCount);
    return !m_bAcquired;
}

CMultiLock::CMultiLock(CSyncObject* ppObjects[], DWORD dwCount, BOOL bInitialLock)
    : m_objects(ppObjects, ppObjects + dwCount), m_locked(dwCount, false) {
    if (bInitialLock)
        Lock();
}

CMultiLock::~CMultiLock() { Unlock(); }

DWORD CMultiLock::Lock(DWORD dwTimeOut, BOOL bWaitForAll, DWORD) {
    std::vector<HANDLE> handles;
    std::vector<size_t> owners;
    for (size_t i = 0; i < m_objects.size(); ++i) {
        if (m_objects[i]->m_hObject) {
            handles.push_back(m_objects[i]->m_hObject);
            owners.push_back(i);
        } else if (bWaitForAll && !m_locked[i]) {
            // Critical sections have no kernel handle; they are entered directly.
            m_locked[i] = m_objects[i]->Lock(dwTimeOut) != FALSE;
        }
    }
    if (handles.empty())
        return WAIT_OBJECT_0;
    DWORD r = WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), bWaitForAll, dwTimeOut);
    if (r < WAIT_OBJECT_0 + handles.size()) {
        if (bWaitForAll) {
            for (size_t owner : owners)
                m_locked[owner] = true;
        } else {
            size_t owner = owners[r - WAIT_OBJECT_0];
            m_locked[owner] = true;
            return static_cast<DWORD>(WAIT_OBJECT_0 + owner);
        }
    }
    return r;
}

BOOL CMultiLock::Unlock() {
    for (size_t i = 0; i < m_objects.size(); ++i)
        if (m_locked[i]) {
            m_objects[i]->Unlock();
            m_locked[i] = false;
        }
    return TRUE;
}
