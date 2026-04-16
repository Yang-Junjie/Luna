#ifndef CACAO_WGPU_INSTANCE_H
#define CACAO_WGPU_INSTANCE_H

#include <webgpu/webgpu.h>
#include "Instance.h"

namespace Cacao
{
    class CACAO_API WGPUInstance : public Instance
    {
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

        ::WGPUInstance GetNativeInstance() const { return m_instance; }
    };
}

#endif
