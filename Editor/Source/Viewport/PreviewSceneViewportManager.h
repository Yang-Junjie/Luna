#pragma once

#include "EditorApi/EditorViewportService.h"

#include <memory>
#include <unordered_map>

namespace luna {

class PreviewSceneViewport;
class Renderer;
class SceneViewportInstance;

class PreviewSceneViewportManager final {
public:
    PreviewSceneViewportManager();
    ~PreviewSceneViewportManager();

    PreviewSceneViewportManager(const PreviewSceneViewportManager&) = delete;
    PreviewSceneViewportManager& operator=(const PreviewSceneViewportManager&) = delete;
    PreviewSceneViewportManager(PreviewSceneViewportManager&&) noexcept;
    PreviewSceneViewportManager& operator=(PreviewSceneViewportManager&&) noexcept;

    bool setPreview(editor::ViewportId viewport_id, const editor::SceneViewportPreviewState& state);
    void clearPreview(editor::ViewportId viewport_id);
    void clearPreviews() noexcept;
    [[nodiscard]] bool syncPreview(editor::ViewportId viewport_id, Renderer& renderer, SceneViewportInstance& viewport);

private:
    std::unordered_map<editor::ViewportId, std::unique_ptr<PreviewSceneViewport>> m_previews;
};

} // namespace luna
