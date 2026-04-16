#ifndef CACAO_CACAODESCRIPTORPOOL_H
#define CACAO_CACAODESCRIPTORPOOL_H
#include "DescriptorSetLayout.h"

namespace Cacao {
class DescriptorSet;

struct DescriptorPoolSize {
    DescriptorType Type;
    uint32_t Count;
};

struct DescriptorPoolCreateInfo {
    uint32_t MaxSets = 100;
    std::vector<DescriptorPoolSize> PoolSizes;
};

class CACAO_API DescriptorPool : public std::enable_shared_from_this<DescriptorPool> {
public:
    virtual ~DescriptorPool() = default;
    virtual void Reset() = 0;
    virtual Ref<DescriptorSet> AllocateDescriptorSet(const Ref<DescriptorSetLayout>& layout) = 0;

    Ref<DescriptorSet> AllocateBindingGroup(const Ref<DescriptorSetLayout>& layout)
    {
        return AllocateDescriptorSet(layout);
    }
};

using BindingGroupAllocator = DescriptorPool;
} // namespace Cacao
#endif
