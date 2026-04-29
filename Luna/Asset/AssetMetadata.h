#pragma once
#include "Asset.h"
#include "yaml-cpp/yaml.h"

#include <filesystem>

namespace luna {
struct AssetMetadata {
    AssetHandle Handle;
    AssetType Type;
    std::string Name;

    // relative project root path
    std::filesystem::path FilePath;

    bool MemoryOnly = false;

    YAML::Node SpecializedConfig;

    template <typename T> T GetConfig(const std::string& key, T defaultValue = T()) const
    {
        if (SpecializedConfig[key]) {
            return SpecializedConfig[key].as<T>();
        }
        return defaultValue;
    }
};
} // namespace luna
