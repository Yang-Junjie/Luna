#include "Renderer/RenderFlow/RenderFeatureSupport.h"

#include <string_view>
#include <utility>

namespace luna::render_flow {
namespace {

void require(bool condition, RenderFeatureSupportResult& result, std::string reason)
{
    if (condition) {
        return;
    }

    result.supported = false;
    result.reasons.push_back(std::move(reason));
}

void addDeferred(RenderFeatureSupportResult& result, std::string reason)
{
    result.deferred_checks.push_back(std::move(reason));
}

void requireCapability(RenderFeatureRHICapabilityFlags flags,
                       RenderFeatureRHICapabilityFlags flag,
                       bool supported,
                       RenderFeatureSupportResult& result,
                       std::string_view name)
{
    if (!(flags & flag)) {
        return;
    }

    require(supported, result, "missing RHI capability: " + std::string(name));
}

void requireResource(RenderFeatureResourceFlags flags,
                     RenderFeatureResourceFlags flag,
                     bool supported,
                     RenderFeatureSupportResult& result,
                     std::string_view name)
{
    if (!(flags & flag)) {
        return;
    }

    require(supported, result, "missing RHI resource support: " + std::string(name));
}

void evaluateSceneInputs(RenderFeatureSceneInputFlags inputs,
                         const SceneRenderContext& scene_context,
                         RenderFeatureSupportResult& result)
{
    if (inputs & RenderFeatureSceneInputFlags::SceneColor) {
        require(scene_context.color_target.isValid(), result, "missing scene input: Scene.Color");
    }
    if (inputs & RenderFeatureSceneInputFlags::Depth) {
        require(scene_context.depth_target.isValid(), result, "missing scene input: Scene.Depth");
    }
    if (inputs & RenderFeatureSceneInputFlags::GBufferBaseColor) {
        addDeferred(result, "graph input required: Scene.GBuffer.BaseColor");
    }
    if (inputs & RenderFeatureSceneInputFlags::GBufferNormalMetallic) {
        addDeferred(result, "graph input required: Scene.GBuffer.NormalMetallic");
    }
    if (inputs & RenderFeatureSceneInputFlags::GBufferWorldPositionRoughness) {
        addDeferred(result, "graph input required: Scene.GBuffer.WorldPositionRoughness");
    }
    if (inputs & RenderFeatureSceneInputFlags::GBufferEmissiveAo) {
        addDeferred(result, "graph input required: Scene.GBuffer.EmissiveAo");
    }
    if (inputs & RenderFeatureSceneInputFlags::Velocity) {
        addDeferred(result, "graph input required: Scene.Velocity");
    }
    if (inputs & RenderFeatureSceneInputFlags::ShadowMap) {
        addDeferred(result, "graph input required: Scene.ShadowMap");
    }
}

std::string join(const std::vector<std::string>& values)
{
    std::string result;
    for (size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            result += "; ";
        }
        result += values[index];
    }
    return result;
}

} // namespace

RenderFeatureSupportResult evaluateRenderFeatureRequirements(const RenderFeatureRequirements& requirements,
                                                             const SceneRenderContext& scene_context)
{
    RenderFeatureSupportResult result{};
    const luna::RHI::RHICapabilities& capabilities = scene_context.capabilities;

    if (requirements.requires_framebuffer_size) {
        require(scene_context.framebuffer_width > 0 && scene_context.framebuffer_height > 0,
                result,
                "missing framebuffer size");
    }

    evaluateSceneInputs(requirements.scene_inputs, scene_context, result);

    const RenderFeatureRHICapabilityFlags rhi_flags = requirements.rhi_capabilities;
    requireCapability(rhi_flags,
                      RenderFeatureRHICapabilityFlags::DefaultRenderFlow,
                      capabilities.supports_default_render_flow,
                      result,
                      "DefaultRenderFlow");
    requireCapability(rhi_flags, RenderFeatureRHICapabilityFlags::ImGui, capabilities.supports_imgui, result, "ImGui");
    requireCapability(rhi_flags,
                      RenderFeatureRHICapabilityFlags::ScenePickReadback,
                      capabilities.supports_scene_pick_readback,
                      result,
                      "ScenePickReadback");
    requireCapability(rhi_flags,
                      RenderFeatureRHICapabilityFlags::GpuTimestamp,
                      capabilities.supports_gpu_timestamp,
                      result,
                      "GpuTimestamp");

    const RenderFeatureResourceFlags resource_flags = requirements.resources;
    requireResource(resource_flags,
                    RenderFeatureResourceFlags::GraphicsPipeline,
                    capabilities.supports_graphics_pipeline,
                    result,
                    "GraphicsPipeline");
    requireResource(resource_flags,
                    RenderFeatureResourceFlags::ComputePipeline,
                    capabilities.supports_compute_pipeline,
                    result,
                    "ComputePipeline");
    requireResource(resource_flags,
                    RenderFeatureResourceFlags::SampledTexture,
                    capabilities.supports_sampled_texture,
                    result,
                    "SampledTexture");
    requireResource(resource_flags,
                    RenderFeatureResourceFlags::StorageTexture,
                    capabilities.supports_storage_texture,
                    result,
                    "StorageTexture");
    requireResource(resource_flags,
                    RenderFeatureResourceFlags::ColorAttachment,
                    capabilities.supports_color_attachment,
                    result,
                    "ColorAttachment");
    requireResource(resource_flags,
                    RenderFeatureResourceFlags::DepthAttachment,
                    capabilities.supports_depth_attachment,
                    result,
                    "DepthAttachment");
    requireResource(resource_flags,
                    RenderFeatureResourceFlags::UniformBuffer,
                    capabilities.supports_uniform_buffer,
                    result,
                    "UniformBuffer");
    requireResource(resource_flags,
                    RenderFeatureResourceFlags::StorageBuffer,
                    capabilities.supports_storage_buffer,
                    result,
                    "StorageBuffer");
    requireResource(
        resource_flags, RenderFeatureResourceFlags::Sampler, capabilities.supports_sampler, result, "Sampler");

    return result;
}

RenderFeatureSupportResult evaluateRenderFeatureSupport(const IRenderFeature& feature,
                                                        const SceneRenderContext& scene_context)
{
    return evaluateRenderFeatureRequirements(feature.contract().requirements, scene_context);
}

std::string summarizeRenderFeatureSupport(const RenderFeatureSupportResult& result)
{
    if (!result.supported) {
        return join(result.reasons);
    }
    return "supported";
}

} // namespace luna::render_flow
