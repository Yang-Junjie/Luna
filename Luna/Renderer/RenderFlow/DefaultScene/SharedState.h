#pragma once

#include "Renderer/RenderGraph.h"
#include "Renderer/RenderFlow/DefaultScene/SceneAssetResources.h"
#include "Renderer/RenderFlow/DefaultScene/SceneGpuTypes.h"

namespace luna {
class Material;
class RenderWorld;
} // namespace luna

namespace luna::render_flow::default_scene {

class DrawQueue;
class EnvironmentResources;
class ScenePipelineResources;

struct GBufferTextures {
    RenderGraphTextureHandle base_color;
    RenderGraphTextureHandle normal_metallic;
    RenderGraphTextureHandle world_position_roughness;
    RenderGraphTextureHandle emissive_ao;
};

struct ShadowResources {
    RenderGraphTextureHandle shadow_map;
    RenderGraphTextureHandle shadow_depth;
    render_flow::default_scene_detail::ShadowRenderParams render_params{};
};

class PassSharedState final {
public:
    PassSharedState(SceneAssetResources& assets,
                    ScenePipelineResources& pipelines,
                    DrawQueue& draw_queue,
                    EnvironmentResources& environment,
                    Material& default_material);

    void setWorld(const RenderWorld& world) noexcept;
    [[nodiscard]] SceneAssetResources& assets() const noexcept;
    [[nodiscard]] ScenePipelineResources& pipelines() const noexcept;
    [[nodiscard]] DrawQueue& drawQueue() const noexcept;
    [[nodiscard]] EnvironmentResources& environment() const noexcept;
    [[nodiscard]] Material& defaultMaterial() const noexcept;
    [[nodiscard]] const RenderWorld* world() const noexcept;
    void setShadowParams(const render_flow::default_scene_detail::ShadowRenderParams& shadow_params) noexcept;
    [[nodiscard]] const render_flow::default_scene_detail::ShadowRenderParams& shadowParams() const noexcept;

private:
    SceneAssetResources* m_assets{nullptr};
    ScenePipelineResources* m_pipelines{nullptr};
    DrawQueue* m_draw_queue{nullptr};
    EnvironmentResources* m_environment{nullptr};
    Material* m_default_material{nullptr};
    const RenderWorld* m_world{nullptr};
    render_flow::default_scene_detail::ShadowRenderParams m_shadow_params{};
};

} // namespace luna::render_flow::default_scene
