#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace luna::editor {

enum class PluginRuntimeKind {
    BuiltinNative,
    Native,
    Lua,
};

enum class PluginSourceKind {
    Unknown,
    Official,
    Installed,
    Development,
};

enum class PluginLoadState {
    Registered,
    Loaded,
    Disabled,
    Failed,
};

struct PluginInfo {
    std::string id;
    std::string display_name;
    std::string version;
    PluginRuntimeKind runtime{PluginRuntimeKind::BuiltinNative};
    PluginSourceKind source{PluginSourceKind::Unknown};
    PluginLoadState state{PluginLoadState::Registered};
    std::filesystem::path root_path;
    std::filesystem::path entry_path;
    std::filesystem::path resolved_entry_path;
    std::vector<std::string> dependencies;
    bool enabled{true};
    bool entry_exists{false};
    std::string status;
};

class PluginService {
public:
    virtual ~PluginService() = default;

    [[nodiscard]] virtual std::vector<PluginInfo> plugins() const = 0;
};

} // namespace luna::editor
