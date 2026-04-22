#include "Impls/D3D11/D3D11PipelineLayout.h"
#include "Impls/D3D11/D3D11BindingGroup.h"

#include <algorithm>

namespace luna::RHI {
D3D11PipelineLayout::D3D11PipelineLayout(const PipelineLayoutCreateInfo& info)
    : m_createInfo(info)
{
    m_setRegisterBases.resize(info.SetLayouts.size(), 0);

    uint32_t registerBase = 0;
    for (size_t i = 0; i < info.SetLayouts.size(); ++i) {
        m_setRegisterBases[i] = registerBase;

        auto d3dLayout = std::dynamic_pointer_cast<D3D11DescriptorSetLayout>(info.SetLayouts[i]);
        if (!d3dLayout) {
            continue;
        }

        uint32_t maxBinding = 0;
        bool hasBindings = false;
        for (const auto& binding : d3dLayout->GetBindings()) {
            if (binding.Count == 0) {
                continue;
            }

            maxBinding = (std::max)(maxBinding, binding.Binding + binding.Count - 1);
            hasBindings = true;
        }

        if (hasBindings) {
            registerBase += maxBinding + 1;
        }
    }
}
} // namespace luna::RHI
