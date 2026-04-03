#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace luna::paths {

inline std::filesystem::path project_root()
{
    return std::filesystem::path{LUNA_PROJECT_ROOT}.lexically_normal();
}

inline std::filesystem::path shader_root()
{
    return std::filesystem::path{LUNA_SHADER_ROOT}.lexically_normal();
}

inline std::filesystem::path asset_root()
{
    return std::filesystem::path{LUNA_ASSET_ROOT}.lexically_normal();
}

inline std::filesystem::path shader(std::string_view relativePath)
{
    return (shader_root() / relativePath).lexically_normal();
}

inline std::filesystem::path asset(std::string_view relativePath)
{
    return (asset_root() / relativePath).lexically_normal();
}

inline std::string display(const std::filesystem::path& path)
{
    return path.lexically_normal().generic_string();
}

} // namespace luna::paths
