#include "Impls/OpenGL/GLAdapter.h"
#include "Impls/OpenGL/GLInstance.h"
#include "Impls/OpenGL/GLSurface.h"
#include "ShaderCompiler.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Cacao {
BackendType GLInstance::GetType() const
{
#ifdef CACAO_GLES
    return BackendType::OpenGLES;
#else
    return BackendType::OpenGL;
#endif
}

bool GLInstance::Initialize(const InstanceCreateInfo& createInfo)
{
    m_createInfo = createInfo;

#if defined(_WIN32) && !defined(CACAO_GLES)
    HWND tmpWnd =
        CreateWindowExA(0, "STATIC", "", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!tmpWnd) {
        return false;
    }

    HDC hdc = GetDC(tmpWnd);
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;

    int fmt = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, fmt, &pfd);

    HGLRC ctx = wglCreateContext(hdc);
    wglMakeCurrent(hdc, ctx);

    if (!gladLoadGL()) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(ctx);
        ReleaseDC(tmpWnd, hdc);
        DestroyWindow(tmpWnd);
        return false;
    }

    glGetIntegerv(GL_MAJOR_VERSION, &m_glMajor);
    glGetIntegerv(GL_MINOR_VERSION, &m_glMinor);

    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    m_renderer = renderer ? renderer : "Unknown";
    m_vendor = vendor ? vendor : "Unknown";

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(ctx);
    ReleaseDC(tmpWnd, hdc);
    DestroyWindow(tmpWnd);
#endif

    return true;
}

std::vector<Ref<Adapter>> GLInstance::EnumerateAdapters()
{
    return {GLAdapter::Create(m_renderer, m_vendor, m_glMajor, m_glMinor)};
}

bool GLInstance::IsFeatureEnabled(InstanceFeature feature) const
{
    for (auto& f : m_createInfo.enabledFeatures) {
        if (f == feature) {
            return true;
        }
    }
    return false;
}

Ref<Surface> GLInstance::CreateSurface(const NativeWindowHandle& windowHandle)
{
    return GLSurface::Create(windowHandle);
}

Ref<ShaderCompiler> GLInstance::CreateShaderCompiler()
{
    return ShaderCompiler::Create(BackendType::OpenGL);
}
} // namespace Cacao
