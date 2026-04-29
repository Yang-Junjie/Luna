#pragma once

// Concrete GPU state for the default scene feature.
// Owns pipelines, descriptor layouts, descriptor sets, samplers, and scene-wide buffers.
// Rebuild policy and shader path defaults stay in PipelineResources.

#include "Renderer/RenderFlow/RenderFlowTypes.h"
#include "Renderer/RenderFlow/DefaultScene/DrawQueue.h"
#include "Renderer/RenderFlow/DefaultScene/GpuTypes.h"
#include "Renderer/RenderFlow/DefaultScene/PassResources.h"
#include "Renderer/RenderFlow/LightingExtensionInputs.h"
#include "Renderer/Resources/TextureUpload.h"

#include <array>
#include <optional>

namespace luna {
class Camera;
class RenderWorld;
}

namespace luna::RHI {
class Buffer;
class CommandBufferEncoder;
class DescriptorPool;
class DescriptorSet;
class DescriptorSetLayout;
class GraphicsPipeline;
class PipelineLayout;
class Sampler;
class ShaderModule;
class Texture;
} // namespace luna::RHI

namespace luna::render_flow::default_scene {

class PipelineState final {
public:
    void shutdown();
    [[nodiscard]] bool hasAnyState() const noexcept;
    [[nodiscard]] bool hasCompleteState(const SceneRenderContext& context) const noexcept;
    void rebuild(const SceneRenderContext& context, const SceneShaderPaths& shader_paths);
    void updateSceneBindings(const luna::RHI::Ref<luna::RHI::Texture>& environment_texture,
                             const luna::RHI::Ref<luna::RHI::Texture>& prefiltered_environment_texture,
                             const luna::RHI::Ref<luna::RHI::Texture>& brdf_lut_texture);
    void updateSceneParameters(const SceneRenderContext& context,
                               const RenderWorld& world,
                               const RenderFeatureFrameContext& frame_context,
                               float environment_mip_count,
                               const std::array<glm::vec4, 9>& irradiance_sh,
                               const render_flow::default_scene_detail::ShadowRenderParams& shadow_params);
    void updateLightingResources(luna::RHI::CommandBufferEncoder& commands,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_base_color,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_normal_metallic,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_world_position_roughness,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_emissive_ao,
                                 const luna::RHI::Ref<luna::RHI::Texture>& velocity_texture,
                                 const luna::RHI::Ref<luna::RHI::Texture>& pick_texture,
                                 const luna::render_flow::LightingExtensionTextureRefs& lighting_extensions);
    void updateShadowResources(const luna::RHI::Ref<luna::RHI::Texture>& shadow_map);

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Device>& device() const noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::DescriptorPool>& descriptorPool() const noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::DescriptorSetLayout>& materialLayout() const noexcept;
    [[nodiscard]] DrawPassResources geometryPassResources() const noexcept;
    [[nodiscard]] DrawPassResources shadowPassResources() const noexcept;
    [[nodiscard]] DrawPassResources transparentPassResources() const noexcept;
    [[nodiscard]] LightingPassResources lightingPassResources() const noexcept;
    [[nodiscard]] DebugViewPassResources debugViewPassResources() const noexcept;

private:
    struct State {
        luna::RHI::Ref<luna::RHI::Device> device;
        luna::RHI::BackendType backend_type{luna::RHI::BackendType::Auto};
        luna::RHI::Format surface_format{luna::RHI::Format::UNDEFINED};

        luna::RHI::Ref<luna::RHI::GraphicsPipeline> geometry_pipeline;
        luna::RHI::Ref<luna::RHI::GraphicsPipeline> shadow_pipeline;
        luna::RHI::Ref<luna::RHI::GraphicsPipeline> lighting_pipeline;
        luna::RHI::Ref<luna::RHI::GraphicsPipeline> debug_view_pipeline;
        luna::RHI::Ref<luna::RHI::GraphicsPipeline> transparent_pipeline;

        luna::RHI::Ref<luna::RHI::PipelineLayout> geometry_pipeline_layout;
        luna::RHI::Ref<luna::RHI::PipelineLayout> shadow_pipeline_layout;
        luna::RHI::Ref<luna::RHI::PipelineLayout> lighting_pipeline_layout;
        luna::RHI::Ref<luna::RHI::PipelineLayout> transparent_pipeline_layout;

        luna::RHI::Ref<luna::RHI::DescriptorSetLayout> material_layout;
        luna::RHI::Ref<luna::RHI::DescriptorSetLayout> gbuffer_layout;
        luna::RHI::Ref<luna::RHI::DescriptorSetLayout> scene_layout;
        luna::RHI::Ref<luna::RHI::DescriptorPool> descriptor_pool;

        luna::RHI::Ref<luna::RHI::Sampler> gbuffer_sampler;
        luna::RHI::Ref<luna::RHI::Sampler> environment_source_sampler;
        luna::RHI::Ref<luna::RHI::Sampler> shadow_sampler;

        luna::RHI::Ref<luna::RHI::DescriptorSet> gbuffer_descriptor_set;
        luna::RHI::Ref<luna::RHI::DescriptorSet> scene_descriptor_set;
        luna::RHI::Ref<luna::RHI::DescriptorSet> lighting_scene_descriptor_set;
        luna::RHI::Ref<luna::RHI::Buffer> scene_params_buffer;

        luna::RHI::Ref<luna::RHI::Texture> bound_environment_texture;
        luna::RHI::Ref<luna::RHI::Texture> bound_prefiltered_environment_texture;
        luna::RHI::Ref<luna::RHI::Texture> bound_brdf_lut_texture;
        luna::RHI::Ref<luna::RHI::Texture> bound_shadow_map_texture;
        renderer_detail::PendingTextureUpload default_ambient_occlusion_texture;
        renderer_detail::PendingTextureUpload default_reflection_texture;
        renderer_detail::PendingTextureUpload default_indirect_diffuse_texture;
        renderer_detail::PendingTextureUpload default_indirect_specular_texture;
        bool scene_bindings_valid{false};
        bool shadow_bindings_valid{false};

        luna::RHI::Ref<luna::RHI::ShaderModule> geometry_vertex_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> transparent_vertex_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> shadow_vertex_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> shadow_fragment_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> geometry_fragment_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> lighting_vertex_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> lighting_fragment_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> debug_view_fragment_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> transparent_fragment_shader;
    };

    void reset() noexcept;

private:
    State m_state{};
};

} // namespace luna::render_flow::default_scene






