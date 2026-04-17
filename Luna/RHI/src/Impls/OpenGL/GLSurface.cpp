#include "Device.h"
#include "Impls/OpenGL/GLQueue.h"
#include "Impls/OpenGL/GLSurface.h"

namespace luna::RHI {
GLSurface::GLSurface(const NativeWindowHandle& windowHandle)
{
#ifdef _WIN32
    m_hwnd = static_cast<HWND>(windowHandle.hWnd);
    m_hdc = GetDC(m_hwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;

    int pixelFormat = ChoosePixelFormat(m_hdc, &pfd);
    SetPixelFormat(m_hdc, pixelFormat, &pfd);

    m_context = wglCreateContext(m_hdc);
    wglMakeCurrent(m_hdc, m_context);

    gladLoadGL();
#endif
}

Ref<GLSurface> GLSurface::Create(const NativeWindowHandle& windowHandle)
{
    return std::make_shared<GLSurface>(windowHandle);
}

GLSurface::~GLSurface()
{
#ifdef _WIN32
    if (m_context) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(m_context);
    }
    if (m_hdc && m_hwnd) {
        ReleaseDC(m_hwnd, m_hdc);
    }
#endif
}

void GLSurface::MakeCurrent()
{
#ifdef _WIN32
    wglMakeCurrent(m_hdc, m_context);
#endif
}

void GLSurface::SwapBuffers()
{
#ifdef _WIN32
    ::SwapBuffers(m_hdc);
#endif
}

SurfaceCapabilities GLSurface::GetCapabilities(const Ref<Adapter>&)
{
    SurfaceCapabilities caps{};
    caps.minImageCount = 2;
    caps.maxImageCount = 3;

#ifdef _WIN32
    RECT rect;
    GetClientRect(m_hwnd, &rect);
    caps.currentExtent = {static_cast<uint32_t>(rect.right - rect.left), static_cast<uint32_t>(rect.bottom - rect.top)};
#endif
    caps.minImageExtent = caps.currentExtent;
    caps.maxImageExtent = caps.currentExtent;
    return caps;
}

std::vector<SurfaceFormat> GLSurface::GetSupportedFormats(const Ref<Adapter>&)
{
    return {{Format::RGBA8_UNORM, ColorSpace::SRGB_NONLINEAR}, {Format::BGRA8_UNORM, ColorSpace::SRGB_NONLINEAR}};
}

Ref<Queue> GLSurface::GetPresentQueue(const Ref<Device>& device)
{
    return device->GetQueue(QueueType::Graphics, 0);
}

std::vector<PresentMode> GLSurface::GetSupportedPresentModes(const Ref<Adapter>&)
{
    return {PresentMode::Fifo, PresentMode::Immediate};
}
} // namespace luna::RHI
