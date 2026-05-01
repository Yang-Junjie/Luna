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
    [[nodiscard]] std::string defaultBackendName() const;
    [[nodiscard]] std::unique_ptr<IScriptRuntime> createRuntime(std::string_view backend_name) const;
    [[nodiscard]] std::unique_ptr<IScriptRuntime> createRuntimeForProject(const ProjectInfo* project_info) const;
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
