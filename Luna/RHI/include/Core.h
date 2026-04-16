#ifndef CACAO_CORE_H
#define CACAO_CORE_H
#include <cstdint>

#include <filesystem>
#include <fstream>
#include <memory>
#include <ostream>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#ifdef CACAO_STATIC
#define CACAO_API
#elif defined(_WIN32)
#ifdef CACAO_BUILD_DLL
#define CACAO_API __declspec(dllexport)
#else
#define CACAO_API __declspec(dllimport)
#endif
#else
#ifdef CACAO_BUILD_DLL
#define CACAO_API __attribute__((visibility("default")))
#else
#define CACAO_API
#endif
#endif
namespace Cacao {
template <typename T> using Box = std::unique_ptr<T>;
template <typename T> using Ref = std::shared_ptr<T>;

template <typename T, typename... Args> constexpr Box<T> CreateBox(Args&&... args)
{
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename T, typename... Args> constexpr Ref<T> CreateRef(Args&&... args)
{
    return std::make_shared<T>(std::forward<Args>(args)...);
}

template <typename T> struct to_string {
    static std::string Convert(const T& value)
    {
        if constexpr (std::is_arithmetic_v<T>) {
            return std::to_string(value);
        } else {
            return "[Unknown Type]";
        }
    }
};

template <typename T> struct StringProxy {
    const T& Value;
};

template <typename T> std::ostream& operator<<(std::ostream& os, const StringProxy<T>& proxy)
{
    os << to_string<T>::Convert(proxy.Value);
    return os;
}

template <typename T> StringProxy<T> ToString(const T& value)
{
    return StringProxy<T>{value};
}
enum class Format {
    R8_UNORM,
    R8_SNORM,
    R8_UINT,
    R8_SINT,
    RG8_UNORM,
    RG8_SNORM,
    RG8_UINT,
    RG8_SINT,
    RGBA8_UNORM,
    RGBA8_SNORM,
    RGBA8_UINT,
    RGBA8_SINT,
    RGBA8_SRGB,
    BGRA8_UNORM,
    BGRA8_SRGB,
    R16_UNORM,
    R16_SNORM,
    R16_UINT,
    R16_SINT,
    R16_FLOAT,
    RG16_UNORM,
    RG16_SNORM,
    RG16_UINT,
    RG16_SINT,
    RG16_FLOAT,
    RGBA16_UNORM,
    RGBA16_SNORM,
    RGBA16_UINT,
    RGBA16_SINT,
    RGBA16_FLOAT,
    R32_UINT,
    R32_SINT,
    R32_FLOAT,
    RG32_UINT,
    RG32_SINT,
    RG32_FLOAT,
    RGB32_UINT,
    RGB32_SINT,
    RGB32_FLOAT,
    RGBA32_UINT,
    RGBA32_SINT,
    RGBA32_FLOAT,
    RGB10A2_UNORM,
    RGB10A2_UINT,
    RG11B10_FLOAT,
    RGB9E5_FLOAT,
    D16_UNORM,
    D24_UNORM_S8_UINT,
    D32_FLOAT,
    D32_FLOAT_S8_UINT,
    S8_UINT,
    BC1_RGB_UNORM,
    BC1_RGB_SRGB,
    BC1_RGBA_UNORM,
    BC1_RGBA_SRGB,
    BC2_UNORM,
    BC2_SRGB,
    BC3_UNORM,
    BC3_SRGB,
    BC4_UNORM,
    BC4_SNORM,
    BC5_UNORM,
    BC5_SNORM,
    BC6H_UFLOAT,
    BC6H_SFLOAT,
    BC7_UNORM,
    BC7_SRGB,
    D24S8 = D24_UNORM_S8_UINT,
    D32F = D32_FLOAT,
    RGB16_FLOAT = RG11B10_FLOAT,
    UNDEFINED,
};
enum class ColorSpace {
    SRGB_NONLINEAR,
    EXTENDED_SRGB_LINEAR,
    EXTENDED_SRGB_NONLINEAR,
    HDR10_ST2084,
    HDR10_HLG,
    DOLBY_VISION,
    ADOBERGB_LINEAR,
    ADOBERGB_NONLINEAR,
    DISPLAY_P3_NONLINEAR,
    DISPLAY_P3_LINEAR,
    DCI_P3_NONLINEAR,
    BT709_LINEAR,
    BT709_NONLINEAR,
    BT2020_LINEAR,
    BT2020_NONLINEAR,
    PASS_THROUGH,
    LINEAR = EXTENDED_SRGB_LINEAR,
    DISPLAY_P3 = DISPLAY_P3_NONLINEAR,
};

struct Extent2D {
    uint32_t width;
    uint32_t height;
};
enum class ShaderStage : uint32_t {
    None = 0,
    Vertex = 1 << 0,
    Fragment = 1 << 1,
    Compute = 1 << 2,
    Geometry = 1 << 3,
    TessellationControl = 1 << 4,
    TessellationEvaluation = 1 << 5,
    RayGen = 1 << 6,
    RayAnyHit = 1 << 7,
    RayClosestHit = 1 << 8,
    RayMiss = 1 << 9,
    RayIntersection = 1 << 10,
    Callable = 1 << 11,
    Mesh = 1 << 12,
    Task = 1 << 13,
    AllGraphics = Vertex | Fragment | Geometry | TessellationControl | TessellationEvaluation,
    AllRayTracing = RayGen | RayAnyHit | RayClosestHit | RayMiss | RayIntersection | Callable,
    AllMeshShading = Mesh | Task,
    All = 0xFF'FF'FF'FF
};

inline ShaderStage operator|(ShaderStage a, ShaderStage b)
{
    return static_cast<ShaderStage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline ShaderStage& operator|=(ShaderStage& a, ShaderStage b)
{
    a = a | b;
    return a;
}

inline bool operator&(ShaderStage a, ShaderStage b)
{
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}
} // namespace Cacao
#endif
