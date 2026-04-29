#include "Core/Log.h"
#include "Renderer/RenderFlow/RenderFeatureBindingContract.h"

#include <string>

namespace luna::render_flow {

luna::RHI::DescriptorSetLayoutCreateInfo
    makeRenderFeatureDescriptorSetLayoutCreateInfo(std::span<const RenderFeatureDescriptorBinding> bindings)
{
    luna::RHI::DescriptorSetLayoutCreateInfo create_info;
    create_info.Bindings.reserve(bindings.size());
    for (const RenderFeatureDescriptorBinding& binding : bindings) {
        create_info.Bindings.push_back(luna::RHI::DescriptorSetLayoutBinding{
            .Binding = binding.binding,
            .Type = binding.type,
            .Count = binding.count,
            .StageFlags = binding.stages,
        });
    }
    return create_info;
}

ShaderBindingContract makeRenderFeatureShaderBindingContract(const RenderFeatureDescriptorSetContract& descriptor_set)
{
    ShaderBindingContract contract = makeShaderBindingContract(descriptor_set.contract_name);
    contract.bindings.reserve(descriptor_set.bindings.size());
    for (const RenderFeatureDescriptorBinding& binding : descriptor_set.bindings) {
        contract.bindings.push_back(ShaderBindingRequirement{
            .name = std::string(binding.name),
            .shader_name = std::string(binding.shader_name),
            .set_name = std::string(descriptor_set.set_name),
            .logical_set = descriptor_set.logical_set,
            .logical_binding = binding.binding,
            .set = descriptor_set.set,
            .binding = binding.binding,
            .type = binding.type,
            .count = binding.count,
            .stages = binding.stages,
        });
    }
    return contract;
}

ShaderBindingValidationResult
    validateRenderFeatureShaderModuleBindings(const luna::RHI::Ref<luna::RHI::ShaderModule>& shader,
                                              const ShaderBindingContract& contract,
                                              const std::filesystem::path& shader_file,
                                              std::string_view entry_point)
{
    if (!shader) {
        return {};
    }

    const std::string shader_file_string = shader_file.string();
    return validateShaderBindingContract(
        ShaderBindingValidationContext{
            .shader_file = shader_file_string,
            .entry_point = entry_point,
            .shader_stage = shader->GetStage(),
        },
        contract,
        shader->GetReflection());
}

bool validateAndLogRenderFeatureShaderModuleBindings(const luna::RHI::Ref<luna::RHI::ShaderModule>& shader,
                                                     const ShaderBindingContract& contract,
                                                     const std::filesystem::path& shader_file,
                                                     std::string_view entry_point)
{
    const ShaderBindingValidationResult result =
        validateRenderFeatureShaderModuleBindings(shader, contract, shader_file, entry_point);
    for (const std::string& issue : result.issues) {
        LUNA_RENDERER_WARN("Shader binding contract mismatch: {}", issue);
    }
    return result.valid();
}

} // namespace luna::render_flow
