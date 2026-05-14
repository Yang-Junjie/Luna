#pragma once

#include "EditorApi/EditorTypes.h"
#include "Viewport/SceneViewportInstance.h"

#include <memory>
#include <unordered_map>

namespace luna {

class SceneViewportInstanceManager final {
public:
    SceneViewportInstance& defaultViewport();
    const SceneViewportInstance& defaultViewport() const;
    SceneViewportInstance& runtimeViewport();
    const SceneViewportInstance& runtimeViewport() const;

    editor::ViewportId defaultViewportId() const noexcept;
    bool createViewport(editor::ViewportId viewport_id);
    bool destroyViewport(editor::ViewportId viewport_id, Renderer& renderer);
    SceneViewportInstance* findViewport(editor::ViewportId viewport_id) noexcept;
    const SceneViewportInstance* findViewport(editor::ViewportId viewport_id) const noexcept;
    bool isViewportValid(editor::ViewportId viewport_id) const noexcept;
    void clearPluginViewports(Renderer& renderer);

private:
    SceneViewportInstance m_default_viewport;
    SceneViewportInstance m_runtime_viewport;
    std::unordered_map<editor::ViewportId, std::unique_ptr<SceneViewportInstance>> m_plugin_viewports;
};

} // namespace luna
