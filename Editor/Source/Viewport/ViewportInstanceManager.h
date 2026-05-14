#pragma once

#include "EditorApi/EditorTypes.h"
#include "Viewport/ViewportInstance.h"

#include <memory>
#include <unordered_map>

namespace luna {

class ViewportInstanceManager final {
public:
    ViewportInstance& defaultViewport();
    const ViewportInstance& defaultViewport() const;
    ViewportInstance& runtimeViewport();
    const ViewportInstance& runtimeViewport() const;

    editor::ViewportId defaultViewportId() const noexcept;
    editor::ViewportId createViewport();
    bool destroyViewport(editor::ViewportId viewport_id, Renderer& renderer);
    ViewportInstance* findViewport(editor::ViewportId viewport_id) noexcept;
    const ViewportInstance* findViewport(editor::ViewportId viewport_id) const noexcept;
    bool isViewportValid(editor::ViewportId viewport_id) const noexcept;
    void clearPluginViewports(Renderer& renderer);

private:
    ViewportInstance m_default_viewport;
    ViewportInstance m_runtime_viewport;
    std::unordered_map<editor::ViewportId, std::unique_ptr<ViewportInstance>> m_plugin_viewports;
    editor::ViewportId m_next_viewport_id{editor::kDefaultViewportId + 1u};
};

} // namespace luna
