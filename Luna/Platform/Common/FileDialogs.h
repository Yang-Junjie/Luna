#pragma once
#include <filesystem>
#include <string>

namespace luna {
namespace FileDialogs {

std::filesystem::path openFile(const char* filter, const std::string& defaultPath);
std::filesystem::path saveFile(const char* filter, const std::string& defaultPath);
std::filesystem::path selectDirectory(const std::string& defaultPath);

} // namespace FileDialogs
} // namespace luna
