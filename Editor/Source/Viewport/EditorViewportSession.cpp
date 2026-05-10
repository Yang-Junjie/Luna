#include "Viewport/EditorViewportSession.h"

#include "EditorCamera.h"
#include "Renderer/Renderer.h"

#include <Swapchain.h>
#include <algorithm>
#include <chrono>
#include <utility>

namespace luna {

EditorViewportSession::EditorViewportSession(editor::EditorViewportId viewport_id,
                                             editor::EditorRenderPlaneId plane_id)
{
    m_state.viewport_id = viewport_id;
    m_state.render_plane.descriptor.viewport_id = viewport_id;
    m_state.render_plane.descriptor.plane_id = plane_id;
    m_state.render_plane.descriptor.label = m_state.title;
}

void EditorViewportSession::configureRenderer(Renderer& renderer, bool imgui_overlay_enabled) const
{
    renderer.setSceneOutputMode(imgui_overlay_enabled ? Renderer::SceneOutputMode::OffscreenTexture
                                                      : Renderer::SceneOutputMode::Swapchain);
}

void EditorViewportSession::resetRenderer(Renderer& renderer)
{
    renderer.setSceneOutputMode(Renderer::SceneOutputMode::Swapchain);
    renderer.setRenderDebugViewMode(RenderDebugViewMode::None);
    renderer.setScenePickDebugVisualizationEnabled(false);
    renderer.setDefaultRenderFeatureEnabled("EditorInfiniteGrid", false);
    (void) editor::releaseEditorRenderPlane(m_state.render_plane);
}

void EditorViewportSession::setPickDebugVisualization(Renderer& renderer, bool enabled) const
{
    renderer.setScenePickDebugVisualizationEnabled(enabled);
}

void EditorViewportSession::setEditorGrid(Renderer& renderer, bool enabled, bool runtime_viewport_enabled) const
{
    const bool editor_grid_enabled = enabled && !runtime_viewport_enabled &&
                                     renderer.getSceneOutputMode() == Renderer::SceneOutputMode::OffscreenTexture;
    renderer.setDefaultRenderFeatureEnabled("EditorInfiniteGrid", editor_grid_enabled);
}

editor::EditorViewportCommandResult EditorViewportSession::applyCommand(const editor::EditorViewportCommand& command)
{
    return editor::applyEditorViewportCommand(m_state, command);
}

const editor::EditorViewportState& EditorViewportSession::sync(Renderer& renderer,
                                                               EditorCamera& camera,
                                                               uint32_t width,
                                                               uint32_t height,
                                                               bool focused,
                                                               bool hovered,
                                                               bool input_enabled,
                                                               bool mouse_captured,
                                                               bool runtime_viewport_enabled,
                                                               editor::EditorTransformTool transform_tool,
                                                               editor::EditorTransformSpace transform_space,
                                                               std::string_view title,
                                                               editor::EditorViewportKind kind)
{
    camera.setViewportSize(static_cast<float>(width), static_cast<float>(height));
    renderer.setSceneOutputSize(width, height);

    m_state.viewport_id = m_state.viewport_id == 0 ? 1 : m_state.viewport_id;
    m_state.title = std::string(title);
    m_state.kind = runtime_viewport_enabled ? editor::EditorViewportKind::Preview : kind;
    m_state.size = {width, height};
    m_state.camera = editor::EditorViewportCameraState::fromCamera(camera.getCamera());
    m_state.interaction.visible = true;
    m_state.interaction.focused = focused;
    m_state.interaction.hovered = hovered;
    m_state.interaction.input_enabled = input_enabled;
    m_state.interaction.mouse_captured = mouse_captured;
    m_state.interaction.runtime_viewport = runtime_viewport_enabled;
    m_state.interaction.pick_debug_enabled = renderer.isScenePickDebugVisualizationEnabled();
    m_state.transform_tool = transform_tool;
    m_state.transform_space = transform_space;
    m_state.debug_view_mode = renderer.getRenderDebugViewMode();
    m_state.debug_velocity_scale = renderer.getRenderDebugVelocityScale();

    const auto& scene_texture = renderer.getSceneOutputTexture();
    const auto& swapchain = renderer.getSwapchain();
    const bool offscreen = renderer.getSceneOutputMode() == Renderer::SceneOutputMode::OffscreenTexture;
    const bool texture_ready = offscreen ? static_cast<bool>(scene_texture) : static_cast<bool>(swapchain);
    const luna::RHI::Extent2D viewport_extent = renderer.getSceneOutputSize();

    editor::EditorRenderPlaneDescriptor descriptor;
    descriptor.plane_id = m_state.render_plane.descriptor.plane_id;
    descriptor.viewport_id = m_state.viewport_id;
    descriptor.kind = runtime_viewport_enabled ? editor::EditorRenderPlaneKind::Preview
                                                : editor::EditorRenderPlaneKind::SceneViewport;
    descriptor.transport = offscreen ? (texture_ready ? editor::EditorRenderTransportKind::SharedTexture
                                                      : editor::EditorRenderTransportKind::None)
                                     : (texture_ready ? editor::EditorRenderTransportKind::NativeSurface
                                                      : editor::EditorRenderTransportKind::None);
    descriptor.format = scene_texture ? scene_texture->GetFormat() : (swapchain ? swapchain->GetFormat()
                                                                                : luna::RHI::Format::UNDEFINED);
    descriptor.width = viewport_extent.width;
    descriptor.height = viewport_extent.height;
    descriptor.y_flip = offscreen && renderer.getCapabilities().conventions.imgui_render_target_requires_uv_y_flip;
    descriptor.presentable = texture_ready && viewport_extent.width > 0 && viewport_extent.height > 0;
    descriptor.binding_token = descriptor.transport == editor::EditorRenderTransportKind::None
                                   ? std::string{}
                                   : (offscreen ? "renderer.scene_output.texture" : "renderer.scene_output.swapchain");
    descriptor.label = m_state.title;

    const bool descriptor_changed = editor::bindEditorRenderPlane(m_state.render_plane, std::move(descriptor));
    const editor::EditorFrameId frame_id = ++m_frame_sequence;
    editor::presentEditorRenderPlaneFrame(
        m_state.render_plane,
        frame_id,
        m_frame_sequence,
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                  std::chrono::steady_clock::now().time_since_epoch())
                                  .count()));
    (void) descriptor_changed;
    return m_state;
}

bool EditorViewportSession::requestScenePick(Renderer& renderer, uint32_t pixel_x, uint32_t pixel_y) const
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

std::optional<uint32_t> EditorViewportSession::consumeScenePickResult(Renderer& renderer) const
{
    return renderer.consumeScenePickResult();
}

const editor::EditorViewportState& EditorViewportSession::state() const noexcept
{
    return m_state;
}

} // namespace luna
