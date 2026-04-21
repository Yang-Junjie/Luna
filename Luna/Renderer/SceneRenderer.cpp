#include "Renderer/SceneRendererInternal.h"

#include <utility>

namespace luna {

SceneRenderer::SceneRenderer()
    : m_impl(std::make_unique<Implementation>())
{}

SceneRenderer::~SceneRenderer()
{
    shutdown();
}

void SceneRenderer::setShaderPaths(ShaderPaths shader_paths)
{
    m_impl->setShaderPaths(std::move(shader_paths));
}

void SceneRenderer::shutdown()
{
    m_impl->shutdown();
}

void SceneRenderer::beginScene(const Camera& camera)
{
    m_impl->beginScene(camera);
}

void SceneRenderer::clearSubmittedMeshes()
{
    m_impl->clearSubmittedMeshes();
}

void SceneRenderer::submitStaticMesh(const glm::mat4& transform,
                                     std::shared_ptr<Mesh> mesh,
                                     std::shared_ptr<Material> material)
{
    m_impl->submitStaticMesh(transform, std::move(mesh), std::move(material));
}

void SceneRenderer::submitStaticMesh(const glm::mat4& transform,
                                     std::shared_ptr<Mesh> mesh,
                                     const std::vector<std::shared_ptr<Material>>& submesh_materials)
{
    m_impl->submitStaticMesh(transform, std::move(mesh), submesh_materials);
}

void SceneRenderer::buildRenderGraph(rhi::RenderGraphBuilder& graph, const RenderContext& context)
{
    m_impl->buildRenderGraph(graph, context);
}

} // namespace luna
