#ifndef CACAO_CACAOINSTANCE_H
#define CACAO_CACAOINSTANCE_H
#include "Core.h"
namespace Cacao
{
    class ShaderCompiler;
}
namespace Cacao
{
    class Surface;
}
namespace Cacao
{
    class Adapter;
    struct NativeWindowHandle
    {
#if defined(_WIN32)
        void* hWnd = nullptr;
        void* hInst = nullptr;
#elif defined(__APPLE__)
        void* metalLayer = nullptr;
#elif defined(__linux__) && !defined(__ANDROID__)
#if defined(USE_WAYLAND)
        wl_display* wlDisplay = nullptr;
        wl_surface* wlSurface = nullptr;
#elif defined(USE_XCB)
        xcb_connection_t* xcbConnection = nullptr;
        xcb_window_t xcbWindow = 0;
#elif defined(USE_XLIB)
        Display* x11Display = nullptr;
        Window x11Window = 0;
#endif
#elif defined(__ANDROID__)
        void* aNativeWindow = nullptr;
#else
        void* placeholder = nullptr;
#endif
        bool IsValid() const
        {
#if defined(_WIN32)
            return hWnd != nullptr && hInst != nullptr;
#elif defined(__APPLE__)
            return metalLayer != nullptr;
#elif defined(__linux__) && !defined(__ANDROID__)
#if defined(USE_WAYLAND)
            return wlDisplay != nullptr && wlSurface != nullptr;
#elif defined(USE_XCB)
            return xcbConnection != nullptr && xcbWindow != 0;
#elif defined(USE_XLIB)
            return x11Display != nullptr && x11Window != 0;
#endif
#elif defined(__ANDROID__)
            return aNativeWindow != nullptr;
#else
            return placeholder != nullptr;
#endif
        }
    };
    enum class BackendType
    {
        Auto,
        Vulkan,
        DirectX12,
        DirectX11,
        Metal,
        OpenGL,
        OpenGLES,
        WebGPU,
    };
    enum class InstanceFeature
    {
        ValidationLayer = 0x00000001,
        Surface = 0x00000002,
        RayTracing = 0x00000004,
        MeshShader = 0x00000008,
        BindlessDescriptors = 0x00000016,
    };
    struct InstanceCreateInfo
    {
        BackendType type = BackendType::Auto;
        std::string applicationName;
        uint32_t appVersion = 1;
        std::vector<InstanceFeature> enabledFeatures;
    };
    class CACAO_API Instance : public std::enable_shared_from_this<Instance>
    {
    public:
        virtual ~Instance() = default;
        static Ref<Instance> Create(const InstanceCreateInfo& createInfo);
        [[nodiscard]] virtual BackendType GetType() const = 0;
        virtual bool Initialize(const InstanceCreateInfo& createInfo) = 0;
        virtual std::vector<Ref<Adapter>> EnumerateAdapters() = 0;
        virtual bool IsFeatureEnabled(InstanceFeature feature) const = 0;
        virtual Ref<Surface> CreateSurface(const NativeWindowHandle& windowHandle) = 0;
        virtual Ref<ShaderCompiler> CreateShaderCompiler() = 0;
    };
    template <>
    struct to_string<BackendType>
    {
        static std::string convert(BackendType type)
        {
            switch (type)
            {
            case BackendType::Auto:
                return "Auto";
            case BackendType::Vulkan:
                return "Vulkan";
            case BackendType::DirectX12:
                return "DirectX12";
            case BackendType::DirectX11:
                return "DirectX11";
            case BackendType::Metal:
                return "Metal";
            case BackendType::OpenGL:
                return "OpenGL";
            case BackendType::OpenGLES:
                return "OpenGLES";
            case BackendType::WebGPU:
                return "WebGPU";
            default:
                return "Unknown";
            }
        }
    };
    template <>
    struct to_string<InstanceFeature>
    {
        static std::string convert(InstanceFeature feature)
        {
            switch (feature)
            {
            case InstanceFeature::ValidationLayer:
                return "ValidationLayer";
            case InstanceFeature::Surface:
                return "Surface";
            case InstanceFeature::RayTracing:
                return "RayTracing";
            case InstanceFeature::MeshShader:
                return "MeshShader";
            case InstanceFeature::BindlessDescriptors:
                return "BindlessDescriptors";
            default:
                return "Unknown";
            }
        }
    };
}
#endif
