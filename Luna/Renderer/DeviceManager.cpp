#include "Renderer/DeviceManager.hpp"

#include "Core/log.h"
#include "Renderer/Vulkan/DeviceManager_VK.hpp"

namespace luna::renderer {

std::unique_ptr<DeviceManager> DeviceManager::create(GraphicsAPI api)
{
    switch (api) {
        case GraphicsAPI::Vulkan:
            return std::make_unique<vulkan::DeviceManager_VK>();
        default:
            LUNA_CORE_ERROR("Unsupported graphics API {}", static_cast<int>(api));
            return nullptr;
    }
}

} // namespace luna::renderer
