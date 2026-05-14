#pragma once

#include "EditorApi/EditorTypes.h"
#include "Viewport/TextureViewportInstance.h"

#include <memory>
#include <unordered_map>

namespace luna {

class TextureViewportInstanceManager final {
public:
    bool createViewport(editor::ViewportId viewport_id);
    bool destroyViewport(editor::ViewportId viewport_id);
    TextureViewportInstance* findViewport(editor::ViewportId viewport_id) noexcept;
    const TextureViewportInstance* findViewport(editor::ViewportId viewport_id) const noexcept;
    bool isViewportValid(editor::ViewportId viewport_id) const noexcept;
    void clearViewports() noexcept;

private:
    std::unordered_map<editor::ViewportId, std::unique_ptr<TextureViewportInstance>> m_viewports;
};

} // namespace luna
