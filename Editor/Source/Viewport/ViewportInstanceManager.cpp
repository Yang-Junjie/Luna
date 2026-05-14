#include "Viewport/ViewportInstanceManager.h"

#include "Renderer/Renderer.h"

namespace luna {

ViewportInstance& ViewportInstanceManager::defaultViewport()
{
    return m_default_viewport;
}

const ViewportInstance& ViewportInstanceManager::defaultViewport() const
{
    return m_default_viewport;
}

ViewportInstance& ViewportInstanceManager::runtimeViewport()
{
    return m_runtime_viewport;
}

const ViewportInstance& ViewportInstanceManager::runtimeViewport() const
{
    return m_runtime_viewport;
}

editor::ViewportId ViewportInstanceManager::defaultViewportId() const noexcept
{
    return editor::kDefaultViewportId;
}

editor::ViewportId ViewportInstanceManager::createViewport()
{
    editor::ViewportId viewport_id = m_next_viewport_id++;
    while (viewport_id == editor::kInvalidViewportId || viewport_id == editor::kDefaultViewportId ||
           m_plugin_viewports.contains(viewport_id)) {
        viewport_id = m_next_viewport_id++;
    }

    m_plugin_viewports.emplace(
        viewport_id,
        std::make_unique<ViewportInstance>(ViewportInstance::RendererViewportKind::Owned));
    return viewport_id;
}

bool ViewportInstanceManager::destroyViewport(editor::ViewportId viewport_id, Renderer& renderer)
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

ViewportInstance* ViewportInstanceManager::findViewport(editor::ViewportId viewport_id) noexcept
{
    if (viewport_id == editor::kDefaultViewportId) {
        return &m_default_viewport;
    }

    const auto viewport_it = m_plugin_viewports.find(viewport_id);
    return viewport_it != m_plugin_viewports.end() ? viewport_it->second.get() : nullptr;
}

const ViewportInstance* ViewportInstanceManager::findViewport(editor::ViewportId viewport_id) const noexcept
{
    if (viewport_id == editor::kDefaultViewportId) {
        return &m_default_viewport;
    }

    const auto viewport_it = m_plugin_viewports.find(viewport_id);
    return viewport_it != m_plugin_viewports.end() ? viewport_it->second.get() : nullptr;
}

bool ViewportInstanceManager::isViewportValid(editor::ViewportId viewport_id) const noexcept
{
    return findViewport(viewport_id) != nullptr;
}

void ViewportInstanceManager::clearPluginViewports(Renderer& renderer)
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
