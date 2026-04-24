#pragma once

// Owns the long-lived GPU objects required by the scene renderer.
// This includes pipelines, descriptor layouts, descriptor sets, and scene-wide buffers,
// but not uploaded mesh/material asset caches.

#include "Renderer/SceneRenderer/SceneRenderer.h"
#include "Renderer/SceneRenderer/SceneRendererDrawQueue.h"

#include <array>
#include <optional>

namespace luna {
class Camera;
}

namespace luna::RHI {
class Buffer;
class DescriptorPool;
class DescriptorSet;
class DescriptorSetLayout;
class GraphicsPipeline;
class PipelineLayout;
class Sampler;
class ShaderModule;
} // namespace luna::RHI

namespace luna::scene_renderer {

class PipelineLibrary final {
public:
    void shutdown();
    [[nodiscard]] bool hasCompleteState(const SceneRenderer::RenderContext& context) const noexcept;
    void rebuild(const SceneRenderer::RenderContext& context, const SceneRenderer::ShaderPaths& shader_paths);
    void updateSceneBindings(const luna::RHI::Ref<luna::RHI::Texture>& environment_texture);
    void updateSceneParameters(const SceneRenderer::RenderContext& context,
                               const Camera& camera,
                               const DrawQueue& draw_queue,
                               float environment_mip_count,
                               const std::array<glm::vec4, 9>& irradiance_sh);
    void updateLightingResources(const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_base_color,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_normal_metallic,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_world_position_roughness,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_emissive_ao,
                                 const luna::RHI::Ref<luna::RHI::Texture>& pick_texture);

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Device>& device() const noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::DescriptorPool>& descriptorPool() const noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::DescriptorSetLayout>& materialLayout() const noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& geometryPipeline() const noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& lightingPipeline() const noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& transparentPipeline() const noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::DescriptorSet>& sceneDescriptorSet() const noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::DescriptorSet>& gbufferDescriptorSet() const noexcept;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Sampler>& gbufferSampler() const noexcept;

private:
    struct State {
        luna::RHI::Ref<luna::RHI::Device> device;
        luna::RHI::BackendType backend_type{luna::RHI::BackendType::Auto};
        luna::RHI::Format surface_format{luna::RHI::Format::UNDEFINED};

        luna::RHI::Ref<luna::RHI::GraphicsPipeline> geometry_pipeline;
        luna::RHI::Ref<luna::RHI::GraphicsPipeline> lighting_pipeline;
        luna::RHI::Ref<luna::RHI::GraphicsPipeline> transparent_pipeline;

        luna::RHI::Ref<luna::RHI::PipelineLayout> geometry_pipeline_layout;
        luna::RHI::Ref<luna::RHI::PipelineLayout> lighting_pipeline_layout;
        luna::RHI::Ref<luna::RHI::PipelineLayout> transparent_pipeline_layout;

        luna::RHI::Ref<luna::RHI::DescriptorSetLayout> material_layout;
        luna::RHI::Ref<luna::RHI::DescriptorSetLayout> gbuffer_layout;
        luna::RHI::Ref<luna::RHI::DescriptorSetLayout> scene_layout;
        luna::RHI::Ref<luna::RHI::DescriptorPool> descriptor_pool;

        luna::RHI::Ref<luna::RHI::Sampler> gbuffer_sampler;
        luna::RHI::Ref<luna::RHI::Sampler> environment_source_sampler;

        luna::RHI::Ref<luna::RHI::DescriptorSet> gbuffer_descriptor_set;
        luna::RHI::Ref<luna::RHI::DescriptorSet> scene_descriptor_set;
        luna::RHI::Ref<luna::RHI::Buffer> scene_params_buffer;

        luna::RHI::Ref<luna::RHI::ShaderModule> geometry_vertex_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> geometry_fragment_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> lighting_vertex_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> lighting_fragment_shader;
        luna::RHI::Ref<luna::RHI::ShaderModule> transparent_fragment_shader;
    };

    void reset() noexcept;

private:
    State m_state{};
};

} // namespace luna::scene_renderer
