#include <Instance.h>

#include <algorithm>
#include <stdexcept>
#include <string_view>

#ifdef LUNA_RHI_BACKEND_VULKAN
#include "Impls/Vulkan/VKInstance.h"
#endif

#ifdef LUNA_RHI_BACKEND_D3D12
#include "Impls/D3D12/D3D12Instance.h"
#endif

#ifdef LUNA_RHI_BACKEND_D3D11
#include "Impls/D3D11/D3D11Instance.h"
#endif

#ifdef LUNA_RHI_BACKEND_METAL
#include "Impls/Metal/MTLInstance.h"
#endif

#if defined(LUNA_RHI_BACKEND_OPENGL) || defined(LUNA_RHI_BACKEND_OPENGLES)
#include "Impls/OpenGL/GLInstance.h"
#endif

#ifdef LUNA_RHI_BACKEND_WEBGPU
#include "Impls/WebGPU/WGPUInstance.h"
#endif

namespace luna::RHI {
namespace {

std::string compiledBackendListString()
{
    const std::vector<BackendType> backends = Instance::GetCompiledBackends();
    return DescribeBackendTypes(backends);
}

std::vector<BackendType> backendPreferenceOrder()
{
    std::vector<BackendType> order;
#if defined(_WIN32)
    order = {
        BackendType::DirectX12,
        BackendType::Vulkan,
        BackendType::DirectX11,
        BackendType::OpenGL,
        BackendType::WebGPU,
    };
#elif defined(__APPLE__)
    order = {
        BackendType::Metal,
        BackendType::Vulkan,
        BackendType::WebGPU,
        BackendType::OpenGL,
    };
#elif defined(__ANDROID__)
    order = {
        BackendType::Vulkan,
        BackendType::OpenGLES,
    };
#elif defined(__linux__)
    order = {
        BackendType::Vulkan,
        BackendType::OpenGL,
        BackendType::WebGPU,
    };
#elif defined(__EMSCRIPTEN__)
    order = {
        BackendType::WebGPU,
        BackendType::OpenGLES,
    };
#else
    order = Instance::GetCompiledBackends();
#endif
    return order;
}

template <typename InstanceType>
Ref<Instance> createInitializedInstance(const InstanceCreateInfo& createInfo, std::string_view backend_name)
{
    auto instance = CreateRef<InstanceType>();
    if (!instance->Initialize(createInfo)) {
        throw std::runtime_error("Failed to initialize " + std::string(backend_name) + " instance");
    }
    return instance;
}

} // namespace

std::vector<BackendType> Instance::GetCompiledBackends()
{
    std::vector<BackendType> backends;
#ifdef LUNA_RHI_BACKEND_VULKAN
    backends.push_back(BackendType::Vulkan);
#endif
#ifdef LUNA_RHI_BACKEND_D3D12
    backends.push_back(BackendType::DirectX12);
#endif
#ifdef LUNA_RHI_BACKEND_D3D11
    backends.push_back(BackendType::DirectX11);
#endif
#ifdef LUNA_RHI_BACKEND_METAL
    backends.push_back(BackendType::Metal);
#endif
#ifdef LUNA_RHI_BACKEND_OPENGL
    backends.push_back(BackendType::OpenGL);
#endif
#ifdef LUNA_RHI_BACKEND_OPENGLES
    backends.push_back(BackendType::OpenGLES);
#endif
#ifdef LUNA_RHI_BACKEND_WEBGPU
    backends.push_back(BackendType::WebGPU);
#endif
    return backends;
}

bool Instance::IsBackendCompiled(BackendType backend)
{
    if (backend == BackendType::Auto) {
        return !GetCompiledBackends().empty();
    }

    const std::vector<BackendType> backends = GetCompiledBackends();
    return std::find(backends.begin(), backends.end(), backend) != backends.end();
}

BackendType Instance::GetDefaultBackend()
{
    const std::vector<BackendType> compiled_backends = GetCompiledBackends();
    for (const BackendType backend : backendPreferenceOrder()) {
        if (std::find(compiled_backends.begin(), compiled_backends.end(), backend) != compiled_backends.end()) {
            return backend;
        }
    }

    throw std::runtime_error("No RHI backend was compiled");
}

Ref<Instance> Instance::Create(const InstanceCreateInfo& createInfo)
{
    InstanceCreateInfo resolved = createInfo;
    if (resolved.type == BackendType::Auto) {
        resolved.type = GetDefaultBackend();
    }

    if (!IsBackendCompiled(resolved.type)) {
        throw std::runtime_error("Requested RHI backend '" + std::string(BackendTypeToString(resolved.type)) +
                                 "' is not compiled into this build. Compiled backends: " +
                                 compiledBackendListString());
    }

    switch (resolved.type) {
#ifdef LUNA_RHI_BACKEND_VULKAN
        case BackendType::Vulkan:
            return createInitializedInstance<VKInstance>(resolved, "Vulkan");
#endif

#ifdef LUNA_RHI_BACKEND_D3D12
        case BackendType::DirectX12:
            return createInitializedInstance<D3D12Instance>(resolved, "D3D12");
#endif

#ifdef LUNA_RHI_BACKEND_D3D11
        case BackendType::DirectX11:
            return createInitializedInstance<D3D11Instance>(resolved, "D3D11");
#endif

#ifdef LUNA_RHI_BACKEND_METAL
        case BackendType::Metal:
            return createInitializedInstance<MTLInstance>(resolved, "Metal");
#endif

#if defined(LUNA_RHI_BACKEND_OPENGL) || defined(LUNA_RHI_BACKEND_OPENGLES)
        case BackendType::OpenGL:
        case BackendType::OpenGLES:
            return createInitializedInstance<GLInstance>(resolved, "OpenGL");
#endif

#ifdef LUNA_RHI_BACKEND_WEBGPU
        case BackendType::WebGPU:
            return createInitializedInstance<WGPUInstance>(resolved, "WebGPU");
#endif

        case BackendType::Auto:
        default:
            throw std::runtime_error("Unsupported RHI backend type: " +
                                     std::to_string(static_cast<int>(resolved.type)));
    }
}
} // namespace luna::RHI
