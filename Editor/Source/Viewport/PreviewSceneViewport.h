#pragma once

#include "EditorApi/EditorViewportService.h"
#include "Renderer/Camera.h"
#include "Scene/Scene.h"

namespace luna {

class Renderer;
class SceneViewportInstance;

class PreviewSceneViewport final {
public:
    [[nodiscard]] const editor::SceneViewportPreviewState& state() const noexcept;
    [[nodiscard]] bool dirty() const noexcept;

    bool setState(const editor::SceneViewportPreviewState& state);
    void sync(Renderer& renderer, SceneViewportInstance& viewport);

private:
    [[nodiscard]] static AssetHandle previewMeshHandle(const editor::SceneViewportPreviewState& state);
    void rebuildScene();
    static void applyCamera(Camera& camera, const editor::SceneViewportPreviewState& state);
    static bool sameState(const editor::SceneViewportPreviewState& lhs,
                          const editor::SceneViewportPreviewState& rhs) noexcept;

    editor::SceneViewportPreviewState m_state{};
    Scene m_scene;
    Camera m_camera;
    bool m_dirty{true};
};

} // namespace luna
