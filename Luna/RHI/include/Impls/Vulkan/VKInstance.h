#ifndef LUNA_RHI_VKINSTANCE_H
#define LUNA_RHI_VKINSTANCE_H
#if defined(__ANDROID__)
#include <android/native_window.h>
#elif defined(_WIN32)
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__) && !defined(__ANDROID__)
#if defined(USE_WAYLAND)
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <wayland-client.h>
#elif defined(USE_XCB)
#define VK_USE_PLATFORM_XCB_KHR
#include <xcb/xcb.h>
#elif defined(USE_XLIB)
#define VK_USE_PLATFORM_XLIB_KHR
#include <X11/Xlib.h>
#ifdef None
#undef None
#endif
#ifdef True
#undef True
#endif
#ifdef False
#undef False
#endif
#else
#error "Linux platform defined but no backend (USE_WAYLAND, USE_XCB, USE_XLIB) selected."
#endif
#elif defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#include <TargetConditionals.h>
#endif
#include <Instance.h>
#include <vulkan/vulkan.hpp>

namespace luna::RHI {
class LUNA_RHI_API VKInstance : public Instance {
private:
    vk::Instance m_instance;
    VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;
    InstanceCreateInfo m_createInfo;
    friend class VKInstance;
    friend class VKDevice;
    friend class VKAdapter;

    vk::Instance& GetVulkanInstance()
    {
        return m_instance;
    }

public:
    ~VKInstance() override;
    [[nodiscard]] BackendType GetType() const override;
    bool Initialize(const InstanceCreateInfo& createInfo) override;
    std::vector<Ref<Adapter>> EnumerateAdapters() override;
    bool IsFeatureEnabled(InstanceFeature feature) const override;
    Ref<Surface> CreateSurface(const NativeWindowHandle& windowHandle) override;
    Ref<ShaderCompiler> CreateShaderCompiler() override;

    const vk::Instance& GetNativeHandle() const
    {
        return m_instance;
    }
};
} // namespace luna::RHI
#endif
