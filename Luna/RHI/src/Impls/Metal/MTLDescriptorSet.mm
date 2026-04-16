#ifdef __APPLE__
#include "Impls/Metal/MTLDescriptorSet.h"

namespace Cacao
{
    Ref<DescriptorSet> MTLDescriptorPool::AllocateDescriptorSet(const Ref<DescriptorSetLayout>& layout)
    {
        // Metal uses argument buffers instead of descriptor sets.
        // MTLDescriptorSet stores binding info that is applied when
        // BindDescriptorSets is called on the command encoder.
        return std::make_shared<MTLDescriptorSet>(layout);
    }
}
#endif // __APPLE__
