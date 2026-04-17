#ifndef LUNA_RHI_GLSURFACE_H
#define LUNA_RHI_GLSURFACE_H
#include "GLCommon.h"
#include "Instance.h"
#include "Surface.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace luna::RHI {
class LUNA_RHI_API GLSurface final : public Surface {
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

    void* GetNativeHDC() const
    {
        return m_hdc;
    }

private:
#ifdef _WIN32
    HWND m_hwnd = nullptr;
    HDC m_hdc = nullptr;
    HGLRC m_context = nullptr;
#endif
};
} // namespace luna::RHI

#endif
