#include "Renderer/RenderFlow/DefaultScene/SharedState.h"

namespace luna::render_flow::default_scene {

PassSharedState::PassSharedState(AssetCache& assets,
                                 PipelineResources& pipelines,
                                 DrawQueue& draw_queue,
                                 EnvironmentResources& environment,
                                 Material& default_material)
    : m_assets(&assets),
      m_pipelines(&pipelines),
      m_draw_queue(&draw_queue),
      m_environment(&environment),
      m_default_material(&default_material)
{}

void PassSharedState::setWorld(const RenderWorld& world) noexcept
{
    m_world = &world;
}

AssetCache& PassSharedState::assets() const noexcept
{
    return *m_assets;
}

PipelineResources& PassSharedState::pipelines() const noexcept
{
    return *m_pipelines;
}

DrawQueue& PassSharedState::drawQueue() const noexcept
{
    return *m_draw_queue;
}

EnvironmentResources& PassSharedState::environment() const noexcept
{
    return *m_environment;
}

Material& PassSharedState::defaultMaterial() const noexcept
{
    return *m_default_material;
}

const RenderWorld* PassSharedState::world() const noexcept
{
    return m_world;
}

void PassSharedState::setShadowParams(
    const render_flow::default_scene_detail::ShadowRenderParams& shadow_params) noexcept
{
    m_shadow_params = shadow_params;
}

const render_flow::default_scene_detail::ShadowRenderParams& PassSharedState::shadowParams() const noexcept
{
    return m_shadow_params;
}

} // namespace luna::render_flow::default_scene
