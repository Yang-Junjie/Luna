#pragma once

#include "EditorApi/EditorNativePluginApi.h"

namespace luna::editor::native {

using Vec2 = LunaEditorVec2;
using Vec3 = LunaEditorVec3;
using Vec4 = LunaEditorVec4;
using TextureView = LunaEditorTextureView;

[[nodiscard]] constexpr Vec2 vec2(float x, float y) noexcept
{
    return Vec2{.x = x, .y = y};
}

[[nodiscard]] constexpr Vec2 fillWidth() noexcept
{
    return vec2(-1.0f, 0.0f);
}

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
