#pragma once

#include "Scene/Components/ScriptComponent.h"

#include <string>
#include <vector>

namespace luna {

struct ScriptPropertySchema {
    std::string name;
    std::string displayName;
    std::string description;
    ScriptPropertyType type{ScriptPropertyType::Float};
    ScriptProperty defaultValue;
};

struct ScriptSchemaRequest {
    std::string assetName;
    std::string typeName;
    std::string language;
    std::string source;
};

} // namespace luna
