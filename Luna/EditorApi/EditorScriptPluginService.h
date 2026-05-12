#pragma once

#include "Script/ScriptPluginManifest.h"

#include <string>
#include <vector>

namespace luna::editor {

class ScriptPluginService {
public:
    virtual ~ScriptPluginService() = default;

    virtual void refreshProjectScriptPlugins() = 0;
    [[nodiscard]] virtual const std::vector<ScriptPluginCandidate>& getDiscoveredScriptPlugins() const = 0;
    [[nodiscard]] virtual const std::string& getScriptPluginStatus() const = 0;
    [[nodiscard]] virtual const ScriptPluginCandidate* getSelectedScriptPluginCandidate() const = 0;
    virtual bool selectScriptPlugin(const ScriptPluginCandidate* candidate) = 0;
};

} // namespace luna::editor
