#include "Viewport/TextureViewportInstanceManager.h"

namespace luna {

bool TextureViewportInstanceManager::createViewport(editor::ViewportId viewport_id)
{
    if (viewport_id == editor::kInvalidViewportId || viewport_id == editor::kDefaultViewportId ||
        m_viewports.contains(viewport_id)) {
        return false;
    }

    m_viewports.emplace(viewport_id, std::make_unique<TextureViewportInstance>());
    return true;
}

bool TextureViewportInstanceManager::destroyViewport(editor::ViewportId viewport_id)
{
    return m_viewports.erase(viewport_id) > 0;
}

TextureViewportInstance* TextureViewportInstanceManager::findViewport(editor::ViewportId viewport_id) noexcept
{
    const auto viewport_it = m_viewports.find(viewport_id);
    return viewport_it != m_viewports.end() ? viewport_it->second.get() : nullptr;
}

const TextureViewportInstance*
    TextureViewportInstanceManager::findViewport(editor::ViewportId viewport_id) const noexcept
{
    const auto viewport_it = m_viewports.find(viewport_id);
    return viewport_it != m_viewports.end() ? viewport_it->second.get() : nullptr;
}

bool TextureViewportInstanceManager::isViewportValid(editor::ViewportId viewport_id) const noexcept
{
    return findViewport(viewport_id) != nullptr;
}

void TextureViewportInstanceManager::clearViewports() noexcept
{
    m_viewports.clear();
}

} // namespace luna
