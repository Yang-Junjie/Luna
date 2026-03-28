#include "Core/log.h"
#include "Vulkan/vk_engine.h"

#include <exception>

int main()
{
#ifndef NDEBUG
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Trace;
#else
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Info;
#endif

    luna::Logger::init("logs/luna.log", kLogLevel);
    LUNA_CORE_INFO("Application starting");

    VulkanEngine engine;
    try {
        engine.init();
        engine.run();
        engine.cleanup();
        LUNA_CORE_INFO("Application shutdown cleanly");
        luna::Logger::shutdown();
        return 0;
    } catch (const std::exception& e) {
        LUNA_CORE_FATAL("Fatal error: {}", e.what());
        engine.cleanup();
        luna::Logger::shutdown();
        return 1;
    }
}
