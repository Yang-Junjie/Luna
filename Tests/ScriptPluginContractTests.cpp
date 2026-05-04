#include "Core/Log.h"
#include "Project/ProjectInfo.h"
#include "Script/ScriptPluginDiscovery.h"
#include "Script/ScriptPluginManager.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
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

} // namespace

int main()
{
    luna::Logger::init("", luna::Logger::Level::Warn);

    TestContext context;
    testDiscoveryManifestContract(context);
    testProjectSelectionContract(context);
    testSelectionRejectsAmbiguousBackendAndHostMismatch(context);
    testOfficialLuaPluginLoadContract(context);

    luna::Logger::shutdown();
    return context.result();
}
