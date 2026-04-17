
#include "Log.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <Logging.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <string_view>
#include <utility>

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

void cacaoLogBridge(Cacao::LogLevel level, std::string_view message, void*)
{
    switch (level) {
        case Cacao::LogLevel::Trace:
            SPDLOG_LOGGER_TRACE(::luna::Logger::core().get(), "{}", message);
            break;
        case Cacao::LogLevel::Debug:
            SPDLOG_LOGGER_DEBUG(::luna::Logger::core().get(), "{}", message);
            break;
        case Cacao::LogLevel::Info:
            SPDLOG_LOGGER_INFO(::luna::Logger::core().get(), "{}", message);
            break;
        case Cacao::LogLevel::Warn:
            SPDLOG_LOGGER_WARN(::luna::Logger::core().get(), "{}", message);
            break;
        case Cacao::LogLevel::Error:
            SPDLOG_LOGGER_ERROR(::luna::Logger::core().get(), "{}", message);
            break;
        case Cacao::LogLevel::Fatal:
            SPDLOG_LOGGER_CRITICAL(::luna::Logger::core().get(), "{}", message);
            break;
        default:
            SPDLOG_LOGGER_INFO(::luna::Logger::core().get(), "{}", message);
            break;
    }

    ::luna::Logger::flush();
}

} // namespace

void Logger::init(const std::string& log_file, Level level)
{
    shutdown();

    m_s_log_file = log_file;
    m_s_level = level;

    try {
        m_s_sinks.clear();

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_formatter(makeFormatter("%^[%T.%e] [%n] [%*]%$ %v"));
        m_s_sinks.push_back(console_sink);

        if (!m_s_log_file.empty()) {
            const std::filesystem::path log_path{m_s_log_file};
            if (log_path.has_parent_path()) {
                std::filesystem::create_directories(log_path.parent_path());
            }

            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path.string(), true);
            file_sink->set_formatter(makeFormatter("[%Y-%m-%d %H:%M:%S.%e] [%P:%t] [%n] [%*] %v"));
            m_s_sinks.push_back(std::move(file_sink));
        }

        const auto spdlog_level = toSpdlogLevel(level);
        for (const auto& sink : m_s_sinks) {
            sink->set_level(spdlog_level);
        }

        m_s_core_logger = std::make_shared<spdlog::logger>("LunaCore", m_s_sinks.begin(), m_s_sinks.end());
        m_s_runtime_logger = std::make_shared<spdlog::logger>("Runtime", m_s_sinks.begin(), m_s_sinks.end());
        m_s_editor_logger = std::make_shared<spdlog::logger>("Editor", m_s_sinks.begin(), m_s_sinks.end());

        configureLogger(m_s_core_logger, spdlog_level);
        configureLogger(m_s_runtime_logger, spdlog_level);
        configureLogger(m_s_editor_logger, spdlog_level);

        spdlog::register_logger(m_s_core_logger);
        spdlog::register_logger(m_s_runtime_logger);
        spdlog::register_logger(m_s_editor_logger);

        m_s_initialized = true;
        Cacao::SetLogCallback(cacaoLogBridge);
        Cacao::SetVulkanValidationMessageFilterEnabled(true);

        if (m_s_log_file.empty()) {
            m_s_core_logger->info("Logger initialized (console only)");
        } else {
            m_s_core_logger->info("Logger initialized, output file: {}", m_s_log_file);
        }
    } catch (const std::exception& ex) {
        m_s_initialized = false;
        m_s_sinks.clear();
        m_s_core_logger.reset();
        m_s_runtime_logger.reset();
        m_s_editor_logger.reset();
        std::cerr << "Logger initialization failed: " << ex.what() << '\n';
    }
}

void Logger::shutdown()
{
    if (!m_s_initialized) {
        return;
    }

    Cacao::SetLogCallback(nullptr);

    if (m_s_core_logger) {
        m_s_core_logger->info("Logger shutdown");
        m_s_core_logger->flush();
    }

    if (m_s_runtime_logger) {
        m_s_runtime_logger->flush();
    }

    if (m_s_editor_logger) {
        m_s_editor_logger->flush();
    }

    spdlog::drop("LunaCore");
    spdlog::drop("Runtime");
    spdlog::drop("Editor");

    m_s_core_logger.reset();
    m_s_runtime_logger.reset();
    m_s_editor_logger.reset();
    m_s_sinks.clear();
    m_s_initialized = false;
}

bool Logger::isInitialized()
{
    return m_s_initialized;
}

void Logger::setLevel(Level level)
{
    m_s_level = level;
    if (!m_s_initialized) {
        return;
    }

    const auto spdlog_level = toSpdlogLevel(level);

    for (const auto& sink : m_s_sinks) {
        sink->set_level(spdlog_level);
    }

    configureLogger(m_s_core_logger, spdlog_level);
    configureLogger(m_s_runtime_logger, spdlog_level);
    configureLogger(m_s_editor_logger, spdlog_level);
}

Logger::Level Logger::getLevel()
{
    return m_s_level;
}

const std::shared_ptr<spdlog::logger>& Logger::get(Type type)
{
    ensureInitialized();
    switch (type) {
        case Type::Core:
            return m_s_core_logger;
        case Type::Runtime:
            return m_s_runtime_logger;
        case Type::Editor:
            return m_s_editor_logger;
        default:
            return m_s_core_logger;
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
    if (!m_s_initialized) {
        return;
    }

    if (m_s_core_logger) {
        m_s_core_logger->flush();
    }
    if (m_s_runtime_logger) {
        m_s_runtime_logger->flush();
    }
    if (m_s_editor_logger) {
        m_s_editor_logger->flush();
    }
}

void Logger::ensureInitialized()
{
    if (!m_s_initialized) {
        init(m_s_log_file, m_s_level);
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
