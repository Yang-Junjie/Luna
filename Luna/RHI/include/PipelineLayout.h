#ifndef LUNA_RHI_PIPELINELAYOUT_H
#define LUNA_RHI_PIPELINELAYOUT_H
#include "PipelineDefs.h"

#include <memory>
#include <vector>

namespace luna::RHI {
class DescriptorSetLayout;

struct PipelineLayoutCreateInfo {
    std::vector<Ref<DescriptorSetLayout>> SetLayouts;
    std::vector<PushConstantRange> PushConstantRanges;
};

class LUNA_RHI_API PipelineLayout : public std::enable_shared_from_this<PipelineLayout> {
public:
    virtual ~PipelineLayout() = default;
};
} // namespace luna::RHI
#endif
