#pragma once

#include "Luna/Editor/Native/NativeTypes.h"

namespace luna::editor::native {

class Scene final {
public:
    constexpr Scene() noexcept = default;
    explicit constexpr Scene(const LunaEditorSceneApi* api) noexcept
        : api_(api)
    {
    }

    [[nodiscard]] bool label(char* out_label, size_t out_label_size) const noexcept
    {
        return api_ != nullptr && api_->scene_label != nullptr && out_label != nullptr &&
               api_->scene_label(api_->api_user_data, out_label, out_label_size) != 0;
    }

    [[nodiscard]] size_t entityCount() const noexcept
    {
        if (api_ != nullptr && api_->entity_count != nullptr) {
            return api_->entity_count(api_->api_user_data);
        }
        return 0u;
    }

    [[nodiscard]] bool canEdit() const noexcept
    {
        return api_ != nullptr && api_->can_edit_scene != nullptr && api_->can_edit_scene(api_->api_user_data) != 0;
    }

    [[nodiscard]] bool entityExists(uint64_t entity_id) const noexcept
    {
        return api_ != nullptr && api_->entity_exists != nullptr &&
               api_->entity_exists(api_->api_user_data, entity_id) != 0;
    }

    [[nodiscard]] uint64_t createEntity(const char* name) const noexcept
    {
        if (api_ != nullptr && api_->create_entity != nullptr) {
            return api_->create_entity(api_->api_user_data, name);
        }
        return 0u;
    }

    [[nodiscard]] uint64_t createEntity(const LunaEditorSceneEntityCreateRequest& request) const noexcept
    {
        if (api_ != nullptr && api_->create_entity_ex != nullptr) {
            return api_->create_entity_ex(api_->api_user_data, &request);
        }
        return 0u;
    }

    [[nodiscard]] bool destroyEntity(uint64_t entity_id) const noexcept
    {
        return api_ != nullptr && api_->destroy_entity != nullptr &&
               api_->destroy_entity(api_->api_user_data, entity_id) != 0;
    }

    [[nodiscard]] bool getCameraComponent(uint64_t entity_id, LunaEditorSceneCameraComponent* out_component) const noexcept
    {
        return api_ != nullptr && api_->get_camera_component != nullptr && out_component != nullptr &&
               api_->get_camera_component(api_->api_user_data, entity_id, out_component) != 0;
    }

    [[nodiscard]] bool setCameraComponent(uint64_t entity_id,
                                          const LunaEditorSceneCameraComponent& component) const noexcept
    {
        return api_ != nullptr && api_->set_camera_component != nullptr &&
               api_->set_camera_component(api_->api_user_data, entity_id, &component) != 0;
    }

    [[nodiscard]] bool addComponent(uint64_t entity_id, LunaEditorSceneComponentKind component_kind) const noexcept
    {
        return api_ != nullptr && api_->add_component != nullptr &&
               api_->add_component(api_->api_user_data, entity_id, component_kind) != 0;
    }

    [[nodiscard]] bool removeComponent(uint64_t entity_id, LunaEditorSceneComponentKind component_kind) const noexcept
    {
        return api_ != nullptr && api_->remove_component != nullptr &&
               api_->remove_component(api_->api_user_data, entity_id, component_kind) != 0;
    }

    [[nodiscard]] const LunaEditorSceneApi* native() const noexcept
    {
        return api_;
    }

private:
    const LunaEditorSceneApi* api_{};
};

[[nodiscard]] inline LunaEditorSceneCameraComponent makeDefaultPerspectiveCamera() noexcept
{
    LunaEditorSceneCameraComponent camera{};
    camera.struct_size = sizeof(LunaEditorSceneCameraComponent);
    camera.api_version = LUNA_EDITOR_SCENE_CAMERA_COMPONENT_API_VERSION;
    camera.primary = 1;
    camera.fixed_aspect_ratio = 0;
    camera.projection = 0;
    camera.perspective_vertical_fov_degrees = 50.0f;
    camera.perspective_near = 0.05f;
    camera.perspective_far = 500.0f;
    camera.orthographic_size = 10.0f;
    camera.orthographic_near = -100.0f;
    camera.orthographic_far = 100.0f;
    return camera;
}

} // namespace luna::editor::native
