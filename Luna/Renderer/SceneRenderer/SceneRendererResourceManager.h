#pragma once

// Coordinates the internal subsystems behind SceneRenderer.
// Serves as the boundary that ties together pipeline state, environment resources,
// and uploaded draw assets so passes can ask for ready-to-use GPU resources.

#include "Renderer/SceneRenderer/SceneRenderer.h"
#include "Renderer/SceneRenderer/SceneRendererDrawQueue.h"

#include <memory>
#include <optional>

namespace luna::RHI {
class Buffer;
class CommandBufferEncoder;
class DescriptorSet;
class GraphicsPipeline;
class Sampler;
class Texture;
} // namespace luna::RHI

namespace luna::scene_renderer {

class DrawQueue;
struct DrawCommand;

struct ResolvedDrawResources {
    luna::RHI::Ref<luna::RHI::Buffer> vertex_buffer;
    luna::RHI::Ref<luna::RHI::Buffer> index_buffer;
    luna::RHI::Ref<luna::RHI::DescriptorSet> material_descriptor_set;
    uint32_t index_count{0};

    [[nodiscard]] bool isValid() const
    {
        return vertex_buffer && index_buffer && material_descriptor_set && index_count > 0;
    }
};

class ResourceManager final {
public:
    ResourceManager();
    ~ResourceManager();

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    void setShaderPaths(SceneRenderer::ShaderPaths shader_paths);
    void shutdown();

    void ensurePipelines(const SceneRenderer::RenderContext& context);
    void prepareOpaqueDraws(luna::RHI::CommandBufferEncoder& commands,
                            const DrawQueue& draw_queue,
                            const Material& default_material);
    void prepareTransparentDraws(luna::RHI::CommandBufferEncoder& commands,
                                 const DrawQueue& draw_queue,
                                 const Material& default_material);
    void uploadEnvironmentIfNeeded(luna::RHI::CommandBufferEncoder& commands);
    void updateSceneParameters(const SceneRenderer::RenderContext& context,
                               const Camera& camera,
                               const DrawQueue& draw_queue);
    void updateLightingResources(const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_base_color,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_normal_metallic,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_world_position_roughness,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_emissive_ao,
                                 const luna::RHI::Ref<luna::RHI::Texture>& pick_texture);

    [[nodiscard]] ResolvedDrawResources resolveDrawResources(const DrawCommand& draw_command,
                                                             const Material& default_material) const;

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& geometryPipeline() const;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& lightingPipeline() const;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& transparentPipeline() const;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::DescriptorSet>& sceneDescriptorSet() const;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::DescriptorSet>& gbufferDescriptorSet() const;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Sampler>& gbufferSampler() const;

private:
    class Implementation;
    std::unique_ptr<Implementation> m_impl;
};

} // namespace luna::scene_renderer
