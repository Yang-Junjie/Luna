#pragma once

#include "EditorApi/EditorNativePluginApi.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace luna::editor::native {

using Vec2 = LunaEditorVec2;
using Vec3 = LunaEditorVec3;
using Vec4 = LunaEditorVec4;
using TextureView = LunaEditorTextureView;
using AssetHandle = uint64_t;
using EntityId = uint64_t;
using ViewportId = uint64_t;

[[nodiscard]] constexpr Vec2 vec2(float x, float y) noexcept
{
    return Vec2{.x = x, .y = y};
}

[[nodiscard]] constexpr Vec2 fillWidth() noexcept
{
    return vec2(-1.0f, 0.0f);
}

struct AssetInfo {
    AssetHandle handle{};
    LunaEditorAssetType type{LunaEditorAssetType_None};
    bool exists{};
    bool builtin{};
    bool loading{};
    bool memory_only{};
    std::string label;
    std::string detail;
    std::string project_path;
    std::string absolute_path;
};

struct AssetRefreshResult {
    bool success{};
    bool project_loaded{};
    uint64_t revision{};
    std::string message;
    size_t discovered_assets{};
    size_t imported_missing_assets{};
    size_t loaded_existing_metadata{};
    size_t rebuilt_metadata{};
    size_t unsupported_files_skipped{};
    size_t failed_assets{};
    size_t missing_metadata_after_sync{};
    size_t script_files_skipped_no_plugin{};
    size_t script_files_skipped_unsupported_language{};
    size_t generated_model_files{};
    size_t generated_model_metadata{};
    size_t generated_material_files{};
    size_t generated_material_metadata{};
    size_t generated_texture_metadata{};
    size_t failed_generated_model_assets{};
};

struct ProjectInfo {
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::string start_scene;
    std::string assets_path;
    std::string selected_script_plugin_id;
    std::string selected_script_backend_name;
};

struct SceneEntityInfo {
    EntityId id{};
    EntityId parent_id{};
    uint32_t component_flags{};
    size_t child_count{};
    std::string name;
    std::string parent_name;
};

struct SceneEntityCreateRequest {
    LunaEditorSceneEntityCreateKind kind{LunaEditorSceneEntityCreateKind_Empty};
    const char* name{};
    EntityId parent_id{};
    AssetHandle asset_handle{};

    [[nodiscard]] LunaEditorSceneEntityCreateRequest native() const noexcept
    {
        LunaEditorSceneEntityCreateRequest request{};
        request.struct_size = sizeof(LunaEditorSceneEntityCreateRequest);
        request.api_version = LUNA_EDITOR_SCENE_ENTITY_CREATE_REQUEST_API_VERSION;
        request.kind = kind;
        request.name = name;
        request.parent_id = parent_id;
        request.asset_handle = asset_handle;
        return request;
    }
};

struct MeshComponent {
    AssetHandle mesh_handle{};
    uint32_t first_submesh{};
    uint32_t submesh_count{};
    std::vector<AssetHandle> submesh_material_handles;
};

struct CommandDescriptor {
    const char* id{};
    const char* label{};
    const char* description{};
    const char* shortcut{};
    void* user_data{};
    int (*can_execute)(void* user_data, const LunaEditorHostApi* host_api){};
    int (*is_checked)(void* user_data, const LunaEditorHostApi* host_api){};
    void (*execute)(void* user_data, const LunaEditorHostApi* host_api){};

    [[nodiscard]] LunaEditorCommandDescriptor native() const noexcept
    {
        LunaEditorCommandDescriptor descriptor{};
        descriptor.struct_size = sizeof(LunaEditorCommandDescriptor);
        descriptor.api_version = LUNA_EDITOR_COMMAND_DESCRIPTOR_API_VERSION;
        descriptor.id = id;
        descriptor.label = label;
        descriptor.description = description;
        descriptor.shortcut = shortcut;
        descriptor.command_user_data = user_data;
        descriptor.can_execute = can_execute;
        descriptor.is_checked = is_checked;
        descriptor.execute = execute;
        return descriptor;
    }
};

struct WindowDescriptor {
    const char* id{};
    const char* title{};
    bool default_open{};
    Vec2 default_size{};
    uint32_t flags{LunaEditorWindowFlag_None};
    void* user_data{};
    void (*draw)(void* user_data, const LunaEditorHostApi* host_api){};

    [[nodiscard]] LunaEditorWindowDescriptor native() const noexcept
    {
        LunaEditorWindowDescriptor descriptor{};
        descriptor.struct_size = sizeof(LunaEditorWindowDescriptor);
        descriptor.api_version = LUNA_EDITOR_WINDOW_DESCRIPTOR_API_VERSION;
        descriptor.id = id;
        descriptor.title = title;
        descriptor.default_open = default_open ? 1 : 0;
        descriptor.default_size = default_size;
        descriptor.flags = flags;
        descriptor.window_user_data = user_data;
        descriptor.draw = draw;
        return descriptor;
    }
};

struct MenuItemDescriptor {
    const char* menu_path{};
    const char* command_id{};
    const char* label{};
    const char* shortcut{};

    [[nodiscard]] LunaEditorMenuItemDescriptor native() const noexcept
    {
        LunaEditorMenuItemDescriptor descriptor{};
        descriptor.struct_size = sizeof(LunaEditorMenuItemDescriptor);
        descriptor.api_version = LUNA_EDITOR_MENU_ITEM_DESCRIPTOR_API_VERSION;
        descriptor.menu_path = menu_path;
        descriptor.command_id = command_id;
        descriptor.label = label;
        descriptor.shortcut = shortcut;
        return descriptor;
    }
};

struct PluginDescriptor {
    const char* plugin_id{};
    const char* display_name{};
    const char* version{};
    void* user_data{};
    int (*on_load)(void* user_data, const LunaEditorHostApi* host_api){};
    void (*on_unload)(void* user_data, const LunaEditorHostApi* host_api){};

    [[nodiscard]] LunaEditorPluginApi native() const noexcept
    {
        LunaEditorPluginApi descriptor{};
        descriptor.struct_size = sizeof(LunaEditorPluginApi);
        descriptor.api_version = LUNA_EDITOR_PLUGIN_API_VERSION;
        descriptor.plugin_id = plugin_id;
        descriptor.display_name = display_name;
        descriptor.version = version;
        descriptor.plugin_user_data = user_data;
        descriptor.on_load = on_load;
        descriptor.on_unload = on_unload;
        return descriptor;
    }
};

[[nodiscard]] inline bool isCompatibleHost(uint32_t host_api_version, const LunaEditorHostApi* host_api) noexcept
{
    return host_api_version == LUNA_EDITOR_HOST_API_VERSION && host_api != nullptr &&
           host_api->struct_size >= sizeof(LunaEditorHostApi) && host_api->api_version == LUNA_EDITOR_HOST_API_VERSION;
}

[[nodiscard]] inline bool writePluginApi(const PluginDescriptor& descriptor, LunaEditorPluginApi* out_plugin_api) noexcept
{
    if (out_plugin_api == nullptr || descriptor.plugin_id == nullptr || descriptor.display_name == nullptr ||
        descriptor.version == nullptr || descriptor.on_load == nullptr || descriptor.on_unload == nullptr) {
        return false;
    }

    *out_plugin_api = descriptor.native();
    return true;
}

} // namespace luna::editor::native
