#ifndef LUNA_RHI_LOGGING_H
#define LUNA_RHI_LOGGING_H

#include "Core.h"

#include <string_view>

namespace luna::RHI {

enum class LogLevel : uint8_t {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
};

using LogCallback = void (*)(LogLevel level, std::string_view message, void* userData);

LUNA_RHI_API void SetLogCallback(LogCallback callback, void* userData = nullptr);
LUNA_RHI_API void LogMessage(LogLevel level, std::string_view message);
LUNA_RHI_API void SetVulkanValidationMessageFilterEnabled(bool enabled);
LUNA_RHI_API bool IsVulkanValidationMessageFilterEnabled();

} // namespace luna::RHI

#endif
