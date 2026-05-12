#pragma once

#include "EditorApi/EditorSceneService.h"
#include "EditorApi/EditorTypes.h"

#include <string>
#include <string_view>
#include <vector>

namespace luna::editor {

struct ScriptLanguageStatus {
    bool available{false};
    std::string language;
    std::string message;
};

struct ScriptAssetValidation {
    bool accepted{false};
    std::string language;
    std::string message;
};

struct ScriptSchemaSyncResult {
    bool success{false};
    std::string message;
    std::vector<SceneScriptProperty> properties;
};

class ScriptService {
public:
    virtual ~ScriptService() = default;

    [[nodiscard]] virtual ScriptLanguageStatus projectScriptLanguage() const = 0;
    [[nodiscard]] virtual ScriptAssetValidation validateScriptAsset(AssetHandle script_asset) const = 0;
    [[nodiscard]] virtual ScriptSchemaSyncResult syncScriptProperties(const SceneScriptEntry& script) const = 0;
};

} // namespace luna::editor
