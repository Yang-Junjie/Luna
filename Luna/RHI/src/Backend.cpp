#include <Backend.h>

#include <algorithm>
#include <cctype>

namespace luna::RHI {

const char* BackendTypeToString(BackendType type) noexcept
{
    switch (type) {
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

std::optional<BackendType> ParseBackendType(std::string_view value)
{
    std::string normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (normalized == "auto") {
        return BackendType::Auto;
    }
    if (normalized == "vulkan" || normalized == "vk") {
        return BackendType::Vulkan;
    }
    if (normalized == "d3d12" || normalized == "dx12" || normalized == "directx12") {
        return BackendType::DirectX12;
    }
    if (normalized == "d3d11" || normalized == "dx11" || normalized == "directx11") {
        return BackendType::DirectX11;
    }
    if (normalized == "metal" || normalized == "mtl") {
        return BackendType::Metal;
    }
    if (normalized == "opengl" || normalized == "gl") {
        return BackendType::OpenGL;
    }
    if (normalized == "opengles" || normalized == "gles") {
        return BackendType::OpenGLES;
    }
    if (normalized == "webgpu" || normalized == "wgpu") {
        return BackendType::WebGPU;
    }

    return std::nullopt;
}

std::string DescribeBackendTypes(std::span<const BackendType> backends)
{
    if (backends.empty()) {
        return "none";
    }

    std::string result;
    for (const BackendType backend : backends) {
        if (!result.empty()) {
            result += ", ";
        }
        result += BackendTypeToString(backend);
    }
    return result;
}

} // namespace luna::RHI
