#include "Viewport/ViewportInstance.h"

#include "EditorCamera.h"
#include "Renderer/Renderer.h"

#include <Swapchain.h>
#include <algorithm>

namespace luna {

ViewportInstance::ViewportInstance(RendererViewportKind kind)
    : m_renderer_viewport_kind(kind)
{}

ViewportInstance::~ViewportInstance() = default;

ViewportInstance::ViewportInstance(ViewportInstance&& other) noexcept
    : m_state(other.m_state),
      m_renderer_viewport_kind(other.m_renderer_viewport_kind),
      m_renderer_viewport(other.m_renderer_viewport)
{
    other.m_state = {};
    other.m_renderer_viewport_kind = RendererViewportKind::Default;
    other.m_renderer_viewport = Renderer::kInvalidSceneViewportHandle;
}

ViewportInstance& ViewportInstance::operator=(ViewportInstance&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    m_state = other.m_state;
    m_renderer_viewport_kind = other.m_renderer_viewport_kind;
    m_renderer_viewport = other.m_renderer_viewport;
    other.m_state = {};
    other.m_renderer_viewport_kind = RendererViewportKind::Default;
    other.m_renderer_viewport = Renderer::kInvalidSceneViewportHandle;
    return *this;
}

void ViewportInstance::configureRenderer(Renderer& renderer, bool imgui_overlay_enabled) const
{
    const Renderer::SceneViewportHandle handle = rendererViewportHandle(renderer);
    if (handle == Renderer::kInvalidSceneViewportHandle) {
        return;
    }

    if (m_renderer_viewport_kind == RendererViewportKind::Owned) {
        renderer.setSceneViewportOutputMode(handle, Renderer::SceneOutputMode::OffscreenTexture);
        return;
    }

    renderer.setSceneViewportOutputMode(handle,
                                        imgui_overlay_enabled ? Renderer::SceneOutputMode::OffscreenTexture
                                                              : Renderer::SceneOutputMode::Swapchain);
}

void ViewportInstance::resetRenderer(Renderer& renderer)
{
    if (m_renderer_viewport_kind == RendererViewportKind::Owned) {
        release(renderer);
        return;
    }

    renderer.setSceneViewportOutputMode(rendererViewportHandle(renderer), Renderer::SceneOutputMode::Swapchain);
    renderer.setRenderDebugViewMode(RenderDebugViewMode::None);
    renderer.setScenePickDebugVisualizationEnabled(false);
    renderer.setDefaultRenderFeatureEnabled("EditorInfiniteGrid", false);
    m_state = {};
}

void ViewportInstance::release(Renderer& renderer)
{
    if (m_renderer_viewport_kind == RendererViewportKind::Owned &&
        m_renderer_viewport != Renderer::kInvalidSceneViewportHandle) {
        renderer.destroySceneViewportHandle(m_renderer_viewport);
        m_renderer_viewport = Renderer::kInvalidSceneViewportHandle;
    }
    m_state = {};
}

void ViewportInstance::setPickDebugVisualization(Renderer& renderer, bool enabled) const
{
    if (m_renderer_viewport_kind == RendererViewportKind::Default) {
        renderer.setScenePickDebugVisualizationEnabled(enabled);
    }
}

void ViewportInstance::setEditorGrid(Renderer& renderer, bool enabled, bool runtime_viewport_enabled) const
{
    if (m_renderer_viewport_kind != RendererViewportKind::Default) {
        return;
    }

    const bool editor_grid_enabled = enabled && !runtime_viewport_enabled &&
                                     renderer.getSceneOutputMode() == Renderer::SceneOutputMode::OffscreenTexture;
    renderer.setDefaultRenderFeatureEnabled("EditorInfiniteGrid", editor_grid_enabled);
}

const ViewportInstanceState& ViewportInstance::sync(Renderer& renderer, EditorCamera& camera, uint32_t width, uint32_t height)
{
    const Renderer::SceneViewportHandle handle = ensureRendererViewport(renderer);
    if (handle == Renderer::kInvalidSceneViewportHandle) {
        m_state = {};
        return m_state;
    }

    if (m_renderer_viewport_kind == RendererViewportKind::Default) {
        camera.setViewportSize(static_cast<float>(width), static_cast<float>(height));
    }
    renderer.setSceneViewportOutputSize(handle, width, height);

    const auto& scene_texture = renderer.getSceneViewportOutputTexture(handle);
    const auto& swapchain = renderer.getSwapchain();
    const bool offscreen =
        m_renderer_viewport_kind == RendererViewportKind::Owned ||
        renderer.getSceneOutputMode() == Renderer::SceneOutputMode::OffscreenTexture;
    const bool texture_ready = offscreen ? static_cast<bool>(scene_texture) : static_cast<bool>(swapchain);
    const luna::RHI::Extent2D viewport_extent = renderer.getSceneViewportOutputSize(handle);

    m_state.width = viewport_extent.width;
    m_state.height = viewport_extent.height;
    m_state.y_flip = offscreen && renderer.getCapabilities().conventions.imgui_render_target_requires_uv_y_flip;
    m_state.presentable = texture_ready && viewport_extent.width > 0 && viewport_extent.height > 0;
    return m_state;
}

bool ViewportInstance::requestScenePick(Renderer& renderer, uint32_t pixel_x, uint32_t pixel_y) const
{
    if (m_renderer_viewport_kind != RendererViewportKind::Default) {
        return false;
    }

    const Renderer::SceneViewportHandle handle = rendererViewportHandle(renderer);
    const auto& scene_texture = renderer.getSceneViewportOutputTexture(handle);
    if (!scene_texture || renderer.getSceneOutputMode() != Renderer::SceneOutputMode::OffscreenTexture) {
        return false;
    }

    const uint32_t texture_width = scene_texture->GetWidth();
    const uint32_t texture_height = scene_texture->GetHeight();
    if (texture_width == 0 || texture_height == 0) {
        return false;
    }

    const uint32_t clamped_x = (std::min)(pixel_x, texture_width - 1);
    const uint32_t clamped_color_y = (std::min)(pixel_y, texture_height - 1);
    const bool pick_y_matches_display_y = renderer.getCapabilities().conventions.scene_pick_y_matches_display_y;
    const uint32_t pick_pixel_y =
        pick_y_matches_display_y ? clamped_color_y : (texture_height - 1) - clamped_color_y;

    renderer.requestScenePick(clamped_x, pick_pixel_y);
    return true;
}

std::optional<uint32_t> ViewportInstance::consumeScenePickResult(Renderer& renderer) const
{
    return m_renderer_viewport_kind == RendererViewportKind::Default ? renderer.consumeScenePickResult() : std::nullopt;
}

const ViewportInstanceState& ViewportInstance::state() const noexcept
{
    return m_state;
}

Renderer::SceneViewportHandle ViewportInstance::rendererViewportHandle(Renderer& renderer)
{
    return m_renderer_viewport_kind == RendererViewportKind::Default ? renderer.getDefaultSceneViewportHandle()
                                                                    : ensureRendererViewport(renderer);
}

Renderer::SceneViewportHandle ViewportInstance::rendererViewportHandle(const Renderer& renderer) const
{
    return m_renderer_viewport_kind == RendererViewportKind::Default ? renderer.getDefaultSceneViewportHandle()
                                                                    : m_renderer_viewport;
}

bool ViewportInstance::ownsRendererViewport() const noexcept
{
    return m_renderer_viewport_kind == RendererViewportKind::Owned;
}

Renderer::SceneViewportHandle ViewportInstance::ensureRendererViewport(Renderer& renderer)
{
    if (m_renderer_viewport_kind == RendererViewportKind::Default) {
        return renderer.getDefaultSceneViewportHandle();
    }

    if (m_renderer_viewport == Renderer::kInvalidSceneViewportHandle ||
        !renderer.isSceneViewportHandleValid(m_renderer_viewport)) {
        m_renderer_viewport = renderer.createSceneViewportHandle();
    }
    return m_renderer_viewport;
}

} // namespace luna
