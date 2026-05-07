#pragma once

#include "Asset/Asset.h"
#include "Core/UUID.h"

#include <cstdint>

#include <glm/vec3.hpp>
#include <string>
#include <vector>

namespace luna {

enum class ScriptPropertyType : uint8_t {
    Bool,
    Int,
    Float,
    String,
    Vec3,
    Entity,
    Asset,
};

struct ScriptProperty {
    std::string name;
    ScriptPropertyType type{ScriptPropertyType::Float};
    bool boolValue{false};
    int intValue{0};
    float floatValue{0.0f};
    std::string stringValue;
    glm::vec3 vec3Value{0.0f};
    UUID entityValue{0};
    AssetHandle assetValue{0};
};

struct ScriptEntry {
    UUID id;
    bool enabled{true};
    AssetHandle scriptAsset{0};
    std::string typeName;
    int executionOrder{0};
    std::vector<ScriptProperty> properties;
};

struct ScriptComponent {
    bool enabled{true};
    std::vector<ScriptEntry> scripts;
};

} // namespace luna
