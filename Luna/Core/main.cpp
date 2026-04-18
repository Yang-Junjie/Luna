#include "Core/Application.h"
#include "Core/Log.h"

#include <exception>
#include <memory>

int main(int argc, char** argv)
{
#ifndef NDEBUG
    constexpr luna::Logger::Level k_log_level = luna::Logger::Level::Trace;
#else
    constexpr luna::Logger::Level k_log_level = luna::Logger::Level::Info;
#endif

    luna::Logger::init("logs/luna.log", k_log_level);
    LUNA_CORE_INFO("Application starting");

    try {
        std::unique_ptr<luna::Application> app(luna::createApplication(argc, argv));
        if (app == nullptr) {
            LUNA_CORE_FATAL("Application creation returned null");
            luna::Logger::shutdown();
            return 1;
        }

        if (!app->initialize()) {
            LUNA_CORE_FATAL("Application initialization failed");
            luna::Logger::shutdown();
            return 1;
        }

        app->run();
        app.reset();
        LUNA_CORE_INFO("Application shutdown cleanly");
        luna::Logger::shutdown();
        return 0;
    } catch (const std::exception& error) {
        LUNA_CORE_FATAL("Unhandled exception: {}", error.what());
        luna::Logger::shutdown();
        return 1;
    } catch (...) {
        LUNA_CORE_FATAL("Unhandled non-standard exception");
        luna::Logger::shutdown();
        return 1;
    }
}
