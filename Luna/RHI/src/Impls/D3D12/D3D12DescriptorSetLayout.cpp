#include "Impls/D3D12/D3D12DescriptorSetLayout.h"

namespace Cacao {
D3D12DescriptorSetLayout::D3D12DescriptorSetLayout(const Ref<Device>& device, const DescriptorSetLayoutCreateInfo& info)
    : m_device(device),
      m_bindings(info.Bindings)
{}
} // namespace Cacao
