#include "Core/Log.h"
#include "EditorEnginePaths.h"
#include "EditorApi/EditorNativePluginApi.h"
#include "Platform/Common/DynamicLibrary.h"
#include "Shell/EditorPluginDependencyResolver.h"
#include "Shell/EditorPluginManager.h"
#include "Shell/EditorPluginManifest.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

constexpr const char* kExpectedPluginId = "luna.test.native";
constexpr const char* kCommandId = "luna.test.native.open";
constexpr const char* kWindowId = "luna.test.native.window";
constexpr const char* kMenuPath = "Tools/Fake Native";

class TempDirectory {
public:
    explicit TempDirectory(std::string_view name)
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path() / ("Luna-" + std::string(name) + "-" + std::to_string(now));
        std::filesystem::create_directories(m_path);
    }

    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;

    ~TempDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_path, ec);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept
    {
        return m_path;
    }

private:
    std::filesystem::path m_path;
};

class TestContext {
public:
    bool expect(bool condition, std::string_view message)
    {
        if (condition) {
            return true;
        }

        ++m_failures;
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }

    [[nodiscard]] int result() const noexcept
    {
        return m_failures == 0 ? 0 : 1;
    }

private:
    int m_failures{0};
};

void writeTextFile(const std::filesystem::path& path, std::string_view contents)
{
    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::binary);
    file << contents;
}

std::filesystem::path testPluginBinaryPath(std::string_view target_name)
{
#if defined(_WIN32)
    constexpr std::string_view extension = ".dll";
#elif defined(__APPLE__)
    constexpr std::string_view extension = ".dylib";
#else
    constexpr std::string_view extension = ".so";
#endif

    return std::filesystem::path(LUNA_TEST_EDITOR_PLUGIN_DIR) / (std::string(target_name) + std::string(extension));
}

std::string yamlQuotedPath(const std::filesystem::path& path)
{
    const std::string value = path.generic_string();
    std::string quoted;
    quoted.reserve(value.size() + 2);
    quoted.push_back('"');
    for (const char character : value) {
        if (character == '"' || character == '\\') {
            quoted.push_back('\\');
        }
        quoted.push_back(character);
    }
    quoted.push_back('"');
    return quoted;
}

void writeEditorPluginManifest(const std::filesystem::path& plugins_root,
                               std::string_view directory_name,
                               std::string_view plugin_id,
                               std::string_view display_name,
                               const std::filesystem::path& entry_path,
                               const std::vector<std::string_view>& dependencies)
{
    std::ostringstream manifest;
    manifest << "EditorPlugin:\n"
             << "  Id: " << plugin_id << "\n"
             << "  DisplayName: " << display_name << "\n"
             << "  Runtime: Native\n"
             << "  Version: 0.1.0\n"
             << "  Enabled: true\n"
             << "  Entry: " << yamlQuotedPath(entry_path) << "\n";
    if (!dependencies.empty()) {
        manifest << "  Dependencies:\n";
        for (const std::string_view dependency : dependencies) {
            manifest << "    - " << dependency << "\n";
        }
    }

    writeTextFile(plugins_root / std::string(directory_name) / "editor-plugin.yaml", manifest.str());
}

enum class NativeLoadStatus {
    Loaded,
    MissingLibrary,
    MissingSymbol,
    CreateFailed,
    PluginApiSizeMismatch,
    PluginApiVersionMismatch,
    EmptyPluginId,
    PluginIdMismatch,
    MissingCallbacks,
    OnLoadFailed,
};

const char* statusName(NativeLoadStatus status)
{
    switch (status) {
        case NativeLoadStatus::Loaded:
            return "Loaded";
        case NativeLoadStatus::MissingLibrary:
            return "MissingLibrary";
        case NativeLoadStatus::MissingSymbol:
            return "MissingSymbol";
        case NativeLoadStatus::CreateFailed:
            return "CreateFailed";
        case NativeLoadStatus::PluginApiSizeMismatch:
            return "PluginApiSizeMismatch";
        case NativeLoadStatus::PluginApiVersionMismatch:
            return "PluginApiVersionMismatch";
        case NativeLoadStatus::EmptyPluginId:
            return "EmptyPluginId";
        case NativeLoadStatus::PluginIdMismatch:
            return "PluginIdMismatch";
        case NativeLoadStatus::MissingCallbacks:
            return "MissingCallbacks";
        case NativeLoadStatus::OnLoadFailed:
            return "OnLoadFailed";
    }
    return "Unknown";
}

struct TestNativeHost {
    struct CommandRecord {
        std::string id;
        std::string owner_id;
        void* user_data{nullptr};
        int (*can_execute)(void*, const LunaEditorHostApi*){nullptr};
        int (*is_checked)(void*, const LunaEditorHostApi*){nullptr};
        void (*execute)(void*, const LunaEditorHostApi*){nullptr};
    };

    struct WindowRecord {
        std::string id;
        std::string owner_id;
        bool open{false};
        void* user_data{nullptr};
        void (*draw)(void*, const LunaEditorHostApi*){nullptr};
    };

    struct MenuRecord {
        std::string menu_path;
        std::string command_id;
        std::string label;
        std::string shortcut;
        std::string owner_id;
    };

    struct SceneEntityRecord {
        uint64_t id{0};
        uint64_t parent_id{0};
        std::string name;
        uint32_t component_flags{LunaEditorSceneEntityComponentFlag_Transform};
        LunaEditorSceneTransform transform{
            .translation = LunaEditorVec3{.x = 0.0f, .y = 0.0f, .z = 0.0f},
            .rotation_degrees = LunaEditorVec3{.x = 0.0f, .y = 0.0f, .z = 0.0f},
            .scale = LunaEditorVec3{.x = 1.0f, .y = 1.0f, .z = 1.0f},
        };
        bool has_camera{false};
        LunaEditorSceneCameraComponent camera{};
        bool has_light{false};
        LunaEditorSceneLightComponent light{};
        bool has_mesh{false};
        uint64_t mesh_handle{0u};
        uint32_t mesh_first_submesh{0u};
        uint32_t mesh_submesh_count{0u};
        std::vector<uint64_t> mesh_submesh_material_handles;
    };

    explicit TestNativeHost(std::string owner)
        : owner_id(std::move(owner))
    {
        entities.emplace(1u, SceneEntityRecord{.id = 1u, .name = "Root"});

        api.struct_size = sizeof(LunaEditorHostApi);
        api.api_version = LUNA_EDITOR_HOST_API_VERSION;
        api.host_user_data = this;
        api.log = LunaEditorLogApi{
            .struct_size = sizeof(LunaEditorLogApi),
            .api_version = LUNA_EDITOR_LOG_API_VERSION,
            .api_user_data = this,
            .log = &log,
        };
        api.ui = LunaEditorUiApi{
            .struct_size = sizeof(LunaEditorUiApi),
            .api_version = LUNA_EDITOR_UI_API_VERSION,
            .api_user_data = this,
            .text = &text,
            .separator = &separator,
            .button = &button,
        };
        api.commands = LunaEditorCommandApi{
            .struct_size = sizeof(LunaEditorCommandApi),
            .api_version = LUNA_EDITOR_COMMAND_API_VERSION,
            .api_user_data = this,
            .register_command = &registerCommand,
            .unregister_command = &unregisterCommand,
            .execute_command = &executeCommand,
            .can_execute_command = &canExecuteCommand,
            .is_command_checked = &isCommandChecked,
        };
        api.windows = LunaEditorWindowApi{
            .struct_size = sizeof(LunaEditorWindowApi),
            .api_version = LUNA_EDITOR_WINDOW_API_VERSION,
            .api_user_data = this,
            .register_window = &registerWindow,
            .unregister_window = &unregisterWindow,
            .is_window_open = &isWindowOpen,
            .set_window_open = &setWindowOpen,
        };
        api.assets = LunaEditorAssetApi{
            .struct_size = sizeof(LunaEditorAssetApi),
            .api_version = LUNA_EDITOR_ASSET_API_VERSION,
            .api_user_data = this,
            .describe_asset = &describeAsset,
            .asset_info = &assetInfo,
            .list_assets = &listAssets,
            .asset_exists = &assetExists,
            .asset_revision = &assetRevision,
            .accepts_asset_type = &acceptsAssetType,
        };
        api.plugin_assets = LunaEditorPluginAssetApi{
            .struct_size = sizeof(LunaEditorPluginAssetApi),
            .api_version = LUNA_EDITOR_PLUGIN_ASSET_API_VERSION,
            .api_user_data = this,
            .plugin_root_path = &pluginRootPath,
            .asset_root_path = &pluginAssetRootPath,
            .exists = &pluginAssetExists,
            .read_text = &pluginAssetReadText,
            .read_bytes = &pluginAssetReadBytes,
        };
        api.menus = LunaEditorMenuApi{
            .struct_size = sizeof(LunaEditorMenuApi),
            .api_version = LUNA_EDITOR_MENU_API_VERSION,
            .api_user_data = this,
            .add_menu_item = &addMenuItem,
            .remove_menu_item = &removeMenuItem,
            .remove_menu_items_for_command = &removeMenuItemsForCommand,
        };
        api.project = LunaEditorProjectApi{
            .struct_size = sizeof(LunaEditorProjectApi),
            .api_version = LUNA_EDITOR_PROJECT_API_VERSION,
            .api_user_data = this,
            .has_project_loaded = &hasProjectLoaded,
            .project_root_path = &projectRootPath,
            .project_info = &projectInfo,
            .save_project = &saveProject,
        };
        api.scene = LunaEditorSceneApi{
            .struct_size = sizeof(LunaEditorSceneApi),
            .api_version = LUNA_EDITOR_SCENE_API_VERSION,
            .api_user_data = this,
            .scene_label = &sceneLabel,
            .entity_count = &entityCount,
            .enumerate_entities = &enumerateEntities,
            .entity_exists = &entityExists,
            .entity_info = &entityInfo,
            .create_entity = &createEntity,
            .set_entity_name = &setEntityName,
            .get_entity_transform = &getEntityTransform,
            .set_entity_transform = &setEntityTransform,
            .get_camera_component = &getCameraComponent,
            .set_camera_component = &setCameraComponent,
            .get_light_component = &getLightComponent,
            .set_light_component = &setLightComponent,
            .get_mesh_component = &getMeshComponent,
            .set_mesh_component = &setMeshComponent,
        };
        api.selection = LunaEditorSelectionApi{
            .struct_size = sizeof(LunaEditorSelectionApi),
            .api_version = LUNA_EDITOR_SELECTION_API_VERSION,
            .api_user_data = this,
            .selected_entity_id = &selectedEntityId,
            .select_entity = &selectEntity,
            .clear_selection = &clearSelection,
        };
        api.viewport = LunaEditorViewportApi{
            .struct_size = sizeof(LunaEditorViewportApi),
            .api_version = LUNA_EDITOR_VIEWPORT_API_VERSION,
            .api_user_data = this,
            .sync_scene_viewport = &syncSceneViewport,
            .scene_texture_view = &sceneTextureView,
            .editor_camera_position = &editorCameraPosition,
            .gizmo_operation_name = &gizmoOperationName,
            .gizmo_mode_name = &gizmoModeName,
            .pick_debug_visualization_enabled = &pickDebugVisualizationEnabled,
            .set_pick_debug_visualization_enabled = &setPickDebugVisualizationEnabled,
            .editor_grid_enabled = &editorGridEnabled,
            .set_editor_grid_enabled = &setEditorGridEnabled,
            .default_scene_viewport = &defaultSceneViewport,
            .create_scene_viewport = &createSceneViewport,
            .destroy_scene_viewport = &destroySceneViewport,
            .is_scene_viewport_valid = &isSceneViewportValid,
            .sync_scene_viewport_ex = &syncSceneViewportEx,
            .scene_texture_view_ex = &sceneTextureViewEx,
        };
        api.runtime_viewport = LunaEditorRuntimeViewportApi{
            .struct_size = sizeof(LunaEditorRuntimeViewportApi),
            .api_version = LUNA_EDITOR_RUNTIME_VIEWPORT_API_VERSION,
            .api_user_data = this,
            .is_runtime_viewport_enabled = &isRuntimeViewportEnabled,
            .is_runtime_viewport_requested = &isRuntimeViewportRequested,
            .set_runtime_viewport_requested = &setRuntimeViewportRequested,
            .runtime_entity_count = &runtimeEntityCount,
        };
    }

    void unregisterContributionsForOwner(std::string_view owner)
    {
        for (auto it = commands.begin(); it != commands.end();) {
            if (it->second.owner_id == owner) {
                it = commands.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = windows.begin(); it != windows.end();) {
            if (it->second.owner_id == owner) {
                it = windows.erase(it);
            } else {
                ++it;
            }
        }

        menus.erase(std::remove_if(menus.begin(),
                                   menus.end(),
                                   [&](const MenuRecord& item) {
                                       return item.owner_id == owner;
                                   }),
                    menus.end());

        for (auto it = scene_viewport_owners.begin(); it != scene_viewport_owners.end();) {
            if (it->second == owner) {
                it = scene_viewport_owners.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool drawWindow(std::string_view id)
    {
        const auto it = windows.find(std::string(id));
        if (it == windows.end() || it->second.draw == nullptr) {
            return false;
        }
        it->second.draw(it->second.user_data, &api);
        return true;
    }

    std::string owner_id;
    LunaEditorHostApi api{};
    std::unordered_map<std::string, CommandRecord> commands;
    std::unordered_map<std::string, WindowRecord> windows;
    std::vector<MenuRecord> menus;
    std::vector<std::string> logs;
    std::string plugin_root_path{"F:/FakePlugin"};
    std::string plugin_asset_root_path{"F:/FakePlugin/assets"};
    std::string plugin_asset_text{"fake plugin asset text"};
    std::vector<uint8_t> plugin_asset_bytes{1u, 2u, 3u, 4u};
    std::string project_root_path{"F:/FakeProject"};
    std::string project_name{"Fake Project"};
    std::string scene_label{"Fake Scene"};
    std::unordered_map<uint64_t, SceneEntityRecord> entities;
    uint64_t selected_entity_id{0};
    uint64_t next_entity_id{100};
    LunaEditorTextureView scene_texture_view{
        .texture_id = 0x1234u,
        .width = 960u,
        .height = 540u,
        .y_flip = 0,
    };
    uint64_t next_viewport_id{2u};
    uint64_t created_viewport_id{0u};
    std::unordered_map<uint64_t, std::string> scene_viewport_owners;
    LunaEditorVec3 editor_camera_position{1.0f, 2.0f, 3.0f};
    std::string gizmo_operation_name{"Translate"};
    std::string gizmo_mode_name{"Local"};
    bool pick_debug_visualization_enabled{false};
    bool editor_grid_enabled{true};
    bool runtime_viewport_enabled{false};
    bool runtime_viewport_requested{false};
    size_t runtime_entity_count_value{17u};
    int text_count{0};
    int separator_count{0};
    int button_count{0};
    int describe_asset_count{0};
    int plugin_asset_read_text_count{0};
    int project_info_count{0};
    int scene_info_count{0};
    int created_entity_count{0};
    bool next_button_pressed{false};

private:
    static TestNativeHost* self(void* api_user_data)
    {
        return static_cast<TestNativeHost*>(api_user_data);
    }

    static std::string copyString(const char* value)
    {
        return value != nullptr ? std::string(value) : std::string{};
    }

    static void log(void* api_user_data, LunaEditorLogLevel, const char* message)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            host->logs.push_back(copyString(message));
        }
    }

    static void text(void* api_user_data, const char*)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            ++host->text_count;
        }
    }

    static void separator(void* api_user_data)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            ++host->separator_count;
        }
    }

    static int button(void* api_user_data, const char*, const LunaEditorVec2*, uint32_t)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr) {
            return 0;
        }
        ++host->button_count;
        const bool pressed = host->next_button_pressed;
        host->next_button_pressed = false;
        return pressed ? 1 : 0;
    }

    static int registerCommand(void* api_user_data, const LunaEditorCommandDescriptor* descriptor)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || descriptor == nullptr || descriptor->struct_size < sizeof(LunaEditorCommandDescriptor) ||
            descriptor->api_version != LUNA_EDITOR_COMMAND_DESCRIPTOR_API_VERSION || descriptor->id == nullptr ||
            descriptor->id[0] == '\0' || descriptor->execute == nullptr) {
            return 0;
        }

        CommandRecord record{};
        record.id = descriptor->id;
        record.owner_id = host->owner_id;
        record.user_data = descriptor->command_user_data;
        record.can_execute = descriptor->can_execute;
        record.is_checked = descriptor->is_checked;
        record.execute = descriptor->execute;
        host->commands[record.id] = record;
        return 1;
    }

    static void unregisterCommand(void* api_user_data, const char* id)
    {
        if (TestNativeHost* host = self(api_user_data); host != nullptr && id != nullptr) {
            host->commands.erase(id);
        }
    }

    static int executeCommand(void* api_user_data, const char* id)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || id == nullptr) {
            return 0;
        }

        const auto it = host->commands.find(id);
        if (it == host->commands.end() || it->second.execute == nullptr) {
            return 0;
        }
        if (it->second.can_execute != nullptr && it->second.can_execute(it->second.user_data, &host->api) == 0) {
            return 0;
        }

        it->second.execute(it->second.user_data, &host->api);
        return 1;
    }

    static int canExecuteCommand(void* api_user_data, const char* id)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || id == nullptr) {
            return 0;
        }

        const auto it = host->commands.find(id);
        if (it == host->commands.end()) {
            return 0;
        }
        return it->second.can_execute == nullptr || it->second.can_execute(it->second.user_data, &host->api) != 0 ? 1
                                                                                                                  : 0;
    }

    static int isCommandChecked(void* api_user_data, const char* id)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || id == nullptr) {
            return 0;
        }

        const auto it = host->commands.find(id);
        if (it == host->commands.end() || it->second.is_checked == nullptr) {
            return 0;
        }
        return it->second.is_checked(it->second.user_data, &host->api) != 0 ? 1 : 0;
    }

    static int registerWindow(void* api_user_data, const LunaEditorWindowDescriptor* descriptor)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || descriptor == nullptr || descriptor->struct_size < sizeof(LunaEditorWindowDescriptor) ||
            descriptor->api_version != LUNA_EDITOR_WINDOW_DESCRIPTOR_API_VERSION || descriptor->id == nullptr ||
            descriptor->id[0] == '\0' || descriptor->title == nullptr || descriptor->draw == nullptr) {
            return 0;
        }

        WindowRecord record{};
        record.id = descriptor->id;
        record.owner_id = host->owner_id;
        record.open = descriptor->default_open != 0;
        record.user_data = descriptor->window_user_data;
        record.draw = descriptor->draw;
        host->windows[record.id] = record;
        return 1;
    }

    static void unregisterWindow(void* api_user_data, const char* id)
    {
        if (TestNativeHost* host = self(api_user_data); host != nullptr && id != nullptr) {
            host->windows.erase(id);
        }
    }

    static int isWindowOpen(void* api_user_data, const char* id)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || id == nullptr) {
            return 0;
        }

        const auto it = host->windows.find(id);
        return it != host->windows.end() && it->second.open ? 1 : 0;
    }

    static void setWindowOpen(void* api_user_data, const char* id, int open)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || id == nullptr) {
            return;
        }

        const auto it = host->windows.find(id);
        if (it != host->windows.end()) {
            it->second.open = open != 0;
        }
    }

    static void copyToBuffer(char* out_value, size_t out_value_size, const std::string& value)
    {
        if (out_value == nullptr || out_value_size == 0u) {
            return;
        }

        const size_t copy_size = (std::min)(out_value_size - 1u, value.size());
        if (copy_size > 0u) {
            std::memcpy(out_value, value.data(), copy_size);
        }
        out_value[copy_size] = '\0';
    }

    static bool fillAssetInfo(TestNativeHost& host, uint64_t handle, LunaEditorAssetInfo* out_info)
    {
        if (out_info == nullptr || out_info->struct_size < sizeof(LunaEditorAssetInfo) ||
            out_info->api_version != LUNA_EDITOR_ASSET_INFO_API_VERSION) {
            return false;
        }

        out_info->handle = handle;
        out_info->type = LunaEditorAssetType_Texture;
        out_info->exists = 1;
        out_info->builtin = 0;
        out_info->loading = 0;
        out_info->memory_only = 0;
        copyToBuffer(out_info->label, out_info->label_size, "Fake Asset");
        copyToBuffer(out_info->detail, out_info->detail_size, "Texture");
        copyToBuffer(out_info->project_path, out_info->project_path_size, "Assets/Fake.png");
        copyToBuffer(out_info->absolute_path, out_info->absolute_path_size, host.plugin_asset_root_path + "/Fake.png");
        return true;
    }

    static int describeAsset(void* api_user_data, uint64_t handle, LunaEditorAssetInfo* out_info)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr) {
            return 0;
        }
        ++host->describe_asset_count;
        return fillAssetInfo(*host, handle, out_info) ? 1 : 0;
    }

    static int assetInfo(void* api_user_data, uint64_t handle, LunaEditorAssetInfo* out_info)
    {
        return describeAsset(api_user_data, handle, out_info);
    }

    static size_t listAssets(void* api_user_data,
                             uint32_t,
                             int,
                             void* user_data,
                             LunaEditorEnumerateAssetFn enumerate_fn)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr) {
            return 0u;
        }
        if (enumerate_fn == nullptr) {
            return 1u;
        }

        char label[64]{};
        LunaEditorAssetInfo info{};
        info.struct_size = sizeof(LunaEditorAssetInfo);
        info.api_version = LUNA_EDITOR_ASSET_INFO_API_VERSION;
        info.label = label;
        info.label_size = sizeof(label);
        if (!fillAssetInfo(*host, 42u, &info)) {
            return 0u;
        }
        return enumerate_fn(user_data, &info) != 0 ? 1u : 0u;
    }

    static int assetExists(void*, uint64_t handle)
    {
        return handle != 0u ? 1 : 0;
    }

    static uint64_t assetRevision(void*)
    {
        return 7u;
    }

    static int acceptsAssetType(void*, uint32_t type, const uint32_t* accepted_types, size_t accepted_type_count)
    {
        if (accepted_type_count == 0u) {
            return 1;
        }
        for (size_t index = 0; index < accepted_type_count; ++index) {
            if (accepted_types != nullptr && accepted_types[index] == type) {
                return 1;
            }
        }
        return 0;
    }

    static int pluginRootPath(void* api_user_data, char* out_path, size_t out_path_size)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            copyToBuffer(out_path, out_path_size, host->plugin_root_path);
            return 1;
        }
        return 0;
    }

    static int pluginAssetRootPath(void* api_user_data, char* out_path, size_t out_path_size)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            copyToBuffer(out_path, out_path_size, host->plugin_asset_root_path);
            return 1;
        }
        return 0;
    }

    static int pluginAssetExists(void* api_user_data, const char* relative_asset_path)
    {
        return self(api_user_data) != nullptr && relative_asset_path != nullptr &&
                       std::string_view(relative_asset_path) == "fixture.txt"
                   ? 1
                   : 0;
    }

    static int pluginAssetReadText(void* api_user_data,
                                   const char* relative_asset_path,
                                   char* out_text,
                                   size_t out_text_size,
                                   size_t* out_required_size)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || relative_asset_path == nullptr ||
            std::string_view(relative_asset_path) != "fixture.txt") {
            return 0;
        }

        ++host->plugin_asset_read_text_count;
        const size_t required_size = host->plugin_asset_text.size() + 1u;
        if (out_required_size != nullptr) {
            *out_required_size = required_size;
        }
        if (out_text == nullptr || out_text_size == 0u) {
            return 1;
        }
        copyToBuffer(out_text, out_text_size, host->plugin_asset_text);
        return out_text_size >= required_size ? 1 : 0;
    }

    static int pluginAssetReadBytes(void* api_user_data,
                                    const char* relative_asset_path,
                                    void* out_data,
                                    size_t out_data_size,
                                    size_t* out_required_size)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || relative_asset_path == nullptr ||
            std::string_view(relative_asset_path) != "fixture.bin") {
            return 0;
        }

        if (out_required_size != nullptr) {
            *out_required_size = host->plugin_asset_bytes.size();
        }
        if (out_data == nullptr || out_data_size == 0u) {
            return 1;
        }
        const size_t copy_size = (std::min)(out_data_size, host->plugin_asset_bytes.size());
        if (copy_size > 0u) {
            std::memcpy(out_data, host->plugin_asset_bytes.data(), copy_size);
        }
        return out_data_size >= host->plugin_asset_bytes.size() ? 1 : 0;
    }

    static int addMenuItem(void* api_user_data, const LunaEditorMenuItemDescriptor* descriptor)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || descriptor == nullptr ||
            descriptor->struct_size < sizeof(LunaEditorMenuItemDescriptor) ||
            descriptor->api_version != LUNA_EDITOR_MENU_ITEM_DESCRIPTOR_API_VERSION ||
            descriptor->menu_path == nullptr || descriptor->command_id == nullptr) {
            return 0;
        }

        host->menus.push_back(MenuRecord{
            .menu_path = descriptor->menu_path,
            .command_id = descriptor->command_id,
            .label = descriptor->label != nullptr ? descriptor->label : "",
            .shortcut = descriptor->shortcut != nullptr ? descriptor->shortcut : "",
            .owner_id = host->owner_id,
        });
        return 1;
    }

    static void removeMenuItem(void* api_user_data, const char* menu_path, const char* command_id)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || menu_path == nullptr || command_id == nullptr) {
            return;
        }

        host->menus.erase(std::remove_if(host->menus.begin(),
                                         host->menus.end(),
                                         [&](const MenuRecord& item) {
                                             return item.menu_path == menu_path && item.command_id == command_id;
                                         }),
                          host->menus.end());
    }

    static void removeMenuItemsForCommand(void* api_user_data, const char* command_id)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || command_id == nullptr) {
            return;
        }

        host->menus.erase(std::remove_if(host->menus.begin(),
                                         host->menus.end(),
                                         [&](const MenuRecord& item) {
                                             return item.command_id == command_id;
                                         }),
                          host->menus.end());
    }

    static int hasProjectLoaded(void* api_user_data)
    {
        return self(api_user_data) != nullptr ? 1 : 0;
    }

    static int projectRootPath(void* api_user_data, char* out_path, size_t out_path_size)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            copyToBuffer(out_path, out_path_size, host->project_root_path);
            return 1;
        }
        return 0;
    }

    static int projectInfo(void* api_user_data, LunaEditorProjectInfo* out_info)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || out_info == nullptr || out_info->struct_size < sizeof(LunaEditorProjectInfo) ||
            out_info->api_version != LUNA_EDITOR_PROJECT_INFO_API_VERSION) {
            return 0;
        }

        ++host->project_info_count;
        copyToBuffer(out_info->name, out_info->name_size, host->project_name);
        copyToBuffer(out_info->version, out_info->version_size, "0.1.0");
        copyToBuffer(out_info->author, out_info->author_size, "Test");
        copyToBuffer(out_info->description, out_info->description_size, "Contract test project");
        copyToBuffer(out_info->start_scene, out_info->start_scene_size, "Assets/Fake.lunascene");
        copyToBuffer(out_info->assets_path, out_info->assets_path_size, "Assets");
        copyToBuffer(out_info->selected_script_plugin_id, out_info->selected_script_plugin_id_size, "luna.test.script");
        copyToBuffer(out_info->selected_script_backend_name,
                     out_info->selected_script_backend_name_size,
                     "ContractScript");
        return 1;
    }

    static int saveProject(void*)
    {
        return 1;
    }

    static int sceneLabel(void* api_user_data, char* out_label, size_t out_label_size)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            copyToBuffer(out_label, out_label_size, host->scene_label);
            return 1;
        }
        return 0;
    }

    static size_t entityCount(void* api_user_data)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            return host->entities.size();
        }
        return 0u;
    }

    static bool fillSceneEntityInfo(TestNativeHost& host, const SceneEntityRecord& entity, LunaEditorSceneEntityInfo* out_info)
    {
        if (out_info == nullptr || out_info->struct_size < sizeof(LunaEditorSceneEntityInfo) ||
            out_info->api_version != LUNA_EDITOR_SCENE_ENTITY_INFO_API_VERSION) {
            return false;
        }

        out_info->id = entity.id;
        out_info->parent_id = entity.parent_id;
        out_info->component_flags = entity.component_flags;
        out_info->child_count = 0u;
        for (const auto& [_, candidate] : host.entities) {
            if (candidate.parent_id == entity.id) {
                ++out_info->child_count;
            }
        }
        copyToBuffer(out_info->name, out_info->name_size, entity.name);
        if (entity.parent_id != 0u) {
            const auto parent_it = host.entities.find(entity.parent_id);
            if (parent_it != host.entities.end()) {
                copyToBuffer(out_info->parent_name, out_info->parent_name_size, parent_it->second.name);
            }
        }
        return true;
    }

    static size_t enumerateEntities(void* api_user_data, void* user_data, LunaEditorEnumerateSceneEntityFn enumerate_fn)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr) {
            return 0u;
        }
        if (enumerate_fn == nullptr) {
            return host->entities.size();
        }

        size_t count = 0u;
        for (const auto& [_, entity] : host->entities) {
            char name[64]{};
            char parent_name[64]{};
            LunaEditorSceneEntityInfo info{};
            info.struct_size = sizeof(LunaEditorSceneEntityInfo);
            info.api_version = LUNA_EDITOR_SCENE_ENTITY_INFO_API_VERSION;
            info.name = name;
            info.name_size = sizeof(name);
            info.parent_name = parent_name;
            info.parent_name_size = sizeof(parent_name);
            if (!fillSceneEntityInfo(*host, entity, &info)) {
                continue;
            }
            ++count;
            if (enumerate_fn(user_data, &info) == 0) {
                break;
            }
        }
        return count;
    }

    static int entityExists(void* api_user_data, uint64_t entity_id)
    {
        TestNativeHost* host = self(api_user_data);
        return host != nullptr && host->entities.contains(entity_id) ? 1 : 0;
    }

    static int entityInfo(void* api_user_data, uint64_t entity_id, LunaEditorSceneEntityInfo* out_info)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr) {
            return 0;
        }

        const auto it = host->entities.find(entity_id);
        if (it == host->entities.end()) {
            return 0;
        }
        ++host->scene_info_count;
        return fillSceneEntityInfo(*host, it->second, out_info) ? 1 : 0;
    }

    static uint64_t createEntity(void* api_user_data, const char* name)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr) {
            return 0u;
        }

        const uint64_t entity_id = host->next_entity_id++;
        host->entities.emplace(entity_id,
                               SceneEntityRecord{
                                   .id = entity_id,
                                   .name = name != nullptr && name[0] != '\0' ? std::string(name)
                                                                              : std::string("Entity"),
                               });
        ++host->created_entity_count;
        return entity_id;
    }

    static int setEntityName(void* api_user_data, uint64_t entity_id, const char* name)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || name == nullptr) {
            return 0;
        }

        const auto it = host->entities.find(entity_id);
        if (it == host->entities.end()) {
            return 0;
        }
        it->second.name = name;
        return 1;
    }

    static int getEntityTransform(void* api_user_data, uint64_t entity_id, LunaEditorSceneTransform* out_transform)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || out_transform == nullptr) {
            return 0;
        }

        const auto it = host->entities.find(entity_id);
        if (it == host->entities.end()) {
            return 0;
        }
        *out_transform = it->second.transform;
        return 1;
    }

    static int setEntityTransform(void* api_user_data, uint64_t entity_id, const LunaEditorSceneTransform* transform)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || transform == nullptr) {
            return 0;
        }

        const auto it = host->entities.find(entity_id);
        if (it == host->entities.end()) {
            return 0;
        }
        it->second.transform = *transform;
        return 1;
    }

    static int getCameraComponent(void* api_user_data, uint64_t entity_id, LunaEditorSceneCameraComponent* out_component)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || out_component == nullptr || out_component->struct_size < sizeof(LunaEditorSceneCameraComponent) ||
            out_component->api_version != LUNA_EDITOR_SCENE_CAMERA_COMPONENT_API_VERSION) {
            return 0;
        }

        const auto it = host->entities.find(entity_id);
        if (it == host->entities.end() || !it->second.has_camera) {
            return 0;
        }

        *out_component = it->second.camera;
        out_component->struct_size = sizeof(LunaEditorSceneCameraComponent);
        out_component->api_version = LUNA_EDITOR_SCENE_CAMERA_COMPONENT_API_VERSION;
        return 1;
    }

    static int setCameraComponent(void* api_user_data,
                                  uint64_t entity_id,
                                  const LunaEditorSceneCameraComponent* component)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || component == nullptr) {
            return 0;
        }

        const auto it = host->entities.find(entity_id);
        if (it == host->entities.end()) {
            return 0;
        }

        it->second.has_camera = true;
        it->second.camera = *component;
        it->second.camera.struct_size = sizeof(LunaEditorSceneCameraComponent);
        it->second.camera.api_version = LUNA_EDITOR_SCENE_CAMERA_COMPONENT_API_VERSION;
        it->second.component_flags |= LunaEditorSceneEntityComponentFlag_Camera;
        return 1;
    }

    static int getLightComponent(void* api_user_data, uint64_t entity_id, LunaEditorSceneLightComponent* out_component)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || out_component == nullptr || out_component->struct_size < sizeof(LunaEditorSceneLightComponent) ||
            out_component->api_version != LUNA_EDITOR_SCENE_LIGHT_COMPONENT_API_VERSION) {
            return 0;
        }

        const auto it = host->entities.find(entity_id);
        if (it == host->entities.end() || !it->second.has_light) {
            return 0;
        }

        *out_component = it->second.light;
        out_component->struct_size = sizeof(LunaEditorSceneLightComponent);
        out_component->api_version = LUNA_EDITOR_SCENE_LIGHT_COMPONENT_API_VERSION;
        return 1;
    }

    static int setLightComponent(void* api_user_data,
                                 uint64_t entity_id,
                                 const LunaEditorSceneLightComponent* component)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || component == nullptr) {
            return 0;
        }

        const auto it = host->entities.find(entity_id);
        if (it == host->entities.end()) {
            return 0;
        }

        it->second.has_light = true;
        it->second.light = *component;
        it->second.light.struct_size = sizeof(LunaEditorSceneLightComponent);
        it->second.light.api_version = LUNA_EDITOR_SCENE_LIGHT_COMPONENT_API_VERSION;
        it->second.component_flags |= LunaEditorSceneEntityComponentFlag_Light;
        return 1;
    }

    static int getMeshComponent(void* api_user_data, uint64_t entity_id, LunaEditorSceneMeshComponent* out_component)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || out_component == nullptr || out_component->struct_size < sizeof(LunaEditorSceneMeshComponent) ||
            out_component->api_version != LUNA_EDITOR_SCENE_MESH_COMPONENT_API_VERSION) {
            return 0;
        }

        const auto it = host->entities.find(entity_id);
        if (it == host->entities.end() || !it->second.has_mesh) {
            return 0;
        }

        out_component->mesh_handle = it->second.mesh_handle;
        out_component->first_submesh = it->second.mesh_first_submesh;
        out_component->submesh_count = it->second.mesh_submesh_count;
        out_component->submesh_material_count = it->second.mesh_submesh_material_handles.size();

        size_t copy_count = it->second.mesh_submesh_material_handles.size();
        if (copy_count > out_component->submesh_material_capacity) {
            copy_count = out_component->submesh_material_capacity;
        }
        if (out_component->submesh_material_handles != nullptr) {
            for (size_t index = 0; index < copy_count; ++index) {
                out_component->submesh_material_handles[index] = it->second.mesh_submesh_material_handles[index];
            }
        }
        out_component->struct_size = sizeof(LunaEditorSceneMeshComponent);
        out_component->api_version = LUNA_EDITOR_SCENE_MESH_COMPONENT_API_VERSION;
        return copy_count == it->second.mesh_submesh_material_handles.size() ? 1 : 0;
    }

    static int setMeshComponent(void* api_user_data,
                                uint64_t entity_id,
                                const LunaEditorSceneMeshComponent* component)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || component == nullptr) {
            return 0;
        }

        const auto it = host->entities.find(entity_id);
        if (it == host->entities.end()) {
            return 0;
        }
        if (component->submesh_material_count > 0u && component->submesh_material_handles == nullptr) {
            return 0;
        }

        it->second.has_mesh = true;
        it->second.mesh_handle = component->mesh_handle;
        it->second.mesh_first_submesh = component->first_submesh;
        it->second.mesh_submesh_count = component->submesh_count;
        it->second.mesh_submesh_material_handles.clear();
        it->second.mesh_submesh_material_handles.reserve(component->submesh_material_count);
        for (size_t index = 0; index < component->submesh_material_count; ++index) {
            it->second.mesh_submesh_material_handles.push_back(component->submesh_material_handles[index]);
        }
        it->second.component_flags |= LunaEditorSceneEntityComponentFlag_Mesh;
        return 1;
    }

    static uint64_t selectedEntityId(void* api_user_data)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            return host->selected_entity_id;
        }
        return 0u;
    }

    static void selectEntity(void* api_user_data, uint64_t entity_id)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            host->selected_entity_id = entity_id;
        }
    }

    static void clearSelection(void* api_user_data)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            host->selected_entity_id = 0u;
        }
    }

    static uint64_t defaultSceneViewport(void* api_user_data)
    {
        return self(api_user_data) != nullptr ? 1u : 0u;
    }

    static uint64_t createSceneViewport(void* api_user_data, const char*)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr) {
            return 0u;
        }

        host->created_viewport_id = host->next_viewport_id++;
        host->scene_viewport_owners[host->created_viewport_id] = host->owner_id;
        return host->created_viewport_id;
    }

    static void destroySceneViewport(void* api_user_data, uint64_t viewport_id)
    {
        if (TestNativeHost* host = self(api_user_data); host != nullptr && host->created_viewport_id == viewport_id) {
            host->created_viewport_id = 0u;
            host->scene_viewport_owners.erase(viewport_id);
        }
    }

    static int isSceneViewportValid(void* api_user_data, uint64_t viewport_id)
    {
        TestNativeHost* host = self(api_user_data);
        return host != nullptr && (viewport_id == 1u || host->scene_viewport_owners.contains(viewport_id)) ? 1 : 0;
    }

    static int syncSceneViewportEx(void* api_user_data,
                                   uint64_t viewport_id,
                                   uint32_t framebuffer_width,
                                   uint32_t framebuffer_height,
                                   LunaEditorViewportPresentation* out_presentation)
    {
        if (isSceneViewportValid(api_user_data, viewport_id) == 0) {
            return 0;
        }
        return syncSceneViewport(api_user_data, framebuffer_width, framebuffer_height, out_presentation);
    }

    static int sceneTextureViewEx(void* api_user_data, uint64_t viewport_id, LunaEditorTextureView* out_texture)
    {
        if (isSceneViewportValid(api_user_data, viewport_id) == 0) {
            return 0;
        }
        return sceneTextureView(api_user_data, out_texture);
    }

    static int syncSceneViewport(void* api_user_data,
                                 uint32_t framebuffer_width,
                                 uint32_t framebuffer_height,
                                 LunaEditorViewportPresentation* out_presentation)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || out_presentation == nullptr ||
            out_presentation->struct_size < sizeof(LunaEditorViewportPresentation) ||
            out_presentation->api_version != LUNA_EDITOR_VIEWPORT_API_VERSION) {
            return 0;
        }

        out_presentation->scene_texture = host->scene_texture_view;
        out_presentation->framebuffer_width = framebuffer_width;
        out_presentation->framebuffer_height = framebuffer_height;
        out_presentation->presentable = 1;
        return 1;
    }

    static int sceneTextureView(void* api_user_data, LunaEditorTextureView* out_texture)
    {
        TestNativeHost* host = self(api_user_data);
        if (host == nullptr || out_texture == nullptr) {
            return 0;
        }

        *out_texture = host->scene_texture_view;
        return 1;
    }

    static void editorCameraPosition(void* api_user_data, LunaEditorVec3* out_position)
    {
        if (TestNativeHost* host = self(api_user_data); host != nullptr && out_position != nullptr) {
            *out_position = host->editor_camera_position;
        }
    }

    static int gizmoOperationName(void* api_user_data, char* out_value, size_t out_value_size)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            copyToBuffer(out_value, out_value_size, host->gizmo_operation_name);
            return 1;
        }
        return 0;
    }

    static int gizmoModeName(void* api_user_data, char* out_value, size_t out_value_size)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            copyToBuffer(out_value, out_value_size, host->gizmo_mode_name);
            return 1;
        }
        return 0;
    }

    static int pickDebugVisualizationEnabled(void* api_user_data)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            return host->pick_debug_visualization_enabled ? 1 : 0;
        }
        return 0;
    }

    static void setPickDebugVisualizationEnabled(void* api_user_data, int enabled)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            host->pick_debug_visualization_enabled = enabled != 0;
        }
    }

    static int editorGridEnabled(void* api_user_data)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            return host->editor_grid_enabled ? 1 : 0;
        }
        return 0;
    }

    static void setEditorGridEnabled(void* api_user_data, int enabled)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            host->editor_grid_enabled = enabled != 0;
        }
    }

    static int isRuntimeViewportEnabled(void* api_user_data)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            return host->runtime_viewport_enabled ? 1 : 0;
        }
        return 0;
    }

    static int isRuntimeViewportRequested(void* api_user_data)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            return host->runtime_viewport_requested ? 1 : 0;
        }
        return 0;
    }

    static void setRuntimeViewportRequested(void* api_user_data, int enabled)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            host->runtime_viewport_requested = enabled != 0;
        }
    }

    static size_t runtimeEntityCount(void* api_user_data)
    {
        if (TestNativeHost* host = self(api_user_data)) {
            return host->runtime_entity_count_value;
        }
        return 0u;
    }
};

struct NativePluginLoadResult {
    NativeLoadStatus status{NativeLoadStatus::MissingLibrary};
    std::shared_ptr<luna::DynamicLibrary> library;
    std::unique_ptr<TestNativeHost> host;
    LunaEditorPluginApi plugin_api{};
    bool loaded{false};

    void unload()
    {
        if (!loaded || host == nullptr) {
            return;
        }

        if (plugin_api.on_unload != nullptr) {
            plugin_api.on_unload(plugin_api.plugin_user_data, &host->api);
        }
        host->unregisterContributionsForOwner(host->owner_id);
        loaded = false;
    }
};

NativePluginLoadResult loadNativePluginForTest(const std::filesystem::path& path, std::string_view expected_plugin_id)
{
    NativePluginLoadResult result{};
    result.host = std::make_unique<TestNativeHost>(std::string(expected_plugin_id));

    result.library = luna::DynamicLibrary::load(path);
    if (!result.library) {
        result.status = NativeLoadStatus::MissingLibrary;
        return result;
    }

    auto* create_plugin_fn =
        reinterpret_cast<LunaCreateEditorPluginFn>(result.library->findSymbol(LUNA_EDITOR_CREATE_PLUGIN_SYMBOL));
    if (create_plugin_fn == nullptr) {
        result.status = NativeLoadStatus::MissingSymbol;
        return result;
    }

    result.plugin_api = LunaEditorPluginApi{
        .struct_size = sizeof(LunaEditorPluginApi),
        .api_version = LUNA_EDITOR_PLUGIN_API_VERSION,
    };
    if (create_plugin_fn(LUNA_EDITOR_HOST_API_VERSION, &result.host->api, &result.plugin_api) == 0) {
        result.status = NativeLoadStatus::CreateFailed;
        return result;
    }
    if (result.plugin_api.struct_size != sizeof(LunaEditorPluginApi)) {
        result.status = NativeLoadStatus::PluginApiSizeMismatch;
        return result;
    }
    if (result.plugin_api.api_version != LUNA_EDITOR_PLUGIN_API_VERSION) {
        result.status = NativeLoadStatus::PluginApiVersionMismatch;
        return result;
    }
    if (result.plugin_api.plugin_id == nullptr || result.plugin_api.plugin_id[0] == '\0') {
        result.status = NativeLoadStatus::EmptyPluginId;
        return result;
    }
    if (expected_plugin_id != result.plugin_api.plugin_id) {
        result.status = NativeLoadStatus::PluginIdMismatch;
        return result;
    }
    if (result.plugin_api.on_load == nullptr || result.plugin_api.on_unload == nullptr) {
        result.status = NativeLoadStatus::MissingCallbacks;
        return result;
    }
    if (result.plugin_api.on_load(result.plugin_api.plugin_user_data, &result.host->api) == 0) {
        result.host->unregisterContributionsForOwner(expected_plugin_id);
        result.status = NativeLoadStatus::OnLoadFailed;
        return result;
    }

    result.status = NativeLoadStatus::Loaded;
    result.loaded = true;
    return result;
}

bool expectStatus(TestContext& context,
                  const NativePluginLoadResult& result,
                  NativeLoadStatus expected,
                  std::string_view label)
{
    if (result.status == expected) {
        return true;
    }

    std::ostringstream message;
    message << label << ": expected " << statusName(expected) << ", got " << statusName(result.status);
    return context.expect(false, message.str());
}

template <typename Container> bool contains(const Container& values, std::string_view value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

const luna::editor::EditorPluginPackage* findPackage(const std::vector<luna::editor::EditorPluginPackage>& packages,
                                                     std::string_view id)
{
    const auto it = std::find_if(packages.begin(), packages.end(), [&](const auto& package) {
        return package.id == id;
    });
    return it != packages.end() ? &*it : nullptr;
}

void testSuccessfulNativePluginLoad(TestContext& context)
{
    NativePluginLoadResult result =
        loadNativePluginForTest(testPluginBinaryPath("LunaTestEditorPluginGood"), kExpectedPluginId);
    if (!expectStatus(context, result, NativeLoadStatus::Loaded, "good native editor plugin")) {
        return;
    }

    context.expect(result.plugin_api.api_version == LUNA_EDITOR_PLUGIN_API_VERSION,
                   "good native editor plugin should use v1 plugin API");
    context.expect(result.host->commands.size() == 1, "good native editor plugin should register one command");
    context.expect(result.host->windows.size() == 1, "good native editor plugin should register one window");
    context.expect(result.host->menus.size() == 1, "good native editor plugin should register one menu item");
    context.expect(result.host->commands.contains(kCommandId), "registered native command id should match");
    context.expect(result.host->windows.contains(kWindowId), "registered native window id should match");
    if (!result.host->menus.empty()) {
        context.expect(result.host->menus.front().menu_path == kMenuPath, "registered native menu path should match");
        context.expect(result.host->menus.front().command_id == kCommandId,
                       "registered native menu command should match");
        context.expect(result.host->menus.front().owner_id == kExpectedPluginId,
                       "registered native menu should be owner-tagged");
    }
    context.expect(result.host->commands[kCommandId].owner_id == kExpectedPluginId,
                   "registered native command should be owner-tagged");
    context.expect(result.host->windows[kWindowId].owner_id == kExpectedPluginId,
                   "registered native window should be owner-tagged");
    context.expect(result.host->describe_asset_count > 0, "native plugin should use host asset API during load");
    context.expect(result.host->plugin_asset_read_text_count > 0,
                   "native plugin should use host plugin asset API during load");
    context.expect(result.host->project_info_count > 0, "native plugin should use host project API during load");
    context.expect(result.host->scene_info_count > 0, "native plugin should use host scene API during load");
    context.expect(result.host->api.viewport.sync_scene_viewport != nullptr,
                   "native plugin should receive default viewport sync API");
    context.expect(result.host->api.viewport.create_scene_viewport != nullptr,
                   "native plugin should receive scene viewport creation API");
    context.expect(result.host->api.viewport.sync_scene_viewport_ex != nullptr,
                   "native plugin should receive viewport-id sync API");
    context.expect(result.host->api.runtime_viewport.set_runtime_viewport_requested != nullptr,
                   "native plugin should receive runtime viewport API");
    context.expect(result.host->pick_debug_visualization_enabled,
                   "native plugin should be able to enable pick debug visualization");
    context.expect(!result.host->editor_grid_enabled, "native plugin should be able to disable editor grid");
    context.expect(result.host->runtime_viewport_requested,
                   "native plugin should be able to request runtime viewport");
    context.expect(result.host->runtime_entity_count_value == 17u,
                   "native plugin should read runtime entity count through host API");

    const auto root_entity = result.host->entities.find(1u);
    context.expect(root_entity != result.host->entities.end(), "root entity should remain available");
    if (root_entity != result.host->entities.end()) {
        context.expect(root_entity->second.has_camera, "native plugin should write a camera component");
        context.expect(root_entity->second.has_light, "native plugin should write a light component");
        context.expect(root_entity->second.has_mesh, "native plugin should write a mesh component");
        context.expect((root_entity->second.component_flags & LunaEditorSceneEntityComponentFlag_Camera) != 0,
                       "root entity should report camera component flag");
        context.expect((root_entity->second.component_flags & LunaEditorSceneEntityComponentFlag_Light) != 0,
                       "root entity should report light component flag");
        context.expect((root_entity->second.component_flags & LunaEditorSceneEntityComponentFlag_Mesh) != 0,
                       "root entity should report mesh component flag");
        context.expect(root_entity->second.camera.primary == 1, "camera component should preserve primary flag");
        context.expect(root_entity->second.light.enabled == 1, "light component should preserve enabled flag");
        context.expect(root_entity->second.mesh_handle == 777u, "mesh component should preserve mesh handle");
        context.expect(root_entity->second.mesh_submesh_material_handles.size() == 1u,
                       "mesh component should preserve submesh material count");
        if (!root_entity->second.mesh_submesh_material_handles.empty()) {
            context.expect(root_entity->second.mesh_submesh_material_handles.front() == 99u,
                           "mesh component should preserve submesh material handle");
        }
    }

    LunaEditorSceneCameraComponent camera{};
    camera.struct_size = sizeof(LunaEditorSceneCameraComponent);
    camera.api_version = LUNA_EDITOR_SCENE_CAMERA_COMPONENT_API_VERSION;
    context.expect(result.host->api.scene.get_camera_component(result.host->api.scene.api_user_data, 1u, &camera) == 1,
                   "native host should return camera component");
    context.expect(camera.primary == 1 && camera.perspective_vertical_fov_degrees == 60.0f,
                   "camera component should round-trip through host API");

    LunaEditorSceneLightComponent light{};
    light.struct_size = sizeof(LunaEditorSceneLightComponent);
    light.api_version = LUNA_EDITOR_SCENE_LIGHT_COMPONENT_API_VERSION;
    context.expect(result.host->api.scene.get_light_component(result.host->api.scene.api_user_data, 1u, &light) == 1,
                   "native host should return light component");
    context.expect(light.enabled == 1 && light.intensity == 3.0f,
                   "light component should round-trip through host API");

    uint64_t mesh_materials[4]{};
    LunaEditorSceneMeshComponent mesh{};
    mesh.struct_size = sizeof(LunaEditorSceneMeshComponent);
    mesh.api_version = LUNA_EDITOR_SCENE_MESH_COMPONENT_API_VERSION;
    mesh.submesh_material_handles = mesh_materials;
    mesh.submesh_material_capacity = 4u;
    context.expect(result.host->api.scene.get_mesh_component(result.host->api.scene.api_user_data, 1u, &mesh) == 1,
                   "native host should return mesh component");
    context.expect(mesh.mesh_handle == 777u && mesh.submesh_material_count == 1u,
                   "mesh component should round-trip through host API");
    context.expect(mesh_materials[0] == 99u, "mesh component should copy material handle through host API");

    context.expect(result.host->api.commands.can_execute_command(result.host->api.commands.api_user_data, kCommandId) ==
                       1,
                   "registered native command should be executable");
    context.expect(result.host->api.commands.is_command_checked(result.host->api.commands.api_user_data, kCommandId) ==
                       0,
                   "registered native command should start unchecked");
    context.expect(result.host->api.commands.execute_command(result.host->api.commands.api_user_data, kCommandId) == 1,
                   "registered native command should execute through host API");
    context.expect(result.host->api.windows.is_window_open(result.host->api.windows.api_user_data, kWindowId) == 1,
                   "native command should open registered window");
    context.expect(result.host->created_entity_count == 1,
                   "native command should create an entity through scene API");
    context.expect(result.host->selected_entity_id == 100u,
                   "native command should select created entity through selection API");
    context.expect(result.host->api.commands.is_command_checked(result.host->api.commands.api_user_data, kCommandId) ==
                       1,
                   "registered native command should reflect open window state");

    context.expect(result.host->drawWindow(kWindowId), "registered native window should draw through callback");
    context.expect(result.host->text_count > 0, "native window draw should use host UI text API");
    context.expect(result.host->button_count > 0, "native window draw should use host UI button API");

    result.host->api.windows.set_window_open(result.host->api.windows.api_user_data, kWindowId, 0);
    result.host->next_button_pressed = true;
    context.expect(result.host->drawWindow(kWindowId), "native window button draw should run");
    context.expect(result.host->api.windows.is_window_open(result.host->api.windows.api_user_data, kWindowId) == 1,
                   "native window button should execute registered command");
    context.expect(result.host->created_entity_count == 2,
                   "native UI button should execute command and create another entity");
    context.expect(result.host->selected_entity_id == 101u,
                   "native UI button command should select the latest created entity");
    context.expect(result.host->scene_viewport_owners.empty(),
                   "native plugin explicit viewport destroy should clear viewport owner records");

    result.unload();
    context.expect(result.host->commands.empty(), "native plugin unload should clean command contributions");
    context.expect(result.host->windows.empty(), "native plugin unload should clean window contributions");
    context.expect(result.host->menus.empty(), "native plugin unload should clean menu contributions");
    context.expect(result.host->scene_viewport_owners.empty(), "native plugin unload should clean viewport contributions");
}

void testNativePluginViewportOwnerCleanup(TestContext& context)
{
    NativePluginLoadResult result =
        loadNativePluginForTest(testPluginBinaryPath("LunaTestEditorPluginLeakViewport"), kExpectedPluginId);
    expectStatus(context, result, NativeLoadStatus::Loaded, "native editor plugin leaking viewport");
    if (result.status != NativeLoadStatus::Loaded || !result.host) {
        return;
    }

    context.expect(!result.host->scene_viewport_owners.empty(),
                   "leaky native plugin should leave a viewport contribution before unload");
    result.unload();
    context.expect(result.host->scene_viewport_owners.empty(),
                   "native plugin unload should clean leaked viewport contributions");
}

void testNativePluginLoadFailures(TestContext& context)
{
    {
        NativePluginLoadResult result =
            loadNativePluginForTest(testPluginBinaryPath("LunaTestEditorPluginMissingSymbol"), kExpectedPluginId);
        expectStatus(context, result, NativeLoadStatus::MissingSymbol, "native editor plugin missing symbol");
    }
    {
        NativePluginLoadResult result =
            loadNativePluginForTest(testPluginBinaryPath("LunaTestEditorPluginApiMismatch"), kExpectedPluginId);
        expectStatus(context, result, NativeLoadStatus::PluginApiVersionMismatch, "native editor plugin API mismatch");
    }
    {
        NativePluginLoadResult result =
            loadNativePluginForTest(testPluginBinaryPath("LunaTestEditorPluginIdMismatch"), kExpectedPluginId);
        expectStatus(context, result, NativeLoadStatus::PluginIdMismatch, "native editor plugin id mismatch");
    }
    {
        NativePluginLoadResult result =
            loadNativePluginForTest(testPluginBinaryPath("LunaTestEditorPluginMissingCallbacks"), kExpectedPluginId);
        expectStatus(context, result, NativeLoadStatus::MissingCallbacks, "native editor plugin missing callbacks");
    }
    {
        NativePluginLoadResult result =
            loadNativePluginForTest(testPluginBinaryPath("LunaTestEditorPluginLoadFailure"), kExpectedPluginId);
        expectStatus(context, result, NativeLoadStatus::OnLoadFailed, "native editor plugin on_load failure");
        context.expect(result.host->commands.empty(),
                       "failed native editor plugin load should clean registered command contributions");
        context.expect(result.host->windows.empty(),
                       "failed native editor plugin load should clean registered window contributions");
        context.expect(result.host->menus.empty(),
                       "failed native editor plugin load should clean registered menu contributions");
    }
}

void testManifestAndDependencyContract(TestContext& context)
{
    TempDirectory temp("NativeEditorPluginManifest");
    const std::filesystem::path plugins_root = temp.path() / "EditorPlugins";
    const std::filesystem::path good_entry = testPluginBinaryPath("LunaTestEditorPluginGood");

    writeEditorPluginManifest(plugins_root, "Good", "luna.test.native-good", "Native Good", good_entry, {});
    writeEditorPluginManifest(plugins_root,
                              "Dependent",
                              "luna.test.native-dependent",
                              "Native Dependent",
                              good_entry,
                              {"luna.test.native-good", "luna.test.native-missing"});

    luna::editor::EditorPluginManifestLoader loader;
    const std::vector<luna::editor::EditorPluginPackage> packages = loader.loadPackagesFromRoot(plugins_root);
    context.expect(packages.size() == 2, "native editor plugin manifests should be discovered");

    const luna::editor::EditorPluginPackage* good = findPackage(packages, "luna.test.native-good");
    const luna::editor::EditorPluginPackage* dependent = findPackage(packages, "luna.test.native-dependent");
    context.expect(good != nullptr, "native good manifest should load");
    context.expect(dependent != nullptr, "native dependent manifest should load");

    if (good != nullptr) {
        context.expect(good->runtime == luna::editor::EditorPluginRuntime::Native,
                       "native manifest should parse Runtime=Native");
        context.expect(good->entry_exists, "native manifest should record existing entry");
        context.expect(good->resolved_entry_path == good_entry.lexically_normal(),
                       "native manifest should resolve absolute entry path");
    }

    if (dependent != nullptr) {
        std::unordered_set<std::string> loaded_ids{"luna.test.native-good"};
        const std::vector<std::string> missing = luna::editor::missingEditorPluginDependencies(*dependent, loaded_ids);
        context.expect(missing.size() == 1 && contains(missing, "luna.test.native-missing"),
                       "dependency resolver should report missing native editor plugin dependency");
        context.expect(!luna::editor::areEditorPluginDependenciesLoaded(*dependent, loaded_ids),
                       "dependency resolver should reject incomplete dependency set");

        loaded_ids.insert("luna.test.native-missing");
        context.expect(luna::editor::areEditorPluginDependenciesLoaded(*dependent, loaded_ids),
                       "dependency resolver should accept complete dependency set");
    }
}

void testEditorEnginePathParsingContract(TestContext& context)
{
    const char* argv[] = {
        "LunaEditor",
        "--engine-data-root",
        "InstallRoot",
        "--editor-plugin-dir=DevA",
        "--editor-plugin-dir",
        "DevB",
    };
    luna::editor::EditorStartupOptions options =
        luna::editor::parseEditorStartupOptions(static_cast<int>(std::size(argv)), const_cast<char**>(argv));

    context.expect(options.engine_data_root_override == std::filesystem::path("InstallRoot"),
                   "engine data root command line option should parse");
    context.expect(options.editor_plugin_path_overrides.size() == 2u,
                   "editor plugin dir command line options should parse");
    if (options.editor_plugin_path_overrides.size() == 2u) {
        context.expect(options.editor_plugin_path_overrides[0] == std::filesystem::path("DevA"),
                       "editor plugin dir equals form should parse");
        context.expect(options.editor_plugin_path_overrides[1] == std::filesystem::path("DevB"),
                       "editor plugin dir separate value form should parse");
    }

    const std::string list_value =
        std::string("One") + luna::editor::editorPathListSeparator() + "Two" +
        luna::editor::editorPathListSeparator();
    const std::vector<std::filesystem::path> split_paths = luna::editor::splitEditorPathList(list_value);
    context.expect(split_paths.size() == 2u, "editor plugin path list should skip empty entries");
    if (split_paths.size() == 2u) {
        context.expect(split_paths[0] == std::filesystem::path("One"), "first path list entry should parse");
        context.expect(split_paths[1] == std::filesystem::path("Two"), "second path list entry should parse");
    }
}

void testEditorPluginPackageRootContract(TestContext& context)
{
    TempDirectory temp("EditorPluginRoots");
    const std::filesystem::path engine_root = temp.path() / "EngineData";
    const std::filesystem::path installed_root = engine_root / "Plugins" / "Editor" / "Installed";
    const std::filesystem::path dev_root = temp.path() / "DevPlugins";
    const std::filesystem::path good_entry = testPluginBinaryPath("LunaTestEditorPluginGood");

    writeEditorPluginManifest(installed_root,
                              "InstalledNative",
                              "luna.test.installed-native",
                              "Installed Native",
                              good_entry,
                              {});
    writeEditorPluginManifest(dev_root, "DevNative", "luna.test.dev-native", "Dev Native", good_entry, {});

    luna::editor::EditorEnginePaths paths{};
    paths.engine_data_root = engine_root;
    paths.official_editor_plugin_roots.push_back(engine_root / "Plugins" / "Editor" / "OfficialMissing");
    paths.installed_editor_plugin_roots.push_back(installed_root);
    paths.development_editor_plugin_roots.push_back(dev_root);
    paths.development_editor_plugin_roots.push_back(engine_root / "Plugins" / "Editor" / "MissingDev");

    const std::vector<luna::editor::EditorPluginPackage> packages = luna::editor::createEditorPluginPackages(paths);
    context.expect(findPackage(packages, "luna.test.installed-native") != nullptr,
                   "installed editor plugin root should contribute packages");
    context.expect(findPackage(packages, "luna.test.dev-native") != nullptr,
                   "development editor plugin root should contribute packages");
}

} // namespace

int main()
{
    luna::Logger::init("", luna::Logger::Level::Warn);

    TestContext context;
    testSuccessfulNativePluginLoad(context);
    testNativePluginViewportOwnerCleanup(context);
    testNativePluginLoadFailures(context);
    testManifestAndDependencyContract(context);
    testEditorEnginePathParsingContract(context);
    testEditorPluginPackageRootContract(context);

    luna::Logger::shutdown();
    return context.result();
}
