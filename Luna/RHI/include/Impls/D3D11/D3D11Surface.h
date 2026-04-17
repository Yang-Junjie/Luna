#ifndef LUNA_RHI_D3D11SURFACE_H
#define LUNA_RHI_D3D11SURFACE_H
#include "D3D11Common.h"

#include <Surface.h>

namespace luna::RHI {
class D3D11Instance;

class LUNA_RHI_API D3D11Surface : public Surface {
public:
    D3D11Surface(Ref<D3D11Instance> instance, HWND hwnd)
        : m_instance(std::move(instance)),
          m_hwnd(hwnd)
    {}

    HWND GetHWND() const
    {
        return m_hwnd;
    }

    SurfaceCapabilities GetCapabilities(const Ref<Adapter>& adapter) override;
    std::vector<SurfaceFormat> GetSupportedFormats(const Ref<Adapter>& adapter) override;
    Ref<Queue> GetPresentQueue(const Ref<Device>& device) override;
    std::vector<PresentMode> GetSupportedPresentModes(const Ref<Adapter>& adapter) override;

private:
    Ref<D3D11Instance> m_instance;
    HWND m_hwnd;
};
} // namespace luna::RHI
#endif
