#pragma once

#include <cstdint>
#include <optional>

namespace luna {

class EditorCamera;
class Renderer;
class Ui;

struct ViewportInstanceState {
    uint32_t width{0};
    uint32_t height{0};
    bool y_flip{false};
    bool presentable{false};
};

class ViewportInstance final {
public:
    void configureRenderer(Renderer& renderer, bool imgui_overlay_enabled) const;
    void resetRenderer(Renderer& renderer);

    void setPickDebugVisualization(Renderer& renderer, bool enabled) const;
    void setEditorGrid(Renderer& renderer, bool enabled, bool runtime_viewport_enabled) const;

    const ViewportInstanceState& sync(Renderer& renderer, EditorCamera& camera, uint32_t width, uint32_t height);

    bool requestScenePick(Renderer& renderer, uint32_t pixel_x, uint32_t pixel_y) const;
    std::optional<uint32_t> consumeScenePickResult(Renderer& renderer) const;

    const ViewportInstanceState& state() const noexcept;

private:
    ViewportInstanceState m_state;
};

} // namespace luna
