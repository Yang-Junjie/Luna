#include "Renderer/Renderer.h"
#include "Viewport/SceneViewportInstanceManager.h"

namespace luna {

SceneViewportInstance& SceneViewportInstanceManager::defaultViewport()
{
    return m_default_viewport;
}

const SceneViewportInstance& SceneViewportInstanceManager::defaultViewport() const
{
    return m_default_viewport;
}

SceneViewportInstance& SceneViewportInstanceManager::runtimeViewport()
{
    return m_runtime_viewport;
}

const SceneViewportInstance& SceneViewportInstanceManager::runtimeViewport() const
{
    return m_runtime_viewport;
}

editor::ViewportId SceneViewportInstanceManager::defaultViewportId() const noexcept
{
    return editor::kDefaultViewportId;
}

bool SceneViewportInstanceManager::createViewport(editor::ViewportId viewport_id)
{
    if (viewport_id == editor::kInvalidViewportId || viewport_id == editor::kDefaultViewportId ||
        m_plugin_viewports.contains(viewport_id)) {
        return false;
    }

    m_plugin_viewports.emplace(
        viewport_id, std::make_unique<SceneViewportInstance>(SceneViewportInstance::RendererViewportKind::Owned));
    return true;
}

bool SceneViewportInstanceManager::destroyViewport(editor::ViewportId viewport_id, Renderer& renderer)
{
    const auto viewport_it = m_plugin_viewports.find(viewport_id);
    if (viewport_it == m_plugin_viewports.end()) {
        return false;
    }

    if (viewport_it->second) {
        viewport_it->second->release(renderer);
    }
    m_plugin_viewports.erase(viewport_it);
    return true;
}

SceneViewportInstance* SceneViewportInstanceManager::findViewport(editor::ViewportId viewport_id) noexcept
{
    if (viewport_id == editor::kDefaultViewportId) {
        return &m_default_viewport;
    }

    const auto viewport_it = m_plugin_viewports.find(viewport_id);
    return viewport_it != m_plugin_viewports.end() ? viewport_it->second.get() : nullptr;
}

const SceneViewportInstance* SceneViewportInstanceManager::findViewport(editor::ViewportId viewport_id) const noexcept
{
    if (viewport_id == editor::kDefaultViewportId) {
        return &m_default_viewport;
    }

    const auto viewport_it = m_plugin_viewports.find(viewport_id);
    return viewport_it != m_plugin_viewports.end() ? viewport_it->second.get() : nullptr;
}

bool SceneViewportInstanceManager::isViewportValid(editor::ViewportId viewport_id) const noexcept
{
    return findViewport(viewport_id) != nullptr;
}

void SceneViewportInstanceManager::clearPluginViewports(Renderer& renderer)
{
    for (auto& [viewport_id, viewport] : m_plugin_viewports) {
        (void) viewport_id;
        if (viewport) {
            viewport->release(renderer);
        }
    }
    m_plugin_viewports.clear();
}

} // namespace luna
