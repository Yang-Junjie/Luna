#ifndef LUNA_RHI_BACKEND_H
#define LUNA_RHI_BACKEND_H

#include "Core.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace luna::RHI {

enum class BackendType {
    Auto,
    Vulkan,
    DirectX12,
    DirectX11,
    Metal,
    OpenGL,
    OpenGLES,
    WebGPU,
};

[[nodiscard]] LUNA_RHI_API const char* BackendTypeToString(BackendType type) noexcept;
[[nodiscard]] LUNA_RHI_API std::optional<BackendType> ParseBackendType(std::string_view value);
[[nodiscard]] LUNA_RHI_API std::string DescribeBackendTypes(std::span<const BackendType> backends);

template <> struct to_string<BackendType> {
    static std::string Convert(BackendType type)
    {
        return BackendTypeToString(type);
    }
};

} // namespace luna::RHI

#endif
