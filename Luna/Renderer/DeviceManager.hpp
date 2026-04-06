#pragma once

#include <cstdint>
#include <memory>

struct GLFWwindow;

namespace luna::renderer {

enum class GraphicsAPI {
    Vulkan
};

struct DeviceManagerCreateInfo {
    const char* appName = "Luna Editor";
    const char* engineName = "Luna";
    std::uint32_t apiVersion = 0;
    bool enableValidation = false;
};

class DeviceManager {
public:
    virtual ~DeviceManager() = default;

    static std::unique_ptr<DeviceManager> create(GraphicsAPI api);

    virtual GraphicsAPI graphicsAPI() const = 0;
    virtual bool initialize(GLFWwindow* window, const DeviceManagerCreateInfo& createInfo) = 0;
    virtual void shutdown() = 0;
};

} // namespace luna::renderer
