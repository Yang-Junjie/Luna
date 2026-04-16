#ifndef CACAO_D3D12INSTANCE_H
#define CACAO_D3D12INSTANCE_H
#include "D3D12Common.h"
#include <Instance.h>

namespace Cacao
{
    class CACAO_API D3D12Instance : public Instance
    {
    private:
        ComPtr<IDXGIFactory6> m_factory;
        InstanceCreateInfo m_createInfo;
        bool m_debugEnabled = false;

        friend class D3D12Adapter;
        friend class D3D12Device;
        friend class D3D12Swapchain;
        IDXGIFactory6* GetFactory() const { return m_factory.Get(); }

    public:
        [[nodiscard]] BackendType GetType() const override;
        bool Initialize(const InstanceCreateInfo& createInfo) override;
        std::vector<Ref<Adapter>> EnumerateAdapters() override;
        bool IsFeatureEnabled(InstanceFeature feature) const override;
        Ref<Surface> CreateSurface(const NativeWindowHandle& windowHandle) override;
        Ref<ShaderCompiler> CreateShaderCompiler() override;
    };
}

#endif
