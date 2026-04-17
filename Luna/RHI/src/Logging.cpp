#include "Logging.h"

#include <atomic>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>

namespace luna::RHI {
namespace {

std::mutex g_log_callback_mutex;
LogCallback g_log_callback = nullptr;
void* g_log_callback_user_data = nullptr;

bool parseBoolean(std::string value, bool default_value)
{
    if (value.empty()) {
        return default_value;
    }

    for (char& character : value) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }

    if (value == "1" || value == "true" || value == "on" || value == "yes") {
        return true;
    }
    if (value == "0" || value == "false" || value == "off" || value == "no") {
        return false;
    }

    return default_value;
}

std::string readEnvironmentVariable(const char* name)
{
#if defined(_WIN32)
    char* value = nullptr;
    size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) {
        return {};
    }

    std::string result(value);
    free(value);
    return result;
#else
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return {};
    }

    return std::string(value);
#endif
}

std::atomic_bool& validationMessageFilterStorage()
{
    static std::atomic_bool enabled{
        parseBoolean(readEnvironmentVariable("LUNA_VK_VALIDATION_FILTER"), true)};
    return enabled;
}

const char* toString(LogLevel level)
{
    switch (level) {
        case LogLevel::Trace:
            return "TRACE";
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warn:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Fatal:
            return "FATAL";
        default:
            return "INFO";
    }
}

} // namespace

void SetLogCallback(LogCallback callback, void* userData)
{
    std::lock_guard<std::mutex> lock(g_log_callback_mutex);
    g_log_callback = callback;
    g_log_callback_user_data = userData;
}

void LogMessage(LogLevel level, std::string_view message)
{
    LogCallback callback = nullptr;
    void* user_data = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_log_callback_mutex);
        callback = g_log_callback;
        user_data = g_log_callback_user_data;
    }

    if (callback != nullptr) {
        callback(level, message, user_data);
        return;
    }

    std::cerr << "[Luna RHI] [" << toString(level) << "] " << message << '\n';
}

void SetVulkanValidationMessageFilterEnabled(bool enabled)
{
    validationMessageFilterStorage().store(enabled, std::memory_order_relaxed);
}

bool IsVulkanValidationMessageFilterEnabled()
{
    return validationMessageFilterStorage().load(std::memory_order_relaxed);
}

} // namespace luna::RHI
