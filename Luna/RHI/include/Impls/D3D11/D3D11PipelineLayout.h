#ifndef LUNA_RHI_D3D11PIPELINELAYOUT_H
#define LUNA_RHI_D3D11PIPELINELAYOUT_H
#include "PipelineLayout.h"

namespace luna::RHI {
class D3D11DescriptorSetLayout;

class LUNA_RHI_API D3D11PipelineLayout : public PipelineLayout {
public:
    explicit D3D11PipelineLayout(const PipelineLayoutCreateInfo& info);
    ~D3D11PipelineLayout() override = default;

    const PipelineLayoutCreateInfo& GetCreateInfo() const
    {
        return m_createInfo;
    }

    uint32_t GetSetRegisterBase(uint32_t setIndex) const
    {
        return setIndex < m_setRegisterBases.size() ? m_setRegisterBases[setIndex] : 0;
    }

private:
    PipelineLayoutCreateInfo m_createInfo;
    std::vector<uint32_t> m_setRegisterBases;
};
} // namespace luna::RHI

#endif
