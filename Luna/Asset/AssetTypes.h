#pragma once
#include <string>

namespace luna {
enum class AssetType {
    None = 0,
    Texture,
    Mesh,
    Material,
    Model,
};

namespace AssetUtils {
inline const char* AssetTypeToString(AssetType type)
{
    switch (type) {
        case AssetType::None:
            return "None";
        case AssetType::Texture:
            return "Texture";
        case AssetType::Mesh:
            return "Mesh";
        case AssetType::Material:
            return "Material";
        case AssetType::Model:
            return "Model";
    }
    return "None";
}

inline AssetType StringToAssetType(const std::string& str)
{
    if (str == "Texture") {
        return AssetType::Texture;
    }
    if (str == "Mesh") {
        return AssetType::Mesh;
    }
    if (str == "Material") {
        return AssetType::Material;
    }
    if (str == "Model") {
        return AssetType::Model;
    }
    return AssetType::None;
}
} // namespace AssetUtils
} // namespace luna
