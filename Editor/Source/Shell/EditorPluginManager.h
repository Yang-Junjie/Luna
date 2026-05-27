#pragma once

#include "Asset/Asset.h"
#include "EditorApi/EditorNativePluginApi.h"
#include "EditorApi/EditorPlugin.h"
#include "EditorApi/EditorPluginService.h"
#include "EditorEnginePaths.h"
#include "Platform/Common/DynamicLibrary.h"
#include "Shell/EditorBuiltinPluginRegistry.h"
#include "Shell/EditorPluginManagerHost.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace luna::editor {

struct NativePluginContext;

enum class EditorPluginRuntime {
    BuiltinNative,
    Native,
    Lua,
};

enum class EditorPluginSource {
    Unknown,
    Official,
    Installed,
    Development,
};

enum class EditorPluginCategory {
    Core,
    Tool,
    Diagnostics,
};

struct EditorPluginPackage {
    std::string id;
    std::string display_name;
    std::string version;
    std::filesystem::path root_path;
    std::filesystem::path entry_path;
    std::filesystem::path resolved_entry_path;
    EditorPluginRuntime runtime{EditorPluginRuntime::BuiltinNative};
    EditorPluginSource source{EditorPluginSource::Unknown};
    EditorPluginCategory category{EditorPluginCategory::Tool};
    std::vector<std::string> dependencies;
    bool enabled{true};
    bool entry_exists{false};
    EditorBuiltinPluginFactory create;
};

enum class EditorPluginLoadState {
    Registered,
    Loaded,
    Disabled,
    Failed,
};

struct EditorPluginDiagnostic {
    EditorPluginPackage package;
    EditorPluginLoadState state{EditorPluginLoadState::Registered};
    std::string status;
    bool load_candidate{true};
};

class EditorPluginManager final {
public:
    explicit EditorPluginManager(EditorPluginManagerHost& shell);
    ~EditorPluginManager();

    void registerPackage(EditorPluginPackage package);
    bool loadRegisteredPackages();
    void unloadAll();

    [[nodiscard]] const std::vector<EditorPluginPackage>& packages() const noexcept;
    [[nodiscard]] std::vector<PluginInfo> pluginInfos() const;

private:
    bool loadPackage(EditorPluginPackage& package, std::string& failure_status);
    bool loadBuiltinPackage(EditorPluginPackage& package, std::string& failure_status);
    bool loadNativePackage(EditorPluginPackage& package, std::string& failure_status);
    void rebuildDiagnosticPackages();

    struct NativePluginInstance {
        EditorPluginPackage package;
        std::shared_ptr<DynamicLibrary> library;
        std::shared_ptr<NativePluginContext> context;
        std::shared_ptr<LunaEditorHostApi> host_api;
        LunaEditorPluginApi plugin_api{};
    };

    EditorPluginManagerHost& m_shell;
    std::vector<EditorPluginPackage> m_packages;
    std::vector<EditorPluginDiagnostic> m_diagnostics;
    std::vector<NativePluginInstance> m_native_plugins;
};

std::vector<EditorPluginPackage> createEditorPluginPackages(const EditorEnginePaths& engine_paths);
std::vector<EditorPluginPackage> createEditorPluginPackages();

} // namespace luna::editor
