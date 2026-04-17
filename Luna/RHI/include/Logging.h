#ifndef CACAO_LOGGING_H
#define CACAO_LOGGING_H

#include "Core.h"

#include <string_view>

namespace Cacao {

enum class LogLevel : uint8_t {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
};

using LogCallback = void (*)(LogLevel level, std::string_view message, void* userData);

CACAO_API void SetLogCallback(LogCallback callback, void* userData = nullptr);
CACAO_API void LogMessage(LogLevel level, std::string_view message);
CACAO_API void SetVulkanValidationMessageFilterEnabled(bool enabled);
CACAO_API bool IsVulkanValidationMessageFilterEnabled();

} // namespace Cacao

#endif
