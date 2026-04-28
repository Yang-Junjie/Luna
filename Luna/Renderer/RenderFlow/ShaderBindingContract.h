#pragma once

#include <cstdint>

#include <DescriptorSetLayout.h>
#include <PipelineDefs.h>
#include <ShaderModule.h>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace luna::render_flow {

struct ShaderBindingRequirement {
    std::string name;
    std::string shader_name;
    std::string set_name;
    uint32_t logical_set{0};
    uint32_t logical_binding{0};
    uint32_t set{0};
    uint32_t binding{0};
    luna::RHI::DescriptorType type{luna::RHI::DescriptorType::UniformBuffer};
    uint32_t count{1};
    luna::RHI::ShaderStage stages{luna::RHI::ShaderStage::AllGraphics};
};

struct ShaderPushConstantRequirement {
    std::string name;
    uint32_t offset{0};
    uint32_t size{0};
    luna::RHI::ShaderStage stages{luna::RHI::ShaderStage::None};
};

struct ShaderBindingContract {
    std::string name;
    std::vector<ShaderBindingRequirement> bindings;
    std::vector<ShaderPushConstantRequirement> push_constants;
};

enum class ShaderBindingAddressMode : uint8_t {
    LogicalSetBinding,
    FlattenedRegisterSpace,
};

struct ShaderBindingValidationContext {
    std::string_view shader_file;
    std::string_view entry_point;
    luna::RHI::ShaderStage shader_stage{luna::RHI::ShaderStage::None};
    bool enforce_stage_for_known_usage{true};
    bool enforce_resource_names{true};
};

struct ShaderBindingValidationResult {
    std::vector<std::string> issues;

    [[nodiscard]] bool valid() const noexcept
    {
        return issues.empty();
    }
};

[[nodiscard]] ShaderBindingContract makeShaderBindingContract(std::string_view name);

void addDescriptorSetRequirements(ShaderBindingContract& contract,
                                  uint32_t set_index,
                                  std::string_view set_name,
                                  std::span<const luna::RHI::DescriptorSetLayoutBinding> bindings);

void addPushConstantRequirements(ShaderBindingContract& contract,
                                 std::string_view name,
                                 std::span<const luna::RHI::PushConstantRange> push_constants);

[[nodiscard]] ShaderBindingValidationResult
    validateShaderBindingContract(const ShaderBindingValidationContext& context,
                                  const ShaderBindingContract& contract,
                                  const luna::RHI::ShaderReflectionData& reflection);

} // namespace luna::render_flow
