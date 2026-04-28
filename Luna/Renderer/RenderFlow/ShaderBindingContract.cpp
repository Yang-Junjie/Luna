#include "Renderer/RenderFlow/ShaderBindingContract.h"

#include <sstream>
#include <unordered_map>

namespace luna::render_flow {
namespace {

struct BindingKey {
    uint32_t set{0};
    uint32_t binding{0};

    [[nodiscard]] bool operator==(const BindingKey& other) const noexcept
    {
        return set == other.set && binding == other.binding;
    }
};

struct BindingKeyHash {
    [[nodiscard]] size_t operator()(const BindingKey& key) const noexcept
    {
        return (static_cast<size_t>(key.set) << 32u) ^ static_cast<size_t>(key.binding);
    }
};

const char* descriptorTypeName(luna::RHI::DescriptorType type)
{
    switch (type) {
        case luna::RHI::DescriptorType::Sampler:
            return "Sampler";
        case luna::RHI::DescriptorType::CombinedImageSampler:
            return "CombinedImageSampler";
        case luna::RHI::DescriptorType::SampledImage:
            return "SampledImage";
        case luna::RHI::DescriptorType::StorageImage:
            return "StorageImage";
        case luna::RHI::DescriptorType::UniformBuffer:
            return "UniformBuffer";
        case luna::RHI::DescriptorType::StorageBuffer:
            return "StorageBuffer";
        case luna::RHI::DescriptorType::UniformBufferDynamic:
            return "UniformBufferDynamic";
        case luna::RHI::DescriptorType::StorageBufferDynamic:
            return "StorageBufferDynamic";
        case luna::RHI::DescriptorType::InputAttachment:
            return "InputAttachment";
        case luna::RHI::DescriptorType::AccelerationStructure:
            return "AccelerationStructure";
        default:
            return "Unknown";
    }
}

std::string shaderStageNames(luna::RHI::ShaderStage stages)
{
    if (stages == luna::RHI::ShaderStage::None) {
        return "None";
    }
    if (stages == luna::RHI::ShaderStage::All) {
        return "All";
    }

    std::string result;
    auto append = [&result, stages](luna::RHI::ShaderStage stage, const char* name) {
        if (!(stages & stage)) {
            return;
        }
        if (!result.empty()) {
            result += "|";
        }
        result += name;
    };

    append(luna::RHI::ShaderStage::Vertex, "Vertex");
    append(luna::RHI::ShaderStage::Fragment, "Fragment");
    append(luna::RHI::ShaderStage::Compute, "Compute");
    append(luna::RHI::ShaderStage::Geometry, "Geometry");
    append(luna::RHI::ShaderStage::TessellationControl, "TessControl");
    append(luna::RHI::ShaderStage::TessellationEvaluation, "TessEval");
    append(luna::RHI::ShaderStage::RayGen, "RayGen");
    append(luna::RHI::ShaderStage::RayAnyHit, "RayAnyHit");
    append(luna::RHI::ShaderStage::RayClosestHit, "RayClosestHit");
    append(luna::RHI::ShaderStage::RayMiss, "RayMiss");
    append(luna::RHI::ShaderStage::RayIntersection, "RayIntersection");
    append(luna::RHI::ShaderStage::Callable, "Callable");
    append(luna::RHI::ShaderStage::Mesh, "Mesh");
    append(luna::RHI::ShaderStage::Task, "Task");

    return result.empty() ? "Unknown" : result;
}

std::string bindingLabel(uint32_t set, uint32_t binding)
{
    std::ostringstream stream;
    stream << "set=" << set << " binding=" << binding;
    return stream.str();
}

std::string usageLabel(const luna::RHI::ShaderResourceBinding& binding)
{
    std::ostringstream stream;
    if (binding.EntryPointUsageKnown) {
        stream << (binding.UsedByEntryPoint ? "used" : "unused");
    } else {
        stream << "usage_unknown";
    }
    return stream.str();
}

std::string shaderPrefix(const ShaderBindingValidationContext& context, const ShaderBindingContract& contract)
{
    std::ostringstream stream;
    stream << "file='" << context.shader_file << "' entry='" << context.entry_point
           << "' stage=" << shaderStageNames(context.shader_stage) << " contract='" << contract.name << "'";
    return stream.str();
}

std::string expectedBindingLabel(const ShaderBindingRequirement& expected)
{
    std::ostringstream stream;
    stream << expected.set_name << "." << expected.name << " logical "
           << bindingLabel(expected.logical_set, expected.logical_binding) << " backend "
           << bindingLabel(expected.set, expected.binding) << " shader_name='"
           << (expected.shader_name.empty() ? "<unspecified>" : expected.shader_name)
           << "' type=" << descriptorTypeName(expected.type) << " count=" << expected.count
           << " stages=" << shaderStageNames(expected.stages);
    return stream.str();
}

std::string reflectedBindingLabel(const luna::RHI::ShaderResourceBinding& reflected)
{
    std::ostringstream stream;
    stream << "name='" << reflected.Name << "' " << bindingLabel(reflected.Set, reflected.Binding)
           << " type=" << descriptorTypeName(reflected.Type) << " count=" << reflected.Count
           << " stage=" << shaderStageNames(reflected.StageFlags) << " usage=" << usageLabel(reflected);
    return stream.str();
}

bool isPushConstantReflectionName(const std::string& name)
{
    return name == "PushConstants" || name == "gPushConstants";
}

} // namespace

ShaderBindingContract makeShaderBindingContract(std::string_view name)
{
    ShaderBindingContract contract;
    contract.name = std::string(name);
    return contract;
}

void addDescriptorSetRequirements(ShaderBindingContract& contract,
                                  uint32_t set_index,
                                  std::string_view set_name,
                                  std::span<const luna::RHI::DescriptorSetLayoutBinding> bindings)
{
    contract.bindings.reserve(contract.bindings.size() + bindings.size());
    for (const luna::RHI::DescriptorSetLayoutBinding& binding : bindings) {
        contract.bindings.push_back(ShaderBindingRequirement{
            .name = {},
            .shader_name = {},
            .set_name = std::string(set_name),
            .logical_set = set_index,
            .logical_binding = binding.Binding,
            .set = set_index,
            .binding = binding.Binding,
            .type = binding.Type,
            .count = binding.Count,
            .stages = binding.StageFlags,
        });
    }
}

void addPushConstantRequirements(ShaderBindingContract& contract,
                                 std::string_view name,
                                 std::span<const luna::RHI::PushConstantRange> push_constants)
{
    contract.push_constants.reserve(contract.push_constants.size() + push_constants.size());
    for (const luna::RHI::PushConstantRange& push_constant : push_constants) {
        contract.push_constants.push_back(ShaderPushConstantRequirement{
            .name = std::string(name),
            .offset = push_constant.Offset,
            .size = push_constant.Size,
            .stages = push_constant.StageFlags,
        });
    }
}

ShaderBindingValidationResult validateShaderBindingContract(const ShaderBindingValidationContext& context,
                                                            const ShaderBindingContract& contract,
                                                            const luna::RHI::ShaderReflectionData& reflection)
{
    ShaderBindingValidationResult result;
    const std::string prefix = shaderPrefix(context, contract);

    std::unordered_map<BindingKey, const ShaderBindingRequirement*, BindingKeyHash> expected_bindings;
    expected_bindings.reserve(contract.bindings.size());
    for (const ShaderBindingRequirement& binding : contract.bindings) {
        expected_bindings.insert_or_assign(BindingKey{binding.set, binding.binding}, &binding);
    }

    std::unordered_map<BindingKey, std::string, BindingKeyHash> reflected_bindings;
    reflected_bindings.reserve(reflection.ResourceBindings.size());

    for (const luna::RHI::ShaderResourceBinding& reflected : reflection.ResourceBindings) {
        if (isPushConstantReflectionName(reflected.Name)) {
            continue;
        }

        const BindingKey key{reflected.Set, reflected.Binding};
        const auto duplicate = reflected_bindings.find(key);
        if (duplicate != reflected_bindings.end()) {
            result.issues.push_back(prefix + " duplicate reflected resource: actual " +
                                    reflectedBindingLabel(reflected) + " previous_name='" + duplicate->second + "'");
            continue;
        }
        reflected_bindings.emplace(key, reflected.Name);

        const auto expected_it = expected_bindings.find(key);
        if (expected_it == expected_bindings.end()) {
            result.issues.push_back(prefix + " unexpected reflected resource: actual " +
                                    reflectedBindingLabel(reflected));
            continue;
        }

        const ShaderBindingRequirement& expected = *expected_it->second;
        if (context.enforce_resource_names && !expected.shader_name.empty() && expected.shader_name != reflected.Name) {
            result.issues.push_back(prefix + " name mismatch: expected " + expectedBindingLabel(expected) +
                                    "; actual " + reflectedBindingLabel(reflected));
        }
        if (expected.type != reflected.Type) {
            result.issues.push_back(prefix + " type mismatch: expected " + expectedBindingLabel(expected) +
                                    "; actual " + reflectedBindingLabel(reflected));
        }
        if (expected.count != reflected.Count) {
            result.issues.push_back(prefix + " count mismatch: expected " + expectedBindingLabel(expected) +
                                    "; actual " + reflectedBindingLabel(reflected));
        }
        if (context.enforce_stage_for_known_usage && reflected.EntryPointUsageKnown && reflected.UsedByEntryPoint &&
            !(expected.stages & context.shader_stage)) {
            result.issues.push_back(prefix + " stage visibility mismatch for used resource: expected " +
                                    expectedBindingLabel(expected) + "; actual " + reflectedBindingLabel(reflected));
        }
    }

    if (reflection.PushConstantSize > 0) {
        bool found_push_constant = false;
        for (const ShaderPushConstantRequirement& expected : contract.push_constants) {
            if ((expected.stages & context.shader_stage) && expected.offset == 0 &&
                expected.size >= reflection.PushConstantSize) {
                found_push_constant = true;
                break;
            }
        }
        if (!found_push_constant) {
            result.issues.push_back(prefix + " push constant mismatch: shader reflected size " +
                                    std::to_string(reflection.PushConstantSize) + " stage " +
                                    shaderStageNames(context.shader_stage) + ", but contract has no compatible range");
        }
    }

    return result;
}

} // namespace luna::render_flow
