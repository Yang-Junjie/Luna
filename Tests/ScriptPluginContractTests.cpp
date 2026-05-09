#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Core/Log.h"
#include "Project/ProjectInfo.h"
#include "Project/ProjectManager.h"
#include "Scene/Components/CameraComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Asset/Editor/ImporterManager.h"
#include "Asset/Editor/ScriptLoader.h"
#include "Script/ScriptPluginDiscovery.h"
#include "Script/ScriptPluginManager.h"
#include "Script/ScriptAsset.h"
#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

class TempDirectory {
public:
    explicit TempDirectory(std::string_view name)
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path() /
                 ("Luna-" + std::string(name) + "-" + std::to_string(now));
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

void createTestProject(TestContext& context,
                       const std::filesystem::path& project_root,
                       std::string_view project_name,
                       std::string_view selected_plugin_id,
                       std::string_view selected_backend_name)
{
    luna::ProjectInfo project;
    project.Name = std::string(project_name);
    project.AssetsPath = "Assets";
    project.Scripting.SelectedPluginId = std::string(selected_plugin_id);
    project.Scripting.SelectedBackendName = std::string(selected_backend_name);

    std::error_code ec;
    std::filesystem::create_directories(project_root / project.AssetsPath, ec);
    context.expect(!ec, "test project asset directory should be created");

    const bool created = luna::ProjectManager::instance().createProject(project_root, project);
    context.expect(created, "test project should be created");
}

std::optional<luna::AssetMetadata> readMetadataFile(const std::filesystem::path& meta_path)
{
    try {
        const YAML::Node data = YAML::LoadFile(meta_path.string());
        const YAML::Node asset = data["Asset"];
        if (!asset) {
            return std::nullopt;
        }

        luna::AssetMetadata metadata;
        if (asset["Name"]) {
            metadata.Name = asset["Name"].as<std::string>();
        }
        if (asset["Handle"]) {
            metadata.Handle = luna::AssetHandle(asset["Handle"].as<uint64_t>());
        }
        if (asset["Type"]) {
            metadata.Type = luna::AssetUtils::StringToAssetType(asset["Type"].as<std::string>());
        }
        if (asset["FilePath"]) {
            metadata.FilePath = asset["FilePath"].as<std::string>();
        }
        if (asset["Config"]) {
            metadata.SpecializedConfig = asset["Config"];
        }
        return metadata;
    } catch (const YAML::Exception&) {
        return std::nullopt;
    }
}

std::string readMetadataLanguage(const std::filesystem::path& meta_path)
{
    const std::optional<luna::AssetMetadata> metadata = readMetadataFile(meta_path);
    if (!metadata.has_value()) {
        return {};
    }

    return metadata->GetConfig<std::string>("Language", "");
}

bool metadataFileExists(const std::filesystem::path& asset_path)
{
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path(asset_path.string() + ".meta"), ec) && !ec;
}

void writeScriptMetadata(const std::filesystem::path& project_root,
                         const std::filesystem::path& relative_script_path,
                         std::string_view language)
{
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "Asset" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "Name" << YAML::Value << relative_script_path.stem().string();
    out << YAML::Key << "Handle" << YAML::Value << 1u;
    out << YAML::Key << "Type" << YAML::Value << "Script";
    out << YAML::Key << "FilePath" << YAML::Value << relative_script_path.generic_string();
    out << YAML::Key << "Config" << YAML::Value << YAML::BeginMap;
    if (!language.empty()) {
        out << YAML::Key << "Language" << YAML::Value << std::string(language);
    }
    out << YAML::EndMap;
    out << YAML::EndMap;
    out << YAML::EndMap;

    writeTextFile(project_root / (relative_script_path.string() + ".meta"), out.c_str());
}

std::shared_ptr<luna::ScriptAsset> loadScriptAssetFromMetadata(const std::filesystem::path& meta_path)
{
    const std::optional<luna::AssetMetadata> metadata = readMetadataFile(meta_path);
    if (!metadata.has_value()) {
        return {};
    }

    luna::ScriptLoader loader;
    return std::dynamic_pointer_cast<luna::ScriptAsset>(loader.load(*metadata));
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

    return std::filesystem::path(LUNA_TEST_SCRIPT_PLUGIN_DIR) / (std::string(target_name) + std::string(extension));
}

std::string yamlQuotedPath(const std::filesystem::path& path)
{
    std::string value = path.generic_string();
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

void writeContractPluginManifest(const std::filesystem::path& plugins_root,
                                 std::string_view plugin_id,
                                 std::string_view display_name,
                                 std::string_view language,
                                 std::string_view backend,
                                 std::span<const std::string_view> supported_extensions,
                                 const std::filesystem::path& entry_path)
{
    std::ostringstream manifest;
    manifest << "Plugin:\n"
             << "  Id: " << plugin_id << "\n"
             << "  DisplayName: " << display_name << "\n"
             << "  Type: Script\n"
             << "  Language: " << language << "\n"
             << "  Backend: " << backend << "\n"
             << "  SupportedExtensions:\n";
    for (const std::string_view extension : supported_extensions) {
        manifest << "    - " << extension << "\n";
    }
    manifest << "  HostApiVersion: 1\n"
             << "  Entry: " << yamlQuotedPath(entry_path) << "\n";

    writeTextFile(plugins_root / std::string(plugin_id) / "plugin.yaml", manifest.str());
}

const luna::ScriptPluginCandidate* findCandidate(const std::vector<luna::ScriptPluginCandidate>& candidates,
                                                 std::string_view plugin_id)
{
    const auto it = std::find_if(candidates.begin(), candidates.end(), [&](const auto& candidate) {
        return candidate.Manifest.PluginId == plugin_id;
    });
    return it != candidates.end() ? &*it : nullptr;
}

const char* stateName(luna::ScriptPluginSelectionState state)
{
    switch (state) {
        case luna::ScriptPluginSelectionState::Unresolved:
            return "Unresolved";
        case luna::ScriptPluginSelectionState::NoProject:
            return "NoProject";
        case luna::ScriptPluginSelectionState::NoPluginsDiscovered:
            return "NoPluginsDiscovered";
        case luna::ScriptPluginSelectionState::MissingSelection:
            return "MissingSelection";
        case luna::ScriptPluginSelectionState::PluginNotFound:
            return "PluginNotFound";
        case luna::ScriptPluginSelectionState::BackendNotFound:
            return "BackendNotFound";
        case luna::ScriptPluginSelectionState::BackendAmbiguous:
            return "BackendAmbiguous";
        case luna::ScriptPluginSelectionState::BackendMismatch:
            return "BackendMismatch";
        case luna::ScriptPluginSelectionState::HostApiMismatch:
            return "HostApiMismatch";
        case luna::ScriptPluginSelectionState::Resolved:
            return "Resolved";
        default:
            return "Unknown";
    }
}

bool sameVec3(const glm::vec3& lhs, const glm::vec3& rhs, float epsilon = 0.0001f)
{
    return std::fabs(lhs.x - rhs.x) <= epsilon && std::fabs(lhs.y - rhs.y) <= epsilon &&
           std::fabs(lhs.z - rhs.z) <= epsilon;
}

bool expectState(TestContext& context,
                 const luna::ScriptPluginSelectionResult& result,
                 luna::ScriptPluginSelectionState expected,
                 std::string_view label)
{
    if (result.State == expected) {
        return true;
    }

    std::ostringstream message;
    message << label << ": expected " << stateName(expected) << ", got " << stateName(result.State);
    if (!result.StatusMessage.empty()) {
        message << " (" << result.StatusMessage << ")";
    }
    return context.expect(false, message.str());
}

bool expectLoadState(TestContext& context,
                     luna::ScriptPluginManager& manager,
                     const luna::ProjectInfo& project,
                     luna::ScriptPluginSelectionState expected,
                     std::string_view label)
{
    return expectState(context, manager.resolveAndLoadProjectSelection(&project), expected, label);
}

void testDiscoveryManifestContract(TestContext& context)
{
    TempDirectory temp("ScriptPluginDiscovery");
    const std::filesystem::path engine_plugins = temp.path() / "EnginePlugins";
    const std::filesystem::path project_root = temp.path() / "Project";
    const std::filesystem::path project_plugins = project_root / "Plugins";

    writeTextFile(engine_plugins / "GoodLua" / "plugin.yaml",
                  R"(Plugin:
  Id: luna.test.lua
  DisplayName: Test Lua
  Type: Script
  Language: Lua
  Backend: Lua
  SupportedExtensions:
    - LUA
    - .TS
  HostApiVersion: 1
)");

    writeTextFile(engine_plugins / "Invalid" / "plugin.yaml",
                  R"(Plugin:
  Id: luna.test.invalid
  DisplayName: Invalid
  Type: Script
  Backend: Broken
)");

    writeTextFile(engine_plugins / "Override" / "plugin.yaml",
                  R"(Plugin:
  Id: luna.test.override
  DisplayName: Engine Override
  Type: Script
  Language: Lua
  Backend: Lua
  SupportedExtensions:
    - .lua
  HostApiVersion: 1
)");

    writeTextFile(project_plugins / "Override" / "plugin.yaml",
                  R"(Plugin:
  Id: luna.test.override
  DisplayName: Project Override
  Type: Script
  Language: Python
  Backend: Python
  SupportedExtensions:
    - py
  HostApiVersion: 1
)");

    luna::ScriptPluginDiscovery discovery;
    luna::ScriptPluginDiscovery::Options options{};
    options.EnginePluginsRoot = engine_plugins;
    options.ProjectPluginsRoot = project_plugins;
    const std::vector<luna::ScriptPluginCandidate> candidates = discovery.discover(options);

    const luna::ScriptPluginCandidate* lua = findCandidate(candidates, "luna.test.lua");
    context.expect(lua != nullptr, "valid script plugin manifest should be discovered");
    if (lua != nullptr) {
        context.expect(lua->Manifest.SupportedExtensions == std::vector<std::string>{".lua", ".ts"},
                       "supported extensions should be normalized");
    }

    context.expect(findCandidate(candidates, "luna.test.invalid") == nullptr,
                   "manifest missing Plugin.Language should be skipped");

    const luna::ScriptPluginCandidate* override = findCandidate(candidates, "luna.test.override");
    context.expect(override != nullptr, "project plugin should override engine plugin with same id");
    if (override != nullptr) {
        context.expect(override->Scope == luna::ScriptPluginScope::Project,
                       "overridden plugin candidate should keep project scope");
        context.expect(override->Manifest.Language == "Python", "project override manifest should be selected");
        context.expect(override->Manifest.SupportedExtensions == std::vector<std::string>{".py"},
                       "project override extension should be normalized");
    }
}

void testProjectSelectionContract(TestContext& context)
{
    luna::ScriptPluginManager& manager = luna::ScriptPluginManager::instance();
    manager.refreshDiscoveredPlugins();

    context.expect(manager.findDiscoveredPlugin("luna.official.lua") != nullptr,
                   "official Lua script plugin should be discoverable from engine plugins");

    expectState(context,
                manager.resolveProjectSelection(nullptr),
                luna::ScriptPluginSelectionState::NoProject,
                "null project");

    luna::ProjectInfo project;
    expectState(context,
                manager.resolveProjectSelection(&project),
                luna::ScriptPluginSelectionState::MissingSelection,
                "missing script plugin selection");

    project.Scripting.SelectedPluginId = "luna.missing.plugin";
    expectState(context,
                manager.resolveProjectSelection(&project),
                luna::ScriptPluginSelectionState::PluginNotFound,
                "missing plugin id");

    project.Scripting.SelectedPluginId = "luna.official.lua";
    project.Scripting.SelectedBackendName = "Python";
    expectState(context,
                manager.resolveProjectSelection(&project),
                luna::ScriptPluginSelectionState::BackendMismatch,
                "plugin/backend mismatch");

    project.Scripting.SelectedPluginId.clear();
    project.Scripting.SelectedBackendName = "Lua";
    const luna::ScriptPluginSelectionResult legacy_backend = manager.resolveProjectSelection(&project);
    expectState(context, legacy_backend, luna::ScriptPluginSelectionState::Resolved, "legacy backend selection");
    context.expect(legacy_backend.AutoSelected, "backend-only selection should auto-select a plugin");
    context.expect(legacy_backend.Candidate != nullptr &&
                       legacy_backend.Candidate->Manifest.PluginId == "luna.official.lua",
                   "backend-only Lua selection should resolve to official Lua plugin");

    project.Scripting.SelectedPluginId = "luna.official.lua";
    project.Scripting.SelectedBackendName = "Lua";
    const luna::ScriptPluginSelectionResult explicit_lua = manager.resolveProjectSelection(&project);
    expectState(context, explicit_lua, luna::ScriptPluginSelectionState::Resolved, "explicit Lua selection");
    context.expect(explicit_lua.ExplicitSelection, "explicit selection should be tracked");
    context.expect(explicit_lua.BackendName == "Lua", "explicit Lua selection should resolve backend name");
}

void testSelectionRejectsAmbiguousBackendAndHostMismatch(TestContext& context)
{
    TempDirectory temp("ScriptPluginSelection");
    const std::filesystem::path project_plugins = temp.path() / "Plugins";

    writeTextFile(project_plugins / "SecondLua" / "plugin.yaml",
                  R"(Plugin:
  Id: luna.test.second-lua
  DisplayName: Test Second Lua
  Type: Script
  Language: Lua
  Backend: Lua
  SupportedExtensions:
    - .lua
  HostApiVersion: 1
)");

    writeTextFile(project_plugins / "FutureHost" / "plugin.yaml",
                  R"(Plugin:
  Id: luna.test.future-host
  DisplayName: Test Future Host
  Type: Script
  Language: FutureScript
  Backend: FutureScript
  SupportedExtensions:
    - future
  HostApiVersion: 999
)");

    luna::ScriptPluginManager& manager = luna::ScriptPluginManager::instance();
    manager.refreshDiscoveredPlugins(temp.path());

    luna::ProjectInfo project;
    project.Scripting.SelectedBackendName = "Lua";
    expectState(context,
                manager.resolveProjectSelection(&project),
                luna::ScriptPluginSelectionState::BackendAmbiguous,
                "backend-only selection should reject multiple providers");

    project.Scripting.SelectedBackendName.clear();
    project.Scripting.SelectedPluginId = "luna.test.future-host";
    expectState(context,
                manager.resolveProjectSelection(&project),
                luna::ScriptPluginSelectionState::HostApiMismatch,
                "host API version mismatch");

    manager.refreshDiscoveredPlugins();
}

void testOfficialLuaPluginLoadContract(TestContext& context)
{
    luna::ScriptPluginManager& manager = luna::ScriptPluginManager::instance();
    manager.refreshDiscoveredPlugins();

    luna::ProjectInfo project;
    project.Scripting.SelectedPluginId = "luna.official.lua";
    project.Scripting.SelectedBackendName = "Lua";

    const luna::ScriptPluginSelectionResult selection = manager.resolveAndLoadProjectSelection(&project);
    expectState(context, selection, luna::ScriptPluginSelectionState::Resolved, "official Lua plugin load");

    const luna::ScriptBackendDescriptor* backend = manager.findBackend("Lua");
    context.expect(backend != nullptr, "official Lua backend should be registered after plugin load");
    if (backend != nullptr) {
        context.expect(backend->name == "Lua", "loaded Lua backend name should match manifest");
        context.expect(backend->language == "Lua", "loaded Lua backend language should match manifest");
        context.expect(backend->supported_extensions == std::vector<std::string>{".lua"},
                       "loaded Lua backend extensions should match manifest");
        context.expect(!backend->built_in, "official Lua backend should come from plugin, not core");
    }

    context.expect(manager.createRuntime("Lua") != nullptr, "loaded Lua backend should create a runtime");
}

void testHostCameraAndInputApiContract(TestContext& context)
{
    const LunaScriptHostApi& host_api = luna::ScriptPluginManager::instance().hostApi();

    context.expect(host_api.entity_has_camera != nullptr, "host API should expose entity_has_camera");
    context.expect(host_api.entity_get_camera != nullptr, "host API should expose entity_get_camera");
    context.expect(host_api.entity_set_camera != nullptr, "host API should expose entity_set_camera");
    context.expect(host_api.entity_set_camera_primary != nullptr, "host API should expose entity_set_camera_primary");
    context.expect(host_api.entity_set_perspective_camera != nullptr,
                   "host API should expose entity_set_perspective_camera");
    context.expect(host_api.entity_set_orthographic_camera != nullptr,
                   "host API should expose entity_set_orthographic_camera");
    context.expect(host_api.input_get_mouse_delta_x != nullptr, "host API should expose input_get_mouse_delta_x");
    context.expect(host_api.input_get_mouse_delta_y != nullptr, "host API should expose input_get_mouse_delta_y");
    context.expect(host_api.input_get_mouse_scroll_x != nullptr, "host API should expose input_get_mouse_scroll_x");
    context.expect(host_api.input_get_mouse_scroll_y != nullptr, "host API should expose input_get_mouse_scroll_y");
    context.expect(host_api.input_set_cursor_mode != nullptr, "host API should expose input_set_cursor_mode");
    context.expect(host_api.input_get_cursor_mode != nullptr, "host API should expose input_get_cursor_mode");
    context.expect(host_api.input_set_mouse_position != nullptr, "host API should expose input_set_mouse_position");
    context.expect(host_api.input_set_raw_mouse_motion != nullptr, "host API should expose input_set_raw_mouse_motion");

    luna::Scene scene;
    luna::Entity camera_entity = scene.entityManager().createEntity("Runtime Camera");
    camera_entity.addComponent<luna::CameraComponent>();
    camera_entity.getComponent<luna::CameraComponent>().primary = false;

    const uint64_t camera_id = static_cast<uint64_t>(camera_entity.getUUID());
    context.expect(host_api.entity_has_camera(&scene, camera_id) == 1,
                   "host camera API should detect a camera component");

    LunaScriptCameraDesc camera_desc{};
    context.expect(host_api.entity_get_camera(&scene, camera_id, &camera_desc) == 1,
                   "host camera API should read a camera component");
    context.expect(camera_desc.primary == 0, "host camera API should read primary state");
    context.expect(camera_desc.projection_type == LunaScriptCameraProjectionType_Perspective,
                   "host camera API should read projection type");

    camera_desc.primary = 1;
    camera_desc.fixed_aspect_ratio = 1;
    camera_desc.projection_type = LunaScriptCameraProjectionType_Orthographic;
    camera_desc.orthographic_size = 14.0f;
    camera_desc.orthographic_near = -25.0f;
    camera_desc.orthographic_far = 75.0f;
    context.expect(host_api.entity_set_camera(&scene, camera_id, &camera_desc) == 1,
                   "host camera API should write a camera component");

    const luna::CameraComponent& written_camera = camera_entity.getComponent<luna::CameraComponent>();
    context.expect(written_camera.primary, "host camera API should write primary state");
    context.expect(written_camera.fixedAspectRatio, "host camera API should write fixed-aspect state");
    context.expect(written_camera.projectionType == luna::Camera::ProjectionType::Orthographic,
                   "host camera API should write projection type");
    context.expect(written_camera.orthographicSize == 14.0f, "host camera API should write orthographic size");

    context.expect(host_api.entity_set_perspective_camera(&scene, camera_id, 1.0f, 0.1f, 250.0f) == 1,
                   "host camera API should switch to perspective projection");
    const luna::CameraComponent& perspective_camera = camera_entity.getComponent<luna::CameraComponent>();
    context.expect(perspective_camera.projectionType == luna::Camera::ProjectionType::Perspective,
                   "host camera API should set perspective projection type");
    context.expect(perspective_camera.perspectiveVerticalFovRadians == 1.0f,
                   "host camera API should set perspective FOV");
    context.expect(perspective_camera.perspectiveNear == 0.1f,
                   "host camera API should set perspective near clip");
    context.expect(perspective_camera.perspectiveFar == 250.0f,
                   "host camera API should set perspective far clip");

    luna::Entity plain_entity = scene.entityManager().createEntity("Plain");
    context.expect(host_api.entity_has_camera(&scene, static_cast<uint64_t>(plain_entity.getUUID())) == 0,
                   "host camera API should report missing camera components");
}

void testLuaRuntimeConstructorCallContract(TestContext& context)
{
    luna::ScriptPluginManager& manager = luna::ScriptPluginManager::instance();

    luna::ProjectInfo project;
    project.Scripting.SelectedPluginId = "luna.official.lua";
    project.Scripting.SelectedBackendName = "Lua";
    expectState(context,
                manager.resolveAndLoadProjectSelection(&project),
                luna::ScriptPluginSelectionState::Resolved,
                "official Lua plugin load for constructor call contract");

    constexpr uint64_t kLargeScriptHandle = 11073451620522104979ull;
    const luna::AssetHandle script_handle{kLargeScriptHandle};

    auto script_asset = std::make_shared<luna::ScriptAsset>();
    script_asset->language = "Lua";
    script_asset->source = R"(
local ConstructorSmoke = {}

function ConstructorSmoke:OnCreate()
    if type(self.script_asset) ~= "string" then
        error("large script_asset handles should be exposed as strings")
    end

    self.entity.rotation = Vec3(0.0, 0.0, 0.0)

    local camera = Camera()
    camera.primary = true
    camera.projection_type = CameraProjection.Orthographic
    camera.orthographic_size = 12.0
    self.entity:set_camera(camera)
end

function ConstructorSmoke:OnUpdate()
    self.entity:translate_world(Vec3(1.0, 2.0, 3.0))
end

return ConstructorSmoke
)";

    luna::AssetManager::get().registerMemoryAsset(script_handle, script_asset);

    luna::AssetMetadata metadata = luna::AssetDatabase::getAssetMetadata(script_handle);
    metadata.Name = "ConstructorSmoke";
    metadata.FilePath = "Assets/Scripts/ConstructorSmoke.lua";
    metadata.Type = luna::AssetType::Script;
    metadata.MemoryOnly = true;
    luna::AssetDatabase::set(script_handle, metadata);

    luna::Scene scene;
    scene.setName("LuaConstructorContract");
    luna::Entity entity = scene.entityManager().createEntity("Scripted Camera");
    entity.addComponent<luna::CameraComponent>();

    auto& script_component = entity.addComponent<luna::ScriptComponent>();
    luna::ScriptEntry script_entry{};
    script_entry.id = luna::UUID{42};
    script_entry.scriptAsset = script_handle;
    script_entry.typeName = "ConstructorSmoke";
    script_component.scripts.push_back(std::move(script_entry));

    std::unique_ptr<luna::IScriptRuntime> runtime = manager.createRuntime("Lua");
    if (!context.expect(runtime != nullptr, "Lua runtime should be created for constructor call contract")) {
        return;
    }

    if (!context.expect(runtime->initialize(), "Lua runtime should initialize for constructor call contract")) {
        return;
    }

    runtime->onRuntimeStart(scene);
    runtime->onUpdate(scene, luna::Timestep{1.0f});
    runtime->onRuntimeStop(scene);
    runtime->shutdown();

    context.expect(sameVec3(entity.transform().translation, glm::vec3{1.0f, 2.0f, 3.0f}),
                   "Vec3(...) should construct values accepted by entity movement APIs");

    const luna::CameraComponent& camera = entity.getComponent<luna::CameraComponent>();
    context.expect(camera.primary, "Camera() should construct values accepted by set_camera");
    context.expect(camera.projectionType == luna::Camera::ProjectionType::Orthographic,
                   "Camera() should preserve projection type");
    context.expect(std::fabs(camera.orthographicSize - 12.0f) <= 0.0001f,
                   "Camera() should preserve orthographic size");
}

void testLuaPropertySchemaMetadataContract(TestContext& context)
{
    luna::ScriptPluginManager& manager = luna::ScriptPluginManager::instance();

    luna::ProjectInfo project;
    project.Scripting.SelectedPluginId = "luna.official.lua";
    project.Scripting.SelectedBackendName = "Lua";
    expectState(context,
                manager.resolveAndLoadProjectSelection(&project),
                luna::ScriptPluginSelectionState::Resolved,
                "official Lua plugin load for property metadata contract");

    luna::ScriptSchemaRequest request{};
    request.assetName = "MetadataSmoke.lua";
    request.typeName = "MetadataSmoke";
    request.language = "Lua";
    request.source = R"(
local MetadataSmoke = {}

MetadataSmoke.Properties = {
    speed = {
        type = "Float",
        default = 2.5,
        display_name = "Move Speed",
        description = "Base movement speed.",
        category = "Movement",
        min = 0.0,
        max = 20.0,
        step = 0.25
    },
    capture_button = {
        type = "Int",
        default = MouseCode.Right,
        options = {
            { label = "Right Mouse", value = MouseCode.Right },
            { label = "Middle Mouse", value = MouseCode.Middle }
        }
    },
    mode = {
        type = "String",
        default = "walk",
        options = {
            { label = "Walk", value = "walk" },
            { label = "Fly", value = "fly" }
        }
    },
    camera = {
        type = "Entity",
        entity_filter = "Camera"
    },
    preview = {
        type = "Asset",
        asset_type = "Texture"
    }
}

return MetadataSmoke
)";

    const std::vector<luna::ScriptPropertySchema> schemas = manager.getPropertySchema("Lua", request);
    context.expect(schemas.size() == 5, "Lua schema metadata contract should enumerate all properties");

    auto find_schema = [&](std::string_view name) -> const luna::ScriptPropertySchema* {
        const auto it = std::find_if(schemas.begin(), schemas.end(), [&](const luna::ScriptPropertySchema& schema) {
            return schema.name == name;
        });
        return it != schemas.end() ? &*it : nullptr;
    };

    const luna::ScriptPropertySchema* speed = find_schema("speed");
    if (context.expect(speed != nullptr, "speed schema should exist")) {
        context.expect(speed->metadata.displayName == "Move Speed", "schema should preserve display_name");
        context.expect(speed->metadata.description == "Base movement speed.", "schema should preserve description");
        context.expect(speed->metadata.category == "Movement", "schema should preserve category");
        context.expect(speed->metadata.hasMinValue && speed->metadata.minValue == 0.0f,
                       "schema should preserve min metadata");
        context.expect(speed->metadata.hasMaxValue && speed->metadata.maxValue == 20.0f,
                       "schema should preserve max metadata");
        context.expect(speed->metadata.hasStepValue && speed->metadata.stepValue == 0.25f,
                       "schema should preserve step metadata");
        context.expect(speed->defaultValue.floatValue == 2.5f, "schema should preserve default float value");
    }

    const luna::ScriptPropertySchema* capture_button = find_schema("capture_button");
    if (context.expect(capture_button != nullptr, "capture_button schema should exist")) {
        context.expect(capture_button->metadata.options.size() == 2, "int options should be preserved");
        if (capture_button->metadata.options.size() == 2) {
            context.expect(capture_button->metadata.options[0].label == "Right Mouse",
                           "int option label should be preserved");
            context.expect(capture_button->metadata.options[0].intValue == 2,
                           "int option value should be preserved");
        }
    }

    const luna::ScriptPropertySchema* mode = find_schema("mode");
    if (context.expect(mode != nullptr, "mode schema should exist")) {
        context.expect(mode->metadata.options.size() == 2, "string options should be preserved");
        if (mode->metadata.options.size() == 2) {
            context.expect(mode->metadata.options[1].label == "Fly", "string option label should be preserved");
            context.expect(mode->metadata.options[1].stringValue == "fly",
                           "string option value should be preserved");
        }
    }

    const luna::ScriptPropertySchema* camera = find_schema("camera");
    if (context.expect(camera != nullptr, "camera schema should exist")) {
        context.expect(camera->metadata.entityFilter == "Camera", "entity filter should be preserved");
    }

    const luna::ScriptPropertySchema* preview = find_schema("preview");
    if (context.expect(preview != nullptr, "preview schema should exist")) {
        context.expect(preview->metadata.assetType == "Texture", "asset type filter should be preserved");
    }
}

void testPluginDllManifestContract(TestContext& context)
{
    constexpr std::string_view contract_extensions[] = {".contract"};

    TempDirectory temp("ScriptPluginDllContract");
    const std::filesystem::path project_plugins = temp.path() / "Plugins";

    writeContractPluginManifest(project_plugins,
                                "luna.test.contract-good",
                                "Contract Good",
                                "ContractLanguage",
                                "ContractBackend",
                                contract_extensions,
                                testPluginBinaryPath("LunaTestScriptPluginGood"));
    writeContractPluginManifest(project_plugins,
                                "luna.test.contract-backend-mismatch",
                                "Contract Backend Mismatch",
                                "ContractLanguage",
                                "ContractBackend",
                                contract_extensions,
                                testPluginBinaryPath("LunaTestScriptPluginBackendMismatch"));
    writeContractPluginManifest(project_plugins,
                                "luna.test.contract-language-mismatch",
                                "Contract Language Mismatch",
                                "ContractLanguage",
                                "ContractBackend",
                                contract_extensions,
                                testPluginBinaryPath("LunaTestScriptPluginLanguageMismatch"));
    writeContractPluginManifest(project_plugins,
                                "luna.test.contract-extension-mismatch",
                                "Contract Extension Mismatch",
                                "ContractLanguage",
                                "ContractBackend",
                                contract_extensions,
                                testPluginBinaryPath("LunaTestScriptPluginExtensionMismatch"));
    writeContractPluginManifest(project_plugins,
                                "luna.test.contract-empty-extensions",
                                "Contract Empty Extensions",
                                "ContractLanguage",
                                "ContractBackend",
                                contract_extensions,
                                testPluginBinaryPath("LunaTestScriptPluginEmptyExtensions"));

    luna::ScriptPluginManager& manager = luna::ScriptPluginManager::instance();
    manager.refreshDiscoveredPlugins(temp.path());

    luna::ProjectInfo project;
    project.Scripting.SelectedPluginId = "luna.test.contract-good";
    project.Scripting.SelectedBackendName = "ContractBackend";
    expectLoadState(context,
                    manager,
                    project,
                    luna::ScriptPluginSelectionState::Resolved,
                    "matching DLL/manifest contract should load");

    const luna::ScriptBackendDescriptor* good_backend = manager.findBackend("ContractBackend");
    context.expect(good_backend != nullptr, "matching DLL/manifest backend should be registered");
    if (good_backend != nullptr) {
        context.expect(good_backend->language == "ContractLanguage",
                       "matching DLL/manifest backend should keep language");
        context.expect(good_backend->supported_extensions == std::vector<std::string>{".contract"},
                       "matching DLL/manifest backend should keep supported extensions");
        context.expect(manager.createRuntime("ContractBackend") != nullptr,
                       "matching DLL/manifest backend should create a runtime");
    }

    project.Scripting.SelectedPluginId = "luna.test.contract-backend-mismatch";
    expectLoadState(context,
                    manager,
                    project,
                    luna::ScriptPluginSelectionState::BackendNotFound,
                    "DLL missing manifest backend should be rejected");
    context.expect(manager.findBackend("ContractBackend") == nullptr,
                   "failed plugin load should clear the previously loaded contract backend");

    project.Scripting.SelectedPluginId = "luna.test.contract-language-mismatch";
    expectLoadState(context,
                    manager,
                    project,
                    luna::ScriptPluginSelectionState::BackendNotFound,
                    "DLL language mismatch should be rejected");

    project.Scripting.SelectedPluginId = "luna.test.contract-extension-mismatch";
    expectLoadState(context,
                    manager,
                    project,
                    luna::ScriptPluginSelectionState::BackendNotFound,
                    "DLL supported extension mismatch should be rejected");

    project.Scripting.SelectedPluginId = "luna.test.contract-empty-extensions";
    expectLoadState(context,
                    manager,
                    project,
                    luna::ScriptPluginSelectionState::BackendNotFound,
                    "DLL with no supported extensions should be rejected");

    manager.refreshDiscoveredPlugins();
}

void testFailedPluginLoadClearsPreviouslyLoadedBackend(TestContext& context)
{
    constexpr std::string_view contract_extensions[] = {".contract"};

    TempDirectory temp("ScriptPluginLoadFailure");
    const std::filesystem::path project_plugins = temp.path() / "Plugins";

    writeContractPluginManifest(project_plugins,
                                "luna.test.contract-language-mismatch",
                                "Contract Language Mismatch",
                                "ContractLanguage",
                                "ContractBackend",
                                contract_extensions,
                                testPluginBinaryPath("LunaTestScriptPluginLanguageMismatch"));

    luna::ScriptPluginManager& manager = luna::ScriptPluginManager::instance();
    manager.refreshDiscoveredPlugins();

    luna::ProjectInfo project;
    project.Scripting.SelectedPluginId = "luna.official.lua";
    project.Scripting.SelectedBackendName = "Lua";
    expectLoadState(context,
                    manager,
                    project,
                    luna::ScriptPluginSelectionState::Resolved,
                    "official Lua plugin should load before failure test");
    context.expect(manager.findBackend("Lua") != nullptr, "Lua backend should be registered before failed load");

    manager.refreshDiscoveredPlugins(temp.path());
    project.Scripting.SelectedPluginId = "luna.test.contract-language-mismatch";
    project.Scripting.SelectedBackendName = "ContractBackend";
    expectLoadState(context,
                    manager,
                    project,
                    luna::ScriptPluginSelectionState::BackendNotFound,
                    "loading a bad plugin after Lua should fail");

    context.expect(manager.findBackend("Lua") == nullptr,
                   "failed plugin load should not leave the previously loaded Lua backend active");
    context.expect(manager.findBackend("ContractBackend") == nullptr,
                   "failed plugin load should not register the bad backend");

    manager.refreshDiscoveredPlugins();
}

void testLuaScriptImportWritesLanguageMetadata(TestContext& context)
{
    TempDirectory temp("LuaScriptImport");
    const std::filesystem::path project_root = temp.path() / "LuaProject";
    createTestProject(context, project_root, "LuaScriptImportProject", "luna.official.lua", "Lua");

    const std::filesystem::path script_path = project_root / "Assets" / "Player.lua";
    writeTextFile(script_path, "return { value = 42 }\n");

    luna::AssetDatabase::clear();
    const luna::ImporterManager::ImportStats stats = luna::ImporterManager::syncProjectAssets();

    context.expect(stats.discoveredAssets == 1, "Lua script import should discover one supported asset");
    context.expect(stats.importedMissingAssets == 1, "Lua script import should create missing metadata");
    context.expect(stats.scriptFilesSkippedNoPlugin == 0, "Lua script import should not skip for missing plugin");
    context.expect(stats.scriptFilesSkippedUnsupportedLanguage == 0,
                   "Lua script import should not skip for unsupported script language");

    const std::filesystem::path meta_path = project_root / "Assets" / "Player.lua.meta";
    context.expect(metadataFileExists(script_path), "Lua script import should write metadata file");
    context.expect(readMetadataLanguage(meta_path) == "Lua", "Lua script metadata should declare Language=Lua");

    const std::shared_ptr<luna::ScriptAsset> script_asset = loadScriptAssetFromMetadata(meta_path);
    context.expect(script_asset != nullptr, "Lua script loader should load imported metadata");
    if (script_asset != nullptr) {
        context.expect(script_asset->language == "Lua", "loaded Lua script asset should keep Lua language");
        context.expect(script_asset->source == "return { value = 42 }\n",
                       "loaded Lua script asset should read script source");
    }

    luna::AssetDatabase::clear();
}

void testScriptImportRebuildsWrongLanguageMetadata(TestContext& context)
{
    TempDirectory temp("LuaScriptImportRebuild");
    const std::filesystem::path project_root = temp.path() / "LuaProject";
    createTestProject(context, project_root, "LuaScriptImportRebuildProject", "luna.official.lua", "Lua");

    const std::filesystem::path relative_script_path = std::filesystem::path("Assets") / "Player.lua";
    const std::filesystem::path script_path = project_root / relative_script_path;
    writeTextFile(script_path, "return {}\n");
    writeScriptMetadata(project_root, relative_script_path, "Python");

    luna::AssetDatabase::clear();
    const luna::ImporterManager::ImportStats stats = luna::ImporterManager::syncProjectAssets();

    context.expect(stats.discoveredAssets == 1, "wrong-language Lua metadata should still be discovered");
    context.expect(stats.rebuiltMetadata == 1, "wrong-language Lua metadata should be rebuilt");
    context.expect(readMetadataLanguage(project_root / "Assets" / "Player.lua.meta") == "Lua",
                   "rebuilt Lua script metadata should declare selected project language");

    luna::AssetDatabase::clear();
}

void testScriptImportSkipsWhenNoPluginSelected(TestContext& context)
{
    TempDirectory temp("ScriptImportNoPlugin");
    const std::filesystem::path project_root = temp.path() / "NoScriptProject";
    createTestProject(context, project_root, "ScriptImportNoPluginProject", "", "");

    const std::filesystem::path script_path = project_root / "Assets" / "Player.lua";
    writeTextFile(script_path, "return {}\n");

    luna::AssetDatabase::clear();
    const luna::ImporterManager::ImportStats stats = luna::ImporterManager::syncProjectAssets();

    context.expect(stats.discoveredAssets == 0, "script import without selection should not discover Lua as supported");
    context.expect(stats.importedMissingAssets == 0,
                   "script import without selection should not create script metadata");
    context.expect(stats.scriptFilesSkippedNoPlugin == 1,
                   "script import without selection should count skipped script file");
    context.expect(!metadataFileExists(script_path),
                   "script import without selection should not write script metadata");

    writeScriptMetadata(project_root, std::filesystem::path("Assets") / "Player.lua", "Lua");
    context.expect(loadScriptAssetFromMetadata(project_root / "Assets" / "Player.lua.meta") == nullptr,
                   "script loader should reject script asset when no project plugin is selected");

    luna::AssetDatabase::clear();
}

void testScriptImportSkipsOtherScriptLanguageExtensions(TestContext& context)
{
    constexpr std::array<std::string_view, 1> contract_extensions{".contract"};

    TempDirectory temp("ScriptImportMixedLanguage");
    const std::filesystem::path project_root = temp.path() / "LuaProject";
    writeContractPluginManifest(project_root / "Plugins",
                                "luna.test.contract-good",
                                "Contract Good",
                                "ContractLanguage",
                                "ContractBackend",
                                contract_extensions,
                                testPluginBinaryPath("LunaTestScriptPluginGood"));

    createTestProject(context, project_root, "ScriptImportMixedLanguageProject", "luna.official.lua", "Lua");

    const std::filesystem::path script_path = project_root / "Assets" / "Tool.contract";
    writeTextFile(script_path, "contract script\n");

    luna::AssetDatabase::clear();
    const luna::ImporterManager::ImportStats stats = luna::ImporterManager::syncProjectAssets();

    context.expect(stats.discoveredAssets == 0,
                   "Lua project should not discover another script language extension as supported");
    context.expect(stats.scriptFilesSkippedUnsupportedLanguage == 1,
                   "Lua project should count skipped file from another discovered script language");
    context.expect(!metadataFileExists(script_path),
                   "Lua project should not write metadata for another script language extension");

    writeScriptMetadata(project_root, std::filesystem::path("Assets") / "Tool.contract", "ContractLanguage");
    context.expect(loadScriptAssetFromMetadata(project_root / "Assets" / "Tool.contract.meta") == nullptr,
                   "script loader should reject metadata language that differs from selected Lua project language");

    luna::AssetDatabase::clear();
    luna::ScriptPluginManager::instance().refreshDiscoveredPlugins();
}

} // namespace

int main()
{
    luna::Logger::init("", luna::Logger::Level::Warn);

    TestContext context;
    testDiscoveryManifestContract(context);
    testProjectSelectionContract(context);
    testSelectionRejectsAmbiguousBackendAndHostMismatch(context);
    testOfficialLuaPluginLoadContract(context);
    testHostCameraAndInputApiContract(context);
    testLuaRuntimeConstructorCallContract(context);
    testLuaPropertySchemaMetadataContract(context);
    testPluginDllManifestContract(context);
    testFailedPluginLoadClearsPreviouslyLoadedBackend(context);
    testLuaScriptImportWritesLanguageMetadata(context);
    testScriptImportRebuildsWrongLanguageMetadata(context);
    testScriptImportSkipsWhenNoPluginSelected(context);
    testScriptImportSkipsOtherScriptLanguageExtensions(context);

    luna::Logger::shutdown();
    return context.result();
}
