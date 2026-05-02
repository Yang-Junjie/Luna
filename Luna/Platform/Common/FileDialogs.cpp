#include "Platform/Common/FileDialogs.h"

#include "Core/Log.h"

namespace luna::FileDialogs {

std::filesystem::path openFile(const char*, const std::string&)
{
    LUNA_PLATFORM_WARN("Native open file dialog is not implemented on this platform");
    return {};
}

std::filesystem::path saveFile(const char*, const std::string&)
{
    LUNA_PLATFORM_WARN("Native save file dialog is not implemented on this platform");
    return {};
}

std::filesystem::path selectDirectory(const std::string&)
{
    LUNA_PLATFORM_WARN("Native select directory dialog is not implemented on this platform");
    return {};
}

} // namespace luna::FileDialogs
