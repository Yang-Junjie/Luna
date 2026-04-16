#ifndef CACAO_D3D12DESCRIPTORSETLAYOUT_H
#define CACAO_D3D12DESCRIPTORSETLAYOUT_H
#include "D3D12Common.h"
#include "DescriptorSetLayout.h"

namespace Cacao
{
    class CACAO_API D3D12DescriptorSetLayout : public DescriptorSetLayout
    {
    private:
        std::vector<DescriptorSetLayoutBinding> m_bindings;
        Ref<Device> m_device;

    public:
        D3D12DescriptorSetLayout(const Ref<Device>& device, const DescriptorSetLayoutCreateInfo& info);
        const std::vector<DescriptorSetLayoutBinding>& GetBindings() const { return m_bindings; }
    };
}

#endif
