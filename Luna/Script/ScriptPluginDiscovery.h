#pragma once

#include "ScriptPluginManifest.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace luna {

class ScriptPluginDiscovery {
public:
    struct Options {
        std::filesystem::path EnginePluginsRoot;
        std::optional<std::filesystem::path> ProjectPluginsRoot;
    };

    [[nodiscard]] std::vector<ScriptPluginCandidate> discover(const Options& options) const;
};

} // namespace luna
