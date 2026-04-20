#include "Impls/D3D12/D3D12DescriptorSetLayout.h"
#include "Impls/D3D12/D3D12Device.h"
#include "Impls/D3D12/D3D12PipelineLayout.h"

#include <algorithm>
#include <cstdio>

#include <stdexcept>
#include <string>

namespace luna::RHI {
D3D12PipelineLayout::D3D12PipelineLayout(const Ref<Device>& device, const PipelineLayoutCreateInfo& info)
    : m_device(device),
      m_createInfo(info)
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(device);

    std::vector<D3D12_ROOT_PARAMETER> rootParams;

    if (!info.PushConstantRanges.empty()) {
        D3D12_ROOT_PARAMETER pushParam = {};
        pushParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        pushParam.Constants.ShaderRegister = 15;
        pushParam.Constants.RegisterSpace = 0;
        uint32_t totalSize = 0;
        for (auto& range : info.PushConstantRanges) {
            totalSize = std::max(totalSize, range.Offset + range.Size);
        }
        pushParam.Constants.Num32BitValues = (totalSize + 3) / 4;
        pushParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParams.push_back(pushParam);
    }

    // Root parameters for descriptor tables (one per set layout)
    // Keep descriptor ranges alive until root signature is serialized
    std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> srvCbvUavRanges(info.SetLayouts.size());
    std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> samplerRanges(info.SetLayouts.size());
    uint32_t registerBase = 0;
    for (size_t i = 0; i < info.SetLayouts.size(); i++) {
        auto d3dLayout = std::dynamic_pointer_cast<D3D12DescriptorSetLayout>(info.SetLayouts[i]);
        if (!d3dLayout) {
            continue;
        }
        uint32_t maxBinding = 0;
        bool hasBindings = false;
        for (const auto& binding : d3dLayout->GetBindings()) {
            D3D12_DESCRIPTOR_RANGE range = {};
            range.NumDescriptors = binding.Count;
            range.BaseShaderRegister = registerBase + binding.Binding;
            range.RegisterSpace = 0;
            range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
            maxBinding = (std::max)(maxBinding, binding.Binding + binding.Count - 1);
            hasBindings = true;

            if (binding.Type == DescriptorType::Sampler) {
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                samplerRanges[i].push_back(range);
            } else {
                switch (binding.Type) {
                    case DescriptorType::UniformBuffer:
                    case DescriptorType::UniformBufferDynamic:
                        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                        break;
                    case DescriptorType::StorageImage:
                        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                        break;
                    default:
                        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                        break;
                }
                srvCbvUavRanges[i].push_back(range);
            }
        }

        if (!srvCbvUavRanges[i].empty()) {
            D3D12_ROOT_PARAMETER tableParam = {};
            tableParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            tableParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            tableParam.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(srvCbvUavRanges[i].size());
            tableParam.DescriptorTable.pDescriptorRanges = srvCbvUavRanges[i].data();
            rootParams.push_back(tableParam);
        }
        if (!samplerRanges[i].empty()) {
            D3D12_ROOT_PARAMETER tableParam = {};
            tableParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            tableParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            tableParam.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(samplerRanges[i].size());
            tableParam.DescriptorTable.pDescriptorRanges = samplerRanges[i].data();
            rootParams.push_back(tableParam);
        }

        if (hasBindings) {
            registerBase += maxBinding + 1;
        }
    }

    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.NumParameters = static_cast<UINT>(rootParams.size());
    rsDesc.pParameters = rootParams.empty() ? nullptr : rootParams.data();
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signatureBlob, errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        char hrBuffer[16];
        snprintf(hrBuffer, sizeof(hrBuffer), "%08X", static_cast<unsigned>(hr));
        std::string message = "D3D12SerializeRootSignature failed: 0x";
        message += hrBuffer;
        if (errorBlob && errorBlob->GetBufferPointer() != nullptr) {
            message += " ";
            message += static_cast<const char*>(errorBlob->GetBufferPointer());
        }
        throw std::runtime_error(message);
    }

    hr = d3dDevice->GetHandle()->CreateRootSignature(
        0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
    if (FAILED(hr)) {
        char hrBuffer[16];
        snprintf(hrBuffer, sizeof(hrBuffer), "%08X", static_cast<unsigned>(hr));
        std::string message = "ID3D12Device::CreateRootSignature failed: 0x";
        message += hrBuffer;
        throw std::runtime_error(message);
    }
}

Ref<D3D12PipelineLayout> D3D12PipelineLayout::Create(const Ref<Device>& device, const PipelineLayoutCreateInfo& info)
{
    return std::make_shared<D3D12PipelineLayout>(device, info);
}
} // namespace luna::RHI
