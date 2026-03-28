#include "Vulkan/vk_engine.h"

#include <exception>
#include <iostream>

int main()
{
    VulkanEngine engine;
    try {
        engine.init();
        engine.run();
        engine.cleanup();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        engine.cleanup();
        return 1;
    }
}
