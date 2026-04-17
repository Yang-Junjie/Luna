#ifndef LUNA_RHI_WGPU_DESCRIPTORSET_H
#define LUNA_RHI_WGPU_DESCRIPTORSET_H

#include "DescriptorPool.h"
#include "DescriptorSet.h"
#include "DescriptorSetLayout.h"
#include "PipelineCache.h"
#include "PipelineLayout.h"
#include "Sampler.h"
#include "ShaderModule.h"

namespace luna::RHI {
class LUNA_RHI_API WGPUDescriptorSetLayout : public DescriptorSetLayout {
private:
    DescriptorSetLayoutCreateInfo m_createInfo;
    // TODO: WGPUBindGroupLayout m_layout;

public:
    WGPUDescriptorSetLayout(const DescriptorSetLayoutCreateInfo& info);
    ~WGPUDescriptorSetLayout() override = default;
};

class LUNA_RHI_API WGPUDescriptorPool : public DescriptorPool {
private:
    DescriptorPoolCreateInfo m_createInfo;

public:
    WGPUDescriptorPool(const DescriptorPoolCreateInfo& info);
    ~WGPUDescriptorPool() override = default;
    Ref<DescriptorSet> AllocateDescriptorSet(const Ref<DescriptorSetLayout>& layout) override;
};

class LUNA_RHI_API WGPUDescriptorSet : public DescriptorSet {
private:
    // TODO: WGPUBindGroup m_bindGroup;

public:
    WGPUDescriptorSet();
    ~WGPUDescriptorSet() override = default;

    void WriteBuffer(const BufferWriteInfo& info) override;
    void WriteBuffers(const BufferWriteInfos& infos) override;
    void WriteTexture(const TextureWriteInfo& info) override;
    void WriteTextures(const TextureWriteInfos& infos) override;
    void WriteSampler(const SamplerWriteInfo& info) override;
    void WriteSamplers(const SamplerWriteInfos& infos) override;
    void WriteAccelerationStructure(const AccelerationStructureWriteInfo& info) override;
    void WriteAccelerationStructures(const AccelerationStructureWriteInfos& infos) override;
    void Update() override;
};

class LUNA_RHI_API WGPUSampler : public Sampler {
private:
    // TODO: WGPUSampler m_sampler;

public:
    WGPUSampler(const SamplerCreateInfo& info);
    ~WGPUSampler() override = default;
};

class LUNA_RHI_API WGPUPipelineLayout : public PipelineLayout {
private:
    PipelineLayoutCreateInfo m_createInfo;
    // TODO: WGPUPipelineLayout m_layout;

public:
    WGPUPipelineLayout(const PipelineLayoutCreateInfo& info);
    ~WGPUPipelineLayout() override = default;
};

class LUNA_RHI_API WGPUPipelineCache : public PipelineCache {
public:
    WGPUPipelineCache() = default;
    ~WGPUPipelineCache() override = default;
    std::vector<uint8_t> GetData() const override;
};

class LUNA_RHI_API WGPUShaderModule : public ShaderModule {
private:
    // TODO: WGPUShaderModule m_module;

public:
    WGPUShaderModule(const ShaderBlob& blob, const ShaderCreateInfo& info);
    ~WGPUShaderModule() override = default;
};
} // namespace luna::RHI

#endif
