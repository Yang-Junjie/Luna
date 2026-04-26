#pragma once

#include "Renderer/RenderFlow/RenderPass.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/RenderFlow/DefaultScene/Support.h"

namespace luna {
class Material;
class RenderWorld;
} // namespace luna

namespace luna::render_flow::default_scene {

class DrawQueue;
class ResourceManager;

struct DefaultSceneGBufferTextures {
    RenderGraphTextureHandle base_color;
    RenderGraphTextureHandle normal_metallic;
    RenderGraphTextureHandle world_position_roughness;
    RenderGraphTextureHandle emissive_ao;
};

struct DefaultSceneShadowResources {
    RenderGraphTextureHandle shadow_map;
    RenderGraphTextureHandle shadow_depth;
    render_flow::default_scene_detail::ShadowRenderParams render_params{};
};

class DefaultScenePassSharedState final {
public:
    DefaultScenePassSharedState(ResourceManager& resources, DrawQueue& draw_queue, Material& default_material);

    void setWorld(const RenderWorld& world) noexcept;
    [[nodiscard]] ResourceManager& resources() const noexcept;
    [[nodiscard]] DrawQueue& drawQueue() const noexcept;
    [[nodiscard]] Material& defaultMaterial() const noexcept;
    [[nodiscard]] const RenderWorld* world() const noexcept;
    [[nodiscard]] DefaultSceneGBufferTextures& gbuffer() noexcept;
    [[nodiscard]] const DefaultSceneGBufferTextures& gbuffer() const noexcept;
    [[nodiscard]] DefaultSceneShadowResources& shadow() noexcept;
    [[nodiscard]] const DefaultSceneShadowResources& shadow() const noexcept;

private:
    ResourceManager* m_resources{nullptr};
    DrawQueue* m_draw_queue{nullptr};
    Material* m_default_material{nullptr};
    const RenderWorld* m_world{nullptr};
    DefaultSceneGBufferTextures m_gbuffer{};
    DefaultSceneShadowResources m_shadow{};
};

class DefaultSceneShadowDepthPass final : public IRenderPass {
public:
    explicit DefaultSceneShadowDepthPass(DefaultScenePassSharedState& state);

    [[nodiscard]] const char* name() const noexcept override;
    void setup(RenderPassContext& context) override;

private:
    void execute(RenderGraphRasterPassContext& pass_context, const SceneRenderContext& context);

private:
    DefaultScenePassSharedState* m_state{nullptr};
};

class DefaultSceneEnvironmentPass final : public IRenderPass {
public:
    explicit DefaultSceneEnvironmentPass(DefaultScenePassSharedState& state);

    [[nodiscard]] const char* name() const noexcept override;
    void setup(RenderPassContext& context) override;

private:
    void execute(RenderGraphComputePassContext& pass_context, const SceneRenderContext& context);

private:
    DefaultScenePassSharedState* m_state{nullptr};
};

class DefaultSceneGeometryPass final : public IRenderPass {
public:
    explicit DefaultSceneGeometryPass(DefaultScenePassSharedState& state);

    [[nodiscard]] const char* name() const noexcept override;
    void setup(RenderPassContext& context) override;

private:
    void execute(RenderGraphRasterPassContext& pass_context, const SceneRenderContext& context);

private:
    DefaultScenePassSharedState* m_state{nullptr};
};

class DefaultSceneLightingPass final : public IRenderPass {
public:
    explicit DefaultSceneLightingPass(DefaultScenePassSharedState& state);

    [[nodiscard]] const char* name() const noexcept override;
    void setup(RenderPassContext& context) override;

private:
    void execute(RenderGraphRasterPassContext& pass_context, const SceneRenderContext& context);

private:
    DefaultScenePassSharedState* m_state{nullptr};
};

class DefaultSceneTransparentPass final : public IRenderPass {
public:
    explicit DefaultSceneTransparentPass(DefaultScenePassSharedState& state);

    [[nodiscard]] const char* name() const noexcept override;
    void setup(RenderPassContext& context) override;

private:
    void execute(RenderGraphRasterPassContext& pass_context, const SceneRenderContext& context);

private:
    DefaultScenePassSharedState* m_state{nullptr};
};

} // namespace luna::render_flow::default_scene



