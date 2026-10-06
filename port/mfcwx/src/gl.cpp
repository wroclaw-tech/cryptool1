#include "windows_impl.h"

#include <wx/glcanvas.h>

#include <dlfcn.h>
#include <map>

namespace mfcwx {

namespace {

// A GL canvas covering the client area of a window that asked for an OpenGL pixel format. Paint,
// mouse and keyboard input are forwarded to the window, which renders through the WGL calls.
class GLSurface : public wxGLCanvas {
public:
    GLSurface(wxWindow* owner, const int* attribs)
        : wxGLCanvas(owner, wxID_ANY, attribs, wxPoint(0, 0), owner->GetClientSize(), wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS),
          m_owner(owner) {
        Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
            wxPaintDC dc(this);
            if (IsManagedWindow(m_owner))
                DispatchMessageTo(ToHwnd(m_owner), WM_PAINT, 0, 0);
        });
        Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent&) {});
        owner->Bind(wxEVT_SIZE, [this](wxSizeEvent& e) {
            SetSize(m_owner->GetClientSize());
            e.Skip();
        });
        auto forwardMouse = [this](wxMouseEvent& e, UINT msg) {
            if (IsManagedWindow(m_owner))
                DispatchMessageTo(ToHwnd(m_owner), msg, MouseFlags(e), MAKELPARAM(e.GetX(), e.GetY()));
            e.Skip();
        };
        Bind(wxEVT_LEFT_DOWN, [forwardMouse](wxMouseEvent& e) { forwardMouse(e, WM_LBUTTONDOWN); });
        Bind(wxEVT_LEFT_UP, [forwardMouse](wxMouseEvent& e) { forwardMouse(e, WM_LBUTTONUP); });
        Bind(wxEVT_LEFT_DCLICK, [forwardMouse](wxMouseEvent& e) { forwardMouse(e, WM_LBUTTONDBLCLK); });
        Bind(wxEVT_RIGHT_DOWN, [forwardMouse](wxMouseEvent& e) { forwardMouse(e, WM_RBUTTONDOWN); });
        Bind(wxEVT_RIGHT_UP, [forwardMouse](wxMouseEvent& e) { forwardMouse(e, WM_RBUTTONUP); });
        Bind(wxEVT_MIDDLE_DOWN, [forwardMouse](wxMouseEvent& e) { forwardMouse(e, WM_MBUTTONDOWN); });
        Bind(wxEVT_MIDDLE_UP, [forwardMouse](wxMouseEvent& e) { forwardMouse(e, WM_MBUTTONUP); });
        Bind(wxEVT_MOTION, [forwardMouse](wxMouseEvent& e) { forwardMouse(e, WM_MOUSEMOVE); });
        Bind(wxEVT_MOUSEWHEEL, [this](wxMouseEvent& e) {
            int delta = e.GetWheelRotation() * WHEEL_DELTA / std::max(1, e.GetWheelDelta());
            wxPoint screen = ClientToScreen(e.GetPosition());
            if (IsManagedWindow(m_owner))
                DispatchMessageTo(ToHwnd(m_owner), WM_MOUSEWHEEL,
                                  MAKEWPARAM(MouseFlags(e), static_cast<WORD>(static_cast<short>(delta))),
                                  MAKELPARAM(screen.x, screen.y));
        });
        auto forwardKey = [this](wxKeyEvent& e, UINT msg) {
            UINT vk = VirtualKeyFromWx(e.GetKeyCode());
            if (vk && IsManagedWindow(m_owner))
                DispatchMessageTo(ToHwnd(m_owner), msg, vk, KeyLParam(e, msg == WM_KEYUP));
            e.Skip();
        };
        Bind(wxEVT_KEY_DOWN, [forwardKey](wxKeyEvent& e) { forwardKey(e, WM_KEYDOWN); });
        Bind(wxEVT_KEY_UP, [forwardKey](wxKeyEvent& e) { forwardKey(e, WM_KEYUP); });
    }

private:
    wxWindow* m_owner;
};

struct GLContextHandle {
    wxGLContext* context = nullptr;
    GLSurface* surface = nullptr;
};

std::map<wxWindow*, GLSurface*>& Surfaces() {
    static std::map<wxWindow*, GLSurface*> s;
    return s;
}

HGLRC g_current = nullptr;
HDC g_currentDC = nullptr;

wxWindow* WindowOfDC(HDC hdc) {
    CDC* dc = hdc ? CDC::FromHandle(hdc) : nullptr;
    CWnd* wnd = dc ? dc->GetWindow() : nullptr;
    return wnd && wnd->m_hWnd ? wnd->GetWx() : nullptr;
}

GLSurface* SurfaceFor(HDC hdc) {
    wxWindow* w = WindowOfDC(hdc);
    if (!w)
        return nullptr;
    auto it = Surfaces().find(w);
    return it == Surfaces().end() ? nullptr : it->second;
}

} // namespace

} // namespace mfcwx

using namespace mfcwx;

int ChoosePixelFormat(HDC, const PIXELFORMATDESCRIPTOR*) { return 1; }

BOOL SetPixelFormat(HDC hdc, int, const PIXELFORMATDESCRIPTOR* ppfd) {
    return OnMain([&]() -> BOOL {
        wxWindow* w = WindowOfDC(hdc);
        if (!w)
            return FALSE;
        if (Surfaces().count(w))
            return TRUE;
        int depth = ppfd && ppfd->cDepthBits ? ppfd->cDepthBits : 24;
        std::vector<int> attribs = {WX_GL_RGBA, WX_GL_DEPTH_SIZE, depth};
        if (!ppfd || (ppfd->dwFlags & PFD_DOUBLEBUFFER))
            attribs.push_back(WX_GL_DOUBLEBUFFER);
        attribs.push_back(0);
        auto* surface = new GLSurface(w, attribs.data());
        Surfaces()[w] = surface;
        w->Bind(wxEVT_DESTROY, [w](wxWindowDestroyEvent& e) {
            if (e.GetWindow() == w)
                Surfaces().erase(w);
            e.Skip();
        });
        return TRUE;
    });
}

int GetPixelFormat(HDC hdc) { return SurfaceFor(hdc) ? 1 : 0; }

int DescribePixelFormat(HDC, int, UINT nBytes, LPPIXELFORMATDESCRIPTOR ppfd) {
    if (ppfd && nBytes >= sizeof(PIXELFORMATDESCRIPTOR)) {
        memset(ppfd, 0, sizeof *ppfd);
        ppfd->nSize = sizeof *ppfd;
        ppfd->nVersion = 1;
        ppfd->dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        ppfd->iPixelType = PFD_TYPE_RGBA;
        ppfd->cColorBits = 32;
        ppfd->cRedBits = ppfd->cGreenBits = ppfd->cBlueBits = ppfd->cAlphaBits = 8;
        ppfd->cDepthBits = 24;
        ppfd->iLayerType = PFD_MAIN_PLANE;
    }
    return 1;
}

BOOL SwapBuffers(HDC hdc) {
    return OnMain([&]() -> BOOL {
        GLSurface* s = SurfaceFor(hdc);
        if (!s && g_current)
            s = reinterpret_cast<GLContextHandle*>(g_current)->surface;
        return s ? s->SwapBuffers() : FALSE;
    });
}

HGLRC wglCreateContext(HDC hdc) {
    return OnMain([&]() -> HGLRC {
        GLSurface* s = SurfaceFor(hdc);
        if (!s)
            return nullptr;
        auto* h = new GLContextHandle;
        h->surface = s;
        h->context = new wxGLContext(s);
        return reinterpret_cast<HGLRC>(h);
    });
}

BOOL wglDeleteContext(HGLRC hglrc) {
    if (!hglrc)
        return FALSE;
    OnMain([&] {
        auto* h = reinterpret_cast<GLContextHandle*>(hglrc);
        if (g_current == hglrc) {
            g_current = nullptr;
            g_currentDC = nullptr;
        }
        delete h->context;
        delete h;
    });
    return TRUE;
}

BOOL wglMakeCurrent(HDC hdc, HGLRC hglrc) {
    return OnMain([&]() -> BOOL {
        g_current = hglrc;
        g_currentDC = hdc;
        if (!hglrc)
            return TRUE;
        auto* h = reinterpret_cast<GLContextHandle*>(hglrc);
        GLSurface* s = SurfaceFor(hdc);
        if (!s)
            s = h->surface;
        if (!s || !s->IsShownOnScreen())
            return TRUE;
        return s->SetCurrent(*h->context) ? TRUE : FALSE;
    });
}

HGLRC wglGetCurrentContext() { return g_current; }
HDC wglGetCurrentDC() { return g_currentDC; }

void* wglGetProcAddress(const char* name) { return dlsym(RTLD_DEFAULT, name); }
