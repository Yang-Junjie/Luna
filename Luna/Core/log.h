#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

namespace luna {

class Logger {
public:
    enum class Type : uint8_t {
        Core = 0,
        Runtime = 1,
        Editor = 2
    };

    enum class Level : uint8_t {
        Trace = 0,
        Debug,
        Info,
        Warn,
        Error,
        Fatal,
        Off
    };

    static void init(const std::string& log_file = {}, Level level = Level::Info);
    static void shutdown();

    static bool isInitialized();
    static void setLevel(Level level);
    static Level getLevel();

    static const std::shared_ptr<spdlog::logger>& get(Type type);
    static const std::shared_ptr<spdlog::logger>& core();
    static const std::shared_ptr<spdlog::logger>& runtime();
    static const std::shared_ptr<spdlog::logger>& editor();
    static void flush();

private:
    static void ensureInitialized();
    static void configureLogger(const std::shared_ptr<spdlog::logger>& logger, spdlog::level::level_enum level);
    static spdlog::level::level_enum toSpdlogLevel(Level level);

private:
    static inline bool s_initialized = false;
    static inline Level s_level = Level::Info;
    static inline std::string s_logFile;
    static inline std::vector<spdlog::sink_ptr> s_sinks;
    static inline std::shared_ptr<spdlog::logger> s_coreLogger;
    static inline std::shared_ptr<spdlog::logger> s_runtimeLogger;
    static inline std::shared_ptr<spdlog::logger> s_editorLogger;
};

} // namespace luna

#define LUNA_CORE_TRACE(...) SPDLOG_LOGGER_TRACE(::luna::Logger::core().get(), __VA_ARGS__)
#define LUNA_CORE_DEBUG(...) SPDLOG_LOGGER_DEBUG(::luna::Logger::core().get(), __VA_ARGS__)
#define LUNA_CORE_INFO(...) SPDLOG_LOGGER_INFO(::luna::Logger::core().get(), __VA_ARGS__)
#define LUNA_CORE_WARN(...) SPDLOG_LOGGER_WARN(::luna::Logger::core().get(), __VA_ARGS__)
#define LUNA_CORE_ERROR(...) SPDLOG_LOGGER_ERROR(::luna::Logger::core().get(), __VA_ARGS__)
#define LUNA_CORE_FATAL(...) SPDLOG_LOGGER_CRITICAL(::luna::Logger::core().get(), __VA_ARGS__)

#define LUNA_RUNTIME_TRACE(...) SPDLOG_LOGGER_TRACE(::luna::Logger::runtime().get(), __VA_ARGS__)
#define LUNA_RUNTIME_DEBUG(...) SPDLOG_LOGGER_DEBUG(::luna::Logger::runtime().get(), __VA_ARGS__)
#define LUNA_RUNTIME_INFO(...) SPDLOG_LOGGER_INFO(::luna::Logger::runtime().get(), __VA_ARGS__)
#define LUNA_RUNTIME_WARN(...) SPDLOG_LOGGER_WARN(::luna::Logger::runtime().get(), __VA_ARGS__)
#define LUNA_RUNTIME_ERROR(...) SPDLOG_LOGGER_ERROR(::luna::Logger::runtime().get(), __VA_ARGS__)
#define LUNA_RUNTIME_FATAL(...) SPDLOG_LOGGER_CRITICAL(::luna::Logger::runtime().get(), __VA_ARGS__)

#define LUNA_EDITOR_TRACE(...) SPDLOG_LOGGER_TRACE(::luna::Logger::editor().get(), __VA_ARGS__)
#define LUNA_EDITOR_DEBUG(...) SPDLOG_LOGGER_DEBUG(::luna::Logger::editor().get(), __VA_ARGS__)
#define LUNA_EDITOR_INFO(...) SPDLOG_LOGGER_INFO(::luna::Logger::editor().get(), __VA_ARGS__)
#define LUNA_EDITOR_WARN(...) SPDLOG_LOGGER_WARN(::luna::Logger::editor().get(), __VA_ARGS__)
#define LUNA_EDITOR_ERROR(...) SPDLOG_LOGGER_ERROR(::luna::Logger::editor().get(), __VA_ARGS__)
#define LUNA_EDITOR_FATAL(...) SPDLOG_LOGGER_CRITICAL(::luna::Logger::editor().get(), __VA_ARGS__)
