#pragma once

// Manages environment textures used by scene lighting.
// Loads fallback or project-provided environment data, prepares upload state,
// and exposes irradiance data needed by scene parameter updates.

#include "Renderer/RenderFlow/DefaultScene/Constants.h"
#include "Renderer/RenderFlow/RenderFlowTypes.h"
#include "Renderer/Resources/TextureUpload.h"

#include <array>

namespace luna::RHI {
class ComputePipeline;
class DescriptorPool;
class DescriptorSet;
class DescriptorSetLayout;
class PipelineLayout;
class Sampler;
class ShaderModule;
class Texture;
class TextureView;
} // namespace luna::RHI

namespace luna::render_flow::default_scene {

class EnvironmentResources final {
public:
    void reset();
    void ensure(const SceneRenderContext& context);
    void uploadIfNeeded(luna::RHI::CommandBufferEncoder& commands);
    void precomputeIfNeeded(luna::RHI::CommandBufferEncoder& commands);

    [[nodiscard]] const renderer_detail::PendingTextureUpload& sourceTexture() const noexcept
    {
        return m_source_texture;
    }

    [[nodiscard]] const std::array<glm::vec4, 9>& irradianceSH() const noexcept
    {
        return m_irradiance_sh;
    }

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Texture>& prefilteredTexture() const noexcept
    {
        return m_prefiltered_texture;
    }

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Texture>& brdfLutTexture() const noexcept
    {
        return m_brdf_lut_texture;
    }

    [[nodiscard]] float prefilteredMaxMipLevel() const noexcept;

    [[nodiscard]] bool hasPrecomputedIbl() const noexcept
    {
        return m_precomputed;
    }

private:
    luna::RHI::Ref<luna::RHI::Device> m_device;
    luna::RHI::BackendType m_backend_type{luna::RHI::BackendType::Auto};
    renderer_detail::PendingTextureUpload m_source_texture;
    std::array<glm::vec4, 9> m_irradiance_sh{};

    luna::RHI::Ref<luna::RHI::Texture> m_environment_cube_texture;
    luna::RHI::Ref<luna::RHI::Texture> m_irradiance_texture;
    luna::RHI::Ref<luna::RHI::Texture> m_prefiltered_texture;
    luna::RHI::Ref<luna::RHI::Texture> m_brdf_lut_texture;

    luna::RHI::Ref<luna::RHI::TextureView> m_environment_cube_uav;
    luna::RHI::Ref<luna::RHI::TextureView> m_irradiance_uav;
    std::array<luna::RHI::Ref<luna::RHI::TextureView>,
               render_flow::default_scene_detail::kEnvironmentPrefilterMipLevels>
        m_prefiltered_uavs{};
    luna::RHI::Ref<luna::RHI::TextureView> m_brdf_lut_uav;

    luna::RHI::Ref<luna::RHI::DescriptorSetLayout> m_equirect_to_cube_layout;
    luna::RHI::Ref<luna::RHI::DescriptorSetLayout> m_cube_filter_layout;
    luna::RHI::Ref<luna::RHI::DescriptorSetLayout> m_brdf_lut_layout;
    luna::RHI::Ref<luna::RHI::PipelineLayout> m_equirect_to_cube_pipeline_layout;
    luna::RHI::Ref<luna::RHI::PipelineLayout> m_cube_filter_pipeline_layout;
    luna::RHI::Ref<luna::RHI::PipelineLayout> m_brdf_lut_pipeline_layout;
    luna::RHI::Ref<luna::RHI::DescriptorPool> m_descriptor_pool;
    luna::RHI::Ref<luna::RHI::DescriptorSet> m_equirect_to_cube_descriptor_set;
    luna::RHI::Ref<luna::RHI::DescriptorSet> m_irradiance_descriptor_set;
    std::array<luna::RHI::Ref<luna::RHI::DescriptorSet>,
               render_flow::default_scene_detail::kEnvironmentPrefilterMipLevels>
        m_prefilter_descriptor_sets{};
    luna::RHI::Ref<luna::RHI::DescriptorSet> m_brdf_lut_descriptor_set;
    luna::RHI::Ref<luna::RHI::Sampler> m_sampler;

    luna::RHI::Ref<luna::RHI::ShaderModule> m_equirect_to_cube_shader;
    luna::RHI::Ref<luna::RHI::ShaderModule> m_irradiance_shader;
    luna::RHI::Ref<luna::RHI::ShaderModule> m_prefilter_shader;
    luna::RHI::Ref<luna::RHI::ShaderModule> m_brdf_lut_shader;
    luna::RHI::Ref<luna::RHI::ComputePipeline> m_equirect_to_cube_pipeline;
    luna::RHI::Ref<luna::RHI::ComputePipeline> m_irradiance_pipeline;
    luna::RHI::Ref<luna::RHI::ComputePipeline> m_prefilter_pipeline;
    luna::RHI::Ref<luna::RHI::ComputePipeline> m_brdf_lut_pipeline;

    bool m_precomputed{false};
};

} // namespace luna::render_flow::default_scene
