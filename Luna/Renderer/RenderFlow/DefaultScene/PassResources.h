#pragma once

#include <Core.h>

namespace luna::RHI {
class DescriptorSet;
class GraphicsPipeline;
class Sampler;
} // namespace luna::RHI

namespace luna::render_flow::default_scene {

struct DrawPassResources {
    luna::RHI::Ref<luna::RHI::GraphicsPipeline> pipeline;
    luna::RHI::Ref<luna::RHI::DescriptorSet> scene_descriptor_set;

    [[nodiscard]] bool isValid() const
    {
        return pipeline && scene_descriptor_set;
    }
};

struct LightingPassResources {
    luna::RHI::Ref<luna::RHI::GraphicsPipeline> pipeline;
    luna::RHI::Ref<luna::RHI::DescriptorSet> gbuffer_descriptor_set;
    luna::RHI::Ref<luna::RHI::DescriptorSet> scene_descriptor_set;
    luna::RHI::Ref<luna::RHI::Sampler> gbuffer_sampler;

    [[nodiscard]] bool isValid() const
    {
        return pipeline && gbuffer_descriptor_set && scene_descriptor_set && gbuffer_sampler;
    }
};

struct DebugViewPassResources {
    luna::RHI::Ref<luna::RHI::GraphicsPipeline> pipeline;
    luna::RHI::Ref<luna::RHI::DescriptorSet> gbuffer_descriptor_set;
    luna::RHI::Ref<luna::RHI::DescriptorSet> scene_descriptor_set;
    luna::RHI::Ref<luna::RHI::Sampler> gbuffer_sampler;

    [[nodiscard]] bool isValid() const
    {
        return pipeline && gbuffer_descriptor_set && scene_descriptor_set && gbuffer_sampler;
    }
};

struct SkyPassResources {
    luna::RHI::Ref<luna::RHI::GraphicsPipeline> pipeline;
    luna::RHI::Ref<luna::RHI::DescriptorSet> gbuffer_descriptor_set;
    luna::RHI::Ref<luna::RHI::DescriptorSet> scene_descriptor_set;
    luna::RHI::Ref<luna::RHI::Sampler> gbuffer_sampler;

    [[nodiscard]] bool isValid() const
    {
        return pipeline && gbuffer_descriptor_set && scene_descriptor_set && gbuffer_sampler;
    }
};

} // namespace luna::render_flow::default_scene
