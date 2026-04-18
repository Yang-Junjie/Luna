#ifndef LUNA_RHI_D3D12TEXTURE_H
#define LUNA_RHI_D3D12TEXTURE_H
#include "D3D12Common.h"
#include "D3D12MemAlloc.h"
#include "Texture.h"

#include <memory>

namespace luna::RHI {
class D3D12Device;

class LUNA_RHI_API D3D12TextureView final : public TextureView {
public:
    D3D12TextureView(const Ref<Texture>& texture,
                     const TextureViewDesc& desc,
                     const std::shared_ptr<D3D12Device>& d3dDevice);
    ~D3D12TextureView() override;

    Ref<Texture> GetTexture() const override
    {
        return m_texture.lock();
    }

    const TextureViewDesc& GetDesc() const override
    {
        return m_desc;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVHandle() const
    {
        return m_srvHandle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const
    {
        return m_rtvHandle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const
    {
        return m_dsvHandle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetUAVHandle() const
    {
        return m_uavHandle;
    }

    bool HasSRV() const
    {
        return m_hasSRV;
    }

    bool HasRTV() const
    {
        return m_hasRTV;
    }

    bool HasDSV() const
    {
        return m_hasDSV;
    }

    bool HasUAV() const
    {
        return m_hasUAV;
    }

private:
    std::weak_ptr<Texture> m_texture;
    std::shared_ptr<D3D12Device> m_device;
    TextureViewDesc m_desc;
    D3D12_CPU_DESCRIPTOR_HANDLE m_srvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_dsvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_uavHandle = {};
    bool m_hasSRV = false, m_hasRTV = false, m_hasDSV = false, m_hasUAV = false;
};

class LUNA_RHI_API D3D12Texture final : public Texture {
public:
    D3D12Texture(const Ref<Device>& device, const TextureCreateInfo& info);
    D3D12Texture(const Ref<Device>& device, ComPtr<ID3D12Resource> resource, const TextureCreateInfo& info);

    uint32_t GetWidth() const override
    {
        return m_createInfo.Width;
    }

    uint32_t GetHeight() const override
    {
        return m_createInfo.Height;
    }

    uint32_t GetDepth() const override
    {
        return m_createInfo.Depth;
    }

    uint32_t GetMipLevels() const override
    {
        return m_createInfo.MipLevels;
    }

    uint32_t GetArrayLayers() const override
    {
        return m_createInfo.ArrayLayers;
    }

    Format GetFormat() const override
    {
        return m_createInfo.Format;
    }

    TextureType GetType() const override
    {
        return m_createInfo.Type;
    }

    SampleCount GetSampleCount() const override
    {
        return m_createInfo.SampleCount;
    }

    TextureUsageFlags GetUsage() const override
    {
        return m_createInfo.Usage;
    }

    ResourceState GetCurrentState() const override
    {
        return m_currentState;
    }

    Ref<TextureView> CreateView(const TextureViewDesc& desc) override;

    Ref<TextureView> GetDefaultView() override
    {
        CreateDefaultViewIfNeeded();
        return m_defaultView;
    }

    void CreateDefaultViewIfNeeded() override;

    ID3D12Resource* GetHandle() const
    {
        return m_resource.Get();
    }

    void SetCurrentState(ResourceState state)
    {
        m_currentState = state;
    }

    ~D3D12Texture() override;

private:
    ComPtr<ID3D12Resource> m_resource;
    D3D12MA::Allocation* m_allocation = nullptr;
    Ref<Device> m_device;
    TextureCreateInfo m_createInfo;
    ResourceState m_currentState = ResourceState::Undefined;
    Ref<TextureView> m_defaultView;
    bool m_ownsResource = true;

    friend class D3D12Device;
    friend class D3D12Swapchain;
    friend class D3D12CommandBufferEncoder;
};
} // namespace luna::RHI
#endif
