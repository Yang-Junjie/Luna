#include "log.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <utility>

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace luna {

namespace {

class UppercaseLevelFlag final : public spdlog::custom_flag_formatter {
public:
    void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override
    {
        std::string_view level_name = "INFO";

        switch (msg.level) {
            case spdlog::level::trace:
                level_name = "TRACE";
                break;
            case spdlog::level::debug:
                level_name = "DEBUG";
                break;
            case spdlog::level::info:
                level_name = "INFO";
                break;
            case spdlog::level::warn:
                level_name = "WARNING";
                break;
            case spdlog::level::err:
                level_name = "ERROR";
                break;
            case spdlog::level::critical:
                level_name = "FATAL";
                break;
            case spdlog::level::off:
                level_name = "OFF";
                break;
            default:
                level_name = "INFO";
                break;
        }

        dest.append(level_name.data(), level_name.data() + level_name.size());
    }

    std::unique_ptr<custom_flag_formatter> clone() const override
    {
        return std::make_unique<UppercaseLevelFlag>();
    }
};

std::unique_ptr<spdlog::formatter> makeFormatter(const char* pattern)
{
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<UppercaseLevelFlag>('*').set_pattern(pattern);
    return formatter;
}

} // namespace

void Logger::init(const std::string& log_file, Level level)
{
    shutdown();

    s_logFile = log_file;
    s_level = level;

    try {
        s_sinks.clear();

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_formatter(makeFormatter("%^[%T.%e] [%n] [%*]%$ %v"));
        s_sinks.push_back(console_sink);

        if (!s_logFile.empty()) {
            const std::filesystem::path log_path{s_logFile};
            if (log_path.has_parent_path()) {
                std::filesystem::create_directories(log_path.parent_path());
            }

            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string(), true);
            file_sink->set_formatter(makeFormatter("[%Y-%m-%d %H:%M:%S.%e] [%P:%t] [%n] [%*] %v"));
            s_sinks.push_back(std::move(file_sink));
        }

        const auto spdlog_level = toSpdlogLevel(level);
        for (const auto& sink : s_sinks) {
            sink->set_level(spdlog_level);
        }

        s_coreLogger = std::make_shared<spdlog::logger>("LunaCore", s_sinks.begin(), s_sinks.end());
        s_runtimeLogger = std::make_shared<spdlog::logger>("Runtime", s_sinks.begin(), s_sinks.end());
        s_editorLogger = std::make_shared<spdlog::logger>("Editor", s_sinks.begin(), s_sinks.end());

        configureLogger(s_coreLogger, spdlog_level);
        configureLogger(s_runtimeLogger, spdlog_level);
        configureLogger(s_editorLogger, spdlog_level);

        spdlog::register_logger(s_coreLogger);
        spdlog::register_logger(s_runtimeLogger);
        spdlog::register_logger(s_editorLogger);

        s_initialized = true;

        if (s_logFile.empty()) {
            s_coreLogger->info("Logger initialized (console only)");
        } else {
            s_coreLogger->info("Logger initialized, output file: {}", s_logFile);
        }
    } catch (const std::exception& ex) {
        s_initialized = false;
        s_sinks.clear();
        s_coreLogger.reset();
        s_runtimeLogger.reset();
        s_editorLogger.reset();
        std::cerr << "Logger initialization failed: " << ex.what() << '\n';
    }
}

void Logger::shutdown()
{
    if (!s_initialized) {
        return;
    }

    if (s_coreLogger) {
        s_coreLogger->info("Logger shutdown");
        s_coreLogger->flush();
    }

    if (s_runtimeLogger) {
        s_runtimeLogger->flush();
    }

    if (s_editorLogger) {
        s_editorLogger->flush();
    }

    spdlog::drop("LunaCore");
    spdlog::drop("Runtime");
    spdlog::drop("Editor");

    s_coreLogger.reset();
    s_runtimeLogger.reset();
    s_editorLogger.reset();
    s_sinks.clear();
    s_initialized = false;
}

bool Logger::isInitialized()
{
    return s_initialized;
}

void Logger::setLevel(Level level)
{
    s_level = level;
    if (!s_initialized) {
        return;
    }

    const auto spdlog_level = toSpdlogLevel(level);

    for (const auto& sink : s_sinks) {
        sink->set_level(spdlog_level);
    }

    configureLogger(s_coreLogger, spdlog_level);
    configureLogger(s_runtimeLogger, spdlog_level);
    configureLogger(s_editorLogger, spdlog_level);
}

Logger::Level Logger::getLevel()
{
    return s_level;
}

const std::shared_ptr<spdlog::logger>& Logger::get(Type type)
{
    ensureInitialized();
    switch (type) {
        case Type::Core:
            return s_coreLogger;
        case Type::Runtime:
            return s_runtimeLogger;
        case Type::Editor:
            return s_editorLogger;
        default:
            return s_coreLogger;
    }
}

const std::shared_ptr<spdlog::logger>& Logger::core()
{
    return get(Type::Core);
}

const std::shared_ptr<spdlog::logger>& Logger::runtime()
{
    return get(Type::Runtime);
}

const std::shared_ptr<spdlog::logger>& Logger::editor()
{
    return get(Type::Editor);
}

void Logger::flush()
{
    if (!s_initialized) {
        return;
    }

    if (s_coreLogger) {
        s_coreLogger->flush();
    }
    if (s_runtimeLogger) {
        s_runtimeLogger->flush();
    }
    if (s_editorLogger) {
        s_editorLogger->flush();
    }
}

void Logger::ensureInitialized()
{
    if (!s_initialized) {
        init(s_logFile, s_level);
    }
}

void Logger::configureLogger(const std::shared_ptr<spdlog::logger>& logger, spdlog::level::level_enum level)
{
    if (!logger) {
        return;
    }

    logger->set_level(level);
    logger->flush_on(spdlog::level::err);
}

spdlog::level::level_enum Logger::toSpdlogLevel(Level level)
{
    switch (level) {
        case Level::Trace:
            return spdlog::level::trace;
        case Level::Debug:
            return spdlog::level::debug;
        case Level::Info:
            return spdlog::level::info;
        case Level::Warn:
            return spdlog::level::warn;
        case Level::Error:
            return spdlog::level::err;
        case Level::Fatal:
            return spdlog::level::critical;
        case Level::Off:
            return spdlog::level::off;
        default:
            return spdlog::level::info;
    }
}

} // namespace luna
