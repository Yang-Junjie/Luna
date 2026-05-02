#pragma once
#include "Asset.h"
#include "yaml-cpp/yaml.h"

#include <filesystem>

namespace luna {
struct AssetMetadata {
    AssetHandle Handle{0};
    AssetType Type{AssetType::None};
    std::string Name;

    // relative project root path
    std::filesystem::path FilePath;

    bool MemoryOnly = false;

    YAML::Node SpecializedConfig;

    template <typename T> T GetConfig(const std::string& key, T defaultValue = T()) const
    {
        try {
            if (SpecializedConfig && SpecializedConfig[key]) {
                return SpecializedConfig[key].as<T>();
            }
        } catch (const YAML::Exception&) {
        }
        return defaultValue;
    }
};
} // namespace luna
