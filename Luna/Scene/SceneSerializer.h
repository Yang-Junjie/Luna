#pragma once

#include <filesystem>

namespace luna {

class Scene;

class SceneSerializer {
public:
    static constexpr const char* FileExtension = ".lunascene";

    static std::filesystem::path normalizeScenePath(const std::filesystem::path& scene_path);
    static bool serialize(const Scene& scene, const std::filesystem::path& scene_path);
    static bool deserialize(Scene& scene, const std::filesystem::path& scene_path);
};

} // namespace luna
