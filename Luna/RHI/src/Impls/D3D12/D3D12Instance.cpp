#include "Impls/D3D12/D3D12Adapter.h"
#include "Impls/D3D12/D3D12Instance.h"
#include "Impls/D3D12/D3D12Surface.h"

#include <d3d12sdklayers.h>
#include <ShaderCompiler.h>

namespace Cacao {
BackendType D3D12Instance::GetType() const
{
    return BackendType::DirectX12;
}

bool D3D12Instance::Initialize(const InstanceCreateInfo& createInfo)
{
    m_createInfo = createInfo;
    UINT dxgiFlags = 0;

    bool enableValidation = false;
    for (auto f : createInfo.enabledFeatures) {
        if (f == InstanceFeature::ValidationLayer) {
            enableValidation = true;
            break;
        }
    }

    if (enableValidation) {
        ComPtr<ID3D12Debug3> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
            debugController->EnableDebugLayer();
            m_debugEnabled = true;
            dxgiFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }

    HRESULT hr = CreateDXGIFactory2(dxgiFlags, IID_PPV_ARGS(&m_factory));
    return SUCCEEDED(hr);
}

std::vector<Ref<Adapter>> D3D12Instance::EnumerateAdapters()
{
    std::vector<Ref<Adapter>> adapters;
    ComPtr<IDXGIAdapter4> adapter;

    for (UINT i = 0; m_factory->EnumAdapterByGpuPreference(
                         i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
         ++i) {
        DXGI_ADAPTER_DESC3 desc;
        adapter->GetDesc3(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) {
            continue;
        }

        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr))) {
            auto inst = std::dynamic_pointer_cast<D3D12Instance>(shared_from_this());
            adapters.push_back(D3D12Adapter::Create(inst, adapter));
        }
        adapter.Reset();
    }
    return adapters;
}

bool D3D12Instance::IsFeatureEnabled(InstanceFeature feature) const
{
    if (feature == InstanceFeature::ValidationLayer) {
        return m_debugEnabled;
    }
    return false;
}

Ref<Surface> D3D12Instance::CreateSurface(const NativeWindowHandle& windowHandle)
{
    if (!windowHandle.hWnd) {
        return nullptr;
    }
    auto self = std::dynamic_pointer_cast<D3D12Instance>(shared_from_this());
    return CreateRef<D3D12Surface>(self, static_cast<HWND>(windowHandle.hWnd));
}

Ref<ShaderCompiler> D3D12Instance::CreateShaderCompiler()
{
    return ShaderCompiler::Create(BackendType::DirectX12);
}
} // namespace Cacao
