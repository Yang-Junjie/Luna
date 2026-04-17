#ifndef LUNA_RHI_D3D11BINDINGGROUP_H
#define LUNA_RHI_D3D11BINDINGGROUP_H
#include "D3D11Common.h"

#include <DescriptorPool.h>
#include <DescriptorSet.h>
#include <DescriptorSetLayout.h>

namespace luna::RHI {
class D3D11Device;

class LUNA_RHI_API D3D11DescriptorSetLayout : public DescriptorSetLayout {
public:
    D3D11DescriptorSetLayout(const DescriptorSetLayoutCreateInfo& info)
        : m_bindings(info.Bindings)
    {}

    const std::vector<DescriptorSetLayoutBinding>& GetBindings() const
    {
        return m_bindings;
    }

private:
    std::vector<DescriptorSetLayoutBinding> m_bindings;
};

struct D3D11BoundResource {
    ComPtr<ID3D11Buffer> constantBuffer;
    ComPtr<ID3D11Buffer> structuredBuffer;
    ComPtr<ID3D11Buffer> sourceBuffer;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11UnorderedAccessView> uav;
    ComPtr<ID3D11SamplerState> sampler;
    uint32_t slot = 0;
    DescriptorType type = DescriptorType::UniformBuffer;
};

class LUNA_RHI_API D3D11DescriptorSet : public DescriptorSet {
public:
    D3D11DescriptorSet(Ref<D3D11Device> device, Ref<D3D11DescriptorSetLayout> layout);

    void WriteBuffer(const BufferWriteInfo& info) override;
    void WriteBuffers(const BufferWriteInfos& infos) override;
    void WriteTexture(const TextureWriteInfo& info) override;
    void WriteTextures(const TextureWriteInfos& infos) override;
    void WriteSampler(const SamplerWriteInfo& info) override;
    void WriteSamplers(const SamplerWriteInfos& infos) override;

    void WriteAccelerationStructure(const AccelerationStructureWriteInfo& info) override {}

    void WriteAccelerationStructures(const AccelerationStructureWriteInfos& infos) override {}

    void Update() override {}

    void Bind(ID3D11DeviceContext* ctx, ShaderStage stages) const;

private:
    Ref<D3D11Device> m_device;
    Ref<D3D11DescriptorSetLayout> m_layout;
    std::vector<D3D11BoundResource> m_resources;
};

class LUNA_RHI_API D3D11DescriptorPool : public DescriptorPool {
public:
    D3D11DescriptorPool(Ref<D3D11Device> device, const DescriptorPoolCreateInfo& info);
    void Reset() override;
    Ref<DescriptorSet> AllocateDescriptorSet(const Ref<DescriptorSetLayout>& layout) override;

private:
    Ref<D3D11Device> m_device;
};
} // namespace luna::RHI
#endif
