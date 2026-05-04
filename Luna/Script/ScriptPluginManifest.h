#pragma once

#include "ScriptHostApi.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace luna {

enum class ScriptPluginScope {
    Engine,
    Project,
};

struct ScriptPluginManifest {
    std::string PluginId;
    std::string DisplayName;
    std::string Type;
    std::string Language;
    std::string BackendName;
    std::string Version;
    std::vector<std::string> SupportedExtensions;
    uint32_t HostApiVersion{LUNA_SCRIPT_HOST_API_VERSION};
    std::filesystem::path Entry;
};

struct ScriptPluginCandidate {
    ScriptPluginManifest Manifest;
    std::filesystem::path ManifestPath;
    std::filesystem::path PluginRootPath;
    std::filesystem::path ResolvedEntryPath;
    ScriptPluginScope Scope{ScriptPluginScope::Engine};
    bool EntryExists{false};
};

} // namespace luna
