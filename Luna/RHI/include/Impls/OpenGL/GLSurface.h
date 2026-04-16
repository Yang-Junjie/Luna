#ifndef CACAO_GLSURFACE_H
#define CACAO_GLSURFACE_H
#include "Surface.h"
#include "Instance.h"
#include "GLCommon.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Cacao
{
    class CACAO_API GLSurface final : public Surface
    {
    public:
        GLSurface(const NativeWindowHandle& windowHandle);
        static Ref<GLSurface> Create(const NativeWindowHandle& windowHandle);
        ~GLSurface() override;

        SurfaceCapabilities GetCapabilities(const Ref<Adapter>& adapter) override;
        std::vector<SurfaceFormat> GetSupportedFormats(const Ref<Adapter>& adapter) override;
        Ref<Queue> GetPresentQueue(const Ref<Device>& device) override;
        std::vector<PresentMode> GetSupportedPresentModes(const Ref<Adapter>& adapter) override;

        void MakeCurrent();
        void SwapBuffers();
        void* GetNativeHDC() const { return m_hdc; }

    private:
#ifdef _WIN32
        HWND m_hwnd = nullptr;
        HDC m_hdc = nullptr;
        HGLRC m_context = nullptr;
#endif
    };
}

#endif
