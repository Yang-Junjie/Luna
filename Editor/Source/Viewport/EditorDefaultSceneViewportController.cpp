#include "Authoring/EditorAuthoringController.h"
#include "EditorApi/EditorUi.h"
#include "EditorCamera.h"
#include "EditorRuntimeViewportController.h"
#include "EditorStyle.h"
#include "EditorUI.h"
#include "Imgui/ImGuiContext.h"
#include "Renderer/Renderer.h"
#include "Viewport/EditorDefaultSceneViewportController.h"
#include "Viewport/EditorViewportCoordinator.h"
#include "Viewport/EditorViewportGizmoController.h"
#include "Viewport/SceneViewportInstance.h"

#include <cmath>

#include <algorithm>
#include <imgui.h>
#include <ImGuizmo.h>

namespace {

luna::editor::Vec2 toEditorVec2(const ImVec2& value) noexcept
{
    return luna::editor::Vec2{.x = value.x, .y = value.y};
}

bool requestViewportPick(luna::Renderer& renderer,
                         luna::SceneViewportInstance& viewport,
                         const ImVec2& image_min,
                         const ImVec2& image_max,
                         const ImVec2& uv0,
                         const ImVec2& uv1,
                         luna::RHI::Extent2D texture_extent,
                         bool editor_camera_mouse_captured)
{
    if (texture_extent.width == 0 || texture_extent.height == 0 || editor_camera_mouse_captured || ImGuizmo::IsOver() ||
        ImGuizmo::IsUsing() || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        return false;
    }

    const ImVec2 mouse_position = ImGui::GetMousePos();
    const float image_width = image_max.x - image_min.x;
    const float image_height = image_max.y - image_min.y;
    if (image_width <= 0.0f || image_height <= 0.0f) {
        return false;
    }

    if (mouse_position.x < image_min.x || mouse_position.x >= image_max.x || mouse_position.y < image_min.y ||
        mouse_position.y >= image_max.y) {
        return false;
    }

    const float local_x = std::clamp((mouse_position.x - image_min.x) / image_width, 0.0f, 0.999999f);
    const float local_y = std::clamp((mouse_position.y - image_min.y) / image_height, 0.0f, 0.999999f);

    const float texture_u = std::clamp(uv0.x + (uv1.x - uv0.x) * local_x, 0.0f, 0.999999f);
    const float texture_v = std::clamp(uv0.y + (uv1.y - uv0.y) * local_y, 0.0f, 0.999999f);

    const uint32_t pixel_x = static_cast<uint32_t>(texture_u * static_cast<float>(texture_extent.width));
    const uint32_t color_pixel_y = (std::min)(
        static_cast<uint32_t>(texture_v * static_cast<float>(texture_extent.height)), texture_extent.height - 1);

    return viewport.requestScenePick(renderer, (std::min)(pixel_x, texture_extent.width - 1), color_pixel_y);
}

} // namespace

namespace luna {

EditorDefaultSceneViewportController::DrawResult
    EditorDefaultSceneViewportController::draw(editor::Ui& ui,
                                               std::string_view owner_id,
                                               Renderer& renderer,
                                               EditorCamera& editor_camera,
                                               EditorAuthoringController& authoring,
                                               EditorRuntimeViewportController& runtime_viewport,
                                               EditorViewportCoordinator& viewports,
                                               EditorViewportGizmoController& gizmo,
                                               SceneViewportInstance& active_viewport,
                                               editor::ViewportId active_viewport_id,
                                               Entity selected_entity)
{
    DrawResult result{};
    result.focused = ImGui::IsWindowFocused();

    gizmo.updateShortcuts(
        result.focused, editor_camera.isMouseCaptured(), ImGui::GetIO().WantTextInput, authoring.selectedEntityId());

    const editor::Vec2 available = ui.contentRegionAvail();
    const editor::Vec2 framebuffer_scale = ui.windowFramebufferScale();
    const float viewport_scale_x =
        std::isfinite(framebuffer_scale.x) && framebuffer_scale.x > 0.0f ? framebuffer_scale.x : 1.0f;
    const float viewport_scale_y =
        std::isfinite(framebuffer_scale.y) && framebuffer_scale.y > 0.0f ? framebuffer_scale.y : 1.0f;
    const uint32_t viewport_width = static_cast<uint32_t>((std::max)(available.x * viewport_scale_x, 0.0f));
    const uint32_t viewport_height = static_cast<uint32_t>((std::max)(available.y * viewport_scale_y, 0.0f));
    if (!runtime_viewport.isRuntimeViewportEnabled()) {
        editor_camera.setViewportSize(static_cast<float>(viewport_width), static_cast<float>(viewport_height));
    }

    const auto& viewport_state = active_viewport.sync(renderer, viewport_width, viewport_height);
    const auto& scene_texture =
        renderer.getSceneViewportOutputTexture(active_viewport.rendererViewportHandle(renderer));
    const ImTextureID texture_id = ImGuiRhiContext::GetTextureId(scene_texture);
    if (texture_id != 0 && available.x > 0.0f && available.y > 0.0f) {
        const bool flip_uv_y = viewport_state.y_flip;
        const ImVec2 uv0(0.0f, flip_uv_y ? 1.0f : 0.0f);
        const ImVec2 uv1(1.0f, flip_uv_y ? 0.0f : 1.0f);

        ImGui::Image(texture_id, ImVec2{available.x, available.y}, uv0, uv1);
        const ImVec2 viewport_min = ImGui::GetItemRectMin();
        const ImVec2 viewport_max = ImGui::GetItemRectMax();
        const ImVec2 viewport_size = ImGui::GetItemRectSize();
        const bool viewport_item_hovered = ImGui::IsItemHovered();
        const ViewportInteractionState& interaction = viewports.recordViewportSurfaceInteraction(
            active_viewport_id,
            owner_id,
            ViewportInteractionInput{
                .rect =
                    ViewportSurfaceRect{
                        .min = toEditorVec2(viewport_min),
                        .max = toEditorVec2(viewport_max),
                    },
                .hovered = viewport_item_hovered,
                .active = ImGui::IsItemActive(),
                .clicked = viewport_item_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left),
                .double_clicked = viewport_item_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left),
            });

        const bool allow_gizmo_interaction =
            viewports.isViewportInputAllowed(active_viewport_id) || gizmo.hasActiveTransformTransaction();
        const bool gizmo_active =
            !runtime_viewport.isRuntimeViewportEnabled() &&
            gizmo.draw(viewport_min, viewport_size, allow_gizmo_interaction, editor_camera, authoring, selected_entity);
        if (interaction.clicked && !gizmo_active && !runtime_viewport.isRuntimeViewportEnabled()) {
            (void) requestViewportPick(renderer,
                                       active_viewport,
                                       viewport_min,
                                       viewport_max,
                                       uv0,
                                       uv1,
                                       scene_texture
                                           ? luna::RHI::Extent2D{scene_texture->GetWidth(), scene_texture->GetHeight()}
                                           : luna::RHI::Extent2D{0, 0},
                                       editor_camera.isMouseCaptured());
        }
    } else if (available.x > 0.0f && available.y > 0.0f) {
        ImGui::SetCursorPos(editor::scaleEditorUi(16.0f, 16.0f));
        ui.text("Viewport texture will appear after the first rendered frame.");
    }

    return result;
}

} // namespace luna
