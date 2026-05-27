#pragma once

#include "Renderer/Renderer.h"
#include "Viewport/ViewportSurface.h"

#include <cstdint>

#include <optional>

namespace luna {

class Ui;

using SceneViewportInstanceState = ViewportSurfaceState;

class SceneViewportInstance final {
public:
    enum class RendererViewportKind : uint8_t {
        Default,
        Owned,
    };

    explicit SceneViewportInstance(RendererViewportKind kind = RendererViewportKind::Default);
    ~SceneViewportInstance();

    SceneViewportInstance(const SceneViewportInstance&) = delete;
    SceneViewportInstance& operator=(const SceneViewportInstance&) = delete;
    SceneViewportInstance(SceneViewportInstance&&) noexcept;
    SceneViewportInstance& operator=(SceneViewportInstance&&) noexcept;

    void configureRenderer(Renderer& renderer, bool imgui_overlay_enabled) const;
    void resetRenderer(Renderer& renderer);
    void release(Renderer& renderer);

    void setPickDebugVisualization(Renderer& renderer, bool enabled) const;
    void setEditorGrid(Renderer& renderer, bool enabled, bool runtime_viewport_enabled) const;

    const SceneViewportInstanceState& sync(Renderer& renderer, uint32_t width, uint32_t height);

    bool requestScenePick(Renderer& renderer, uint32_t pixel_x, uint32_t pixel_y) const;
    std::optional<uint32_t> consumeScenePickResult(Renderer& renderer) const;

    const SceneViewportInstanceState& state() const noexcept;
    Renderer::SceneViewportHandle rendererViewportHandle(Renderer& renderer);
    Renderer::SceneViewportHandle rendererViewportHandle(const Renderer& renderer) const;
    bool ownsRendererViewport() const noexcept;

private:
    Renderer::SceneViewportHandle ensureRendererViewport(Renderer& renderer);

    ViewportSurface m_surface;
    RendererViewportKind m_renderer_viewport_kind{RendererViewportKind::Default};
    Renderer::SceneViewportHandle m_renderer_viewport{Renderer::kInvalidSceneViewportHandle};
};

} // namespace luna
