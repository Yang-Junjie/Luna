#pragma once

#include "Scene/Components/ScriptComponent.h"

#include <string>
#include <vector>

namespace luna {

struct ScriptPropertySchema {
    std::string name;
    ScriptPropertyType type{ScriptPropertyType::Float};
    ScriptProperty defaultValue;
    ScriptPropertyMetadata metadata;
};

struct ScriptSchemaRequest {
    std::string assetName;
    std::string typeName;
    std::string language;
    std::string source;
};

} // namespace luna
