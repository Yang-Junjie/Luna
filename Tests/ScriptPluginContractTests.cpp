#include "Asset/AssetDatabase.h"
#include "Core/Log.h"
#include "Project/ProjectInfo.h"
#include "Project/ProjectManager.h"
#include "Asset/Editor/ImporterManager.h"
#include "Asset/Editor/ScriptLoader.h"
#include "Script/ScriptPluginDiscovery.h"
#include "Script/ScriptPluginManager.h"
#include "Script/ScriptAsset.h"
#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <array>
#include <chrono>
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
    testPluginDllManifestContract(context);
    testFailedPluginLoadClearsPreviouslyLoadedBackend(context);
    testLuaScriptImportWritesLanguageMetadata(context);
    testScriptImportRebuildsWrongLanguageMetadata(context);
    testScriptImportSkipsWhenNoPluginSelected(context);
    testScriptImportSkipsOtherScriptLanguageExtensions(context);

    luna::Logger::shutdown();
    return context.result();
}
