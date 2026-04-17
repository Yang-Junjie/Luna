#include <Instance.h>

#ifdef LUNA_RHI_BACKEND_VULKAN
#include "Impls/Vulkan/VKInstance.h"
#endif

#ifdef LUNA_RHI_BACKEND_D3D12
#include "Impls/D3D12/D3D12Instance.h"
#endif

#ifdef LUNA_RHI_BACKEND_D3D11
#include "Impls/D3D11/D3D11Instance.h"
#endif

#if defined(LUNA_RHI_BACKEND_OPENGL) || defined(LUNA_RHI_BACKEND_OPENGLES)
#include "Impls/OpenGL/GLInstance.h"
#endif

namespace luna::RHI {
static BackendType SelectBestBackend()
{
#if defined(_WIN32)
#ifdef LUNA_RHI_BACKEND_D3D12
    return BackendType::DirectX12;
#elif defined(LUNA_RHI_BACKEND_VULKAN)
    return BackendType::Vulkan;
#elif defined(LUNA_RHI_BACKEND_D3D11)
    return BackendType::DirectX11;
#elif defined(LUNA_RHI_BACKEND_OPENGL)
    return BackendType::OpenGL;
#endif
#elif defined(__APPLE__)
#ifdef LUNA_RHI_BACKEND_METAL
    return BackendType::Metal;
#elif defined(LUNA_RHI_BACKEND_VULKAN)
    return BackendType::Vulkan;
#endif
#elif defined(__ANDROID__)
#ifdef LUNA_RHI_BACKEND_VULKAN
    return BackendType::Vulkan;
#elif defined(LUNA_RHI_BACKEND_OPENGLES)
    return BackendType::OpenGLES;
#endif
#elif defined(__linux__)
#ifdef LUNA_RHI_BACKEND_VULKAN
    return BackendType::Vulkan;
#elif defined(LUNA_RHI_BACKEND_OPENGL)
    return BackendType::OpenGL;
#endif
#elif defined(__EMSCRIPTEN__)
#ifdef LUNA_RHI_BACKEND_WEBGPU
    return BackendType::WebGPU;
#endif
#endif
    throw std::runtime_error("No suitable backend available for this platform");
}

Ref<Instance> Instance::Create(const InstanceCreateInfo& createInfo)
{
    InstanceCreateInfo resolved = createInfo;
    if (resolved.type == BackendType::Auto) {
        resolved.type = SelectBestBackend();
    }

    switch (resolved.type) {
#ifdef LUNA_RHI_BACKEND_VULKAN
        case BackendType::Vulkan: {
            auto inst = luna::RHI::CreateRef<VKInstance>();
            if (!inst->Initialize(resolved)) {
                throw std::runtime_error("Failed to initialize Vulkan instance");
            }
            return inst;
        }
#endif

#ifdef LUNA_RHI_BACKEND_D3D12
        case BackendType::DirectX12: {
            auto inst = luna::RHI::CreateRef<D3D12Instance>();
            if (!inst->Initialize(resolved)) {
                throw std::runtime_error("Failed to initialize D3D12 instance");
            }
            return inst;
        }
#endif

#ifdef LUNA_RHI_BACKEND_D3D11
        case BackendType::DirectX11: {
            auto inst = luna::RHI::CreateRef<D3D11Instance>();
            if (!inst->Initialize(resolved)) {
                throw std::runtime_error("Failed to initialize D3D11 instance");
            }
            return inst;
        }
#endif

#if defined(LUNA_RHI_BACKEND_OPENGL) || defined(LUNA_RHI_BACKEND_OPENGLES)
        case BackendType::OpenGL:
        case BackendType::OpenGLES: {
            auto inst = luna::RHI::CreateRef<GLInstance>();
            if (!inst->Initialize(resolved)) {
                throw std::runtime_error("Failed to initialize OpenGL instance");
            }
            return inst;
        }
#endif

        default:
            throw std::runtime_error("Unsupported backend type: " + std::to_string(static_cast<int>(resolved.type)));
    }
}
} // namespace luna::RHI
