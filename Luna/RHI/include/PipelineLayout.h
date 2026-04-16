#ifndef CACAO_CACAOPIPELINELAYOUT_H
#define CACAO_CACAOPIPELINELAYOUT_H
#include "PipelineDefs.h"
#include <memory>
#include <vector>
namespace Cacao
{
    class DescriptorSetLayout;
    struct PipelineLayoutCreateInfo
    {
        std::vector<Ref<DescriptorSetLayout>> SetLayouts;
        std::vector<PushConstantRange> PushConstantRanges;
    };
    class CACAO_API PipelineLayout : public std::enable_shared_from_this<PipelineLayout>
    {
    public:
        virtual ~PipelineLayout() = default;
    };
}
#endif 
