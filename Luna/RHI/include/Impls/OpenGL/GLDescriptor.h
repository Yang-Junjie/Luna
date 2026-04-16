#ifndef CACAO_GLDESCRIPTOR_H
#define CACAO_GLDESCRIPTOR_H
#include "DescriptorPool.h"
#include "DescriptorSet.h"
#include "DescriptorSetLayout.h"
#include "GLBindingGroup.h"
#include "GLCommon.h"
#include "Pipeline.h"
#include "PipelineLayout.h"

namespace Cacao {
class GLBuffer;
class GLTexture;
class GLSampler;

class CACAO_API GLDescriptorSetLayout : public DescriptorSetLayout {
public:
    GLDescriptorSetLayout(const DescriptorSetLayoutCreateInfo& info)
        : m_bindings(info.Bindings)
    {}

    const std::vector<DescriptorSetLayoutBinding>& GetBindings() const
    {
        return m_bindings;
    }

private:
    std::vector<DescriptorSetLayoutBinding> m_bindings;
};

class CACAO_API GLDescriptorSet : public DescriptorSet {
public:
    GLDescriptorSet() = default;

    void Update() override {}

    void WriteBuffer(const BufferWriteInfo& info) override;
    void WriteTexture(const TextureWriteInfo& info) override;
    void WriteSampler(const SamplerWriteInfo& info) override;

    void WriteAccelerationStructure(const AccelerationStructureWriteInfo& info) override {}

    void WriteBuffers(const BufferWriteInfos& infos) override;
    void WriteTextures(const TextureWriteInfos& infos) override;
    void WriteSamplers(const SamplerWriteInfos& infos) override;

    void WriteAccelerationStructures(const AccelerationStructureWriteInfos& infos) override {}

    GLBindingGroup& GetBindingGroup()
    {
        return m_bindingGroup;
    }

private:
    GLBindingGroup m_bindingGroup;
};

class CACAO_API GLDescriptorPool : public DescriptorPool {
public:
    GLDescriptorPool(const DescriptorPoolCreateInfo& info) {}

    Ref<DescriptorSet> AllocateDescriptorSet(const Ref<DescriptorSetLayout>& layout) override;

    void Reset() override {}
};

class CACAO_API GLPipelineLayout : public PipelineLayout {
public:
    GLPipelineLayout(const PipelineLayoutCreateInfo& info) {}
};
} // namespace Cacao

#endif
