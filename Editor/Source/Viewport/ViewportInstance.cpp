#include "Viewport/ViewportInstance.h"

#include "EditorCamera.h"
#include "Renderer/Renderer.h"

#include <Swapchain.h>
#include <algorithm>

namespace luna {

void ViewportInstance::configureRenderer(Renderer& renderer, bool imgui_overlay_enabled) const
{
    renderer.setSceneOutputMode(imgui_overlay_enabled ? Renderer::SceneOutputMode::OffscreenTexture
                                                      : Renderer::SceneOutputMode::Swapchain);
}

void ViewportInstance::resetRenderer(Renderer& renderer)
{
    renderer.setSceneOutputMode(Renderer::SceneOutputMode::Swapchain);
    renderer.setRenderDebugViewMode(RenderDebugViewMode::None);
    renderer.setScenePickDebugVisualizationEnabled(false);
    renderer.setDefaultRenderFeatureEnabled("EditorInfiniteGrid", false);
    m_state = {};
}

void ViewportInstance::setPickDebugVisualization(Renderer& renderer, bool enabled) const
{
    renderer.setScenePickDebugVisualizationEnabled(enabled);
}

void ViewportInstance::setEditorGrid(Renderer& renderer, bool enabled, bool runtime_viewport_enabled) const
{
    const bool editor_grid_enabled = enabled && !runtime_viewport_enabled &&
                                     renderer.getSceneOutputMode() == Renderer::SceneOutputMode::OffscreenTexture;
    renderer.setDefaultRenderFeatureEnabled("EditorInfiniteGrid", editor_grid_enabled);
}

const ViewportInstanceState& ViewportInstance::sync(Renderer& renderer, EditorCamera& camera, uint32_t width, uint32_t height)
{
    camera.setViewportSize(static_cast<float>(width), static_cast<float>(height));
    renderer.setSceneOutputSize(width, height);

    const auto& scene_texture = renderer.getSceneOutputTexture();
    const auto& swapchain = renderer.getSwapchain();
    const bool offscreen = renderer.getSceneOutputMode() == Renderer::SceneOutputMode::OffscreenTexture;
    const bool texture_ready = offscreen ? static_cast<bool>(scene_texture) : static_cast<bool>(swapchain);
    const luna::RHI::Extent2D viewport_extent = renderer.getSceneOutputSize();

    m_state.width = viewport_extent.width;
    m_state.height = viewport_extent.height;
    m_state.y_flip = offscreen && renderer.getCapabilities().conventions.imgui_render_target_requires_uv_y_flip;
    m_state.presentable = texture_ready && viewport_extent.width > 0 && viewport_extent.height > 0;
    return m_state;
}

bool ViewportInstance::requestScenePick(Renderer& renderer, uint32_t pixel_x, uint32_t pixel_y) const
{
    const auto& scene_texture = renderer.getSceneOutputTexture();
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
    return renderer.consumeScenePickResult();
}

const ViewportInstanceState& ViewportInstance::state() const noexcept
{
    return m_state;
}

} // namespace luna
