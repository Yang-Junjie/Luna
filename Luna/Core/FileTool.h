#pragma once

#include <filesystem>
#include <string>

namespace luna::tools {
inline std::string makeRelative(const std::filesystem::path& path, const std::filesystem::path& baseDir)
{
    if (path.empty()) {
        return {};
    }

    std::error_code ec;
    auto rel = std::filesystem::relative(path, baseDir, ec);
    if (ec) {
        return path.generic_string();
    }

    return rel.generic_string();
}

inline std::filesystem::path makeAbsolute(const std::string& stored, const std::filesystem::path& baseDir)
{
    if (stored.empty()) {
        return {};
    }

    std::filesystem::path p = stored;
    if (p.is_absolute()) {
        return p.lexically_normal();
    }

    return (baseDir / p).lexically_normal();
}
} // namespace luna::tools
