#include "afx.h"
#include "runtime.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif

namespace mfcwx {
namespace {

enum class Kind { Thread, Event, Mutex, Semaphore, Process, ProcessThread, File };

struct Object {
    explicit Object(Kind k) : kind(k) {}
    virtual ~Object() = default;
    Kind kind;
    int refs = 1;
    int handles = 1;
    int waiters = 0;
    std::string name;
};

struct ThreadObj : Object {
    ThreadObj() : Object(Kind::Thread) {}
    std::function<DWORD()> body;
    DWORD id = 0;
    DWORD exitCode = STILL_ACTIVE;
    bool started = false;
    bool finished = false;
    bool zombie = false;
    int suspendCount = 0;
    int priority = THREAD_PRIORITY_NORMAL;
    size_t stackSize = 0;
};

struct EventObj : Object {
    EventObj(bool m, bool s) : Object(Kind::Event), manual(m), signaled(s) {}
    bool manual;
    bool signaled;
    unsigned pulseGen = 0;
};

struct MutexObj : Object {
    MutexObj() : Object(Kind::Mutex) {}
    DWORD owner = 0;
    int count = 0;
};

struct SemaphoreObj : Object {
    SemaphoreObj(LONG c, LONG m) : Object(Kind::Semaphore), count(c), max(m) {}
    LONG count;
    LONG max;
};

struct ProcessObj : Object {
    ProcessObj() : Object(Kind::Process) {}
    pid_t pid = 0;
    DWORD exitCode = STILL_ACTIVE;
    bool exited = false;
};

void Release(Object* o);

struct ProcessThreadObj : Object {
    explicit ProcessThreadObj(ProcessObj* p) : Object(Kind::ProcessThread), process(p) {}
    ~ProcessThreadObj() override { Release(process); }
    ProcessObj* process;
};

struct FileObj : Object {
    FileObj(int f, std::string p, bool o) : Object(Kind::File), fd(f), path(std::move(p)), owns(o) {}
    ~FileObj() override {
        if (owns && fd >= 0)
            close(fd);
    }
    int fd;
    std::string path;
    bool owns;
};

// Kernel state lives for the whole process; detached threads may still use it during exit.
std::mutex& Lock() {
    static std::mutex* m = new std::mutex;
    return *m;
}
std::condition_variable& Cv() {
    static std::condition_variable* cv = new std::condition_variable;
    return *cv;
}
std::unordered_set<Object*>& Objects() {
    static auto* s = new std::unordered_set<Object*>;
    return *s;
}
std::map<std::string, Object*>& Names() {
    static auto* m = new std::map<std::string, Object*>;
    return *m;
}
std::deque<ThreadObj*>& Graveyard() {
    static auto* d = new std::deque<ThreadObj*>;
    return *d;
}

constexpr size_t kGraveyardSize = 256;
constexpr size_t kMinStackSize = 16 * 1024 * 1024;
const HANDLE kCurrentThread = reinterpret_cast<HANDLE>(static_cast<intptr_t>(-2));
const HANDLE kCurrentProcess = reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1));

thread_local DWORD t_lastError = 0;
thread_local DWORD t_threadId = 0;
thread_local ThreadObj* t_self = nullptr;
std::atomic<DWORD> g_nextThreadId{0x104};

DWORD NewThreadId() { return g_nextThreadId.fetch_add(4); }

Object* LookupLocked(HANDLE h) {
    if (!h || h == kCurrentThread || h == kCurrentProcess)
        return nullptr;
    auto* o = static_cast<Object*>(h);
    return Objects().count(o) ? o : nullptr;
}

template <class T>
T* LookupLocked(HANDLE h, Kind kind) {
    Object* o = LookupLocked(h);
    return o && o->kind == kind ? static_cast<T*>(o) : nullptr;
}

void Register(Object* o) {
    std::lock_guard<std::mutex> lk(Lock());
    Objects().insert(o);
}

void Release(Object* o) {
    if (!o)
        return;
    std::vector<Object*> doomed;
    {
        std::lock_guard<std::mutex> lk(Lock());
        if (--o->refs > 0)
            return;
        if (o->kind == Kind::Thread) {
            // Freed thread objects stay addressable for a while so that waiting on a stale
            // thread handle (MFC auto-delete pattern) still reports a finished thread.
            auto* t = static_cast<ThreadObj*>(o);
            t->zombie = true;
            Graveyard().push_back(t);
            while (Graveyard().size() > kGraveyardSize) {
                ThreadObj* old = Graveyard().front();
                Graveyard().pop_front();
                Objects().erase(old);
                doomed.push_back(old);
            }
        } else {
            Objects().erase(o);
            if (!o->name.empty()) {
                auto it = Names().find(o->name);
                if (it != Names().end() && it->second == o)
                    Names().erase(it);
            }
            doomed.push_back(o);
        }
    }
    for (Object* d : doomed)
        delete d;
}

bool IsReady(Object* o, DWORD self, unsigned startGen) {
    switch (o->kind) {
    case Kind::Thread:
        return static_cast<ThreadObj*>(o)->finished;
    case Kind::Event: {
        auto* e = static_cast<EventObj*>(o);
        return e->signaled || (e->manual && e->pulseGen != startGen);
    }
    case Kind::Mutex: {
        auto* m = static_cast<MutexObj*>(o);
        return m->owner == 0 || m->owner == self;
    }
    case Kind::Semaphore:
        return static_cast<SemaphoreObj*>(o)->count > 0;
    case Kind::Process:
        return static_cast<ProcessObj*>(o)->exited;
    case Kind::ProcessThread:
        return static_cast<ProcessThreadObj*>(o)->process->exited;
    case Kind::File:
        return true;
    }
    return false;
}

void AcquireLocked(Object* o, DWORD self) {
    switch (o->kind) {
    case Kind::Event: {
        auto* e = static_cast<EventObj*>(o);
        if (!e->manual)
            e->signaled = false;
        break;
    }
    case Kind::Mutex: {
        auto* m = static_cast<MutexObj*>(o);
        m->owner = self;
        ++m->count;
        break;
    }
    case Kind::Semaphore:
        --static_cast<SemaphoreObj*>(o)->count;
        break;
    default:
        break;
    }
}

void CheckSuspendLocked(std::unique_lock<std::mutex>& lk) {
    ThreadObj* self = t_self;
    if (!self)
        return;
    while (self->suspendCount > 0)
        Cv().wait(lk);
}

void CheckSuspend() {
    if (!t_self)
        return;
    std::unique_lock<std::mutex> lk(Lock());
    CheckSuspendLocked(lk);
}

void* ThreadMain(void* arg) {
    auto* t = static_cast<ThreadObj*>(arg);
    t_self = t;
    t_threadId = t->id;
    DWORD code = 0;
    try {
        code = t->body();
    } catch (ThreadExit& e) {
        code = e.code;
    } catch (CException* e) {
        char msg[512] = "";
        e->GetErrorMessage(msg, sizeof msg);
        fprintf(stderr, "mfcwx: uncaught MFC exception in thread %u: %s\n", static_cast<unsigned>(t->id), msg);
        e->Delete();
        code = static_cast<DWORD>(-1);
    }
    t->body = nullptr;
    {
        std::lock_guard<std::mutex> lk(Lock());
        t->exitCode = code;
        t->finished = true;
    }
    Cv().notify_all();
    t_self = nullptr;
    Release(t);
    return nullptr;
}

bool Launch(ThreadObj* t) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    size_t stack = std::max(t->stackSize, kMinStackSize);
    long page = sysconf(_SC_PAGESIZE);
    if (page > 0)
        stack = (stack + static_cast<size_t>(page) - 1) / static_cast<size_t>(page) * static_cast<size_t>(page);
    pthread_attr_setstacksize(&attr, stack);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t thread;
    int rc = pthread_create(&thread, &attr, ThreadMain, t);
    pthread_attr_destroy(&attr);
    return rc == 0;
}

void FailLaunch(ThreadObj* t) {
    {
        std::lock_guard<std::mutex> lk(Lock());
        t->body = nullptr;
        t->exitCode = static_cast<DWORD>(-1);
        t->finished = true;
    }
    Cv().notify_all();
    Release(t);
}

template <class T, class Make>
HANDLE CreateNamed(LPCSTR name, Kind kind, Make make) {
    std::string key = name ? name : "";
    if (!key.empty()) {
        std::lock_guard<std::mutex> lk(Lock());
        auto it = Names().find(key);
        if (it != Names().end()) {
            if (it->second->kind != kind) {
                t_lastError = ERROR_INVALID_HANDLE;
                return nullptr;
            }
            ++it->second->handles;
            ++it->second->refs;
            t_lastError = ERROR_ALREADY_EXISTS;
            return it->second;
        }
    }
    T* o = make();
    o->name = key;
    {
        std::lock_guard<std::mutex> lk(Lock());
        Objects().insert(o);
        if (!key.empty())
            Names()[key] = o;
    }
    t_lastError = 0;
    return o;
}

DWORD WaitImpl(DWORD count, const HANDLE* handles, bool waitAll, DWORD ms) {
    if (count == 0 || count > 64 || !handles) {
        t_lastError = 87;
        return WAIT_FAILED;
    }
    DWORD self = GetCurrentThreadId();
    std::vector<Object*> objs(count);
    std::vector<unsigned> gens(count, 0);
    std::unique_lock<std::mutex> lk(Lock());
    CheckSuspendLocked(lk);
    for (DWORD i = 0; i < count; ++i) {
        Object* o = LookupLocked(handles[i]);
        if (!o) {
            for (DWORD j = 0; j < i; ++j)
                --objs[j]->waiters;
            lk.unlock();
            for (DWORD j = 0; j < i; ++j)
                Release(objs[j]);
            t_lastError = ERROR_INVALID_HANDLE;
            return WAIT_FAILED;
        }
        ++o->refs;
        ++o->waiters;
        objs[i] = o;
        if (o->kind == Kind::Event)
            gens[i] = static_cast<EventObj*>(o)->pulseGen;
    }
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms == INFINITE ? 0 : ms);
    DWORD result = WAIT_TIMEOUT;
    for (bool timedOut = false;;) {
        if (waitAll) {
            bool all = true;
            for (DWORD i = 0; i < count && all; ++i)
                all = IsReady(objs[i], self, gens[i]);
            if (all) {
                for (DWORD i = 0; i < count; ++i)
                    AcquireLocked(objs[i], self);
                result = WAIT_OBJECT_0;
                break;
            }
        } else {
            bool done = false;
            for (DWORD i = 0; i < count; ++i)
                if (IsReady(objs[i], self, gens[i])) {
                    AcquireLocked(objs[i], self);
                    result = WAIT_OBJECT_0 + i;
                    done = true;
                    break;
                }
            if (done)
                break;
        }
        if (ms == 0 || timedOut)
            break;
        if (ms == INFINITE)
            Cv().wait(lk);
        else if (Cv().wait_until(lk, deadline) == std::cv_status::timeout)
            timedOut = true;
    }
    for (Object* o : objs)
        --o->waiters;
    lk.unlock();
    Cv().notify_all();
    for (Object* o : objs)
        Release(o);
    return result;
}

std::vector<std::string> SplitCommandLine(const char* cmd) {
    std::vector<std::string> args;
    if (!cmd)
        return args;
    const char* p = cmd;
    while (*p == ' ' || *p == '\t')
        ++p;
    if (!*p)
        return args;
    std::string prog;
    if (*p == '"') {
        ++p;
        while (*p && *p != '"')
            prog += *p++;
        if (*p == '"')
            ++p;
    } else {
        while (*p && *p != ' ' && *p != '\t')
            prog += *p++;
    }
    args.push_back(prog);
    for (;;) {
        while (*p == ' ' || *p == '\t')
            ++p;
        if (!*p)
            break;
        std::string arg;
        bool quoted = false;
        while (*p && (quoted || (*p != ' ' && *p != '\t'))) {
            if (*p == '\\') {
                size_t n = 0;
                while (*p == '\\') {
                    ++n;
                    ++p;
                }
                if (*p == '"') {
                    arg.append(n / 2, '\\');
                    if (n % 2) {
                        arg += '"';
                        ++p;
                    }
                } else {
                    arg.append(n, '\\');
                }
            } else if (*p == '"') {
                if (quoted && p[1] == '"') {
                    arg += '"';
                    p += 2;
                } else {
                    quoted = !quoted;
                    ++p;
                }
            } else {
                arg += *p++;
            }
        }
        args.push_back(arg);
    }
    return args;
}

bool IsExecutableFile(const std::string& path) {
    struct stat st;
    return !path.empty() && stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && access(path.c_str(), X_OK) == 0;
}

bool EndsWithExe(const std::string& s) { return s.size() > 4 && strcasecmp(s.c_str() + s.size() - 4, ".exe") == 0; }

std::string ResolveProgram(const std::string& appName) {
    std::vector<std::string> candidates{appName};
    if (EndsWithExe(appName))
        candidates.push_back(appName.substr(0, appName.size() - 4));
    for (const std::string& name : candidates) {
        if (name.find('\\') != std::string::npos || name.find('/') != std::string::npos) {
            std::string native = FsPath(name.c_str());
            if (IsExecutableFile(native))
                return native;
            continue;
        }
        std::string native = NativePath(name.c_str());
        std::vector<std::string> dirs;
        std::string exe = ExecutablePath();
        size_t slash = exe.rfind('/');
        if (slash != std::string::npos)
            dirs.push_back(exe.substr(0, slash));
        const char* path = getenv("PATH");
        std::string all = path ? path : "/usr/local/bin:/usr/bin:/bin";
        size_t start = 0;
        for (;;) {
            size_t colon = all.find(':', start);
            std::string dir = all.substr(start, colon == std::string::npos ? std::string::npos : colon - start);
            dirs.push_back(dir.empty() ? "." : dir);
            if (colon == std::string::npos)
                break;
            start = colon + 1;
        }
        dirs.push_back(".");
        for (const std::string& dir : dirs) {
            std::string full = dir + "/" + native;
            if (IsExecutableFile(full))
                return full;
        }
    }
    return std::string();
}

std::string ConvertArgument(const std::string& arg) {
    bool drive = arg.size() >= 3 && arg[1] == ':' && (arg[2] == '\\' || arg[2] == '/') && isalpha(static_cast<unsigned char>(arg[0]));
    if (arg.find('\\') != std::string::npos || drive)
        return NativePath(arg.c_str());
    return AnsiToUtf8(arg.c_str());
}

void ClearCloexec(int fd) {
    int flags = fcntl(fd, F_GETFD);
    if (flags >= 0)
        fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC);
}

} // namespace

HANDLE StartThread(std::function<DWORD()> body, DWORD flags, size_t stackSize, DWORD* threadId) {
    auto* t = new ThreadObj;
    t->body = std::move(body);
    t->id = NewThreadId();
    t->stackSize = stackSize;
    if (threadId)
        *threadId = t->id;
    if (flags & CREATE_SUSPENDED) {
        t->suspendCount = 1;
        Register(t);
        return t;
    }
    t->started = true;
    t->refs = 2;
    Register(t);
    if (!Launch(t)) {
        FailLaunch(t);
        CloseHandle(t);
        t_lastError = ERROR_NOT_ENOUGH_MEMORY;
        return nullptr;
    }
    return t;
}

bool IsProcessMainThread() {
#if defined(__APPLE__)
    return pthread_main_np() != 0;
#elif defined(__linux__)
    return static_cast<pid_t>(syscall(SYS_gettid)) == getpid();
#else
    static const pthread_t mainThread = pthread_self();
    return pthread_equal(mainThread, pthread_self()) != 0;
#endif
}

HANDLE HandleFromFd(int fd, const std::string& nativePath, bool owns) {
    if (fd < 0)
        return INVALID_HANDLE_VALUE;
    auto* f = new FileObj(fd, nativePath, owns);
    Register(f);
    return f;
}

int FdFromHandle(HANDLE h) {
    std::lock_guard<std::mutex> lk(Lock());
    auto* f = LookupLocked<FileObj>(h, Kind::File);
    return f ? f->fd : -1;
}

} // namespace mfcwx

using namespace mfcwx;

DWORD GetLastError(void) { return t_lastError; }

void SetLastError(DWORD err) { t_lastError = err; }

DWORD GetCurrentThreadId(void) {
    if (!t_threadId)
        t_threadId = NewThreadId();
    return t_threadId;
}

DWORD GetCurrentProcessId(void) { return static_cast<DWORD>(getpid()); }

HANDLE GetCurrentProcess(void) { return kCurrentProcess; }

HANDLE GetCurrentThread(void) { return kCurrentThread; }

void Sleep(DWORD ms) {
    CheckSuspend();
    if (ms == 0) {
        sched_yield();
    } else if (ms == INFINITE) {
        for (;;)
            pause();
    } else {
        struct timespec ts;
        ts.tv_sec = ms / 1000;
        ts.tv_nsec = static_cast<long>(ms % 1000) * 1000000L;
        while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
        }
    }
    CheckSuspend();
}

DWORD WaitForSingleObject(HANDLE h, DWORD ms) { return WaitImpl(1, &h, false, ms); }

DWORD WaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL waitAll, DWORD ms) {
    return WaitImpl(count, handles, waitAll != FALSE, ms);
}

BOOL CloseHandle(HANDLE h) {
    if (h == kCurrentThread || h == kCurrentProcess)
        return TRUE;
    Object* o;
    {
        std::lock_guard<std::mutex> lk(Lock());
        o = LookupLocked(h);
        if (!o || o->handles <= 0) {
            t_lastError = ERROR_INVALID_HANDLE;
            return FALSE;
        }
        if (--o->handles == 0 && !o->name.empty()) {
            auto it = Names().find(o->name);
            if (it != Names().end() && it->second == o)
                Names().erase(it);
            o->name.clear();
        }
    }
    Release(o);
    return TRUE;
}

BOOL GetExitCodeThread(HANDLE thread, LPDWORD code) {
    std::lock_guard<std::mutex> lk(Lock());
    if (thread == kCurrentThread) {
        if (code)
            *code = STILL_ACTIVE;
        return TRUE;
    }
    if (auto* t = LookupLocked<ThreadObj>(thread, Kind::Thread)) {
        if (code)
            *code = t->finished ? t->exitCode : STILL_ACTIVE;
        return TRUE;
    }
    if (auto* pt = LookupLocked<ProcessThreadObj>(thread, Kind::ProcessThread)) {
        if (code)
            *code = pt->process->exited ? pt->process->exitCode : STILL_ACTIVE;
        return TRUE;
    }
    t_lastError = ERROR_INVALID_HANDLE;
    return FALSE;
}

BOOL GetExitCodeProcess(HANDLE process, LPDWORD code) {
    std::lock_guard<std::mutex> lk(Lock());
    if (process == kCurrentProcess) {
        if (code)
            *code = STILL_ACTIVE;
        return TRUE;
    }
    auto* p = LookupLocked<ProcessObj>(process, Kind::Process);
    if (!p) {
        t_lastError = ERROR_INVALID_HANDLE;
        return FALSE;
    }
    if (code)
        *code = p->exited ? p->exitCode : STILL_ACTIVE;
    return TRUE;
}

BOOL TerminateThread(HANDLE thread, DWORD code) {
    // Only a thread that has not started yet can be stopped safely.
    {
        std::lock_guard<std::mutex> lk(Lock());
        auto* t = LookupLocked<ThreadObj>(thread, Kind::Thread);
        if (!t) {
            t_lastError = ERROR_INVALID_HANDLE;
            return FALSE;
        }
        if (t->started || t->finished) {
            t_lastError = 50;
            return FALSE;
        }
        t->started = true;
        t->finished = true;
        t->exitCode = code;
        t->body = nullptr;
    }
    Cv().notify_all();
    return TRUE;
}

BOOL TerminateProcess(HANDLE process, UINT code) {
    if (process == kCurrentProcess)
        _exit(static_cast<int>(code));
    pid_t pid;
    {
        std::lock_guard<std::mutex> lk(Lock());
        auto* p = LookupLocked<ProcessObj>(process, Kind::Process);
        if (!p) {
            t_lastError = ERROR_INVALID_HANDLE;
            return FALSE;
        }
        if (p->exited)
            return TRUE;
        pid = p->pid;
    }
    if (kill(pid, SIGKILL) != 0) {
        SetLastErrorFromErrno(errno);
        return FALSE;
    }
    return TRUE;
}

HANDLE CreateThread(LPSECURITY_ATTRIBUTES, SIZE_T stack, LPTHREAD_START_ROUTINE fn, LPVOID param, DWORD flags,
                    LPDWORD threadId) {
    if (!fn) {
        t_lastError = 87;
        return nullptr;
    }
    DWORD id = 0;
    HANDLE h = StartThread([fn, param]() { return fn(param); }, flags, stack, &id);
    if (threadId)
        *threadId = id;
    return h;
}

DWORD ResumeThread(HANDLE thread) {
    ThreadObj* launch = nullptr;
    DWORD previous;
    {
        std::lock_guard<std::mutex> lk(Lock());
        auto* t = LookupLocked<ThreadObj>(thread, Kind::Thread);
        if (!t || t->zombie) {
            t_lastError = ERROR_INVALID_HANDLE;
            return static_cast<DWORD>(-1);
        }
        previous = static_cast<DWORD>(t->suspendCount);
        if (t->suspendCount > 0)
            --t->suspendCount;
        if (t->suspendCount == 0 && !t->started) {
            t->started = true;
            ++t->refs;
            launch = t;
        }
    }
    Cv().notify_all();
    if (launch && !Launch(launch))
        FailLaunch(launch);
    return previous;
}

DWORD SuspendThread(HANDLE thread) {
    // Suspension is cooperative: a running thread stops at its next Sleep or wait.
    std::unique_lock<std::mutex> lk(Lock());
    ThreadObj* t = thread == kCurrentThread ? t_self : LookupLocked<ThreadObj>(thread, Kind::Thread);
    if (!t || t->zombie || t->finished) {
        t_lastError = ERROR_INVALID_HANDLE;
        return static_cast<DWORD>(-1);
    }
    DWORD previous = static_cast<DWORD>(t->suspendCount++);
    if (t == t_self)
        CheckSuspendLocked(lk);
    return previous;
}

void ExitThread(DWORD code) {
    if (t_self)
        throw ThreadExit{code, true};
    pthread_exit(nullptr);
}

BOOL SetThreadPriority(HANDLE thread, int priority) {
    std::lock_guard<std::mutex> lk(Lock());
    ThreadObj* t = thread == kCurrentThread ? t_self : LookupLocked<ThreadObj>(thread, Kind::Thread);
    if (t)
        t->priority = priority;
    else if (thread != kCurrentThread) {
        t_lastError = ERROR_INVALID_HANDLE;
        return FALSE;
    }
    return TRUE;
}

int GetThreadPriority(HANDLE thread) {
    std::lock_guard<std::mutex> lk(Lock());
    ThreadObj* t = thread == kCurrentThread ? t_self : LookupLocked<ThreadObj>(thread, Kind::Thread);
    return t ? t->priority : THREAD_PRIORITY_NORMAL;
}

BOOL SetPriorityClass(HANDLE, DWORD) { return TRUE; }

HANDLE CreateEventA(LPSECURITY_ATTRIBUTES, BOOL manualReset, BOOL initialState, LPCSTR name) {
    return CreateNamed<EventObj>(name, Kind::Event,
                                 [&]() { return new EventObj(manualReset != FALSE, initialState != FALSE); });
}

BOOL SetEvent(HANDLE h) {
    {
        std::lock_guard<std::mutex> lk(Lock());
        auto* e = LookupLocked<EventObj>(h, Kind::Event);
        if (!e) {
            t_lastError = ERROR_INVALID_HANDLE;
            return FALSE;
        }
        e->signaled = true;
    }
    Cv().notify_all();
    return TRUE;
}

BOOL ResetEvent(HANDLE h) {
    std::lock_guard<std::mutex> lk(Lock());
    auto* e = LookupLocked<EventObj>(h, Kind::Event);
    if (!e) {
        t_lastError = ERROR_INVALID_HANDLE;
        return FALSE;
    }
    e->signaled = false;
    return TRUE;
}

BOOL PulseEvent(HANDLE h) {
    {
        std::lock_guard<std::mutex> lk(Lock());
        auto* e = LookupLocked<EventObj>(h, Kind::Event);
        if (!e) {
            t_lastError = ERROR_INVALID_HANDLE;
            return FALSE;
        }
        if (e->manual) {
            ++e->pulseGen;
            e->signaled = false;
        } else if (e->waiters > 0) {
            e->signaled = true;
        }
    }
    Cv().notify_all();
    return TRUE;
}

HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES, BOOL initialOwner, LPCSTR name) {
    DWORD self = GetCurrentThreadId();
    return CreateNamed<MutexObj>(name, Kind::Mutex, [&]() {
        auto* m = new MutexObj;
        if (initialOwner) {
            m->owner = self;
            m->count = 1;
        }
        return m;
    });
}

BOOL ReleaseMutex(HANDLE h) {
    DWORD self = GetCurrentThreadId();
    {
        std::lock_guard<std::mutex> lk(Lock());
        auto* m = LookupLocked<MutexObj>(h, Kind::Mutex);
        if (!m) {
            t_lastError = ERROR_INVALID_HANDLE;
            return FALSE;
        }
        if (m->owner != self || m->count <= 0) {
            t_lastError = 288;
            return FALSE;
        }
        if (--m->count == 0)
            m->owner = 0;
    }
    Cv().notify_all();
    return TRUE;
}

HANDLE CreateSemaphoreA(LPSECURITY_ATTRIBUTES, LONG initial, LONG max, LPCSTR name) {
    if (max <= 0 || initial < 0 || initial > max) {
        t_lastError = 87;
        return nullptr;
    }
    return CreateNamed<SemaphoreObj>(name, Kind::Semaphore, [&]() { return new SemaphoreObj(initial, max); });
}

BOOL ReleaseSemaphore(HANDLE h, LONG count, LPLONG previous) {
    {
        std::lock_guard<std::mutex> lk(Lock());
        auto* s = LookupLocked<SemaphoreObj>(h, Kind::Semaphore);
        if (!s) {
            t_lastError = ERROR_INVALID_HANDLE;
            return FALSE;
        }
        if (count <= 0 || count > s->max - s->count) {
            t_lastError = 298;
            return FALSE;
        }
        if (previous)
            *previous = s->count;
        s->count += count;
    }
    Cv().notify_all();
    return TRUE;
}

static std::recursive_mutex* CriticalSectionImpl(LPCRITICAL_SECTION cs) {
    auto* impl = static_cast<std::recursive_mutex*>(__atomic_load_n(&cs->impl, __ATOMIC_ACQUIRE));
    if (impl)
        return impl;
    auto* fresh = new std::recursive_mutex;
    void* expected = nullptr;
    if (__atomic_compare_exchange_n(&cs->impl, &expected, static_cast<void*>(fresh), false, __ATOMIC_ACQ_REL,
                                    __ATOMIC_ACQUIRE))
        return fresh;
    delete fresh;
    return static_cast<std::recursive_mutex*>(expected);
}

void InitializeCriticalSection(LPCRITICAL_SECTION cs) { cs->impl = new std::recursive_mutex; }

void DeleteCriticalSection(LPCRITICAL_SECTION cs) {
    delete static_cast<std::recursive_mutex*>(cs->impl);
    cs->impl = nullptr;
}

void EnterCriticalSection(LPCRITICAL_SECTION cs) { CriticalSectionImpl(cs)->lock(); }

void LeaveCriticalSection(LPCRITICAL_SECTION cs) { CriticalSectionImpl(cs)->unlock(); }

BOOL TryEnterCriticalSection(LPCRITICAL_SECTION cs) { return CriticalSectionImpl(cs)->try_lock() ? TRUE : FALSE; }

LONG InterlockedIncrement(LONG volatile* p) { return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST); }

LONG InterlockedDecrement(LONG volatile* p) { return __atomic_sub_fetch(p, 1, __ATOMIC_SEQ_CST); }

LONG InterlockedExchange(LONG volatile* p, LONG v) { return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST); }

LONG InterlockedExchangeAdd(LONG volatile* p, LONG v) { return __atomic_fetch_add(p, v, __ATOMIC_SEQ_CST); }

LONG InterlockedCompareExchange(LONG volatile* p, LONG exchange, LONG comparand) {
    __atomic_compare_exchange_n(p, &comparand, exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return comparand;
}

BOOL CreateProcessA(LPCSTR app, LPSTR cmdLine, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD,
                    LPVOID env, LPCSTR curDir, LPSTARTUPINFO si, LPPROCESS_INFORMATION pi) {
    std::vector<std::string> args = SplitCommandLine(cmdLine);
    std::string program = app && *app ? std::string(app) : (args.empty() ? std::string() : args[0]);
    if (program.empty()) {
        t_lastError = 87;
        return FALSE;
    }
    if (args.empty())
        args.push_back(program);
    std::string path = ResolveProgram(program);
    if (path.empty()) {
        t_lastError = ERROR_FILE_NOT_FOUND;
        return FALSE;
    }
    std::vector<std::string> argStore;
    argStore.push_back(path);
    for (size_t i = 1; i < args.size(); ++i)
        argStore.push_back(ConvertArgument(args[i]));
    std::vector<char*> argv;
    for (std::string& a : argStore)
        argv.push_back(&a[0]);
    argv.push_back(nullptr);

    std::vector<std::string> envStore;
    std::vector<char*> envp;
    if (env) {
        for (const char* e = static_cast<const char*>(env); *e; e += strlen(e) + 1)
            envStore.push_back(AnsiToUtf8(e));
        for (std::string& e : envStore)
            envp.push_back(&e[0]);
        envp.push_back(nullptr);
    }
    std::string cwd = curDir && *curDir ? FsPath(curDir) : std::string();

    int redirect[3] = {-1, -1, -1};
    int devNull = -1;
    if (si && (si->dwFlags & STARTF_USESTDHANDLES)) {
        HANDLE stdHandles[3] = {si->hStdInput, si->hStdOutput, si->hStdError};
        for (int i = 0; i < 3; ++i)
            redirect[i] = stdHandles[i] ? FdFromHandle(stdHandles[i]) : -1;
        if (!si->hStdInput) {
            devNull = open("/dev/null", O_RDONLY | O_CLOEXEC);
            redirect[0] = devNull;
        }
    }

    int errPipe[2];
    if (pipe(errPipe) != 0) {
        SetLastErrorFromErrno(errno);
        if (devNull >= 0)
            close(devNull);
        return FALSE;
    }
    fcntl(errPipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(errPipe[1], F_SETFD, FD_CLOEXEC);

    pid_t pid = fork();
    if (pid == 0) {
        sigset_t none;
        sigemptyset(&none);
        pthread_sigmask(SIG_SETMASK, &none, nullptr);
        signal(SIGPIPE, SIG_DFL);
        for (int i = 0; i < 3; ++i) {
            if (redirect[i] < 0)
                continue;
            if (redirect[i] == i)
                ClearCloexec(i);
            else
                dup2(redirect[i], i);
        }
        int err = 0;
        if (!cwd.empty() && chdir(cwd.c_str()) != 0)
            err = errno;
        if (!err) {
            if (env)
                execve(path.c_str(), argv.data(), envp.data());
            else
                execv(path.c_str(), argv.data());
            err = errno;
        }
        ssize_t ignored = write(errPipe[1], &err, sizeof err);
        (void)ignored;
        _exit(127);
    }
    close(errPipe[1]);
    if (devNull >= 0)
        close(devNull);
    if (pid < 0) {
        int err = errno;
        close(errPipe[0]);
        SetLastErrorFromErrno(err);
        return FALSE;
    }
    int childErr = 0;
    ssize_t n;
    while ((n = read(errPipe[0], &childErr, sizeof childErr)) < 0 && errno == EINTR) {
    }
    close(errPipe[0]);
    if (n == static_cast<ssize_t>(sizeof childErr)) {
        int status;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        }
        SetLastErrorFromErrno(childErr);
        return FALSE;
    }

    auto* proc = new ProcessObj;
    proc->pid = pid;
    proc->refs = 3;
    auto* thread = new ProcessThreadObj(proc);
    Register(proc);
    Register(thread);
    std::thread([proc]() {
        int status = 0;
        pid_t r;
        while ((r = waitpid(proc->pid, &status, 0)) < 0 && errno == EINTR) {
        }
        DWORD code = 0;
        if (r == proc->pid) {
            if (WIFEXITED(status))
                code = static_cast<DWORD>(WEXITSTATUS(status));
            else if (WIFSIGNALED(status))
                code = static_cast<DWORD>(128 + WTERMSIG(status));
        }
        {
            std::lock_guard<std::mutex> lk(Lock());
            proc->exitCode = code;
            proc->exited = true;
        }
        Cv().notify_all();
        Release(proc);
    }).detach();

    if (pi) {
        pi->hProcess = proc;
        pi->hThread = thread;
        pi->dwProcessId = static_cast<DWORD>(pid);
        pi->dwThreadId = NewThreadId();
    } else {
        CloseHandle(proc);
        CloseHandle(thread);
    }
    t_lastError = 0;
    return TRUE;
}

UINT WinExec(LPCSTR cmdLine, UINT) {
    STARTUPINFO si;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    PROCESS_INFORMATION pi;
    std::string cmd = cmdLine ? cmdLine : "";
    if (!CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        DWORD err = GetLastError();
        return err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND ? err : 11;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 33;
}

void ExitProcess(UINT code) { exit(static_cast<int>(code)); }

uintptr_t mfcwx_beginthread(void (*fn)(void*), unsigned stack, void* arg) {
    if (!fn)
        return static_cast<uintptr_t>(-1);
    HANDLE h = StartThread([fn, arg]() -> DWORD {
        fn(arg);
        return 0;
    }, 0, stack, nullptr);
    if (!h)
        return static_cast<uintptr_t>(-1);
    // _beginthread handles are closed by the runtime when the thread ends.
    CloseHandle(h);
    return reinterpret_cast<uintptr_t>(h);
}

uintptr_t mfcwx_beginthreadex(void*, unsigned stack, unsigned (*fn)(void*), void* arg, unsigned flags,
                              unsigned* threadId) {
    if (!fn)
        return 0;
    DWORD id = 0;
    HANDLE h = StartThread([fn, arg]() -> DWORD { return fn(arg); }, flags, stack, &id);
    if (threadId)
        *threadId = id;
    return reinterpret_cast<uintptr_t>(h);
}

void mfcwx_endthread(unsigned code) { ExitThread(code); }

void mfcwx_sleep_ms(unsigned long ms) { Sleep(static_cast<DWORD>(ms)); }
