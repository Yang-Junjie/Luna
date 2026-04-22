#pragma once

#include "Renderer/Material.h"
#include "Renderer/SceneRendererCommon.h"
#include "Renderer/SceneRendererDrawQueue.h"
#include "Renderer/Mesh.h"
#include "Renderer/SceneRenderer.h"
#include "Renderer/SceneRendererResourceManager.h"

namespace luna {

struct SceneGBufferTextures {
    rhi::RenderGraphTextureHandle base_color;
    rhi::RenderGraphTextureHandle normal_metallic;
    rhi::RenderGraphTextureHandle world_position_roughness;
    rhi::RenderGraphTextureHandle emissive_ao;
};

class SceneRenderer::Implementation final {
public:
    void setShaderPaths(ShaderPaths shader_paths);
    void shutdown();

    void beginScene(const Camera& camera);
    void clearSubmittedMeshes();
    void submitStaticMesh(const glm::mat4& transform,
                          std::shared_ptr<Mesh> mesh,
                          std::shared_ptr<Material> material,
                          uint32_t picking_id);
    void submitStaticMesh(const glm::mat4& transform,
                          std::shared_ptr<Mesh> mesh,
                          const std::vector<std::shared_ptr<Material>>& submesh_materials,
                          uint32_t picking_id);
    void buildRenderGraph(rhi::RenderGraphBuilder& graph, const RenderContext& context);

private:
    [[nodiscard]] SceneGBufferTextures createGBufferTextures(rhi::RenderGraphBuilder& graph,
                                                             const RenderContext& context) const;

    void executeGeometryPass(rhi::RenderGraphRasterPassContext& pass_context, const RenderContext& context);
    void executeLightingPass(rhi::RenderGraphRasterPassContext& pass_context,
                             const RenderContext& context,
                             const SceneGBufferTextures& gbuffer);
    void executeTransparentPass(rhi::RenderGraphRasterPassContext& pass_context, const RenderContext& context);

private:
    scene_renderer::DrawQueue m_draw_queue{};
    scene_renderer::ResourceManager m_resources{};
    Material m_default_material{};
};

} // namespace luna
