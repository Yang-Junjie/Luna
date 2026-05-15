#pragma once

#include "Asset/Asset.h"
#include "EditorEnginePaths.h"
#include "EditorApi/EditorNativePluginApi.h"
#include "EditorApi/EditorPlugin.h"
#include "Platform/Common/DynamicLibrary.h"
#include "Shell/EditorBuiltinPluginRegistry.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace luna::editor {

class EditorShell;
struct NativePluginContext;

enum class EditorPluginRuntime {
    BuiltinNative,
    Native,
    Lua,
};

struct EditorPluginPackage {
    std::string id;
    std::string display_name;
    std::string version;
    std::filesystem::path root_path;
    std::filesystem::path entry_path;
    std::filesystem::path resolved_entry_path;
    EditorPluginRuntime runtime{EditorPluginRuntime::BuiltinNative};
    std::vector<std::string> dependencies;
    bool enabled{true};
    bool entry_exists{false};
    EditorBuiltinPluginFactory create;
};

class EditorPluginManager final {
public:
    explicit EditorPluginManager(EditorShell& shell);
    ~EditorPluginManager();

    void registerPackage(EditorPluginPackage package);
    bool loadRegisteredPackages();
    void unloadAll();

    [[nodiscard]] const std::vector<EditorPluginPackage>& packages() const noexcept;

private:
    bool loadPackage(EditorPluginPackage& package);
    bool loadBuiltinPackage(EditorPluginPackage& package);
    bool loadNativePackage(EditorPluginPackage& package);

    struct NativePluginInstance {
        EditorPluginPackage package;
        std::shared_ptr<DynamicLibrary> library;
        std::shared_ptr<NativePluginContext> context;
        std::shared_ptr<LunaEditorHostApi> host_api;
        LunaEditorPluginApi plugin_api{};
    };

    EditorShell& m_shell;
    std::vector<EditorPluginPackage> m_packages;
    std::vector<NativePluginInstance> m_native_plugins;
};

std::vector<EditorPluginPackage> createEditorPluginPackages(const EditorEnginePaths& engine_paths);
std::vector<EditorPluginPackage> createEditorPluginPackages();

} // namespace luna::editor
