#pragma once

#include "Renderer/Renderer.h"

#include <cstdint>
#include <optional>

namespace luna {

class EditorCamera;
class Ui;

struct ViewportInstanceState {
    uint32_t width{0};
    uint32_t height{0};
    bool y_flip{false};
    bool presentable{false};
};

class ViewportInstance final {
public:
    enum class RendererViewportKind : uint8_t {
        Default,
        Owned,
    };

    explicit ViewportInstance(RendererViewportKind kind = RendererViewportKind::Default);
    ~ViewportInstance();

    ViewportInstance(const ViewportInstance&) = delete;
    ViewportInstance& operator=(const ViewportInstance&) = delete;
    ViewportInstance(ViewportInstance&&) noexcept;
    ViewportInstance& operator=(ViewportInstance&&) noexcept;

    void configureRenderer(Renderer& renderer, bool imgui_overlay_enabled) const;
    void resetRenderer(Renderer& renderer);
    void release(Renderer& renderer);

    void setPickDebugVisualization(Renderer& renderer, bool enabled) const;
    void setEditorGrid(Renderer& renderer, bool enabled, bool runtime_viewport_enabled) const;

    const ViewportInstanceState& sync(Renderer& renderer, EditorCamera& camera, uint32_t width, uint32_t height);

    bool requestScenePick(Renderer& renderer, uint32_t pixel_x, uint32_t pixel_y) const;
    std::optional<uint32_t> consumeScenePickResult(Renderer& renderer) const;

    const ViewportInstanceState& state() const noexcept;
    Renderer::SceneViewportHandle rendererViewportHandle(Renderer& renderer);
    Renderer::SceneViewportHandle rendererViewportHandle(const Renderer& renderer) const;
    bool ownsRendererViewport() const noexcept;

private:
    Renderer::SceneViewportHandle ensureRendererViewport(Renderer& renderer);

    ViewportInstanceState m_state;
    RendererViewportKind m_renderer_viewport_kind{RendererViewportKind::Default};
    Renderer::SceneViewportHandle m_renderer_viewport{Renderer::kInvalidSceneViewportHandle};
};

} // namespace luna
