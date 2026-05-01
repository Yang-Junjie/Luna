#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace luna {

class Scene;

class SceneSerializer {
public:
    static constexpr const char* FileExtension = ".lunascene";

    static std::filesystem::path normalizeScenePath(const std::filesystem::path& scene_path);
    static std::string serializeToString(const Scene& scene);
    static bool deserializeFromString(Scene& scene, std::string_view scene_data, std::string_view source_name = {});
    static bool serialize(const Scene& scene, const std::filesystem::path& scene_path);
    static bool deserialize(Scene& scene, const std::filesystem::path& scene_path);
};

} // namespace luna
