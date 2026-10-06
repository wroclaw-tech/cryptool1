#include "afxwin.h"
#include "runtime.h"

IMPLEMENT_DYNAMIC(CWinThread, CCmdTarget)

namespace {

thread_local CWinThread* t_currentThread = nullptr;

DWORD RunThread(CWinThread* pThread) {
    t_currentThread = pThread;
    // Like MFC, a secondary thread sees the application's main window as its own.
    CWnd* inherited = nullptr;
    if (!pThread->m_pMainWnd) {
        CWinApp* app = AfxGetApp();
        if (app && app != pThread && app->m_pMainWnd)
            inherited = pThread->m_pMainWnd = app->m_pMainWnd;
    }
    DWORD code = 0;
    bool deleteThread = true;
    try {
        if (pThread->m_pfnThreadProc)
            code = pThread->m_pfnThreadProc(pThread->m_pThreadParams);
        else if (!pThread->InitInstance())
            code = static_cast<DWORD>(pThread->ExitInstance());
        else
            code = static_cast<DWORD>(pThread->Run());
    } catch (mfcwx::ThreadExit& e) {
        code = e.code;
        deleteThread = e.deleteThread;
    }
    t_currentThread = nullptr;
    if (deleteThread && pThread->m_bAutoDelete) {
        delete pThread;
    } else if (inherited && pThread->m_pMainWnd == inherited) {
        pThread->m_pMainWnd = nullptr;
    }
    return code;
}

CWinThread* StartWinThread(CWinThread* pThread, int nPriority, UINT nStackSize, DWORD dwCreateFlags,
                           LPSECURITY_ATTRIBUTES lpSecurityAttrs) {
    if (!pThread->CreateThread(dwCreateFlags | CREATE_SUSPENDED, nStackSize, lpSecurityAttrs)) {
        pThread->m_bAutoDelete = FALSE;
        delete pThread;
        return nullptr;
    }
    pThread->SetThreadPriority(nPriority);
    if (!(dwCreateFlags & CREATE_SUSPENDED))
        pThread->ResumeThread();
    return pThread;
}

} // namespace

CWinThread::CWinThread()
    : m_pMainWnd(nullptr), m_pActiveWnd(nullptr), m_bAutoDelete(TRUE), m_hThread(nullptr), m_nThreadID(0),
      m_pfnThreadProc(nullptr), m_pThreadParams(nullptr) {
    memset(&m_msgCur, 0, sizeof m_msgCur);
}

CWinThread::CWinThread(AFX_THREADPROC pfnThreadProc, LPVOID pParam) : CWinThread() {
    m_pfnThreadProc = pfnThreadProc;
    m_pThreadParams = pParam;
}

CWinThread::~CWinThread() {
    if (m_hThread)
        CloseHandle(m_hThread);
    m_hThread = nullptr;
    if (t_currentThread == this)
        t_currentThread = nullptr;
}

BOOL CWinThread::CreateThread(DWORD dwCreateFlags, UINT nStackSize, LPSECURITY_ATTRIBUTES) {
    if (m_hThread)
        return FALSE;
    CWinThread* self = this;
    DWORD id = 0;
    // Start suspended so the members are set before an auto-deleting thread can finish.
    HANDLE h = mfcwx::StartThread([self]() { return RunThread(self); }, CREATE_SUSPENDED, nStackSize, &id);
    if (!h)
        return FALSE;
    m_hThread = h;
    m_nThreadID = id;
    if (!(dwCreateFlags & CREATE_SUSPENDED))
        ::ResumeThread(h);
    return TRUE;
}

int CWinThread::GetThreadPriority() { return ::GetThreadPriority(m_hThread); }

BOOL CWinThread::SetThreadPriority(int nPriority) { return ::SetThreadPriority(m_hThread, nPriority); }

DWORD CWinThread::SuspendThread() { return ::SuspendThread(m_hThread); }

DWORD CWinThread::ResumeThread() { return ::ResumeThread(m_hThread); }

BOOL CWinThread::PostThreadMessage(UINT, WPARAM, LPARAM) { return FALSE; }

BOOL CWinThread::InitInstance() { return FALSE; }

int CWinThread::ExitInstance() { return static_cast<int>(m_msgCur.wParam); }

int CWinThread::Run() { return ExitInstance(); }

BOOL CWinThread::PreTranslateMessage(MSG*) { return FALSE; }

BOOL CWinThread::PumpMessage() { return FALSE; }

BOOL CWinThread::OnIdle(LONG) { return FALSE; }

CWnd* CWinThread::GetMainWnd() { return m_pActiveWnd ? m_pActiveWnd : m_pMainWnd; }

BOOL CWinThread::ProcessMessageFilter(int, LPMSG) { return FALSE; }

CWinThread* AFXAPI AfxBeginThread(AFX_THREADPROC pfnThreadProc, LPVOID pParam, int nPriority, UINT nStackSize,
                                  DWORD dwCreateFlags, LPSECURITY_ATTRIBUTES lpSecurityAttrs) {
    return StartWinThread(new CWinThread(pfnThreadProc, pParam), nPriority, nStackSize, dwCreateFlags,
                          lpSecurityAttrs);
}

CWinThread* AFXAPI AfxBeginThread(CRuntimeClass* pThreadClass, int nPriority, UINT nStackSize, DWORD dwCreateFlags,
                                  LPSECURITY_ATTRIBUTES lpSecurityAttrs) {
    CObject* object = pThreadClass ? pThreadClass->CreateObject() : nullptr;
    if (!object)
        AfxThrowMemoryException();
    return StartWinThread(static_cast<CWinThread*>(object), nPriority, nStackSize, dwCreateFlags, lpSecurityAttrs);
}

void AFXAPI AfxEndThread(UINT nExitCode, BOOL bDelete) {
    if (mfcwx::IsProcessMainThread())
        exit(static_cast<int>(nExitCode));
    throw mfcwx::ThreadExit{nExitCode, bDelete != FALSE};
}

CWinThread* AFXAPI AfxGetThread() {
    if (t_currentThread)
        return t_currentThread;
    return mfcwx::IsProcessMainThread() ? AfxGetApp() : nullptr;
}
