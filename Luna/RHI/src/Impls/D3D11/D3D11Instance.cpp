#include "Impls/D3D11/D3D11Adapter.h"
#include "Impls/D3D11/D3D11Instance.h"
#include "Impls/D3D11/D3D11Surface.h"
#include "ShaderCompiler.h"

namespace Cacao {
bool D3D11Instance::Initialize(const InstanceCreateInfo& createInfo)
{
    m_createInfo = createInfo;
    UINT flags = 0;
    for (auto f : createInfo.enabledFeatures) {
        if (f == InstanceFeature::ValidationLayer) {
            m_debugEnabled = true;
        }
    }

    HRESULT hr = CreateDXGIFactory2(m_debugEnabled ? DXGI_CREATE_FACTORY_DEBUG : 0, IID_PPV_ARGS(&m_factory));
    return SUCCEEDED(hr);
}

std::vector<Ref<Adapter>> D3D11Instance::EnumerateAdapters()
{
    std::vector<Ref<Adapter>> adapters;
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; m_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        auto self = std::static_pointer_cast<D3D11Instance>(shared_from_this());
        adapters.push_back(CreateRef<D3D11Adapter>(self, adapter));
        adapter.Reset();
    }
    return adapters;
}

bool D3D11Instance::IsFeatureEnabled(InstanceFeature feature) const
{
    for (auto f : m_createInfo.enabledFeatures) {
        if (f == feature) {
            return true;
        }
    }
    return false;
}

Ref<Surface> D3D11Instance::CreateSurface(const NativeWindowHandle& windowHandle)
{
#ifdef _WIN32
    if (!windowHandle.hWnd) {
        return nullptr;
    }
    return CreateRef<D3D11Surface>(std::static_pointer_cast<D3D11Instance>(shared_from_this()),
                                   static_cast<HWND>(windowHandle.hWnd));
#else
    return nullptr;
#endif
}

Ref<ShaderCompiler> D3D11Instance::CreateShaderCompiler()
{
    return ShaderCompiler::Create(BackendType::DirectX11);
}
} // namespace Cacao
