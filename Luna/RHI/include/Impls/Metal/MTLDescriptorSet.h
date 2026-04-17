#ifndef LUNA_RHI_MTLDESCRIPTORSET_H
#define LUNA_RHI_MTLDESCRIPTORSET_H
#ifdef __APPLE__
#include "DescriptorPool.h"
#include "DescriptorSet.h"
#include "DescriptorSetLayout.h"
#include "MTLCommon.h"

namespace luna::RHI {
class LUNA_RHI_API MTLDescriptorSetLayout final : public DescriptorSetLayout {
public:
    MTLDescriptorSetLayout(const DescriptorSetLayoutCreateInfo& info)
        : m_info(info)
    {}

    ~MTLDescriptorSetLayout() override = default;

    const DescriptorSetLayoutCreateInfo& GetCreateInfo() const override
    {
        return m_info;
    }

private:
    DescriptorSetLayoutCreateInfo m_info;
};

class LUNA_RHI_API MTLDescriptorPool final : public DescriptorPool {
public:
    MTLDescriptorPool(const DescriptorPoolCreateInfo& info)
        : m_info(info)
    {}

    ~MTLDescriptorPool() override = default;
    Ref<DescriptorSet> AllocateDescriptorSet(const Ref<DescriptorSetLayout>& layout) override;

    void Reset() override {}

private:
    DescriptorPoolCreateInfo m_info;
};

class LUNA_RHI_API MTLDescriptorSet final : public DescriptorSet {
public:
    MTLDescriptorSet(const Ref<DescriptorSetLayout>& layout)
        : m_layout(layout)
    {}

    ~MTLDescriptorSet() override = default;

    void WriteBuffer(const BufferDescriptorWrite& write) override {}

    void WriteTexture(const TextureDescriptorWrite& write) override {}

    void WriteSampler(const SamplerDescriptorWrite& write) override {}

    void WriteAccelerationStructure(const AccelerationStructureDescriptorWrite& write) override {}

    void Update() override {}

private:
    Ref<DescriptorSetLayout> m_layout;
};
} // namespace luna::RHI
#endif // __APPLE__
#endif
