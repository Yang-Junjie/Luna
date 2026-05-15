#pragma once

#include "Luna/Editor/Native/NativeTypes.h"

namespace luna::editor::native {

class Viewport final {
public:
    constexpr Viewport() noexcept = default;
    explicit constexpr Viewport(const LunaEditorViewportApi* api) noexcept
        : api_(api)
    {
    }

    [[nodiscard]] bool sceneTextureView(TextureView* out_texture) const noexcept
    {
        return api_ != nullptr && api_->scene_texture_view != nullptr && out_texture != nullptr &&
               api_->scene_texture_view(api_->api_user_data, out_texture) != 0;
    }

    [[nodiscard]] Vec3 editorCameraPosition() const noexcept
    {
        Vec3 position{};
        if (api_ != nullptr && api_->editor_camera_position != nullptr) {
            api_->editor_camera_position(api_->api_user_data, &position);
        }
        return position;
    }

    [[nodiscard]] bool gizmoOperationName(char* out_value, size_t out_value_size) const noexcept
    {
        return api_ != nullptr && api_->gizmo_operation_name != nullptr && out_value != nullptr &&
               api_->gizmo_operation_name(api_->api_user_data, out_value, out_value_size) != 0;
    }

    [[nodiscard]] bool gizmoModeName(char* out_value, size_t out_value_size) const noexcept
    {
        return api_ != nullptr && api_->gizmo_mode_name != nullptr && out_value != nullptr &&
               api_->gizmo_mode_name(api_->api_user_data, out_value, out_value_size) != 0;
    }

    [[nodiscard]] bool pickDebugVisualizationEnabled() const noexcept
    {
        return api_ != nullptr && api_->pick_debug_visualization_enabled != nullptr &&
               api_->pick_debug_visualization_enabled(api_->api_user_data) != 0;
    }

    void setPickDebugVisualizationEnabled(bool enabled) const noexcept
    {
        if (api_ != nullptr && api_->set_pick_debug_visualization_enabled != nullptr) {
            api_->set_pick_debug_visualization_enabled(api_->api_user_data, enabled ? 1 : 0);
        }
    }

    [[nodiscard]] bool editorGridEnabled() const noexcept
    {
        return api_ != nullptr && api_->editor_grid_enabled != nullptr &&
               api_->editor_grid_enabled(api_->api_user_data) != 0;
    }

    void setEditorGridEnabled(bool enabled) const noexcept
    {
        if (api_ != nullptr && api_->set_editor_grid_enabled != nullptr) {
            api_->set_editor_grid_enabled(api_->api_user_data, enabled ? 1 : 0);
        }
    }

    [[nodiscard]] uint64_t defaultSceneViewport() const noexcept
    {
        if (api_ != nullptr && api_->default_scene_viewport != nullptr) {
            return api_->default_scene_viewport(api_->api_user_data);
        }
        return 0u;
    }

    [[nodiscard]] uint64_t createSceneViewport(const char* debug_name) const noexcept
    {
        if (api_ != nullptr && api_->create_scene_viewport != nullptr) {
            return api_->create_scene_viewport(api_->api_user_data, debug_name);
        }
        return 0u;
    }

    void destroySceneViewport(uint64_t viewport_id) const noexcept
    {
        if (api_ != nullptr && api_->destroy_scene_viewport != nullptr) {
            api_->destroy_scene_viewport(api_->api_user_data, viewport_id);
        }
    }

    [[nodiscard]] bool isSceneViewportValid(uint64_t viewport_id) const noexcept
    {
        return api_ != nullptr && api_->is_scene_viewport_valid != nullptr &&
               api_->is_scene_viewport_valid(api_->api_user_data, viewport_id) != 0;
    }

    [[nodiscard]] bool syncSceneViewport(uint64_t viewport_id,
                                         uint32_t framebuffer_width,
                                         uint32_t framebuffer_height,
                                         LunaEditorViewportPresentation* out_presentation) const noexcept
    {
        return api_ != nullptr && api_->sync_scene_viewport_ex != nullptr && out_presentation != nullptr &&
               api_->sync_scene_viewport_ex(api_->api_user_data,
                                            viewport_id,
                                            framebuffer_width,
                                            framebuffer_height,
                                            out_presentation) != 0;
    }

    [[nodiscard]] bool sceneTextureView(uint64_t viewport_id, TextureView* out_texture) const noexcept
    {
        return api_ != nullptr && api_->scene_texture_view_ex != nullptr && out_texture != nullptr &&
               api_->scene_texture_view_ex(api_->api_user_data, viewport_id, out_texture) != 0;
    }

    [[nodiscard]] const LunaEditorViewportApi* native() const noexcept
    {
        return api_;
    }

private:
    const LunaEditorViewportApi* api_{};
};

[[nodiscard]] inline LunaEditorViewportPresentation makeViewportPresentation() noexcept
{
    LunaEditorViewportPresentation presentation{};
    presentation.struct_size = sizeof(LunaEditorViewportPresentation);
    presentation.api_version = LUNA_EDITOR_VIEWPORT_API_VERSION;
    return presentation;
}

} // namespace luna::editor::native
