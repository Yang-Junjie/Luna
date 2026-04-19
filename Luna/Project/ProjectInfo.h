#pragma once
#include <filesystem>
#include <string>

namespace luna {
struct ProjectInfo {
    std::string Name{"Unknown"};
    std::string Version{"0.1.0"};
    std::string Author{"Unknown"};
    std::string Description{"A simple Luna project."};

    // relative to project root
    std::filesystem::path StartScene;
    std::filesystem::path AssetsPath;
};
} // namespace luna
