#pragma once

#include "Renderer/RenderFlow/DefaultScene/AssetCache.h"
#include "Renderer/RenderFlow/DefaultScene/GpuTypes.h"
#include "Renderer/RenderFlow/RenderFlowTypes.h"
#include "Renderer/RenderGraph.h"

namespace luna {
class Material;
class RenderWorld;
} // namespace luna

namespace luna::render_flow::default_scene {

class DrawQueue;
class EnvironmentResources;
class PipelineResources;

struct GBufferTextures {
    RenderGraphTextureHandle base_color;
    RenderGraphTextureHandle normal_metallic;
    RenderGraphTextureHandle world_position_roughness;
    RenderGraphTextureHandle emissive_ao;
    RenderGraphTextureHandle velocity;
};

struct ShadowResources {
    RenderGraphTextureHandle shadow_map;
    RenderGraphTextureHandle shadow_depth;
    render_flow::default_scene_detail::ShadowRenderParams render_params{};
};

class PassSharedState final {
public:
    PassSharedState(AssetCache& assets,
                    PipelineResources& pipelines,
                    DrawQueue& draw_queue,
                    EnvironmentResources& environment,
                    Material& default_material);

    void setWorld(const RenderWorld& world) noexcept;
    void setFrameContext(const RenderFeatureFrameContext& frame_context) noexcept;
    [[nodiscard]] AssetCache& assets() const noexcept;
    [[nodiscard]] PipelineResources& pipelines() const noexcept;
    [[nodiscard]] DrawQueue& drawQueue() const noexcept;
    [[nodiscard]] EnvironmentResources& environment() const noexcept;
    [[nodiscard]] Material& defaultMaterial() const noexcept;
    [[nodiscard]] const RenderWorld* world() const noexcept;
    [[nodiscard]] const RenderFeatureFrameContext* frameContext() const noexcept;
    void setShadowParams(const render_flow::default_scene_detail::ShadowRenderParams& shadow_params) noexcept;
    [[nodiscard]] const render_flow::default_scene_detail::ShadowRenderParams& shadowParams() const noexcept;

private:
    AssetCache* m_assets{nullptr};
    PipelineResources* m_pipelines{nullptr};
    DrawQueue* m_draw_queue{nullptr};
    EnvironmentResources* m_environment{nullptr};
    Material* m_default_material{nullptr};
    const RenderWorld* m_world{nullptr};
    RenderFeatureFrameContext m_frame_context{};
    bool m_has_frame_context{false};
    render_flow::default_scene_detail::ShadowRenderParams m_shadow_params{};
};

} // namespace luna::render_flow::default_scene
