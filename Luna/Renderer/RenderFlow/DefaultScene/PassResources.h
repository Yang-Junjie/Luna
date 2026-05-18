#pragma once

#include <Core.h>

namespace luna::RHI {
class DescriptorSet;
class GraphicsPipeline;
class Sampler;
} // namespace luna::RHI

namespace luna::render_flow::default_scene {

struct DrawPassResources {
    RHI::Ref<RHI::GraphicsPipeline> pipeline;
    RHI::Ref<RHI::DescriptorSet> scene_descriptor_set;

    [[nodiscard]] bool isValid() const
    {
        return pipeline && scene_descriptor_set;
    }
};

struct LightingPassResources {
    RHI::Ref<RHI::GraphicsPipeline> pipeline;
    RHI::Ref<RHI::DescriptorSet> gbuffer_descriptor_set;
    RHI::Ref<RHI::DescriptorSet> scene_descriptor_set;
    RHI::Ref<RHI::Sampler> gbuffer_sampler;

    [[nodiscard]] bool isValid() const
    {
        return pipeline && gbuffer_descriptor_set && scene_descriptor_set && gbuffer_sampler;
    }
};

struct DebugViewPassResources {
    RHI::Ref<RHI::GraphicsPipeline> pipeline;
    RHI::Ref<RHI::DescriptorSet> gbuffer_descriptor_set;
    RHI::Ref<RHI::DescriptorSet> scene_descriptor_set;
    RHI::Ref<RHI::Sampler> gbuffer_sampler;

    [[nodiscard]] bool isValid() const
    {
        return pipeline && gbuffer_descriptor_set && scene_descriptor_set && gbuffer_sampler;
    }
};

struct SkyPassResources {
    RHI::Ref<RHI::GraphicsPipeline> pipeline;
    RHI::Ref<RHI::DescriptorSet> gbuffer_descriptor_set;
    RHI::Ref<RHI::DescriptorSet> scene_descriptor_set;
    RHI::Ref<RHI::Sampler> gbuffer_sampler;

    [[nodiscard]] bool isValid() const
    {
        return pipeline && gbuffer_descriptor_set && scene_descriptor_set && gbuffer_sampler;
    }
};

struct TransparentCompositePassResources {
    RHI::Ref<RHI::GraphicsPipeline> pipeline;
    RHI::Ref<RHI::DescriptorSet> descriptor_set;
    RHI::Ref<RHI::Sampler> sampler;

    [[nodiscard]] bool isValid() const
    {
        return pipeline && descriptor_set && sampler;
    }
};

struct PostProcessPassResources {
    RHI::Ref<RHI::GraphicsPipeline> pipeline;
    RHI::Ref<RHI::DescriptorSet> descriptor_set;
    RHI::Ref<RHI::Sampler> sampler;

    [[nodiscard]] bool isValid() const
    {
        return pipeline && descriptor_set && sampler;
    }
};

} // namespace luna::render_flow::default_scene
