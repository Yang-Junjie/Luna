#pragma once

#include <cstdint>

#include <memory>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace luna {

class Logger {
public:
    enum class Type : uint8_t {
        Core = 0,
        Platform = 1,
        Jobs = 2,
        Renderer = 3,
        ImGui = 4,
        RHI = 5,
        Runtime = 6,
        Editor = 7
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
    static const std::shared_ptr<spdlog::logger>& platform();
    static const std::shared_ptr<spdlog::logger>& jobs();
    static const std::shared_ptr<spdlog::logger>& renderer();
    static const std::shared_ptr<spdlog::logger>& imgui();
    static const std::shared_ptr<spdlog::logger>& rhi();
    static const std::shared_ptr<spdlog::logger>& runtime();
    static const std::shared_ptr<spdlog::logger>& editor();
    static void flush();

private:
    static void ensureInitialized();
    static void configureLogger(const std::shared_ptr<spdlog::logger>& logger, spdlog::level::level_enum level);
    static spdlog::level::level_enum toSpdlogLevel(Level level);

private:
    static inline bool m_s_initialized = false;
    static inline Level m_s_level = Level::Info;
    static inline std::string m_s_log_file;
    static inline std::vector<spdlog::sink_ptr> m_s_sinks;
    static inline std::shared_ptr<spdlog::logger> m_s_core_logger;
    static inline std::shared_ptr<spdlog::logger> m_s_platform_logger;
    static inline std::shared_ptr<spdlog::logger> m_s_jobs_logger;
    static inline std::shared_ptr<spdlog::logger> m_s_renderer_logger;
    static inline std::shared_ptr<spdlog::logger> m_s_imgui_logger;
    static inline std::shared_ptr<spdlog::logger> m_s_rhi_logger;
    static inline std::shared_ptr<spdlog::logger> m_s_runtime_logger;
    static inline std::shared_ptr<spdlog::logger> m_s_editor_logger;
};

} // namespace luna

#define LUNA_CORE_TRACE(...) SPDLOG_LOGGER_TRACE(::luna::Logger::core().get(), __VA_ARGS__)
#define LUNA_CORE_DEBUG(...) SPDLOG_LOGGER_DEBUG(::luna::Logger::core().get(), __VA_ARGS__)
#define LUNA_CORE_INFO(...)  SPDLOG_LOGGER_INFO(::luna::Logger::core().get(), __VA_ARGS__)
#define LUNA_CORE_WARN(...)  SPDLOG_LOGGER_WARN(::luna::Logger::core().get(), __VA_ARGS__)
#define LUNA_CORE_ERROR(...) SPDLOG_LOGGER_ERROR(::luna::Logger::core().get(), __VA_ARGS__)
#define LUNA_CORE_FATAL(...) SPDLOG_LOGGER_CRITICAL(::luna::Logger::core().get(), __VA_ARGS__)

#define LUNA_PLATFORM_TRACE(...) SPDLOG_LOGGER_TRACE(::luna::Logger::platform().get(), __VA_ARGS__)
#define LUNA_PLATFORM_DEBUG(...) SPDLOG_LOGGER_DEBUG(::luna::Logger::platform().get(), __VA_ARGS__)
#define LUNA_PLATFORM_INFO(...)  SPDLOG_LOGGER_INFO(::luna::Logger::platform().get(), __VA_ARGS__)
#define LUNA_PLATFORM_WARN(...)  SPDLOG_LOGGER_WARN(::luna::Logger::platform().get(), __VA_ARGS__)
#define LUNA_PLATFORM_ERROR(...) SPDLOG_LOGGER_ERROR(::luna::Logger::platform().get(), __VA_ARGS__)
#define LUNA_PLATFORM_FATAL(...) SPDLOG_LOGGER_CRITICAL(::luna::Logger::platform().get(), __VA_ARGS__)

#define LUNA_JOBS_TRACE(...) SPDLOG_LOGGER_TRACE(::luna::Logger::jobs().get(), __VA_ARGS__)
#define LUNA_JOBS_DEBUG(...) SPDLOG_LOGGER_DEBUG(::luna::Logger::jobs().get(), __VA_ARGS__)
#define LUNA_JOBS_INFO(...)  SPDLOG_LOGGER_INFO(::luna::Logger::jobs().get(), __VA_ARGS__)
#define LUNA_JOBS_WARN(...)  SPDLOG_LOGGER_WARN(::luna::Logger::jobs().get(), __VA_ARGS__)
#define LUNA_JOBS_ERROR(...) SPDLOG_LOGGER_ERROR(::luna::Logger::jobs().get(), __VA_ARGS__)
#define LUNA_JOBS_FATAL(...) SPDLOG_LOGGER_CRITICAL(::luna::Logger::jobs().get(), __VA_ARGS__)

#define LUNA_RENDERER_TRACE(...) SPDLOG_LOGGER_TRACE(::luna::Logger::renderer().get(), __VA_ARGS__)
#define LUNA_RENDERER_DEBUG(...) SPDLOG_LOGGER_DEBUG(::luna::Logger::renderer().get(), __VA_ARGS__)
#define LUNA_RENDERER_INFO(...)  SPDLOG_LOGGER_INFO(::luna::Logger::renderer().get(), __VA_ARGS__)
#define LUNA_RENDERER_WARN(...)  SPDLOG_LOGGER_WARN(::luna::Logger::renderer().get(), __VA_ARGS__)
#define LUNA_RENDERER_ERROR(...) SPDLOG_LOGGER_ERROR(::luna::Logger::renderer().get(), __VA_ARGS__)
#define LUNA_RENDERER_FATAL(...) SPDLOG_LOGGER_CRITICAL(::luna::Logger::renderer().get(), __VA_ARGS__)

#define LUNA_IMGUI_TRACE(...) SPDLOG_LOGGER_TRACE(::luna::Logger::imgui().get(), __VA_ARGS__)
#define LUNA_IMGUI_DEBUG(...) SPDLOG_LOGGER_DEBUG(::luna::Logger::imgui().get(), __VA_ARGS__)
#define LUNA_IMGUI_INFO(...)  SPDLOG_LOGGER_INFO(::luna::Logger::imgui().get(), __VA_ARGS__)
#define LUNA_IMGUI_WARN(...)  SPDLOG_LOGGER_WARN(::luna::Logger::imgui().get(), __VA_ARGS__)
#define LUNA_IMGUI_ERROR(...) SPDLOG_LOGGER_ERROR(::luna::Logger::imgui().get(), __VA_ARGS__)
#define LUNA_IMGUI_FATAL(...) SPDLOG_LOGGER_CRITICAL(::luna::Logger::imgui().get(), __VA_ARGS__)

#define LUNA_RHI_TRACE(...) SPDLOG_LOGGER_TRACE(::luna::Logger::rhi().get(), __VA_ARGS__)
#define LUNA_RHI_DEBUG(...) SPDLOG_LOGGER_DEBUG(::luna::Logger::rhi().get(), __VA_ARGS__)
#define LUNA_RHI_INFO(...)  SPDLOG_LOGGER_INFO(::luna::Logger::rhi().get(), __VA_ARGS__)
#define LUNA_RHI_WARN(...)  SPDLOG_LOGGER_WARN(::luna::Logger::rhi().get(), __VA_ARGS__)
#define LUNA_RHI_ERROR(...) SPDLOG_LOGGER_ERROR(::luna::Logger::rhi().get(), __VA_ARGS__)
#define LUNA_RHI_FATAL(...) SPDLOG_LOGGER_CRITICAL(::luna::Logger::rhi().get(), __VA_ARGS__)

#define LUNA_RUNTIME_TRACE(...) SPDLOG_LOGGER_TRACE(::luna::Logger::runtime().get(), __VA_ARGS__)
#define LUNA_RUNTIME_DEBUG(...) SPDLOG_LOGGER_DEBUG(::luna::Logger::runtime().get(), __VA_ARGS__)
#define LUNA_RUNTIME_INFO(...)  SPDLOG_LOGGER_INFO(::luna::Logger::runtime().get(), __VA_ARGS__)
#define LUNA_RUNTIME_WARN(...)  SPDLOG_LOGGER_WARN(::luna::Logger::runtime().get(), __VA_ARGS__)
#define LUNA_RUNTIME_ERROR(...) SPDLOG_LOGGER_ERROR(::luna::Logger::runtime().get(), __VA_ARGS__)
#define LUNA_RUNTIME_FATAL(...) SPDLOG_LOGGER_CRITICAL(::luna::Logger::runtime().get(), __VA_ARGS__)

#define LUNA_EDITOR_TRACE(...) SPDLOG_LOGGER_TRACE(::luna::Logger::editor().get(), __VA_ARGS__)
#define LUNA_EDITOR_DEBUG(...) SPDLOG_LOGGER_DEBUG(::luna::Logger::editor().get(), __VA_ARGS__)
#define LUNA_EDITOR_INFO(...)  SPDLOG_LOGGER_INFO(::luna::Logger::editor().get(), __VA_ARGS__)
#define LUNA_EDITOR_WARN(...)  SPDLOG_LOGGER_WARN(::luna::Logger::editor().get(), __VA_ARGS__)
#define LUNA_EDITOR_ERROR(...) SPDLOG_LOGGER_ERROR(::luna::Logger::editor().get(), __VA_ARGS__)
#define LUNA_EDITOR_FATAL(...) SPDLOG_LOGGER_CRITICAL(::luna::Logger::editor().get(), __VA_ARGS__)
