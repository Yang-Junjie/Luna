#ifndef CACAO_D3D11INSTANCE_H
#define CACAO_D3D11INSTANCE_H
#include "D3D11Common.h"
#include <Instance.h>

namespace Cacao
{
    class CACAO_API D3D11Instance : public Instance
    {
    public:
        [[nodiscard]] BackendType GetType() const override { return BackendType::DirectX11; }
        bool Initialize(const InstanceCreateInfo& createInfo) override;
        std::vector<Ref<Adapter>> EnumerateAdapters() override;
        bool IsFeatureEnabled(InstanceFeature feature) const override;
        Ref<Surface> CreateSurface(const NativeWindowHandle& windowHandle) override;
        Ref<ShaderCompiler> CreateShaderCompiler() override;

        IDXGIFactory6* GetFactory() const { return m_factory.Get(); }

    private:
        ComPtr<IDXGIFactory6> m_factory;
        InstanceCreateInfo m_createInfo;
        bool m_debugEnabled = false;
    };
}
#endif
