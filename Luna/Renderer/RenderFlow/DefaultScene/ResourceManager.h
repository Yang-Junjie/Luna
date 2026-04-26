#pragma once

// Coordinates scene render flow resources.
// Serves as the boundary that ties together pipeline state, environment resources,
// and uploaded draw assets so passes can ask for ready-to-use GPU resources.

#include "Renderer/RenderFlow/RenderFlowTypes.h"
#include "Renderer/RenderFlow/DefaultScene/DrawQueue.h"
#include "Renderer/RenderFlow/DefaultScene/Support.h"

#include <memory>
#include <optional>
#include <span>

namespace luna::RHI {
class Buffer;
class CommandBufferEncoder;
class DescriptorSet;
class GraphicsPipeline;
class Sampler;
class Texture;
} // namespace luna::RHI

namespace luna {
class RenderWorld;
}

namespace luna::render_flow::default_scene {

class DrawQueue;

struct ResolvedDrawResources {
    luna::RHI::Ref<luna::RHI::Buffer> vertex_buffer;
    luna::RHI::Ref<luna::RHI::Buffer> index_buffer;
    luna::RHI::Ref<luna::RHI::DescriptorSet> material_descriptor_set;
    uint32_t index_count{0};

    [[nodiscard]] bool hasGeometry() const
    {
        return vertex_buffer && index_buffer && index_count > 0;
    }

    [[nodiscard]] bool isValid() const
    {
        return hasGeometry() && material_descriptor_set;
    }
};

class ResourceManager final {
public:
    ResourceManager();
    ~ResourceManager();

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    void setShaderPaths(SceneShaderPaths shader_paths);
    void shutdown();

    void ensurePipelines(const SceneRenderContext& context);
    void prepareDraws(luna::RHI::CommandBufferEncoder& commands,
                      std::span<const DrawCommand> draw_commands,
                      const Material& default_material);
    void prepareEnvironmentIfNeeded(luna::RHI::CommandBufferEncoder& commands, const SceneRenderContext& context);
    void updateSceneParameters(const SceneRenderContext& context,
                               const RenderWorld& world,
                               const render_flow::default_scene_detail::ShadowRenderParams& shadow_params);
    void updateLightingResources(const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_base_color,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_normal_metallic,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_world_position_roughness,
                                 const luna::RHI::Ref<luna::RHI::Texture>& gbuffer_emissive_ao,
                                 const luna::RHI::Ref<luna::RHI::Texture>& pick_texture);
    void updateShadowResources(const luna::RHI::Ref<luna::RHI::Texture>& shadow_map);

    [[nodiscard]] ResolvedDrawResources resolveDrawResources(const DrawCommand& draw_command,
                                                             const Material& default_material) const;

    [[nodiscard]] const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& geometryPipeline() const;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& shadowPipeline() const;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& lightingPipeline() const;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::GraphicsPipeline>& transparentPipeline() const;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::DescriptorSet>& sceneDescriptorSet() const;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::DescriptorSet>& lightingSceneDescriptorSet() const;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::DescriptorSet>& gbufferDescriptorSet() const;
    [[nodiscard]] const luna::RHI::Ref<luna::RHI::Sampler>& gbufferSampler() const;

private:
    class Implementation;
    std::unique_ptr<Implementation> m_impl;
};

} // namespace luna::render_flow::default_scene






