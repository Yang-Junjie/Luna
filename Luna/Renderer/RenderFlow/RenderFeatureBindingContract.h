#pragma once

#include "Renderer/RenderFlow/ShaderBindingContract.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>

#include <DescriptorSetLayout.h>
#include <ShaderModule.h>

namespace luna::render_flow {

struct RenderFeatureDescriptorBinding {
    std::string_view name;
    std::string_view shader_name;
    uint32_t binding{0};
    luna::RHI::DescriptorType type{luna::RHI::DescriptorType::UniformBuffer};
    uint32_t count{1};
    luna::RHI::ShaderStage stages{luna::RHI::ShaderStage::AllGraphics};
};

struct RenderFeatureDescriptorSetContract {
    std::string_view contract_name;
    std::string_view set_name;
    uint32_t logical_set{0};
    uint32_t set{0};
    std::span<const RenderFeatureDescriptorBinding> bindings;
};

[[nodiscard]] luna::RHI::DescriptorSetLayoutCreateInfo makeRenderFeatureDescriptorSetLayoutCreateInfo(
    std::span<const RenderFeatureDescriptorBinding> bindings);

[[nodiscard]] ShaderBindingContract makeRenderFeatureShaderBindingContract(
    const RenderFeatureDescriptorSetContract& descriptor_set);

[[nodiscard]] ShaderBindingValidationResult validateRenderFeatureShaderModuleBindings(
    const luna::RHI::Ref<luna::RHI::ShaderModule>& shader,
    const ShaderBindingContract& contract,
    const std::filesystem::path& shader_file,
    std::string_view entry_point);

bool validateAndLogRenderFeatureShaderModuleBindings(const luna::RHI::Ref<luna::RHI::ShaderModule>& shader,
                                                     const ShaderBindingContract& contract,
                                                     const std::filesystem::path& shader_file,
                                                     std::string_view entry_point);

} // namespace luna::render_flow
