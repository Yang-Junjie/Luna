#include "Impls/D3D11/D3D11Adapter.h"
#include "Impls/D3D11/D3D11BindingGroup.h"
#include "Impls/D3D11/D3D11Buffer.h"
#include "Impls/D3D11/D3D11CommandBufferEncoder.h"
#include "Impls/D3D11/D3D11Device.h"
#include "Impls/D3D11/D3D11Pipeline.h"
#include "Impls/D3D11/D3D11Sampler.h"
#include "Impls/D3D11/D3D11ShaderModule.h"
#include "Impls/D3D11/D3D11Swapchain.h"
#include "Impls/D3D11/D3D11Synchronization.h"
#include "Impls/D3D11/D3D11Texture.h"

namespace Cacao {
D3D11Device::D3D11Device(Ref<D3D11Adapter> adapter)
    : m_adapter(std::move(adapter))
{
    InitDevice();
}

bool D3D11Device::InitDevice()
{
    UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    ComPtr<ID3D11Device> baseDevice;
    ComPtr<ID3D11DeviceContext> baseContext;

    HRESULT hr = D3D11CreateDevice(m_adapter->GetNativeAdapter(),
                                   D3D_DRIVER_TYPE_UNKNOWN,
                                   nullptr,
                                   createFlags,
                                   featureLevels,
                                   _countof(featureLevels),
                                   D3D11_SDK_VERSION,
                                   &baseDevice,
                                   &m_featureLevel,
                                   &baseContext);

    if (FAILED(hr)) {
        return false;
    }
    baseDevice.As(&m_device);
    baseContext.As(&m_immediateContext);
    return m_device != nullptr;
}

Ref<Queue> D3D11Device::GetQueue(QueueType type, uint32_t index)
{
    // DX11 uses immediate context; return a no-op queue wrapper
    if (!m_pseudoQueue) {
        m_pseudoQueue = CreateRef<D3D11Queue>(std::static_pointer_cast<D3D11Device>(shared_from_this()));
    }
    return m_pseudoQueue;
}

Ref<Swapchain> D3D11Device::CreateSwapchain(const SwapchainCreateInfo& createInfo)
{
    return CreateRef<D3D11Swapchain>(std::static_pointer_cast<D3D11Device>(shared_from_this()), createInfo);
}

Ref<Adapter> D3D11Device::GetParentAdapter() const
{
    return m_adapter;
}

Ref<CommandBufferEncoder> D3D11Device::CreateCommandBufferEncoder(CommandBufferType type)
{
    return CreateRef<D3D11CommandBufferEncoder>(std::static_pointer_cast<D3D11Device>(shared_from_this()), type);
}

Ref<Texture> D3D11Device::CreateTexture(const TextureCreateInfo& createInfo)
{
    return CreateRef<D3D11Texture>(std::static_pointer_cast<D3D11Device>(shared_from_this()), createInfo);
}

Ref<Buffer> D3D11Device::CreateBuffer(const BufferCreateInfo& createInfo)
{
    return CreateRef<D3D11Buffer>(std::static_pointer_cast<D3D11Device>(shared_from_this()), createInfo);
}

Ref<Sampler> D3D11Device::CreateSampler(const SamplerCreateInfo& createInfo)
{
    return CreateRef<D3D11Sampler>(std::static_pointer_cast<D3D11Device>(shared_from_this()), createInfo);
}

Ref<DescriptorSetLayout> D3D11Device::CreateDescriptorSetLayout(const DescriptorSetLayoutCreateInfo& info)
{
    ValidateDescriptorSetLayout(info);
    return CreateRef<D3D11DescriptorSetLayout>(info);
}

Ref<DescriptorPool> D3D11Device::CreateDescriptorPool(const DescriptorPoolCreateInfo& info)
{
    return CreateRef<D3D11DescriptorPool>(std::static_pointer_cast<D3D11Device>(shared_from_this()), info);
}

Ref<ShaderModule> D3D11Device::CreateShaderModule(const ShaderBlob& blob, const ShaderCreateInfo& info)
{
    return CreateRef<D3D11ShaderModule>(blob, info);
}

Ref<PipelineLayout> D3D11Device::CreatePipelineLayout(const PipelineLayoutCreateInfo& info)
{
    return nullptr; // DX11 doesn't have pipeline layouts
}

Ref<PipelineCache> D3D11Device::CreatePipelineCache(std::span<const uint8_t> initialData)
{
    return nullptr; // DX11 doesn't have pipeline caches
}

Ref<GraphicsPipeline> D3D11Device::CreateGraphicsPipeline(const GraphicsPipelineCreateInfo& info)
{
    ValidateGraphicsPipeline(info);
    return CreateRef<D3D11GraphicsPipeline>(std::static_pointer_cast<D3D11Device>(shared_from_this()), info);
}

Ref<ComputePipeline> D3D11Device::CreateComputePipeline(const ComputePipelineCreateInfo& info)
{
    return CreateRef<D3D11ComputePipeline>(std::static_pointer_cast<D3D11Device>(shared_from_this()), info);
}

Ref<Synchronization> D3D11Device::CreateSynchronization(uint32_t maxFramesInFlight)
{
    return CreateRef<D3D11Synchronization>(std::static_pointer_cast<D3D11Device>(shared_from_this()),
                                           maxFramesInFlight);
}
} // namespace Cacao
