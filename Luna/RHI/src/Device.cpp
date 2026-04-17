#include <Adapter.h>
#include <DescriptorSetLayout.h>
#include <Device.h>
#include <iostream>
#include <Pipeline.h>

namespace luna::RHI {
bool Device::ValidateGraphicsPipeline(const GraphicsPipelineCreateInfo& info) const
{
    auto adapter = GetParentAdapter();
    if (!adapter) {
        return true;
    }
    auto limits = adapter->QueryLimits();

    if (info.ColorAttachmentFormats.size() > limits.maxColorAttachments) {
        throw std::runtime_error(
            "[Luna RHI] Pipeline validation FAILED: " + std::to_string(info.ColorAttachmentFormats.size()) +
            " color attachments exceeds device limit of " + std::to_string(limits.maxColorAttachments));
    }

    if (info.Multisample.RasterizationSamples > limits.maxMSAASamples) {
        throw std::runtime_error("[Luna RHI] Pipeline validation FAILED: MSAA " +
                                 std::to_string(info.Multisample.RasterizationSamples) + "x exceeds device limit of " +
                                 std::to_string(limits.maxMSAASamples) + "x");
    }

    if (info.Shaders.empty()) {
        throw std::runtime_error("[Luna RHI] Pipeline validation FAILED: No shaders provided");
    }

    for (size_t i = 0; i < info.Shaders.size(); i++) {
        if (!info.Shaders[i]) {
            throw std::runtime_error("[Luna RHI] Pipeline validation FAILED: Shader[" + std::to_string(i) + "] is null");
        }
    }

    if (!info.Layout) {
        throw std::runtime_error("[Luna RHI] Pipeline validation FAILED: No pipeline layout provided");
    }

    return true;
}

bool Device::ValidateDescriptorSetLayout(const DescriptorSetLayoutCreateInfo& info) const
{
    auto adapter = GetParentAdapter();
    if (!adapter) {
        return true;
    }
    auto limits = adapter->QueryLimits();

    if (!limits.supportsStorageBufferWriteInGraphics) {
        for (auto& binding : info.Bindings) {
            if (binding.Type == DescriptorType::StorageBuffer && (binding.StageFlags & ShaderStage::Fragment)) {
                throw std::runtime_error(
                    "[Luna RHI] DescriptorSet validation FAILED: StorageBuffer write in fragment shader "
                    "is not supported on this backend (Tier 2). "
                    "Use StorageBuffer only in compute shaders, or check "
                    "adapter->QueryLimits().supportsStorageBufferWriteInGraphics before binding.");
            }
        }
    }

    return true;
}
} // namespace luna::RHI
