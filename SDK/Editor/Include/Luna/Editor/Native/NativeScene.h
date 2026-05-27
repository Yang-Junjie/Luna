#pragma once

#include "Luna/Editor/Native/NativeTypes.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace luna::editor::native {

class Scene final {
public:
    constexpr Scene() noexcept = default;

    explicit constexpr Scene(const LunaEditorSceneApi* api) noexcept
        : api_(api)
    {}

    [[nodiscard]] bool available() const noexcept
    {
        return api_ != nullptr;
    }

    [[nodiscard]] bool label(char* out_label, size_t out_label_size) const noexcept
    {
        return api_ != nullptr && api_->scene_label != nullptr && out_label != nullptr &&
               api_->scene_label(api_->api_user_data, out_label, out_label_size) != 0;
    }

    [[nodiscard]] std::string label() const
    {
        if (api_ == nullptr || api_->scene_label == nullptr) {
            return {};
        }

        std::array<char, 256> buffer{};
        if (api_->scene_label(api_->api_user_data, buffer.data(), buffer.size()) == 0) {
            return {};
        }
        return buffer.data();
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

    [[nodiscard]] bool openSceneFile(const char* scene_file_path) const noexcept
    {
        return api_ != nullptr && api_->open_scene_file != nullptr && scene_file_path != nullptr &&
               api_->open_scene_file(api_->api_user_data, scene_file_path) != 0;
    }

    [[nodiscard]] std::vector<SceneEntityInfo> entities() const
    {
        std::vector<SceneEntityInfo> result;
        if (api_ == nullptr || api_->enumerate_entities == nullptr) {
            return result;
        }

        result.reserve(entityCount());
        api_->enumerate_entities(api_->api_user_data, &result, &appendEntityInfo);
        return result;
    }

    [[nodiscard]] bool entityExists(uint64_t entity_id) const noexcept
    {
        return api_ != nullptr && api_->entity_exists != nullptr &&
               api_->entity_exists(api_->api_user_data, entity_id) != 0;
    }

    [[nodiscard]] bool entityInfo(EntityId entity_id, LunaEditorSceneEntityInfo* out_info) const noexcept
    {
        return api_ != nullptr && api_->entity_info != nullptr && out_info != nullptr &&
               api_->entity_info(api_->api_user_data, entity_id, out_info) != 0;
    }

    [[nodiscard]] SceneEntityInfo entityInfo(EntityId entity_id) const
    {
        SceneEntityInfo result{};
        if (api_ == nullptr || api_->entity_info == nullptr) {
            return result;
        }

        std::array<char, 256> name{};
        std::array<char, 256> parent_name{};
        LunaEditorSceneEntityInfo native_info = makeNativeEntityInfo(name, parent_name);
        if (api_->entity_info(api_->api_user_data, entity_id, &native_info) == 0) {
            return result;
        }
        return fromNative(native_info);
    }

    [[nodiscard]] bool isEntityDescendantOf(EntityId entity_id, EntityId potential_ancestor_id) const noexcept
    {
        return api_ != nullptr && api_->is_entity_descendant_of != nullptr &&
               api_->is_entity_descendant_of(api_->api_user_data, entity_id, potential_ancestor_id) != 0;
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

    [[nodiscard]] EntityId createEntity(const SceneEntityCreateRequest& request) const noexcept
    {
        const LunaEditorSceneEntityCreateRequest native_request = request.native();
        return createEntity(native_request);
    }

    [[nodiscard]] bool destroyEntity(uint64_t entity_id) const noexcept
    {
        return api_ != nullptr && api_->destroy_entity != nullptr &&
               api_->destroy_entity(api_->api_user_data, entity_id) != 0;
    }

    [[nodiscard]] bool
        reparentEntity(EntityId entity_id, EntityId new_parent_id, bool preserve_world_transform = true) const noexcept
    {
        return api_ != nullptr && api_->reparent_entity != nullptr &&
               api_->reparent_entity(api_->api_user_data, entity_id, new_parent_id, preserve_world_transform ? 1 : 0) !=
                   0;
    }

    [[nodiscard]] bool setEntityName(EntityId entity_id, const char* name) const noexcept
    {
        return api_ != nullptr && api_->set_entity_name != nullptr && name != nullptr &&
               api_->set_entity_name(api_->api_user_data, entity_id, name) != 0;
    }

    [[nodiscard]] bool getTransform(EntityId entity_id, LunaEditorSceneTransform* out_transform) const noexcept
    {
        return api_ != nullptr && api_->get_entity_transform != nullptr && out_transform != nullptr &&
               api_->get_entity_transform(api_->api_user_data, entity_id, out_transform) != 0;
    }

    [[nodiscard]] LunaEditorSceneTransform transform(EntityId entity_id) const noexcept
    {
        LunaEditorSceneTransform value{};
        (void) getTransform(entity_id, &value);
        return value;
    }

    [[nodiscard]] bool setTransform(EntityId entity_id, const LunaEditorSceneTransform& transform) const noexcept
    {
        return api_ != nullptr && api_->set_entity_transform != nullptr &&
               api_->set_entity_transform(api_->api_user_data, entity_id, &transform) != 0;
    }

    [[nodiscard]] bool getCameraComponent(uint64_t entity_id,
                                          LunaEditorSceneCameraComponent* out_component) const noexcept
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

    [[nodiscard]] bool getLightComponent(EntityId entity_id,
                                         LunaEditorSceneLightComponent* out_component) const noexcept
    {
        return api_ != nullptr && api_->get_light_component != nullptr && out_component != nullptr &&
               api_->get_light_component(api_->api_user_data, entity_id, out_component) != 0;
    }

    [[nodiscard]] bool setLightComponent(EntityId entity_id,
                                         const LunaEditorSceneLightComponent& component) const noexcept
    {
        return api_ != nullptr && api_->set_light_component != nullptr &&
               api_->set_light_component(api_->api_user_data, entity_id, &component) != 0;
    }

    [[nodiscard]] bool getMeshComponent(EntityId entity_id, LunaEditorSceneMeshComponent* out_component) const noexcept
    {
        return api_ != nullptr && api_->get_mesh_component != nullptr && out_component != nullptr &&
               api_->get_mesh_component(api_->api_user_data, entity_id, out_component) != 0;
    }

    [[nodiscard]] MeshComponent meshComponent(EntityId entity_id, size_t material_capacity = 16u) const
    {
        MeshComponent result{};
        std::vector<AssetHandle> materials(material_capacity);
        LunaEditorSceneMeshComponent native_mesh = makeMeshComponentView(materials);
        if (!getMeshComponent(entity_id, &native_mesh)) {
            return result;
        }

        result.mesh_handle = native_mesh.mesh_handle;
        result.first_submesh = native_mesh.first_submesh;
        result.submesh_count = native_mesh.submesh_count;
        result.submesh_material_handles.assign(
            materials.begin(), materials.begin() + (std::min)(materials.size(), native_mesh.submesh_material_count));
        return result;
    }

    [[nodiscard]] bool setMeshComponent(EntityId entity_id,
                                        const LunaEditorSceneMeshComponent& component) const noexcept
    {
        return api_ != nullptr && api_->set_mesh_component != nullptr &&
               api_->set_mesh_component(api_->api_user_data, entity_id, &component) != 0;
    }

    [[nodiscard]] bool setMeshComponent(EntityId entity_id, const MeshComponent& component) const
    {
        std::vector<AssetHandle> materials = component.submesh_material_handles;
        LunaEditorSceneMeshComponent native_mesh = makeMeshComponentView(materials);
        native_mesh.mesh_handle = component.mesh_handle;
        native_mesh.first_submesh = component.first_submesh;
        native_mesh.submesh_count = component.submesh_count;
        native_mesh.submesh_material_count = materials.size();
        return setMeshComponent(entity_id, native_mesh);
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
    template <size_t NameSize, size_t ParentNameSize>
    [[nodiscard]] static LunaEditorSceneEntityInfo
        makeNativeEntityInfo(std::array<char, NameSize>& name, std::array<char, ParentNameSize>& parent_name) noexcept
    {
        LunaEditorSceneEntityInfo info{};
        info.struct_size = sizeof(LunaEditorSceneEntityInfo);
        info.api_version = LUNA_EDITOR_SCENE_ENTITY_INFO_API_VERSION;
        info.name = name.data();
        info.name_size = name.size();
        info.parent_name = parent_name.data();
        info.parent_name_size = parent_name.size();
        return info;
    }

    [[nodiscard]] static SceneEntityInfo fromNative(const LunaEditorSceneEntityInfo& info)
    {
        return SceneEntityInfo{
            .id = info.id,
            .parent_id = info.parent_id,
            .component_flags = info.component_flags,
            .child_count = info.child_count,
            .name = info.name != nullptr ? info.name : "",
            .parent_name = info.parent_name != nullptr ? info.parent_name : "",
        };
    }

    [[nodiscard]] static LunaEditorSceneMeshComponent
        makeMeshComponentView(std::vector<AssetHandle>& materials) noexcept
    {
        LunaEditorSceneMeshComponent component{};
        component.struct_size = sizeof(LunaEditorSceneMeshComponent);
        component.api_version = LUNA_EDITOR_SCENE_MESH_COMPONENT_API_VERSION;
        component.submesh_material_handles = materials.empty() ? nullptr : materials.data();
        component.submesh_material_capacity = materials.size();
        component.submesh_material_count = materials.size();
        return component;
    }

    static int appendEntityInfo(void* user_data, const LunaEditorSceneEntityInfo* entity_info)
    {
        auto* result = static_cast<std::vector<SceneEntityInfo>*>(user_data);
        if (result == nullptr || entity_info == nullptr) {
            return 0;
        }

        result->push_back(fromNative(*entity_info));
        return 1;
    }

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

[[nodiscard]] inline LunaEditorSceneLightComponent makeDefaultDirectionalLight() noexcept
{
    LunaEditorSceneLightComponent light{};
    light.struct_size = sizeof(LunaEditorSceneLightComponent);
    light.api_version = LUNA_EDITOR_SCENE_LIGHT_COMPONENT_API_VERSION;
    light.type = 0;
    light.enabled = 1;
    light.color = Vec3{.x = 1.0f, .y = 0.95f, .z = 0.85f};
    light.intensity = 3.0f;
    light.range = 10.0f;
    light.inner_cone_angle_degrees = 10.0f;
    light.outer_cone_angle_degrees = 20.0f;
    return light;
}

} // namespace luna::editor::native
