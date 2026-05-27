#include "Viewport/PreviewSceneViewport.h"
#include "Viewport/PreviewSceneViewportManager.h"

namespace luna {

PreviewSceneViewportManager::PreviewSceneViewportManager() = default;
PreviewSceneViewportManager::~PreviewSceneViewportManager() = default;
PreviewSceneViewportManager::PreviewSceneViewportManager(PreviewSceneViewportManager&&) noexcept = default;
PreviewSceneViewportManager& PreviewSceneViewportManager::operator=(PreviewSceneViewportManager&&) noexcept = default;

bool PreviewSceneViewportManager::setPreview(editor::ViewportId viewport_id,
                                             const editor::SceneViewportPreviewState& state)
{
    auto& preview = m_previews[viewport_id];
    if (!preview) {
        preview = std::make_unique<PreviewSceneViewport>();
    }

    (void) preview->setState(state);
    return true;
}

void PreviewSceneViewportManager::clearPreview(editor::ViewportId viewport_id)
{
    m_previews.erase(viewport_id);
}

void PreviewSceneViewportManager::clearPreviews() noexcept
{
    m_previews.clear();
}

bool PreviewSceneViewportManager::syncPreview(editor::ViewportId viewport_id,
                                              Renderer& renderer,
                                              SceneViewportInstance& viewport)
{
    const auto preview_it = m_previews.find(viewport_id);
    if (preview_it == m_previews.end() || !preview_it->second) {
        return false;
    }

    preview_it->second->sync(renderer, viewport);
    return true;
}

} // namespace luna
