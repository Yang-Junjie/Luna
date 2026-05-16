#include "Core/Log.h"
#include "EditorApi/EditorAssetService.h"
#include "EditorApi/EditorCommandService.h"
#include "EditorApi/EditorHistoryService.h"
#include "EditorApi/EditorMenuService.h"
#include "EditorApi/EditorNativePluginApi.h"
#include "EditorApi/EditorPluginAssetService.h"
#include "EditorApi/EditorProjectService.h"
#include "EditorApi/EditorRenderingService.h"
#include "EditorApi/EditorRuntimeViewportService.h"
#include "EditorApi/EditorSceneService.h"
#include "EditorApi/EditorScriptPluginService.h"
#include "EditorApi/EditorScriptService.h"
#include "EditorApi/EditorSelectionService.h"
#include "EditorApi/EditorShortcutService.h"
#include "EditorApi/EditorUi.h"
#include "EditorApi/EditorViewportService.h"
#include "EditorApi/EditorWindowService.h"
#include "Platform/Common/DynamicLibrary.h"
#include "Shell/EditorPluginManifest.h"
#include "Shell/EditorPluginManager.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

constexpr const char* kExpectedPluginId = "luna.source.native-sample";
constexpr const char* kExpectedWindowId = "luna.source.native-sample.window";
constexpr const char* kOpenCommandId = "luna.source.native-sample.open";
constexpr const char* kCreateEntityCommandId = "luna.source.native-sample.create-entity";
constexpr const char* kCoreCommandsPluginId = "luna.editor.core-commands";

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
    int m_failures{};
};

class ManagerSmokeUi final : public luna::editor::Ui {
public:
    bool beginWindow(std::string_view, std::string_view, bool*, luna::editor::WindowFlags) override { return true; }
    void endWindow() override {}
    void text(std::string_view) override { ++text_count; }
    void textDisabled(std::string_view) override { ++text_count; }
    void textWrapped(std::string_view) override { ++text_count; }
    void bulletText(std::string_view) override { ++text_count; }
    void separator() override {}
    void separatorText(std::string_view) override {}
    void sameLine() override {}
    void spacing() override {}
    void indent(float = 0.0f) override {}
    void unindent(float = 0.0f) override {}
    void beginDisabled() override { ++disabled_depth; }
    void endDisabled() override { --disabled_depth; }
    void setNextItemWidth(float) override {}
    [[nodiscard]] luna::editor::Vec2 contentRegionAvail() const noexcept override { return {.x = 360.0f, .y = 220.0f}; }
    [[nodiscard]] luna::editor::Vec2 windowFramebufferScale() const noexcept override { return {.x = 1.0f, .y = 1.0f}; }

    bool button(std::string_view, luna::editor::Vec2 = {}, luna::editor::ButtonVariant = luna::editor::ButtonVariant::Default) override
    {
        ++button_count;
        return false;
    }
    bool checkbox(std::string_view, bool&) override
    {
        ++checkbox_count;
        return false;
    }
    bool colorEdit3(std::string_view, luna::editor::Vec3&) override
    {
        ++color_edit_count;
        return false;
    }
    bool sliderInt(std::string_view, int&, int, int) override { return false; }
    bool sliderFloat(std::string_view, float&, float, float, std::string_view = "%.3f") override
    {
        ++slider_count;
        return false;
    }
    bool dragFloat3(std::string_view, luna::editor::Vec3&, float, float, float, std::string_view = "%.3f") override
    {
        ++drag_float3_count;
        return false;
    }
    bool dragInt(std::string_view, int&, float, int, int) override { return false; }
    bool dragFloat(std::string_view, float&, float, float, float, std::string_view = "%.3f") override { return false; }
    bool inputText(std::string_view, std::string&, std::size_t = 256) override
    {
        ++input_text_count;
        return false;
    }
    bool inputTextWithHint(std::string_view, std::string_view, std::string&, std::size_t = 256) override
    {
        ++input_text_count;
        return false;
    }
    bool colorEdit4(std::string_view, luna::editor::Vec4&) override { return false; }
    bool treeNode(std::string_view) override { return false; }
    void treePop() override {}
    bool beginCombo(std::string_view, std::string_view) override { return false; }
    void endCombo() override {}
    bool selectable(std::string_view, bool = false) override { return false; }
    void setItemDefaultFocus() override {}
    bool image(const luna::editor::TextureView& texture, luna::editor::Vec2) override
    {
        if (texture.valid()) {
            ++image_count;
            return true;
        }
        return false;
    }
    [[nodiscard]] bool isItemHovered() const noexcept override { return true; }
    [[nodiscard]] bool isItemClicked(luna::editor::MouseButton = luna::editor::MouseButton::Left) const noexcept override
    {
        return false;
    }
    [[nodiscard]] bool isItemDoubleClicked(luna::editor::MouseButton = luna::editor::MouseButton::Left) const noexcept override
    {
        return false;
    }
    [[nodiscard]] bool isItemDeactivatedAfterEdit() const noexcept override { return false; }
    void setTooltip(std::string_view) override { ++tooltip_count; }
    bool invisibleButton(std::string_view, luna::editor::Vec2) override { return false; }
    bool treeNodeEx(std::string_view, std::string_view, luna::editor::TreeNodeFlags) override { return false; }
    bool beginSection(std::string_view, std::string_view, bool = true) override { return true; }
    void endSection() override {}
    bool beginMenu(std::string_view, bool = true) override { return false; }
    void endMenu() override {}
    bool menuItem(std::string_view, bool = false, bool = true) override { return false; }
    void openPopup(std::string_view) override {}
    bool beginPopup(std::string_view) override { return false; }
    bool beginPopupContextItem(std::string_view = {}, luna::editor::MouseButton = luna::editor::MouseButton::Right) override
    {
        return false;
    }
    void closeCurrentPopup() override {}
    void endPopup() override {}
    bool beginDragDropSource() override { return false; }
    bool setDragDropPayload(std::string_view, const void*, std::size_t) override { return false; }
    void endDragDropSource() override {}
    bool beginDragDropTarget() override { return false; }
    bool acceptDragDropPayload(std::string_view, void*, std::size_t) override { return false; }
    bool acceptAssetDragDropPayload(luna::editor::AssetDropPayload&,
                                    const luna::AssetType*,
                                    std::size_t) override
    {
        return false;
    }
    void endDragDropTarget() override {}
    [[nodiscard]] float scale(float value) const noexcept override { return value; }
    [[nodiscard]] luna::editor::Vec2 scaled(luna::editor::Vec2 value) const noexcept override { return value; }
    bool beginTable(std::string_view, int, luna::editor::TableFlags = static_cast<luna::editor::TableFlags>(luna::editor::TableFlag::None), luna::editor::Vec2 = {}) override
    {
        return true;
    }
    void endTable() override {}
    void tableSetupColumn(std::string_view, luna::editor::TableColumnFlags = static_cast<luna::editor::TableColumnFlags>(luna::editor::TableColumnFlag::None), float = 0.0f) override {}
    void tableHeadersRow() override {}
    void tableNextRow() override {}
    bool tableNextColumn() override { return true; }

    int text_count{};
    int button_count{};
    int checkbox_count{};
    int slider_count{};
    int drag_float3_count{};
    int color_edit_count{};
    int input_text_count{};
    int image_count{};
    int tooltip_count{};
    int disabled_depth{};
};

class ManagerSmokeCommandService final : public luna::editor::CommandService {
public:
    explicit ManagerSmokeCommandService(luna::editor::Host& host)
        : m_host(host)
    {}

    bool registerCommand(luna::editor::CommandDescriptor descriptor) override
    {
        if (descriptor.id.empty() || !descriptor.execute) {
            return false;
        }
        commands[descriptor.id] = std::move(descriptor);
        return true;
    }

    void unregisterCommand(std::string_view id) override
    {
        commands.erase(std::string(id));
    }

    bool execute(std::string_view id) override
    {
        return execute(id, std::nullopt);
    }

    bool execute(std::string_view id, luna::editor::CommandSubject subject) override
    {
        const auto it = commands.find(std::string(id));
        if (it == commands.end() || !it->second.execute) {
            return false;
        }
        if (it->second.can_execute && !it->second.can_execute(m_host)) {
            return false;
        }
        m_subjects[it->first] = std::move(subject);
        it->second.execute(m_host);
        return true;
    }

    luna::editor::CommandSubject takeSubject(std::string_view id) override
    {
        const auto it = m_subjects.find(std::string(id));
        if (it == m_subjects.end()) {
            return std::nullopt;
        }
        luna::editor::CommandSubject subject = std::move(it->second);
        m_subjects.erase(it);
        return subject;
    }

    bool canExecute(std::string_view id) const override
    {
        const auto it = commands.find(std::string(id));
        return it != commands.end() && (!it->second.can_execute || it->second.can_execute(m_host));
    }

    bool isChecked(std::string_view id) const override
    {
        const auto it = commands.find(std::string(id));
        return it != commands.end() && it->second.is_checked && it->second.is_checked(m_host);
    }

    void removeOwner(std::string_view owner_id)
    {
        for (auto it = commands.begin(); it != commands.end();) {
            if (it->second.owner_id == owner_id) {
                it = commands.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::unordered_map<std::string, luna::editor::CommandDescriptor> commands;

private:
    luna::editor::Host& m_host;
    std::unordered_map<std::string, luna::editor::CommandSubject> m_subjects;
};

class ManagerSmokeWindowService final : public luna::editor::WindowService {
public:
    bool registerWindow(luna::editor::WindowDescriptor descriptor) override
    {
        if (descriptor.id.empty() || !descriptor.draw) {
            return false;
        }
        open_state[descriptor.id] = descriptor.default_open;
        windows[descriptor.id] = std::move(descriptor);
        return true;
    }

    void unregisterWindow(std::string_view id) override
    {
        windows.erase(std::string(id));
        open_state.erase(std::string(id));
    }

    bool isWindowOpen(std::string_view id) const override
    {
        const auto it = open_state.find(std::string(id));
        return it != open_state.end() && it->second;
    }

    void setWindowOpen(std::string_view id, bool open) override
    {
        if (windows.contains(std::string(id))) {
            open_state[std::string(id)] = open;
        }
    }

    bool drawWindow(std::string_view id, luna::editor::Host& host, luna::editor::Ui& ui)
    {
        const auto it = windows.find(std::string(id));
        if (it == windows.end() || !it->second.draw) {
            return false;
        }
        luna::editor::WindowDrawContext context(host, ui);
        it->second.draw(context);
        return true;
    }

    void removeOwner(std::string_view owner_id)
    {
        for (auto it = windows.begin(); it != windows.end();) {
            if (it->second.owner_id == owner_id) {
                open_state.erase(it->first);
                it = windows.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::unordered_map<std::string, luna::editor::WindowDescriptor> windows;
    std::unordered_map<std::string, bool> open_state;
};

class ManagerSmokeMenuService final : public luna::editor::MenuService {
public:
    bool addMenuItem(luna::editor::MenuItemDescriptor descriptor) override
    {
        if (descriptor.menu_path.empty() || descriptor.command_id.empty()) {
            return false;
        }
        menus.push_back(std::move(descriptor));
        return true;
    }

    void removeMenuItem(std::string_view menu_path, std::string_view command_id) override
    {
        menus.erase(std::remove_if(menus.begin(),
                                   menus.end(),
                                   [&](const auto& item) {
                                       return item.menu_path == menu_path && item.command_id == command_id;
                                   }),
                    menus.end());
    }

    void removeMenuItemsForCommand(std::string_view command_id) override
    {
        menus.erase(std::remove_if(menus.begin(),
                                   menus.end(),
                                   [&](const auto& item) {
                                       return item.command_id == command_id;
                                   }),
                    menus.end());
    }

    void removeOwner(std::string_view owner_id)
    {
        menus.erase(std::remove_if(menus.begin(),
                                   menus.end(),
                                   [&](const auto& item) {
                                       return item.owner_id == owner_id;
                                   }),
                    menus.end());
    }

    std::vector<luna::editor::MenuItemDescriptor> menus;
};

class ManagerSmokeAssetService final : public luna::editor::AssetService {
public:
    ManagerSmokeAssetService()
    {
        asset.handle = luna::AssetHandle(42u);
        asset.type = luna::AssetType::Texture;
        asset.label = "Smoke Texture";
        asset.detail = "Manager smoke asset";
        asset.exists = true;
        asset.project_path = "Textures/smoke.png";
        asset.absolute_path = "F:/FakeProject/Assets/Textures/smoke.png";
    }

    luna::editor::AssetInfo describeAsset(luna::AssetHandle handle) const override
    {
        return handle == asset.handle ? asset : luna::editor::AssetInfo{};
    }

    std::optional<luna::editor::AssetInfo> assetInfo(luna::AssetHandle handle) const override
    {
        return handle == asset.handle ? std::optional<luna::editor::AssetInfo>(asset) : std::nullopt;
    }

    std::optional<luna::editor::AssetInfo> assetInfoByPath(const std::filesystem::path& path) const override
    {
        return path == asset.project_path ? std::optional<luna::editor::AssetInfo>(asset) : std::nullopt;
    }

    std::vector<luna::editor::AssetInfo> listAssets(luna::AssetType type_filter, bool include_builtin) const override
    {
        (void) include_builtin;
        if (type_filter == luna::AssetType::None || type_filter == asset.type) {
            return {asset};
        }
        return {};
    }

    std::vector<luna::editor::AssetInfo> builtinAssets(luna::AssetType) const override { return {}; }
    bool assetExists(luna::AssetHandle handle) const override { return handle == asset.handle; }
    bool assetPathExists(const std::filesystem::path& path) const override { return path == asset.project_path; }
    luna::AssetHandle findAssetHandleByPath(const std::filesystem::path& path) const override
    {
        return path == asset.project_path ? asset.handle : luna::AssetHandle(0u);
    }
    std::optional<std::filesystem::path> assetsRootPath() const override { return std::filesystem::path("F:/FakeProject/Assets"); }
    std::optional<std::filesystem::path> resolveProjectAssetPath(const std::filesystem::path& project_relative_path) const override
    {
        return std::filesystem::path("F:/FakeProject/Assets") / project_relative_path;
    }
    std::optional<std::filesystem::path> makeProjectRelativeAssetPath(const std::filesystem::path& path) const override
    {
        return path.filename();
    }
    luna::editor::AssetRefreshResult refreshAssets() override
    {
        ++revision;
        return {.success = true, .project_loaded = true, .revision = revision, .message = "Smoke refresh finished."};
    }
    uint64_t assetRevision() const noexcept override { return revision; }
    bool isAssetLoading(luna::AssetHandle) const override { return false; }
    bool acceptsAssetType(luna::AssetType type, const luna::AssetType* accepted_types, std::size_t accepted_type_count) const override
    {
        if (accepted_types == nullptr || accepted_type_count == 0u) {
            return true;
        }
        return std::find(accepted_types, accepted_types + accepted_type_count, type) !=
               accepted_types + accepted_type_count;
    }
    std::optional<std::size_t> meshSubmeshCount(luna::AssetHandle) const override { return 1u; }
    bool beginAssetDragDropSource(luna::AssetHandle, std::string_view = {}) override { return false; }

    luna::editor::AssetInfo asset;
    uint64_t revision{7u};
};

class ManagerSmokePluginAssetService final : public luna::editor::PluginAssetService {
public:
    void registerPlugin(std::string_view plugin_id, const std::filesystem::path& root_path)
    {
        roots[std::string(plugin_id)] = root_path;
    }

    void unregisterPlugin(std::string_view plugin_id)
    {
        roots.erase(std::string(plugin_id));
    }

    [[nodiscard]] std::optional<std::filesystem::path> pluginRootPath(std::string_view plugin_id) const override
    {
        const auto it = roots.find(std::string(plugin_id));
        return it != roots.end() ? std::optional<std::filesystem::path>(it->second) : std::nullopt;
    }

    [[nodiscard]] std::optional<std::filesystem::path> assetRootPath(std::string_view plugin_id) const override
    {
        const auto root = pluginRootPath(plugin_id);
        return root ? std::optional<std::filesystem::path>(*root / "assets") : std::nullopt;
    }

    [[nodiscard]] std::optional<std::filesystem::path>
    resolvePath(std::string_view plugin_id, const std::filesystem::path& relative_asset_path) const override
    {
        const auto root = assetRootPath(plugin_id);
        return root ? std::optional<std::filesystem::path>(*root / relative_asset_path) : std::nullopt;
    }

    [[nodiscard]] bool exists(std::string_view plugin_id, const std::filesystem::path& relative_asset_path) const override
    {
        return resolvePath(plugin_id, relative_asset_path).has_value();
    }

    [[nodiscard]] std::optional<std::string>
    readText(std::string_view plugin_id, const std::filesystem::path& relative_asset_path) const override
    {
        return exists(plugin_id, relative_asset_path) ? std::optional<std::string>("Smoke plugin asset text")
                                                      : std::nullopt;
    }

    [[nodiscard]] luna::editor::PluginAssetBytes
    readBytes(std::string_view plugin_id, const std::filesystem::path& relative_asset_path) const override
    {
        return exists(plugin_id, relative_asset_path) ? luna::editor::PluginAssetBytes{.data = {1u, 2u, 3u}}
                                                      : luna::editor::PluginAssetBytes{};
    }

    [[nodiscard]] luna::editor::TextureView texture(std::string_view,
                                                    const std::filesystem::path&) override
    {
        return {};
    }

    std::unordered_map<std::string, std::filesystem::path> roots;
};

class ManagerSmokeProjectService final : public luna::editor::ProjectService {
public:
    [[nodiscard]] bool hasProjectLoaded() const override { return true; }
    [[nodiscard]] std::optional<std::filesystem::path> projectRootPath() const override
    {
        return std::filesystem::path("F:/FakeProject");
    }
    [[nodiscard]] std::optional<luna::ProjectInfo> projectInfo() const override
    {
        luna::ProjectInfo info{};
        info.Name = "Fake Project";
        info.Version = "0.1.0";
        info.Author = "Luna";
        info.Description = "Manager smoke project";
        info.StartScene = "Scenes/main.lunascene";
        info.AssetsPath = "Assets";
        info.Scripting.SelectedPluginId = "luna.script.fake";
        info.Scripting.SelectedBackendName = "Fake";
        return info;
    }
    void setProjectInfo(const luna::ProjectInfo&) override {}
    bool saveProject() override { return true; }
};

class ManagerSmokeSceneService final : public luna::editor::SceneService {
public:
    ManagerSmokeSceneService()
    {
        luna::editor::SceneEntityDetails root{};
        root.id = luna::editor::EntityId(1u);
        root.name = "Root";
        root.components.transform = true;
        root.transform.scale = {.x = 1.0f, .y = 1.0f, .z = 1.0f};
        entities[root.id] = root;
    }

    std::string sceneLabel() const override { return "Smoke Scene"; }
    size_t entityCount() const override { return entities.size(); }
    bool canEditScene() const noexcept override { return can_edit; }
    bool openSceneFile(const std::filesystem::path&) override { return true; }
    std::vector<luna::editor::SceneEntityInfo> entityHierarchy() const override
    {
        std::vector<luna::editor::SceneEntityInfo> result;
        for (const auto& [id, details] : entities) {
            result.push_back({.id = id, .parent_id = details.parent_id, .name = details.name});
        }
        return result;
    }
    bool entityExists(luna::editor::EntityId entity_id) const noexcept override
    {
        return entities.contains(entity_id);
    }
    std::optional<luna::editor::SceneEntityDetails> entityDetails(luna::editor::EntityId entity_id) const override
    {
        const auto it = entities.find(entity_id);
        return it != entities.end() ? std::optional<luna::editor::SceneEntityDetails>(it->second) : std::nullopt;
    }
    bool isEntityDescendantOf(luna::editor::EntityId, luna::editor::EntityId) const override { return false; }
    luna::SceneEnvironmentSettings sceneEnvironmentSettings() const override { return {}; }
    luna::SceneShadowSettings sceneShadowSettings() const override { return {}; }
    bool setSceneEnvironmentSettings(const luna::SceneEnvironmentSettings&) override { return true; }
    bool setSceneShadowSettings(const luna::SceneShadowSettings&) override { return true; }

    luna::editor::EntityId createEntity(std::string name) override
    {
        const luna::editor::EntityId id(next_entity_id++);
        luna::editor::SceneEntityDetails details{};
        details.id = id;
        details.name = name.empty() ? "Entity" : std::move(name);
        details.components.transform = true;
        details.transform.scale = {.x = 1.0f, .y = 1.0f, .z = 1.0f};
        entities[id] = details;
        return id;
    }

    luna::editor::EntityId createEntity(const luna::editor::SceneEntityCreateRequest& request) override
    {
        return createEntity(request.name);
    }

    bool destroyEntity(luna::editor::EntityId entity_id) override
    {
        return entities.erase(entity_id) > 0u;
    }

    bool reparentEntity(luna::editor::EntityId entity_id,
                        luna::editor::EntityId new_parent_id,
                        bool = true) override
    {
        const auto it = entities.find(entity_id);
        if (it == entities.end()) {
            return false;
        }
        it->second.parent_id = new_parent_id;
        return true;
    }

    bool setEntityName(luna::editor::EntityId entity_id, std::string name) override
    {
        const auto it = entities.find(entity_id);
        if (it == entities.end()) {
            return false;
        }
        it->second.name = std::move(name);
        return true;
    }

    bool setEntityTransform(luna::editor::EntityId entity_id, const luna::editor::SceneTransform& transform) override
    {
        const auto it = entities.find(entity_id);
        if (it == entities.end()) {
            return false;
        }
        it->second.transform = transform;
        it->second.components.transform = true;
        return true;
    }

    bool setCameraComponent(luna::editor::EntityId entity_id,
                            const luna::editor::SceneCameraComponent& camera_component) override
    {
        const auto it = entities.find(entity_id);
        if (it == entities.end()) {
            return false;
        }
        it->second.camera = camera_component;
        it->second.components.camera = true;
        return true;
    }

    bool setLightComponent(luna::editor::EntityId entity_id,
                           const luna::editor::SceneLightComponent& light_component) override
    {
        const auto it = entities.find(entity_id);
        if (it == entities.end()) {
            return false;
        }
        it->second.light = light_component;
        it->second.components.light = true;
        return true;
    }

    bool setMeshComponent(luna::editor::EntityId entity_id,
                          const luna::editor::SceneMeshComponent& mesh_component) override
    {
        const auto it = entities.find(entity_id);
        if (it == entities.end()) {
            return false;
        }
        it->second.mesh = mesh_component;
        it->second.components.mesh = true;
        return true;
    }

    bool setScriptComponent(luna::editor::EntityId entity_id,
                            const luna::editor::SceneScriptComponent& script_component) override
    {
        const auto it = entities.find(entity_id);
        if (it == entities.end()) {
            return false;
        }
        it->second.script = script_component;
        it->second.components.script = true;
        return true;
    }

    bool setScriptProperty(luna::editor::EntityId, std::size_t, std::size_t, const luna::editor::SceneScriptProperty&) override
    {
        return false;
    }

    bool addComponent(luna::editor::EntityId entity_id, luna::editor::SceneComponentKind component_kind) override
    {
        const auto it = entities.find(entity_id);
        if (it == entities.end()) {
            return false;
        }
        if (component_kind == luna::editor::SceneComponentKind::Camera) {
            it->second.camera = luna::editor::SceneCameraComponent{};
            it->second.components.camera = true;
        } else if (component_kind == luna::editor::SceneComponentKind::Light) {
            it->second.light = luna::editor::SceneLightComponent{};
            it->second.components.light = true;
        } else if (component_kind == luna::editor::SceneComponentKind::Mesh) {
            it->second.mesh = luna::editor::SceneMeshComponent{};
            it->second.components.mesh = true;
        }
        return true;
    }

    bool removeComponent(luna::editor::EntityId entity_id, luna::editor::SceneComponentKind component_kind) override
    {
        const auto it = entities.find(entity_id);
        if (it == entities.end()) {
            return false;
        }
        if (component_kind == luna::editor::SceneComponentKind::Camera) {
            it->second.camera.reset();
            it->second.components.camera = false;
        } else if (component_kind == luna::editor::SceneComponentKind::Light) {
            it->second.light.reset();
            it->second.components.light = false;
        } else if (component_kind == luna::editor::SceneComponentKind::Mesh) {
            it->second.mesh.reset();
            it->second.components.mesh = false;
        }
        return true;
    }

    bool applyMeshAssetToEntity(luna::editor::EntityId entity_id, luna::AssetHandle mesh_handle) override
    {
        luna::editor::SceneMeshComponent mesh{};
        mesh.mesh_handle = mesh_handle;
        return setMeshComponent(entity_id, mesh);
    }

    std::unordered_map<luna::editor::EntityId, luna::editor::SceneEntityDetails> entities;
    uint64_t next_entity_id{100u};
    bool can_edit{true};
};

class ManagerSmokeSelectionService final : public luna::editor::SelectionService {
public:
    luna::editor::EntityId selectedEntityId() const noexcept override { return selected; }
    void selectEntity(luna::editor::EntityId entity_id) override { selected = entity_id; }
    void clearSelection() override { selected = luna::editor::EntityId(0u); }

    luna::editor::EntityId selected{0u};
};

class ManagerSmokeViewportService final : public luna::editor::ViewportService {
public:
    luna::editor::ViewportId defaultSceneViewport() const noexcept override { return luna::editor::kDefaultViewportId; }
    luna::editor::ViewportId createSceneViewport(std::string_view debug_name = {}) override
    {
        return createSceneViewportForOwner(debug_name, {});
    }
    void destroySceneViewport(luna::editor::ViewportId viewport_id) override
    {
        viewports.erase(viewport_id);
    }
    bool isSceneViewportValid(luna::editor::ViewportId viewport_id) const noexcept override
    {
        return viewport_id == luna::editor::kDefaultViewportId || viewports.contains(viewport_id);
    }
    luna::editor::ViewportPresentation syncSceneViewport(luna::editor::ViewportId viewport_id,
                                                         luna::editor::UVec2 framebuffer_size) override
    {
        if (!isSceneViewportValid(viewport_id)) {
            return {};
        }
        return {
            .scene_texture = luna::editor::TextureView{
                .id = static_cast<luna::editor::TextureHandle>(0x500u + viewport_id),
                .size = framebuffer_size,
                .y_flip = false,
            },
            .framebuffer_size = framebuffer_size,
            .presentable = true,
        };
    }
    luna::editor::TextureView sceneTextureView(luna::editor::ViewportId viewport_id) const override
    {
        if (!isSceneViewportValid(viewport_id)) {
            return {};
        }
        return {
            .id = static_cast<luna::editor::TextureHandle>(0x500u + viewport_id),
            .size = {.x = 320u, .y = 180u},
            .y_flip = false,
        };
    }
    luna::editor::ViewportPresentation syncSceneViewport(luna::editor::UVec2 framebuffer_size) override
    {
        return syncSceneViewport(luna::editor::kDefaultViewportId, framebuffer_size);
    }
    luna::editor::TextureView sceneTextureView() const override
    {
        return sceneTextureView(luna::editor::kDefaultViewportId);
    }
    void drawDefaultSceneViewport(luna::editor::Ui&) override {}
    luna::editor::SceneViewportDrawResult drawSceneViewport(luna::editor::Ui&,
                                                            luna::editor::ViewportId viewport_id,
                                                            luna::editor::SceneViewportDrawOptions = {}) override
    {
        luna::editor::SceneViewportDrawResult result{};
        result.presentation = syncSceneViewport(viewport_id, {.x = 320u, .y = 180u});
        result.drawn = result.presentation.presentable;
        return result;
    }
    luna::editor::ViewportId createTextureViewport(std::string_view = {}) override
    {
        const luna::editor::ViewportId id = next_viewport_id++;
        texture_viewports[id] = {};
        return id;
    }
    void destroyTextureViewport(luna::editor::ViewportId viewport_id) override
    {
        texture_viewports.erase(viewport_id);
    }
    bool isTextureViewportValid(luna::editor::ViewportId viewport_id) const noexcept override
    {
        return texture_viewports.contains(viewport_id);
    }
    luna::editor::TextureViewportPresentation syncTextureViewport(luna::editor::ViewportId viewport_id,
                                                                  luna::editor::TextureView texture,
                                                                  luna::editor::UVec2 framebuffer_size) override
    {
        if (!isTextureViewportValid(viewport_id)) {
            return {};
        }
        luna::editor::TextureViewportPresentation presentation{
            .texture = texture,
            .framebuffer_size = framebuffer_size,
            .presentable = texture.valid(),
        };
        texture_viewports[viewport_id] = presentation;
        return presentation;
    }
    luna::editor::TextureViewportPresentation textureViewportPresentation(luna::editor::ViewportId viewport_id) const override
    {
        const auto it = texture_viewports.find(viewport_id);
        return it != texture_viewports.end() ? it->second : luna::editor::TextureViewportPresentation{};
    }
    luna::editor::TextureViewportDrawResult drawTextureViewport(luna::editor::Ui&,
                                                                luna::editor::ViewportId viewport_id,
                                                                luna::editor::TextureView texture,
                                                                luna::editor::TextureViewportDrawOptions = {}) override
    {
        luna::editor::TextureViewportDrawResult result{};
        result.presentation = syncTextureViewport(viewport_id, texture, texture.size);
        result.drawn = result.presentation.presentable;
        return result;
    }
    luna::editor::Vec3 editorCameraPosition() const noexcept override { return {.x = 1.0f, .y = 2.0f, .z = 3.0f}; }
    std::string gizmoOperationName() const override { return "Translate"; }
    std::string gizmoModeName() const override { return "Local"; }
    bool pickDebugVisualizationEnabled() const noexcept override { return pick_debug; }
    void setPickDebugVisualizationEnabled(bool enabled) override { pick_debug = enabled; }
    bool editorGridEnabled() const noexcept override { return grid_enabled; }
    void setEditorGridEnabled(bool enabled) override { grid_enabled = enabled; }

    luna::editor::ViewportId createSceneViewportForOwner(std::string_view, std::string_view owner_id)
    {
        const luna::editor::ViewportId id = next_viewport_id++;
        viewports[id] = std::string(owner_id);
        return id;
    }

    void destroyViewportsForOwner(std::string_view owner_id)
    {
        for (auto it = viewports.begin(); it != viewports.end();) {
            if (it->second == owner_id) {
                it = viewports.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::unordered_map<luna::editor::ViewportId, std::string> viewports;
    std::unordered_map<luna::editor::ViewportId, luna::editor::TextureViewportPresentation> texture_viewports;
    luna::editor::ViewportId next_viewport_id{2u};
    bool pick_debug{};
    bool grid_enabled{true};
};

class ManagerSmokeRuntimeViewportService final : public luna::editor::RuntimeViewportService {
public:
    bool isRuntimeViewportEnabled() const noexcept override { return false; }
    bool isRuntimeViewportRequested() const noexcept override { return requested; }
    void setRuntimeViewportRequested(bool enabled) override { requested = enabled; }
    size_t runtimeEntityCount() const noexcept override { return 17u; }

    bool requested{};
};

class ManagerSmokeHistoryService final : public luna::editor::HistoryService {
public:
    bool canUndo() const noexcept override { return false; }
    bool canRedo() const noexcept override { return false; }
    bool undo() override { return false; }
    bool redo() override { return false; }
};

class ManagerSmokePluginService final : public luna::editor::PluginService {
public:
    std::vector<luna::editor::PluginInfo> plugins() const override { return {}; }
};

class ManagerSmokeShortcutService final : public luna::editor::ShortcutService {
public:
    bool registerShortcut(luna::editor::ShortcutDescriptor) override { return true; }
    void unregisterShortcut(std::string_view) override {}
    std::string shortcutText(std::string_view) const override { return {}; }
    std::string commandShortcutText(std::string_view) const override { return {}; }
};

class ManagerSmokeRenderingService final : public luna::editor::RenderingService {
public:
    std::string backendName() const override { return "Smoke"; }
    luna::editor::RenderingBackendCapabilities backendCapabilities() const override { return {}; }
    luna::editor::RenderGraphProfileSnapshot renderGraphProfile() const override { return {}; }
    bool isRenderGraphProfilingEnabled() const noexcept override { return false; }
    void setRenderGraphProfilingEnabled(bool) override {}
    std::filesystem::path defaultRenderProfileExportPath(std::string_view = {}) const override { return {}; }
    bool exportRenderGraphProfileChromeTraceJson(const luna::editor::RenderGraphProfileSnapshot&,
                                                 const std::filesystem::path&,
                                                 std::string* = nullptr) const override
    {
        return false;
    }
    std::vector<luna::editor::RenderFeatureInfo> defaultRenderFeatureInfos() const override { return {}; }
    std::vector<luna::editor::RenderFeatureParameterInfo>
    defaultRenderFeatureParameters(std::string_view) const override
    {
        return {};
    }
    bool setDefaultRenderFeatureEnabled(std::string_view, bool) override { return false; }
    bool setDefaultRenderFeatureParameter(std::string_view,
                                          std::string_view,
                                          const luna::editor::RenderFeatureParameterValue&) override
    {
        return false;
    }
    std::vector<luna::editor::RenderDebugViewModeInfo> renderDebugViewModes() const override { return {}; }
    luna::editor::RenderDebugViewMode renderDebugViewMode() const noexcept override
    {
        return luna::editor::RenderDebugViewMode::None;
    }
    void setRenderDebugViewMode(luna::editor::RenderDebugViewMode) override {}
    float renderDebugVelocityScale() const noexcept override { return 1.0f; }
    void setRenderDebugVelocityScale(float) override {}
    luna::editor::TextureView renderDebugTextureView() const override { return {}; }
    float frameTimeMilliseconds() const noexcept override { return 16.0f; }
    float framesPerSecond() const noexcept override { return 60.0f; }
    luna::editor::UVec2 sceneOutputSize() const noexcept override { return {.x = 1280u, .y = 720u}; }
};

class ManagerSmokeScriptPluginService final : public luna::editor::ScriptPluginService {
public:
    void refreshProjectScriptPlugins() override {}
    [[nodiscard]] const std::vector<luna::ScriptPluginCandidate>& getDiscoveredScriptPlugins() const override
    {
        return candidates;
    }
    [[nodiscard]] const std::string& getScriptPluginStatus() const override { return status; }
    [[nodiscard]] const luna::ScriptPluginCandidate* getSelectedScriptPluginCandidate() const override { return nullptr; }
    bool selectScriptPlugin(const luna::ScriptPluginCandidate*) override { return false; }

    std::vector<luna::ScriptPluginCandidate> candidates;
    std::string status;
};

class ManagerSmokeScriptService final : public luna::editor::ScriptService {
public:
    [[nodiscard]] luna::editor::ScriptLanguageStatus projectScriptLanguage() const override { return {}; }
    [[nodiscard]] luna::editor::ScriptAssetValidation validateScriptAsset(luna::AssetHandle) const override { return {}; }
    [[nodiscard]] luna::editor::ScriptSchemaSyncResult
    syncScriptProperties(const luna::editor::SceneScriptEntry&) const override
    {
        return {};
    }
};

class ManagerSmokeBuiltinPlugin final : public luna::editor::Plugin {
public:
    explicit ManagerSmokeBuiltinPlugin(std::string plugin_id)
        : m_id(std::move(plugin_id))
    {}

    luna::editor::PluginDescriptor descriptor() const override
    {
        return {.id = m_id, .display_name = m_id, .version = "0.1.0"};
    }

    bool onLoad(luna::editor::Host&) override { return true; }

private:
    std::string m_id;
};

class ManagerSmokeHost final : public luna::editor::EditorPluginManagerHost {
public:
    ManagerSmokeHost()
        : command_service(*this)
    {}

    luna::editor::Ui& ui() override { return ui_service; }
    luna::editor::AssetService& assets() override { return asset_service; }
    luna::editor::WindowService& windows() override { return window_service; }
    luna::editor::CommandService& commands() override { return command_service; }
    luna::editor::HistoryService& history() override { return history_service; }
    luna::editor::MenuService& menus() override { return menu_service; }
    luna::editor::PluginAssetService& pluginAssets() override { return plugin_asset_service; }
    luna::editor::PluginService& plugins() override { return plugin_service; }
    luna::editor::ProjectService& project() override { return project_service; }
    luna::editor::ScriptPluginService& scriptPlugins() override { return script_plugin_service; }
    luna::editor::ScriptService& scripts() override { return script_service; }
    luna::editor::RenderingService& rendering() override { return rendering_service; }
    luna::editor::SceneService& scene() override { return scene_service; }
    luna::editor::SelectionService& selection() override { return selection_service; }
    luna::editor::ShortcutService& shortcuts() override { return shortcut_service; }
    luna::editor::RuntimeViewportService& runtimeViewport() override { return runtime_viewport_service; }
    luna::editor::ViewportService& viewport() override { return viewport_service; }

    bool loadPlugin(std::unique_ptr<luna::editor::Plugin> plugin,
                    const std::filesystem::path& root_path = {}) override
    {
        if (!plugin) {
            return false;
        }
        luna::editor::PluginDescriptor descriptor = plugin->descriptor();
        if (!root_path.empty()) {
            descriptor.root_path = root_path;
        }
        plugin_asset_service.registerPlugin(descriptor.id, descriptor.root_path);
        if (!plugin->onLoad(*this)) {
            cleanupPluginContributions(descriptor.id);
            return false;
        }
        builtin_plugins.push_back(std::move(plugin));
        return true;
    }

    void unloadPlugins() override
    {
        for (auto it = builtin_plugins.rbegin(); it != builtin_plugins.rend(); ++it) {
            if (*it) {
                const luna::editor::PluginDescriptor descriptor = (*it)->descriptor();
                (*it)->onUnload(*this);
                cleanupPluginContributions(descriptor.id);
            }
        }
        builtin_plugins.clear();
    }

    void registerPluginAssetRoot(std::string_view plugin_id, const std::filesystem::path& root_path) override
    {
        plugin_asset_service.registerPlugin(plugin_id, root_path);
    }

    void cleanupPluginContributions(std::string_view owner_id) override
    {
        viewport_service.destroyViewportsForOwner(owner_id);
        menu_service.removeOwner(owner_id);
        command_service.removeOwner(owner_id);
        window_service.removeOwner(owner_id);
        plugin_asset_service.unregisterPlugin(owner_id);
    }

    luna::editor::ViewportId createSceneViewportForPlugin(std::string_view owner_id,
                                                          std::string_view debug_name) override
    {
        return viewport_service.createSceneViewportForOwner(debug_name, owner_id);
    }

    bool drawWindow(std::string_view id)
    {
        return window_service.drawWindow(id, *this, ui_service);
    }

    ManagerSmokeUi ui_service;
    ManagerSmokeAssetService asset_service;
    ManagerSmokeWindowService window_service;
    ManagerSmokeCommandService command_service;
    ManagerSmokeHistoryService history_service;
    ManagerSmokeMenuService menu_service;
    ManagerSmokePluginAssetService plugin_asset_service;
    ManagerSmokePluginService plugin_service;
    ManagerSmokeProjectService project_service;
    ManagerSmokeScriptPluginService script_plugin_service;
    ManagerSmokeScriptService script_service;
    ManagerSmokeRenderingService rendering_service;
    ManagerSmokeSceneService scene_service;
    ManagerSmokeSelectionService selection_service;
    ManagerSmokeShortcutService shortcut_service;
    ManagerSmokeRuntimeViewportService runtime_viewport_service;
    ManagerSmokeViewportService viewport_service;
    std::vector<std::unique_ptr<luna::editor::Plugin>> builtin_plugins;
};

struct NativeSampleHost {
    struct CommandRecord {
        std::string id;
        void* user_data{};
        int (*can_execute)(void*, const LunaEditorHostApi*){};
        int (*is_checked)(void*, const LunaEditorHostApi*){};
        void (*execute)(void*, const LunaEditorHostApi*){};
    };

    struct WindowRecord {
        std::string id;
        bool open{};
        void* user_data{};
        void (*draw)(void*, const LunaEditorHostApi*){};
    };

    struct MenuRecord {
        std::string menu_path;
        std::string command_id;
    };

    struct EntityRecord {
        uint64_t id{};
        std::string name;
        LunaEditorSceneTransform transform{
            .translation = LunaEditorVec3{.x = 1.0f, .y = 2.0f, .z = 3.0f},
            .rotation_degrees = LunaEditorVec3{.x = 0.0f, .y = 15.0f, .z = 0.0f},
            .scale = LunaEditorVec3{.x = 1.0f, .y = 1.0f, .z = 1.0f},
        };
        bool has_camera{};
        bool has_light{};
        bool has_mesh{};
        LunaEditorSceneCameraComponent camera{};
        LunaEditorSceneLightComponent light{};
        LunaEditorSceneMeshComponent mesh{};
    };

    NativeSampleHost()
    {
        entities.emplace(1u, EntityRecord{.id = 1u, .name = "Root"});
        selected_entity_id = 1u;

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
            .text_disabled = &text,
            .text_wrapped = &text,
            .separator = &separator,
            .separator_text = &separatorText,
            .begin_disabled = &beginDisabled,
            .end_disabled = &endDisabled,
            .content_region_avail = &contentRegionAvail,
            .button = &button,
            .checkbox = &checkbox,
            .color_edit3 = &colorEdit3,
            .slider_float = &sliderFloat,
            .drag_float3 = &dragFloat3,
            .input_text = &inputText,
            .input_text_with_hint = &inputTextWithHint,
            .image = &image,
            .is_item_hovered = &isItemHovered,
            .is_item_deactivated_after_edit = &isItemDeactivatedAfterEdit,
            .set_tooltip = &setTooltip,
            .begin_table = &beginTable,
            .end_table = &endTable,
            .table_setup_column = &tableSetupColumn,
            .table_headers_row = &tableHeadersRow,
            .table_next_row = &tableNextRow,
            .table_next_column = &tableNextColumn,
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
            .asset_info_by_path = &assetInfoByPath,
            .list_assets = &listAssets,
            .asset_path_exists = &assetPathExists,
            .find_asset_handle_by_path = &findAssetHandleByPath,
            .assets_root_path = &assetsRootPath,
            .resolve_project_asset_path = &resolveProjectAssetPath,
            .make_project_relative_asset_path = &makeProjectRelativeAssetPath,
            .refresh_assets = &refreshAssets,
            .asset_revision = &assetRevision,
            .is_asset_loading = &isAssetLoading,
        };
        api.plugin_assets = LunaEditorPluginAssetApi{
            .struct_size = sizeof(LunaEditorPluginAssetApi),
            .api_version = LUNA_EDITOR_PLUGIN_ASSET_API_VERSION,
            .api_user_data = this,
            .plugin_root_path = &pluginRootPath,
            .asset_root_path = &pluginAssetRootPath,
            .read_text = &pluginAssetReadText,
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
            .can_edit_scene = &canEditScene,
            .entity_exists = &entityExists,
            .entity_info = &entityInfo,
            .create_entity = &createEntity,
            .create_entity_ex = &createEntityEx,
            .destroy_entity = &destroyEntity,
            .set_entity_name = &setEntityName,
            .get_entity_transform = &getEntityTransform,
            .set_entity_transform = &setEntityTransform,
            .get_camera_component = &getCameraComponent,
            .set_camera_component = &setCameraComponent,
            .get_light_component = &getLightComponent,
            .set_light_component = &setLightComponent,
            .get_mesh_component = &getMeshComponent,
            .add_component = &addComponent,
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

    bool drawWindow(std::string_view id)
    {
        const auto it = windows.find(std::string(id));
        if (it == windows.end() || it->second.draw == nullptr) {
            return false;
        }
        it->second.draw(it->second.user_data, &api);
        return true;
    }

    LunaEditorHostApi api{};
    std::unordered_map<std::string, CommandRecord> commands;
    std::unordered_map<std::string, WindowRecord> windows;
    std::vector<MenuRecord> menus;
    std::unordered_map<uint64_t, EntityRecord> entities;
    std::vector<std::string> logs;
    uint64_t selected_entity_id{};
    uint64_t next_entity_id{100u};
    uint64_t next_viewport_id{2u};
    std::vector<uint64_t> live_viewports;
    bool editor_grid_enabled{true};
    bool pick_debug_enabled{};
    bool runtime_viewport_requested{};
    bool can_edit_scene{true};
    int text_count{};
    int disabled_depth{};
    int begin_disabled_count{};
    int button_count{};
    int checkbox_count{};
    int slider_count{};
    int drag_float3_count{};
    int color_edit3_count{};
    int input_text_count{};
    int plugin_asset_read_text_count{};
    int project_info_count{};
    int list_assets_count{};
    int scene_label_count{};
    int entity_info_count{};
    int transform_get_count{};
    int viewport_sync_count{};
    int image_count{};
    int tooltip_count{};

private:
    static NativeSampleHost* self(void* api_user_data)
    {
        return static_cast<NativeSampleHost*>(api_user_data);
    }

    static void copyToBuffer(char* out_value, size_t out_value_size, std::string_view value)
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

    static std::string copyString(const char* value)
    {
        return value != nullptr ? std::string(value) : std::string{};
    }

    static void log(void* api_user_data, LunaEditorLogLevel, const char* message)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            host->logs.push_back(copyString(message));
        }
    }

    static void text(void* api_user_data, const char*)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            ++host->text_count;
        }
    }

    static void separator(void*) {}
    static void separatorText(void*, const char*) {}

    static void beginDisabled(void* api_user_data)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            ++host->disabled_depth;
            ++host->begin_disabled_count;
        }
    }

    static void endDisabled(void* api_user_data)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            --host->disabled_depth;
        }
    }

    static void contentRegionAvail(void*, LunaEditorVec2* out_value)
    {
        if (out_value != nullptr) {
            *out_value = LunaEditorVec2{.x = 360.0f, .y = 220.0f};
        }
    }

    static int button(void* api_user_data, const char*, const LunaEditorVec2*, uint32_t)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            ++host->button_count;
        }
        return 0;
    }

    static int checkbox(void* api_user_data, const char*, int*)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            ++host->checkbox_count;
        }
        return 0;
    }

    static int sliderFloat(void* api_user_data, const char*, float*, float, float, const char*)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            ++host->slider_count;
        }
        return 0;
    }

    static int dragFloat3(void* api_user_data, const char*, LunaEditorVec3*, float, float, float, const char*)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            ++host->drag_float3_count;
        }
        return 0;
    }

    static int colorEdit3(void* api_user_data, const char*, LunaEditorVec3*)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            ++host->color_edit3_count;
        }
        return 0;
    }

    static int inputText(void* api_user_data, const char*, char*, size_t)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            ++host->input_text_count;
        }
        return 0;
    }

    static int inputTextWithHint(void* api_user_data, const char*, const char*, char*, size_t)
    {
        return inputText(api_user_data, nullptr, nullptr, 0u);
    }

    static int image(void* api_user_data, const LunaEditorTextureView* texture, const LunaEditorVec2*)
    {
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr || texture == nullptr || texture->texture_id == 0u) {
            return 0;
        }
        ++host->image_count;
        return 1;
    }

    static int isItemHovered(void*)
    {
        return 1;
    }

    static int isItemDeactivatedAfterEdit(void*)
    {
        return 0;
    }

    static void setTooltip(void* api_user_data, const char*)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            ++host->tooltip_count;
        }
    }

    static int beginTable(void*, const char*, int, uint32_t, const LunaEditorVec2*)
    {
        return 1;
    }

    static void endTable(void*) {}
    static void tableSetupColumn(void*, const char*, uint32_t, float) {}
    static void tableHeadersRow(void*) {}
    static void tableNextRow(void*) {}
    static int tableNextColumn(void*) { return 1; }

    static int registerCommand(void* api_user_data, const LunaEditorCommandDescriptor* descriptor)
    {
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr || descriptor == nullptr || descriptor->id == nullptr || descriptor->execute == nullptr) {
            return 0;
        }
        host->commands[descriptor->id] = CommandRecord{
            .id = descriptor->id,
            .user_data = descriptor->command_user_data,
            .can_execute = descriptor->can_execute,
            .is_checked = descriptor->is_checked,
            .execute = descriptor->execute,
        };
        return 1;
    }

    static void unregisterCommand(void* api_user_data, const char* id)
    {
        if (NativeSampleHost* host = self(api_user_data); host != nullptr && id != nullptr) {
            host->commands.erase(id);
        }
    }

    static int executeCommand(void* api_user_data, const char* id)
    {
        NativeSampleHost* host = self(api_user_data);
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
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr || id == nullptr) {
            return 0;
        }
        const auto it = host->commands.find(id);
        if (it == host->commands.end()) {
            return 0;
        }
        return it->second.can_execute == nullptr || it->second.can_execute(it->second.user_data, &host->api) != 0 ? 1 : 0;
    }

    static int isCommandChecked(void* api_user_data, const char* id)
    {
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr || id == nullptr) {
            return 0;
        }
        const auto it = host->commands.find(id);
        if (it == host->commands.end() || it->second.is_checked == nullptr) {
            return 0;
        }
        return it->second.is_checked(it->second.user_data, &host->api);
    }

    static int registerWindow(void* api_user_data, const LunaEditorWindowDescriptor* descriptor)
    {
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr || descriptor == nullptr || descriptor->id == nullptr || descriptor->draw == nullptr) {
            return 0;
        }
        host->windows[descriptor->id] = WindowRecord{
            .id = descriptor->id,
            .open = descriptor->default_open != 0,
            .user_data = descriptor->window_user_data,
            .draw = descriptor->draw,
        };
        return 1;
    }

    static void unregisterWindow(void* api_user_data, const char* id)
    {
        if (NativeSampleHost* host = self(api_user_data); host != nullptr && id != nullptr) {
            host->windows.erase(id);
        }
    }

    static int isWindowOpen(void* api_user_data, const char* id)
    {
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr || id == nullptr) {
            return 0;
        }
        const auto it = host->windows.find(id);
        return it != host->windows.end() && it->second.open ? 1 : 0;
    }

    static void setWindowOpen(void* api_user_data, const char* id, int open)
    {
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr || id == nullptr) {
            return;
        }
        if (auto it = host->windows.find(id); it != host->windows.end()) {
            it->second.open = open != 0;
        }
    }

    static int assetInfoByPath(void*, const char*, LunaEditorAssetInfo*)
    {
        return 0;
    }

    static size_t listAssets(void* api_user_data,
                             uint32_t,
                             int,
                             void* user_data,
                             LunaEditorEnumerateAssetFn enumerate_fn)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            ++host->list_assets_count;
        }
        if (enumerate_fn == nullptr) {
            return 1u;
        }

        char label[64]{};
        char detail[64]{};
        char project_path[128]{};
        char absolute_path[256]{};
        copyToBuffer(label, sizeof(label), "Sample Material");
        copyToBuffer(detail, sizeof(detail), "Material asset");
        copyToBuffer(project_path, sizeof(project_path), "Materials/Sample.lmat");
        copyToBuffer(absolute_path, sizeof(absolute_path), "F:/NativeSampleProject/Assets/Materials/Sample.lmat");

        LunaEditorAssetInfo info{};
        info.struct_size = sizeof(LunaEditorAssetInfo);
        info.api_version = LUNA_EDITOR_ASSET_INFO_API_VERSION;
        info.handle = 42u;
        info.type = LunaEditorAssetType_Material;
        info.exists = 1;
        info.label = label;
        info.label_size = sizeof(label);
        info.detail = detail;
        info.detail_size = sizeof(detail);
        info.project_path = project_path;
        info.project_path_size = sizeof(project_path);
        info.absolute_path = absolute_path;
        info.absolute_path_size = sizeof(absolute_path);
        enumerate_fn(user_data, &info);
        return 1u;
    }

    static int assetPathExists(void*, const char*)
    {
        return 1;
    }

    static uint64_t findAssetHandleByPath(void*, const char*)
    {
        return 42u;
    }

    static int assetsRootPath(void*, char* out_path, size_t out_path_size)
    {
        copyToBuffer(out_path, out_path_size, "F:/NativeSampleProject/Assets");
        return 1;
    }

    static int resolveProjectAssetPath(void*, const char* path, char* out_path, size_t out_path_size)
    {
        copyToBuffer(out_path, out_path_size, std::string("F:/NativeSampleProject/Assets/") + (path != nullptr ? path : ""));
        return 1;
    }

    static int makeProjectRelativeAssetPath(void*, const char*, char* out_path, size_t out_path_size)
    {
        copyToBuffer(out_path, out_path_size, "Materials/Sample.lmat");
        return 1;
    }

    static int refreshAssets(void*, LunaEditorAssetRefreshResult* out_result)
    {
        if (out_result != nullptr) {
            out_result->success = 1;
            out_result->project_loaded = 1;
            out_result->revision = 8u;
            copyToBuffer(out_result->message, out_result->message_size, "refresh ok");
        }
        return 1;
    }

    static uint64_t assetRevision(void*)
    {
        return 7u;
    }

    static int isAssetLoading(void*, uint64_t)
    {
        return 0;
    }

    static int pluginRootPath(void*, char* out_path, size_t out_path_size)
    {
        copyToBuffer(out_path, out_path_size, "F:/NativeSamplePlugin");
        return 1;
    }

    static int pluginAssetRootPath(void*, char* out_path, size_t out_path_size)
    {
        copyToBuffer(out_path, out_path_size, "F:/NativeSamplePlugin/assets");
        return 1;
    }

    static int pluginAssetReadText(void* api_user_data,
                                   const char* relative_asset_path,
                                   char* out_text,
                                   size_t out_text_size,
                                   size_t* out_required_size)
    {
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr || relative_asset_path == nullptr || std::string_view(relative_asset_path) != "welcome.txt") {
            return 0;
        }
        ++host->plugin_asset_read_text_count;
        constexpr std::string_view kText = "NativeSample contract welcome text";
        if (out_required_size != nullptr) {
            *out_required_size = kText.size() + 1u;
        }
        copyToBuffer(out_text, out_text_size, kText);
        return out_text == nullptr || out_text_size >= kText.size() + 1u ? 1 : 0;
    }

    static int addMenuItem(void* api_user_data, const LunaEditorMenuItemDescriptor* descriptor)
    {
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr || descriptor == nullptr || descriptor->menu_path == nullptr ||
            descriptor->command_id == nullptr) {
            return 0;
        }
        host->menus.push_back(MenuRecord{.menu_path = descriptor->menu_path, .command_id = descriptor->command_id});
        return 1;
    }

    static void removeMenuItem(void* api_user_data, const char* menu_path, const char* command_id)
    {
        NativeSampleHost* host = self(api_user_data);
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
        NativeSampleHost* host = self(api_user_data);
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

    static int hasProjectLoaded(void*)
    {
        return 1;
    }

    static int projectRootPath(void*, char* out_path, size_t out_path_size)
    {
        copyToBuffer(out_path, out_path_size, "F:/NativeSampleProject");
        return 1;
    }

    static int projectInfo(void* api_user_data, LunaEditorProjectInfo* out_info)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            ++host->project_info_count;
        }
        if (out_info == nullptr) {
            return 0;
        }
        copyToBuffer(out_info->name, out_info->name_size, "Native Sample Project");
        copyToBuffer(out_info->version, out_info->version_size, "0.1.0");
        copyToBuffer(out_info->author, out_info->author_size, "Luna");
        copyToBuffer(out_info->description, out_info->description_size, "Contract project");
        copyToBuffer(out_info->start_scene, out_info->start_scene_size, "Scenes/Main.lscene");
        copyToBuffer(out_info->assets_path, out_info->assets_path_size, "Assets");
        copyToBuffer(out_info->selected_script_plugin_id, out_info->selected_script_plugin_id_size, "lua");
        copyToBuffer(out_info->selected_script_backend_name, out_info->selected_script_backend_name_size, "Lua");
        return 1;
    }

    static int saveProject(void*)
    {
        return 1;
    }

    static int sceneLabel(void* api_user_data, char* out_label, size_t out_label_size)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            ++host->scene_label_count;
        }
        copyToBuffer(out_label, out_label_size, "Native Sample Scene");
        return 1;
    }

    static size_t entityCount(void* api_user_data)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            return host->entities.size();
        }
        return 0u;
    }

    static int canEditScene(void* api_user_data)
    {
        NativeSampleHost* host = self(api_user_data);
        return host != nullptr && host->can_edit_scene ? 1 : 0;
    }

    static int entityExists(void* api_user_data, uint64_t entity_id)
    {
        NativeSampleHost* host = self(api_user_data);
        return host != nullptr && host->entities.contains(entity_id) ? 1 : 0;
    }

    static int entityInfo(void* api_user_data, uint64_t entity_id, LunaEditorSceneEntityInfo* out_info)
    {
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr || out_info == nullptr) {
            return 0;
        }
        const auto it = host->entities.find(entity_id);
        if (it == host->entities.end()) {
            return 0;
        }
        ++host->entity_info_count;
        out_info->id = entity_id;
        copyToBuffer(out_info->name, out_info->name_size, it->second.name);
        copyToBuffer(out_info->parent_name, out_info->parent_name_size, "");
        return 1;
    }

    static uint64_t createEntity(void* api_user_data, const char* name)
    {
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr) {
            return 0u;
        }
        const uint64_t id = host->next_entity_id++;
        host->entities.emplace(id, EntityRecord{.id = id, .name = name != nullptr ? name : "Entity"});
        return id;
    }

    static uint64_t createEntityEx(void* api_user_data, const LunaEditorSceneEntityCreateRequest* request)
    {
        return createEntity(api_user_data, request != nullptr ? request->name : nullptr);
    }

    static int destroyEntity(void* api_user_data, uint64_t entity_id)
    {
        NativeSampleHost* host = self(api_user_data);
        return host != nullptr && host->entities.erase(entity_id) > 0u ? 1 : 0;
    }

    static int setEntityName(void* api_user_data, uint64_t entity_id, const char* name)
    {
        NativeSampleHost* host = self(api_user_data);
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
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr || out_transform == nullptr) {
            return 0;
        }
        const auto it = host->entities.find(entity_id);
        if (it == host->entities.end()) {
            return 0;
        }
        ++host->transform_get_count;
        *out_transform = it->second.transform;
        return 1;
    }

    static int setEntityTransform(void* api_user_data, uint64_t entity_id, const LunaEditorSceneTransform* transform)
    {
        NativeSampleHost* host = self(api_user_data);
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
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr || out_component == nullptr) {
            return 0;
        }
        const auto it = host->entities.find(entity_id);
        if (it == host->entities.end() || !it->second.has_camera) {
            return 0;
        }
        *out_component = it->second.camera;
        return 1;
    }

    static int setCameraComponent(void* api_user_data,
                                  uint64_t entity_id,
                                  const LunaEditorSceneCameraComponent* component)
    {
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr || component == nullptr) {
            return 0;
        }
        const auto it = host->entities.find(entity_id);
        if (it == host->entities.end()) {
            return 0;
        }
        it->second.has_camera = true;
        it->second.camera = *component;
        return 1;
    }

    static int getLightComponent(void* api_user_data, uint64_t entity_id, LunaEditorSceneLightComponent* out_component)
    {
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr || out_component == nullptr) {
            return 0;
        }
        const auto it = host->entities.find(entity_id);
        if (it == host->entities.end() || !it->second.has_light) {
            return 0;
        }
        *out_component = it->second.light;
        return 1;
    }

    static int setLightComponent(void* api_user_data,
                                 uint64_t entity_id,
                                 const LunaEditorSceneLightComponent* component)
    {
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr || component == nullptr) {
            return 0;
        }
        const auto it = host->entities.find(entity_id);
        if (it == host->entities.end()) {
            return 0;
        }
        it->second.has_light = true;
        it->second.light = *component;
        return 1;
    }

    static int getMeshComponent(void* api_user_data, uint64_t entity_id, LunaEditorSceneMeshComponent* out_component)
    {
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr || out_component == nullptr) {
            return 0;
        }
        const auto it = host->entities.find(entity_id);
        if (it == host->entities.end() || !it->second.has_mesh) {
            return 0;
        }
        *out_component = it->second.mesh;
        return 1;
    }

    static int addComponent(void* api_user_data, uint64_t entity_id, uint32_t component_kind)
    {
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr) {
            return 0;
        }
        const auto it = host->entities.find(entity_id);
        if (it == host->entities.end()) {
            return 0;
        }
        if (component_kind == LunaEditorSceneComponentKind_Camera) {
            it->second.has_camera = true;
        } else if (component_kind == LunaEditorSceneComponentKind_Light) {
            it->second.has_light = true;
        }
        return 1;
    }

    static uint64_t selectedEntityId(void* api_user_data)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            return host->selected_entity_id;
        }
        return 0u;
    }

    static void selectEntity(void* api_user_data, uint64_t entity_id)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            host->selected_entity_id = entity_id;
        }
    }

    static void clearSelection(void* api_user_data)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            host->selected_entity_id = 0u;
        }
    }

    static void editorCameraPosition(void*, LunaEditorVec3* out_position)
    {
        if (out_position != nullptr) {
            *out_position = LunaEditorVec3{.x = 5.0f, .y = 6.0f, .z = 7.0f};
        }
    }

    static int gizmoOperationName(void*, char* out_value, size_t out_value_size)
    {
        copyToBuffer(out_value, out_value_size, "Translate");
        return 1;
    }

    static int gizmoModeName(void*, char* out_value, size_t out_value_size)
    {
        copyToBuffer(out_value, out_value_size, "Local");
        return 1;
    }

    static int pickDebugVisualizationEnabled(void* api_user_data)
    {
        NativeSampleHost* host = self(api_user_data);
        return host != nullptr && host->pick_debug_enabled ? 1 : 0;
    }

    static void setPickDebugVisualizationEnabled(void* api_user_data, int enabled)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            host->pick_debug_enabled = enabled != 0;
        }
    }

    static int editorGridEnabled(void* api_user_data)
    {
        NativeSampleHost* host = self(api_user_data);
        return host != nullptr && host->editor_grid_enabled ? 1 : 0;
    }

    static void setEditorGridEnabled(void* api_user_data, int enabled)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            host->editor_grid_enabled = enabled != 0;
        }
    }

    static uint64_t defaultSceneViewport(void*)
    {
        return 1u;
    }

    static uint64_t createSceneViewport(void* api_user_data, const char*)
    {
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr) {
            return 0u;
        }
        const uint64_t id = host->next_viewport_id++;
        host->live_viewports.push_back(id);
        return id;
    }

    static void destroySceneViewport(void* api_user_data, uint64_t viewport_id)
    {
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr) {
            return;
        }
        host->live_viewports.erase(std::remove(host->live_viewports.begin(), host->live_viewports.end(), viewport_id),
                                   host->live_viewports.end());
    }

    static int isSceneViewportValid(void* api_user_data, uint64_t viewport_id)
    {
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr) {
            return 0;
        }
        if (viewport_id == 1u) {
            return 1;
        }
        return std::find(host->live_viewports.begin(), host->live_viewports.end(), viewport_id) !=
                       host->live_viewports.end()
                   ? 1
                   : 0;
    }

    static int syncSceneViewportEx(void* api_user_data,
                                   uint64_t viewport_id,
                                   uint32_t width,
                                   uint32_t height,
                                   LunaEditorViewportPresentation* out_presentation)
    {
        NativeSampleHost* host = self(api_user_data);
        if (host == nullptr || out_presentation == nullptr || isSceneViewportValid(api_user_data, viewport_id) == 0) {
            return 0;
        }
        ++host->viewport_sync_count;
        out_presentation->scene_texture = LunaEditorTextureView{.texture_id = 0x100u + viewport_id,
                                                                 .width = width,
                                                                 .height = height,
                                                                 .y_flip = 0};
        out_presentation->framebuffer_width = width;
        out_presentation->framebuffer_height = height;
        out_presentation->presentable = 1;
        return 1;
    }

    static int isRuntimeViewportEnabled(void*)
    {
        return 0;
    }

    static int isRuntimeViewportRequested(void* api_user_data)
    {
        NativeSampleHost* host = self(api_user_data);
        return host != nullptr && host->runtime_viewport_requested ? 1 : 0;
    }

    static void setRuntimeViewportRequested(void* api_user_data, int enabled)
    {
        if (NativeSampleHost* host = self(api_user_data)) {
            host->runtime_viewport_requested = enabled != 0;
        }
    }

    static size_t runtimeEntityCount(void*)
    {
        return 23u;
    }
};

std::filesystem::path nativeSampleBinaryPath()
{
#if defined(LUNA_NATIVE_SAMPLE_PLUGIN_BINARY)
    return std::filesystem::absolute(std::filesystem::path(LUNA_NATIVE_SAMPLE_PLUGIN_BINARY)).lexically_normal();
#else
    return {};
#endif
}

std::filesystem::path nativeSamplePackageRoot()
{
#if defined(LUNA_NATIVE_SAMPLE_PLUGIN_ROOT)
    return std::filesystem::absolute(std::filesystem::path(LUNA_NATIVE_SAMPLE_PLUGIN_ROOT)).lexically_normal();
#else
    return {};
#endif
}

std::filesystem::path testEditorPluginBinaryPath(std::string_view target_name)
{
#if defined(LUNA_TEST_EDITOR_PLUGIN_DIR)
#if defined(_WIN32)
    constexpr std::string_view extension = ".dll";
#elif defined(__APPLE__)
    constexpr std::string_view extension = ".dylib";
#else
    constexpr std::string_view extension = ".so";
#endif
    return (std::filesystem::path(LUNA_TEST_EDITOR_PLUGIN_DIR) /
            (std::string(target_name) + std::string(extension)))
        .lexically_normal();
#else
    return {};
#endif
}

bool containsString(const std::vector<std::string>& values, std::string_view value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

bool containsText(std::string_view value, std::string_view fragment)
{
    return value.find(fragment) != std::string_view::npos;
}

const luna::editor::EditorPluginPackage*
findPackageById(const std::vector<luna::editor::EditorPluginPackage>& packages, std::string_view package_id)
{
    const auto it = std::find_if(packages.begin(), packages.end(), [package_id](const auto& package) {
        return package.id == package_id;
    });
    return it != packages.end() ? &(*it) : nullptr;
}

void expectNativeSamplePackage(TestContext& context,
                               const luna::editor::EditorPluginPackage& package,
                               const std::filesystem::path& package_root,
                               const std::filesystem::path& plugin_binary)
{
    context.expect(package.id == kExpectedPluginId, "native sample manifest id should match plugin descriptor");
    context.expect(package.display_name == "Native Sample Tool", "native sample manifest display name should match");
    context.expect(package.runtime == luna::editor::EditorPluginRuntime::Native,
                   "native sample manifest runtime should be Native");
    context.expect(package.enabled, "native sample manifest should be enabled");
    context.expect(package.root_path == package_root, "native sample manifest root should match package root");
    context.expect(!package.entry_path.empty(), "native sample manifest should provide a current-platform entry");
    context.expect(package.entry_exists, "native sample manifest current-platform entry should exist");
    context.expect(package.resolved_entry_path == plugin_binary,
                   "native sample manifest current-platform entry should resolve to built plugin binary");
    context.expect(containsString(package.dependencies, kCoreCommandsPluginId),
                   "native sample manifest should depend on core commands");
}

void expectNativeSampleDiscovered(TestContext& context,
                                  const luna::editor::EditorPluginManifestLoader& loader,
                                  const std::filesystem::path& discovery_root,
                                  const std::filesystem::path& package_root,
                                  const std::filesystem::path& plugin_binary,
                                  std::string_view discovery_label)
{
    const std::vector<luna::editor::EditorPluginPackage> discovered_packages =
        loader.loadPackagesFromRoot(discovery_root);
    const luna::editor::EditorPluginPackage* discovered_package =
        findPackageById(discovered_packages, kExpectedPluginId);
    const std::string message = "native sample manifest should be discoverable when scanning " +
                                std::string(discovery_label);
    if (!context.expect(discovered_package != nullptr, message)) {
        return;
    }

    expectNativeSamplePackage(context, *discovered_package, package_root, plugin_binary);
}

void testNativeSamplePackageManifest(TestContext& context,
                                     const std::filesystem::path& package_root,
                                     const std::filesystem::path& plugin_binary)
{
    context.expect(!package_root.empty(), "native sample package root should be configured");
    context.expect(std::filesystem::exists(package_root), "native sample package root should exist");

    const std::filesystem::path manifest_path = package_root / "editor-plugin.yaml";
    context.expect(std::filesystem::exists(manifest_path), "native sample editor-plugin.yaml should exist");

    const luna::editor::EditorPluginManifestLoader loader;
    const std::optional<luna::editor::EditorPluginPackage> package = loader.loadPackage(manifest_path);
    if (!context.expect(package.has_value(), "native sample manifest should parse through EditorPluginManifestLoader")) {
        return;
    }

    expectNativeSamplePackage(context, *package, package_root, plugin_binary);
    expectNativeSampleDiscovered(context, loader, package_root, package_root, plugin_binary, "the package root");
    expectNativeSampleDiscovered(context,
                                 loader,
                                 package_root.parent_path(),
                                 package_root,
                                 plugin_binary,
                                 "the editor plugin root");
}

const luna::editor::PluginInfo*
findPluginInfoById(const std::vector<luna::editor::PluginInfo>& plugins, std::string_view plugin_id)
{
    const auto it = std::find_if(plugins.begin(), plugins.end(), [plugin_id](const auto& plugin) {
        return plugin.id == plugin_id;
    });
    return it != plugins.end() ? &(*it) : nullptr;
}

void testNativeSampleEditorPluginManagerLoad(TestContext& context,
                                             const std::filesystem::path& package_root,
                                             const std::filesystem::path& plugin_binary)
{
    const luna::editor::EditorPluginManifestLoader loader;
    std::optional<luna::editor::EditorPluginPackage> package =
        loader.loadPackage(package_root / "editor-plugin.yaml");
    if (!context.expect(package.has_value(), "native sample manager smoke should parse manifest")) {
        return;
    }

    ManagerSmokeHost host;
    luna::editor::EditorPluginManager manager(host);
    manager.registerPackage(luna::editor::EditorPluginPackage{
        .id = kCoreCommandsPluginId,
        .display_name = "Core Commands",
        .version = "0.1.0",
        .root_path = package_root.parent_path() / "Official" / "CoreCommands",
        .runtime = luna::editor::EditorPluginRuntime::BuiltinNative,
        .source = luna::editor::EditorPluginSource::Official,
        .category = luna::editor::EditorPluginCategory::Core,
        .enabled = true,
        .create = [] {
            return std::make_unique<ManagerSmokeBuiltinPlugin>(kCoreCommandsPluginId);
        },
    });
    manager.registerPackage(*package);

    context.expect(manager.loadRegisteredPackages(), "EditorPluginManager should load NativeSample through real native path");

    const std::vector<luna::editor::PluginInfo> loaded_plugins = manager.pluginInfos();
    const luna::editor::PluginInfo* native_sample = findPluginInfoById(loaded_plugins, kExpectedPluginId);
    if (context.expect(native_sample != nullptr, "PluginInfo should include NativeSample after manager load")) {
        context.expect(native_sample->runtime == luna::editor::PluginRuntimeKind::Native,
                       "NativeSample PluginInfo runtime should be Native");
        context.expect(native_sample->category == luna::editor::PluginCategoryKind::Tool,
                       "NativeSample PluginInfo category should default to Tool");
        context.expect(native_sample->state == luna::editor::PluginLoadState::Loaded,
                       "NativeSample PluginInfo state should be Loaded");
        context.expect(native_sample->status == "Loaded", "NativeSample PluginInfo status should be Loaded");
        context.expect(native_sample->resolved_entry_path == plugin_binary,
                       "NativeSample PluginInfo resolved entry should point at built binary");
    }

    context.expect(host.command_service.commands.contains(kOpenCommandId),
                   "manager-loaded NativeSample should register open command");
    context.expect(host.command_service.commands.contains(kCreateEntityCommandId),
                   "manager-loaded NativeSample should register create entity command");
    context.expect(host.window_service.windows.contains(kExpectedWindowId),
                   "manager-loaded NativeSample should register window");
    context.expect(host.menu_service.menus.size() == 2u,
                   "manager-loaded NativeSample should register two menu items");

    context.expect(host.command_service.execute(kOpenCommandId),
                   "manager-loaded NativeSample open command should execute");
    context.expect(host.window_service.isWindowOpen(kExpectedWindowId),
                   "manager-loaded NativeSample open command should open window");
    context.expect(host.command_service.execute(kCreateEntityCommandId),
                   "manager-loaded NativeSample create entity command should execute");
    context.expect(static_cast<uint64_t>(host.selection_service.selectedEntityId()) == 100u,
                   "manager-loaded NativeSample should select created entity");

    context.expect(host.drawWindow(kExpectedWindowId), "manager-loaded NativeSample window should draw");
    context.expect(host.ui_service.text_count > 0, "manager-loaded NativeSample should draw text through host UI");
    context.expect(host.ui_service.image_count > 0, "manager-loaded NativeSample should draw viewport image through host UI");
    context.expect(!host.viewport_service.viewports.empty(),
                   "manager-loaded NativeSample should create an independent scene viewport");
    context.expect(host.ui_service.disabled_depth == 0,
                   "manager-loaded NativeSample should balance disabled scopes during draw");

    manager.unloadAll();
    context.expect(host.command_service.commands.empty(),
                   "manager unload should remove NativeSample command contributions");
    context.expect(host.window_service.windows.empty(),
                   "manager unload should remove NativeSample window contributions");
    context.expect(host.menu_service.menus.empty(),
                   "manager unload should remove NativeSample menu contributions");
    context.expect(host.viewport_service.viewports.empty(),
                   "manager unload should remove NativeSample viewport contributions");
    context.expect(host.plugin_asset_service.roots.empty(),
                   "manager unload should unregister NativeSample plugin asset root");
}

void expectManagerDiagnostic(TestContext& context,
                             const luna::editor::EditorPluginManager& manager,
                             std::string_view plugin_id,
                             luna::editor::PluginLoadState expected_state,
                             std::string_view expected_status_fragment,
                             std::string_view label)
{
    const std::vector<luna::editor::PluginInfo> plugins = manager.pluginInfos();
    const luna::editor::PluginInfo* plugin = findPluginInfoById(plugins, plugin_id);
    const std::string missing_message = std::string(label) + " should have PluginInfo";
    if (!context.expect(plugin != nullptr, missing_message)) {
        return;
    }

    const std::string state_message = std::string(label) + " should report expected PluginInfo state";
    context.expect(plugin->state == expected_state, state_message);
    const std::string status_message = std::string(label) + " should report diagnostic status";
    context.expect(containsText(plugin->status, expected_status_fragment), status_message);
}

void registerSmokeCoreCommandsPackage(luna::editor::EditorPluginManager& manager,
                                      const std::filesystem::path& package_root)
{
    manager.registerPackage(luna::editor::EditorPluginPackage{
        .id = kCoreCommandsPluginId,
        .display_name = "Core Commands",
        .version = "0.1.0",
        .root_path = package_root.parent_path() / "Official" / "CoreCommands",
        .runtime = luna::editor::EditorPluginRuntime::BuiltinNative,
        .source = luna::editor::EditorPluginSource::Official,
        .enabled = true,
        .create = [] {
            return std::make_unique<ManagerSmokeBuiltinPlugin>(kCoreCommandsPluginId);
        },
    });
}

void testEditorPluginManagerDiagnostics(TestContext& context,
                                        const std::filesystem::path& package_root,
                                        const std::filesystem::path& plugin_binary)
{
    {
        ManagerSmokeHost host;
        luna::editor::EditorPluginManager manager(host);
        registerSmokeCoreCommandsPackage(manager, package_root);
        manager.registerPackage(luna::editor::EditorPluginPackage{
            .id = "luna.test.native.missing-entry",
            .display_name = "Missing Entry",
            .version = "0.1.0",
            .root_path = package_root,
            .entry_path = "Binaries/Missing/Missing.dll",
            .resolved_entry_path = package_root / "Binaries" / "Missing" / "Missing.dll",
            .runtime = luna::editor::EditorPluginRuntime::Native,
            .source = luna::editor::EditorPluginSource::Development,
            .dependencies = {kCoreCommandsPluginId},
            .enabled = true,
            .entry_exists = false,
        });

        context.expect(!manager.loadRegisteredPackages(),
                       "manager should fail when native plugin entry is missing");
        expectManagerDiagnostic(context,
                                manager,
                                "luna.test.native.missing-entry",
                                luna::editor::PluginLoadState::Failed,
                                "Native plugin entry does not exist",
                                "missing native entry");
    }

    {
        ManagerSmokeHost host;
        luna::editor::EditorPluginManager manager(host);
        manager.registerPackage(luna::editor::EditorPluginPackage{
            .id = "luna.test.native.missing-dependency",
            .display_name = "Missing Dependency",
            .version = "0.1.0",
            .root_path = package_root,
            .entry_path = plugin_binary,
            .resolved_entry_path = plugin_binary,
            .runtime = luna::editor::EditorPluginRuntime::Native,
            .source = luna::editor::EditorPluginSource::Development,
            .dependencies = {"luna.test.native.no-such-dependency"},
            .enabled = true,
            .entry_exists = true,
        });

        context.expect(!manager.loadRegisteredPackages(),
                       "manager should fail when native plugin dependency is missing");
        expectManagerDiagnostic(context,
                                manager,
                                "luna.test.native.missing-dependency",
                                luna::editor::PluginLoadState::Failed,
                                "Dependencies were not met",
                                "missing native dependency");
    }

    {
        ManagerSmokeHost host;
        luna::editor::EditorPluginManager manager(host);
        manager.registerPackage(luna::editor::EditorPluginPackage{
            .id = "luna.test.duplicate",
            .display_name = "Duplicate First",
            .version = "0.1.0",
            .root_path = package_root / "First",
            .runtime = luna::editor::EditorPluginRuntime::BuiltinNative,
            .enabled = true,
            .create = [] {
                return std::make_unique<ManagerSmokeBuiltinPlugin>("luna.test.duplicate");
            },
        });
        manager.registerPackage(luna::editor::EditorPluginPackage{
            .id = "luna.test.duplicate",
            .display_name = "Duplicate Second",
            .version = "0.1.0",
            .root_path = package_root / "Second",
            .runtime = luna::editor::EditorPluginRuntime::BuiltinNative,
            .enabled = true,
            .create = [] {
                return std::make_unique<ManagerSmokeBuiltinPlugin>("luna.test.duplicate");
            },
        });

        context.expect(!manager.loadRegisteredPackages(),
                       "manager should report duplicate plugin ids as failed diagnostics");
        const std::vector<luna::editor::PluginInfo> plugins = manager.pluginInfos();
        const auto duplicate_failed = std::find_if(plugins.begin(), plugins.end(), [](const auto& plugin) {
            return plugin.id == "luna.test.duplicate" &&
                   plugin.state == luna::editor::PluginLoadState::Failed &&
                   containsText(plugin.status, "Duplicate plugin id");
        });
        context.expect(duplicate_failed != plugins.end(),
                       "duplicate plugin id should remain visible in PluginInfo diagnostics");
    }

    struct NativeFailureCase {
        const char* package_id;
        const char* target_name;
        const char* expected_status;
    };
    const NativeFailureCase native_failure_cases[] = {
        {"luna.test.native", "LunaTestEditorPluginMissingSymbol", "Native plugin does not export"},
        {"luna.test.native", "LunaTestEditorPluginApiMismatch", "Native plugin API version mismatch"},
        {"luna.test.native", "LunaTestEditorPluginMissingCallbacks", "Native plugin must provide on_load and on_unload callbacks"},
        {"luna.test.native", "LunaTestEditorPluginLoadFailure", "Native plugin on_load returned failure"},
        {"luna.test.native.expected", "LunaTestEditorPluginIdMismatch", "does not match package id"},
    };

    for (const NativeFailureCase& failure_case : native_failure_cases) {
        const std::filesystem::path failure_binary = testEditorPluginBinaryPath(failure_case.target_name);
        const std::string label = std::string("manager diagnostic for ") + failure_case.target_name;
        if (!context.expect(!failure_binary.empty() && std::filesystem::exists(failure_binary), label + " binary should exist")) {
            continue;
        }

        ManagerSmokeHost host;
        luna::editor::EditorPluginManager manager(host);
        registerSmokeCoreCommandsPackage(manager, package_root);
        manager.registerPackage(luna::editor::EditorPluginPackage{
            .id = failure_case.package_id,
            .display_name = failure_case.target_name,
            .version = "0.1.0",
            .root_path = package_root,
            .entry_path = failure_binary,
            .resolved_entry_path = failure_binary,
            .runtime = luna::editor::EditorPluginRuntime::Native,
            .source = luna::editor::EditorPluginSource::Development,
            .dependencies = {kCoreCommandsPluginId},
            .enabled = true,
            .entry_exists = true,
        });

        context.expect(!manager.loadRegisteredPackages(), label + " should fail through EditorPluginManager");
        expectManagerDiagnostic(context,
                                manager,
                                failure_case.package_id,
                                luna::editor::PluginLoadState::Failed,
                                failure_case.expected_status,
                                label);
        context.expect(host.command_service.commands.empty(),
                       label + " should clean command contributions after failed load");
        context.expect(host.window_service.windows.empty(),
                       label + " should clean window contributions after failed load");
        context.expect(host.menu_service.menus.empty(),
                       label + " should clean menu contributions after failed load");
        context.expect(host.viewport_service.viewports.empty(),
                       label + " should clean viewport contributions after failed load");
        context.expect(!host.plugin_asset_service.roots.contains(failure_case.package_id),
                       label + " should unregister failed plugin asset root after failed load");
    }
}

} // namespace

int main()
{
    luna::Logger::init("", luna::Logger::Level::Warn);

    TestContext context;
    const std::filesystem::path plugin_binary = nativeSampleBinaryPath();
    const std::filesystem::path package_root = nativeSamplePackageRoot();
    context.expect(!plugin_binary.empty(), "native sample plugin binary path should be configured");
    context.expect(std::filesystem::exists(plugin_binary), "native sample plugin binary should exist");
    testNativeSamplePackageManifest(context, package_root, plugin_binary);
    testNativeSampleEditorPluginManagerLoad(context, package_root, plugin_binary);
    testEditorPluginManagerDiagnostics(context, package_root, plugin_binary);

    std::shared_ptr<luna::DynamicLibrary> library = luna::DynamicLibrary::load(plugin_binary);
    if (!context.expect(library != nullptr, "native sample plugin binary should load")) {
        luna::Logger::shutdown();
        return context.result();
    }

    auto* create_plugin =
        reinterpret_cast<LunaCreateEditorPluginFn>(library->findSymbol(LUNA_EDITOR_CREATE_PLUGIN_SYMBOL));
    if (!context.expect(create_plugin != nullptr, "native sample should export LunaCreateEditorPlugin")) {
        luna::Logger::shutdown();
        return context.result();
    }

    NativeSampleHost host;
    LunaEditorPluginApi plugin_api{};
    context.expect(create_plugin(LUNA_EDITOR_HOST_API_VERSION, &host.api, &plugin_api) != 0,
                   "native sample create function should fill plugin API");
    context.expect(plugin_api.struct_size == sizeof(LunaEditorPluginApi), "native sample plugin API size should match");
    context.expect(plugin_api.api_version == LUNA_EDITOR_PLUGIN_API_VERSION, "native sample plugin API version should be v1");
    context.expect(plugin_api.plugin_id != nullptr && std::string_view(plugin_api.plugin_id) == kExpectedPluginId,
                   "native sample plugin id should match manifest");
    context.expect(plugin_api.on_load != nullptr && plugin_api.on_unload != nullptr,
                   "native sample should provide load/unload callbacks");

    if (plugin_api.on_load != nullptr) {
        context.expect(plugin_api.on_load(plugin_api.plugin_user_data, &host.api) != 0,
                       "native sample on_load should succeed");
    }

    context.expect(host.commands.size() == 2u, "native sample should register two commands");
    context.expect(host.commands.contains(kOpenCommandId), "native sample should register open command");
    context.expect(host.commands.contains(kCreateEntityCommandId), "native sample should register create entity command");
    context.expect(host.windows.size() == 1u, "native sample should register one window");
    context.expect(host.windows.contains(kExpectedWindowId), "native sample should register its window");
    context.expect(host.menus.size() == 2u, "native sample should register two menu items");

    context.expect(host.api.commands.execute_command(host.api.commands.api_user_data, kOpenCommandId) == 1,
                   "native sample open command should execute");
    context.expect(host.api.windows.is_window_open(host.api.windows.api_user_data, kExpectedWindowId) == 1,
                   "native sample open command should open the window");
    context.expect(host.api.commands.is_command_checked(host.api.commands.api_user_data, kOpenCommandId) == 1,
                   "native sample open command should reflect window state");

    context.expect(host.api.commands.execute_command(host.api.commands.api_user_data, kCreateEntityCommandId) == 1,
                   "native sample create entity command should execute");
    context.expect(host.selected_entity_id == 100u, "native sample create command should select the created entity");
    context.expect(host.entities.contains(100u), "native sample create command should create an entity");

    context.expect(host.drawWindow(kExpectedWindowId), "native sample window should draw");
    context.expect(host.text_count > 0, "native sample draw should use UI text");
    context.expect(host.button_count > 0, "native sample draw should use UI buttons");
    context.expect(host.checkbox_count > 0, "native sample draw should use UI checkboxes");
    context.expect(host.slider_count > 0, "native sample draw should use UI sliders");
    context.expect(host.color_edit3_count > 0, "native sample draw should use UI color edit");
    context.expect(host.input_text_count > 0, "native sample draw should use UI input text");
    context.expect(host.plugin_asset_read_text_count > 0, "native sample should read plugin assets");
    context.expect(host.project_info_count > 0, "native sample should read project info");
    context.expect(host.list_assets_count > 0, "native sample should enumerate project assets");
    context.expect(host.scene_label_count > 0, "native sample should read scene label");
    context.expect(host.entity_info_count > 0, "native sample should read selected entity info");
    context.expect(host.transform_get_count > 0, "native sample should read selected entity transform");
    context.expect(!host.live_viewports.empty(), "native sample should create an independent scene viewport");
    context.expect(host.viewport_sync_count > 0, "native sample should sync its independent viewport");
    context.expect(host.image_count > 0, "native sample should draw the viewport texture through UI image");
    context.expect(host.tooltip_count > 0, "native sample should use tooltip wrapper for hovered viewport image");
    context.expect(host.disabled_depth == 0, "native sample draw should balance disabled scopes");

    host.can_edit_scene = false;
    context.expect(host.drawWindow(kExpectedWindowId), "native sample should draw while scene editing is disabled");
    context.expect(host.begin_disabled_count > 0, "native sample should disable scene editing controls when runtime owns scene");
    context.expect(host.disabled_depth == 0, "native sample should balance disabled scopes when scene editing is disabled");

    if (plugin_api.on_unload != nullptr) {
        plugin_api.on_unload(plugin_api.plugin_user_data, &host.api);
    }

    context.expect(host.commands.empty(), "native sample unload should unregister commands");
    context.expect(host.windows.empty(), "native sample unload should unregister windows");
    context.expect(host.menus.empty(), "native sample unload should remove menu items");
    context.expect(host.live_viewports.empty(), "native sample unload should destroy scene viewports");

    luna::Logger::shutdown();
    return context.result();
}
