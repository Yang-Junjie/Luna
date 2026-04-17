#ifndef LUNA_RHI_WGPU_INSTANCE_H
#define LUNA_RHI_WGPU_INSTANCE_H

#include "Instance.h"

#include <webgpu/webgpu.h>

namespace luna::RHI {
class LUNA_RHI_API WGPUInstance : public Instance {
private:
    InstanceCreateInfo m_createInfo;
    ::WGPUInstance m_instance = nullptr;

    friend class WGPUAdapter;
    friend class WGPUSurface;

public:
    ~WGPUInstance() override;

    [[nodiscard]] BackendType GetType() const override;
    bool Initialize(const InstanceCreateInfo& createInfo) override;
    std::vector<Ref<Adapter>> EnumerateAdapters() override;
    bool IsFeatureEnabled(InstanceFeature feature) const override;
    Ref<Surface> CreateSurface(const NativeWindowHandle& windowHandle) override;
    Ref<ShaderCompiler> CreateShaderCompiler() override;

    ::WGPUInstance GetNativeInstance() const
    {
        return m_instance;
    }
};
} // namespace luna::RHI

#endif
