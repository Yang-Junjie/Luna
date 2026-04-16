#ifndef CACAO_D3D12SURFACE_H
#define CACAO_D3D12SURFACE_H
#include "D3D12Common.h"

#include <Surface.h>

namespace Cacao {
class D3D12Instance;

class CACAO_API D3D12Surface : public Surface {
public:
    D3D12Surface(Ref<D3D12Instance> instance, HWND hwnd)
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
    Ref<D3D12Instance> m_instance;
    HWND m_hwnd;
};
} // namespace Cacao
#endif
