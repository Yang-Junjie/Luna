#pragma once

// Manages environment textures used by scene lighting.
// Loads fallback or project-provided environment data, prepares upload state,
// and exposes irradiance data needed by scene parameter updates.

#include "Renderer/RenderFlow/DefaultScene/Constants.h"
#include "Renderer/RenderFlow/RenderFlowTypes.h"
#include "Renderer/RenderWorld/RenderTypes.h"
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
    enum class SourceKind : uint8_t {
        DefaultSky,
        TextureAsset,
    };

    struct SourceSignature {
        SourceKind kind{SourceKind::DefaultSky};
        AssetHandle texture_handle{AssetHandle(0)};
        glm::vec3 procedural_sun_direction{0.51214755f, 0.76822126f, 0.38411063f};
        float procedural_sun_intensity{20.0f};
        float procedural_sun_angular_radius{0.02f};
        glm::vec3 procedural_sky_color_zenith{0.15f, 0.30f, 0.60f};
        glm::vec3 procedural_sky_color_horizon{0.60f, 0.50f, 0.40f};
        glm::vec3 procedural_ground_color{0.10f, 0.08f, 0.06f};
        float procedural_sky_exposure{1.5f};
    };

    void reset();
    void ensure(const SceneRenderContext& context,
                const RenderEnvironment* environment,
                const SceneShaderPaths& shader_paths);
    void uploadIfNeeded(RHI::CommandBufferEncoder& commands);
    void precomputeIfNeeded(RHI::CommandBufferEncoder& commands);

    [[nodiscard]] const renderer_detail::PendingTextureUpload& sourceTexture() const noexcept
    {
        return m_source_texture;
    }

    [[nodiscard]] const std::array<glm::vec4, 9>& irradianceSH() const noexcept
    {
        return m_irradiance_sh;
    }

    [[nodiscard]] const RHI::Ref<RHI::Texture>& prefilteredTexture() const noexcept
    {
        return m_prefiltered_texture;
    }

    [[nodiscard]] const RHI::Ref<RHI::Texture>& brdfLutTexture() const noexcept
    {
        return m_brdf_lut_texture;
    }

    [[nodiscard]] float prefilteredMaxMipLevel() const noexcept;

    [[nodiscard]] bool hasPrecomputedIbl() const noexcept
    {
        return m_precomputed;
    }

private:
    RHI::Ref<RHI::Device> m_device;
    RHI::BackendType m_backend_type{RHI::BackendType::Auto};
    SourceSignature m_source_signature{};
    bool m_has_source_signature{false};
    renderer_detail::PendingTextureUpload m_source_texture;
    std::array<glm::vec4, 9> m_irradiance_sh{};

    RHI::Ref<RHI::Texture> m_environment_cube_texture;
    RHI::Ref<RHI::Texture> m_irradiance_texture;
    RHI::Ref<RHI::Texture> m_prefiltered_texture;
    RHI::Ref<RHI::Texture> m_brdf_lut_texture;

    RHI::Ref<RHI::TextureView> m_environment_cube_uav;
    RHI::Ref<RHI::TextureView> m_irradiance_uav;
    std::array<RHI::Ref<RHI::TextureView>, render_flow::default_scene_detail::kEnvironmentPrefilterMipLevels>
        m_prefiltered_uavs{};
    RHI::Ref<RHI::TextureView> m_brdf_lut_uav;

    RHI::Ref<RHI::DescriptorSetLayout> m_equirect_to_cube_layout;
    RHI::Ref<RHI::DescriptorSetLayout> m_cube_filter_layout;
    RHI::Ref<RHI::DescriptorSetLayout> m_prefilter_layout;
    RHI::Ref<RHI::DescriptorSetLayout> m_brdf_lut_layout;
    RHI::Ref<RHI::PipelineLayout> m_equirect_to_cube_pipeline_layout;
    RHI::Ref<RHI::PipelineLayout> m_cube_filter_pipeline_layout;
    RHI::Ref<RHI::PipelineLayout> m_prefilter_pipeline_layout;
    RHI::Ref<RHI::PipelineLayout> m_brdf_lut_pipeline_layout;
    RHI::Ref<RHI::DescriptorPool> m_descriptor_pool;
    RHI::Ref<RHI::DescriptorSet> m_equirect_to_cube_descriptor_set;
    RHI::Ref<RHI::DescriptorSet> m_irradiance_descriptor_set;
    std::array<RHI::Ref<RHI::DescriptorSet>, render_flow::default_scene_detail::kEnvironmentPrefilterMipLevels>
        m_prefilter_descriptor_sets{};
    RHI::Ref<RHI::DescriptorSet> m_brdf_lut_descriptor_set;
    RHI::Ref<RHI::Sampler> m_sampler;

    RHI::Ref<RHI::ShaderModule> m_equirect_to_cube_shader;
    RHI::Ref<RHI::ShaderModule> m_irradiance_shader;
    RHI::Ref<RHI::ShaderModule> m_prefilter_shader;
    RHI::Ref<RHI::ShaderModule> m_brdf_lut_shader;
    RHI::Ref<RHI::ComputePipeline> m_equirect_to_cube_pipeline;
    RHI::Ref<RHI::ComputePipeline> m_irradiance_pipeline;
    RHI::Ref<RHI::ComputePipeline> m_prefilter_pipeline;
    RHI::Ref<RHI::ComputePipeline> m_brdf_lut_pipeline;

    bool m_precomputed{false};
};

} // namespace luna::render_flow::default_scene
