#include <Impls/Vulkan/VKInstance.h>
#include <Impls/Vulkan/VKAdapter.h>
#include <Impls/Vulkan/VKSurface.h>
#include "ShaderCompiler.h"
namespace Cacao
{
    BackendType VKInstance::GetType() const
    {
        return BackendType::Vulkan;
    }
    bool VKInstance::Initialize(const InstanceCreateInfo& createInfo)
    {
        m_createInfo = createInfo;
        vk::ApplicationInfo appInfo = vk::ApplicationInfo()
#ifdef VK_API_VERSION_1_4
            .setApiVersion(VK_API_VERSION_1_4)
#elif defined(VK_API_VERSION_1_3)
            .setApiVersion(VK_API_VERSION_1_3)
#elif defined(VK_API_VERSION_1_2)
            .setApiVersion(VK_API_VERSION_1_2)
#elif defined(VK_API_VERSION_1_1)
            .setApiVersion(VK_API_VERSION_1_1)
#else
            .setApiVersion(VK_API_VERSION_1_0)
#endif
            .setApplicationVersion(createInfo.appVersion)
            .setEngineVersion(1)
            .setPApplicationName(createInfo.applicationName.c_str())
            .setPEngineName("Cacao Engine");
        std::vector<const char*> enabledLayerNames;
        std::vector<const char*> enabledExtensionNames;
        for (const auto& feature : createInfo.enabledFeatures)
        {
            if (feature == InstanceFeature::ValidationLayer)
            {
                enabledLayerNames.push_back("VK_LAYER_KHRONOS_validation");
            }
            switch (feature)
            {
            case InstanceFeature::Surface:
                enabledExtensionNames.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
                break;
            case InstanceFeature::RayTracing:
                enabledExtensionNames.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
                enabledExtensionNames.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
                enabledExtensionNames.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
                break;
            case InstanceFeature::BindlessDescriptors:
                enabledExtensionNames.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
                break;
            case InstanceFeature::MeshShader:
                enabledExtensionNames.push_back(VK_NV_MESH_SHADER_EXTENSION_NAME);
                break;
            default:
            case InstanceFeature::ValidationLayer:
                break;
            }
        }
#if defined(_WIN32)
        enabledExtensionNames.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__ANDROID__)
        enabledExtensionNames.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(__APPLE__)
#if TARGET_OS_MAC
        enabledExtensionNames.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#endif
#elif defined(__linux__)
#if defined(USE_XCB)
        enabledExtensionNames.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(USE_XLIB)
        enabledExtensionNames.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#elif defined(USE_WAYLAND)
        enabledExtensionNames.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#else
#warning "Linux 平台未定义窗口系统 (USE_XCB / USE_XLIB / USE_WAYLAND)，Surface 将无法创建"
#endif
#else
#warning "Unsupported platform: no Vulkan surface extension added"
#endif
        vk::InstanceCreateInfo instanceCreateInfo = vk::InstanceCreateInfo()
                                                    .setPApplicationInfo(&appInfo)
                                                    .setEnabledLayerCount(
                                                        static_cast<uint32_t>(enabledLayerNames.size()))
                                                    .setPpEnabledLayerNames(enabledLayerNames.data())
                                                    .setEnabledExtensionCount(
                                                        static_cast<uint32_t>(enabledExtensionNames.size()))
                                                    .setPpEnabledExtensionNames(enabledExtensionNames.data());
        m_instance = vk::createInstance(instanceCreateInfo);
        if (!m_instance)
        {
            throw std::runtime_error("failed to create Vulkan instance!");
            return false;
        }
        return true;
    }
    std::vector<Ref<Adapter>> VKInstance::EnumerateAdapters()
    {
        std::vector<Ref<Adapter>> adapters;
        auto physicalDevices = m_instance.enumeratePhysicalDevices();
        adapters.resize(physicalDevices.size());
        for (int i = 0; i < physicalDevices.size(); i++)
        {
            adapters[i] = VKAdapter::Create(shared_from_this(), physicalDevices[i]);
        }
        return adapters;
    }
    bool VKInstance::IsFeatureEnabled(InstanceFeature feature) const
    {
        for (const auto& enabledFeature : m_createInfo.enabledFeatures)
        {
            if (enabledFeature == feature)
            {
                return true;
            }
        }
        return false;
    }
    Ref<Surface> VKInstance::CreateSurface(const NativeWindowHandle& windowHandle)
    {
        if (!windowHandle.IsValid())
        {
            throw std::runtime_error("无法创建 Surface：检测到无效的原生窗口句柄 (Invalid NativeWindowHandle)。");
        }
        vk::SurfaceKHR surface;
        try
        {
#if defined(_WIN32)
            vk::Win32SurfaceCreateInfoKHR createInfo = vk::Win32SurfaceCreateInfoKHR()
                                                       .setHwnd(static_cast<HWND>(windowHandle.hWnd))
                                                       .setHinstance(static_cast<HINSTANCE>(windowHandle.hInst));
            surface = m_instance.createWin32SurfaceKHR(createInfo);
#elif defined(__ANDROID__)
            vk::AndroidSurfaceCreateInfoKHR createInfo = vk::AndroidSurfaceCreateInfoKHR()
                .setWindow(static_cast<ANativeWindow*>(windowHandle.aNativeWindow));
            surface = m_instance.createAndroidSurfaceKHR(createInfo);
#elif defined(__linux__)
#if defined(USE_WAYLAND)
            vk::WaylandSurfaceCreateInfoKHR createInfo = vk::WaylandSurfaceCreateInfoKHR()
                                                         .setDisplay(windowHandle.wlDisplay)
                                                         .setSurface(windowHandle.wlSurface);
            surface = m_instance.createWaylandSurfaceKHR(createInfo);
#elif defined(USE_XCB)
            vk::XcbSurfaceCreateInfoKHR createInfo = vk::XcbSurfaceCreateInfoKHR()
                                                     .setConnection(windowHandle.xcbConnection)
                                                     .setWindow(windowHandle.xcbWindow);
            surface = m_instance.createXcbSurfaceKHR(createInfo);
#elif defined(USE_XLIB)
            vk::XlibSurfaceCreateInfoKHR createInfo = vk::XlibSurfaceCreateInfoKHR()
                                                      .setDpy(windowHandle.x11Display)
                                                      .setWindow(windowHandle.x11Window);
            surface = m_instance.createXlibSurfaceKHR(createInfo);
#else
            throw std::runtime_error("未定义的 Linux 显示后端 (需定义 USE_WAYLAND, USE_XCB 或 USE_XLIB)。");
#endif
#elif defined(__APPLE__)
            vk::MetalSurfaceCreateInfoEXT createInfo = vk::MetalSurfaceCreateInfoEXT()
                .setPLayer(windowHandle.metalLayer);
            surface = m_instance.createMetalSurfaceEXT(createInfo);
#else
            throw std::runtime_error("当前平台不支持自动创建 Surface，请检查构建配置。");
#endif
        }
        catch (const vk::SystemError& e)
        {
            throw std::runtime_error(std::string("Vulkan Surface 创建失败 (SystemError): ") + e.what());
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error(std::string("Vulkan Surface 创建期间发生未预期的错误: ") + e.what());
        }
        if (!surface)
        {
            throw std::runtime_error("Vulkan Surface 创建失败：返回了空句柄。");
        }
        return Cacao::CreateRef<VKSurface>(surface);
    }
    Ref<ShaderCompiler> VKInstance::CreateShaderCompiler()
    {
        return ShaderCompiler::Create(BackendType::Vulkan);
    }
}
