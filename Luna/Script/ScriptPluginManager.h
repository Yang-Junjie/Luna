#pragma once

#include "Project/ProjectInfo.h"
#include "ScriptBackend.h"
#include "ScriptPluginManifest.h"
#include "ScriptHostApi.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace luna {

enum class ScriptPluginSelectionState {
    Unresolved,
    NoProject,
    NoPluginsDiscovered,
    MissingSelection,
    PluginNotFound,
    BackendNotFound,
    BackendAmbiguous,
    BackendMismatch,
    HostApiMismatch,
    Resolved,
};

struct ScriptPluginSelectionResult {
    ScriptPluginSelectionState State{ScriptPluginSelectionState::Unresolved};
    const ScriptPluginCandidate* Candidate{nullptr};
    std::string BackendName;
    std::string StatusMessage;
    bool ExplicitSelection{false};
    bool AutoSelected{false};

    [[nodiscard]] bool isResolved() const noexcept
    {
        return State == ScriptPluginSelectionState::Resolved;
    }
};

class ScriptPluginManager {
public:
    static ScriptPluginManager& instance();

    ScriptPluginManager(const ScriptPluginManager&) = delete;
    ScriptPluginManager& operator=(const ScriptPluginManager&) = delete;

    [[nodiscard]] const ScriptBackendDescriptor* findBackend(std::string_view backend_name) const;
    [[nodiscard]] std::vector<ScriptBackendDescriptor> getBackends() const;
    void refreshDiscoveredPlugins(std::optional<std::filesystem::path> project_root_path = std::nullopt);
    [[nodiscard]] const std::vector<ScriptPluginCandidate>& getDiscoveredPlugins() const noexcept;
    [[nodiscard]] const ScriptPluginCandidate* findDiscoveredPlugin(std::string_view plugin_id) const;
    [[nodiscard]] ScriptPluginSelectionResult resolveProjectSelection(const ProjectInfo* project_info) const;
    [[nodiscard]] ScriptPluginSelectionResult resolveAndLoadProjectSelection(const ProjectInfo* project_info);
    [[nodiscard]] std::vector<ScriptPropertySchema> getPropertySchema(std::string_view backend_name,
                                                                       const ScriptSchemaRequest& request) const;
    [[nodiscard]] std::vector<ScriptPropertySchema> getPropertySchemaForProject(const ProjectInfo* project_info,
                                                                                 const ScriptSchemaRequest& request);
    [[nodiscard]] std::unique_ptr<IScriptRuntime> createRuntime(std::string_view backend_name) const;
    [[nodiscard]] std::unique_ptr<IScriptRuntime> createRuntimeForProject(const ProjectInfo* project_info);
    bool registerBackend(std::unique_ptr<IScriptBackend> backend);
    [[nodiscard]] const LunaScriptHostApi& hostApi() const noexcept;

private:
    ScriptPluginManager();
    bool ensurePluginLoaded(const ScriptPluginCandidate* candidate);
    void unloadActivePlugin();
    void registerBuiltinBackends();

private:
    LunaScriptHostApi m_host_api{};
    std::unordered_map<std::string, std::unique_ptr<IScriptBackend>> m_builtin_backends;
    std::unordered_map<std::string, std::unique_ptr<IScriptBackend>> m_plugin_backends;
    std::vector<ScriptPluginCandidate> m_discovered_plugins;
    std::string m_active_plugin_id;
};

} // namespace luna
