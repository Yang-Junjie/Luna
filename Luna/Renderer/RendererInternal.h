#pragma once

#include <Adapter.h>
#include <algorithm>
#include <array>
#include <Instance.h>
#include <string>
#include <Surface.h>
#include <vector>

namespace luna::renderer_detail {

inline luna::RHI::Ref<luna::RHI::Adapter> selectAdapter(const std::vector<luna::RHI::Ref<luna::RHI::Adapter>>& adapters)
{
    if (adapters.empty()) {
        return {};
    }

    const auto discrete_adapter =
        std::find_if(adapters.begin(), adapters.end(), [](const luna::RHI::Ref<luna::RHI::Adapter>& adapter) {
            return adapter && adapter->GetAdapterType() == luna::RHI::AdapterType::Discrete;
        });
    return discrete_adapter != adapters.end() ? *discrete_adapter : adapters.front();
}

inline luna::RHI::SurfaceFormat chooseSurfaceFormat(const std::vector<luna::RHI::SurfaceFormat>& formats)
{
    const auto preferred = std::find_if(formats.begin(), formats.end(), [](const luna::RHI::SurfaceFormat& format) {
        return format.format == luna::RHI::Format::BGRA8_UNORM &&
               format.colorSpace == luna::RHI::ColorSpace::SRGB_NONLINEAR;
    });
    if (preferred != formats.end()) {
        return *preferred;
    }

    const auto fallback = std::find_if(formats.begin(), formats.end(), [](const luna::RHI::SurfaceFormat& format) {
        return format.format == luna::RHI::Format::RGBA8_UNORM || format.format == luna::RHI::Format::BGRA8_UNORM;
    });
    if (fallback != formats.end()) {
        return *fallback;
    }

    return formats.empty()
               ? luna::RHI::SurfaceFormat{luna::RHI::Format::BGRA8_UNORM, luna::RHI::ColorSpace::SRGB_NONLINEAR}
               : formats.front();
}

inline const char* presentModeToString(luna::RHI::PresentMode mode)
{
    switch (mode) {
        case luna::RHI::PresentMode::Immediate:
            return "Immediate";
        case luna::RHI::PresentMode::Mailbox:
            return "Mailbox";
        case luna::RHI::PresentMode::Fifo:
            return "Fifo";
        case luna::RHI::PresentMode::FifoRelaxed:
            return "FifoRelaxed";
        default:
            return "Unknown";
    }
}

inline const char* backendTypeToString(luna::RHI::BackendType type)
{
    switch (type) {
        case luna::RHI::BackendType::Auto:
            return "Auto";
        case luna::RHI::BackendType::Vulkan:
            return "Vulkan";
        case luna::RHI::BackendType::DirectX12:
            return "DirectX12";
        case luna::RHI::BackendType::DirectX11:
            return "DirectX11";
        case luna::RHI::BackendType::Metal:
            return "Metal";
        case luna::RHI::BackendType::OpenGL:
            return "OpenGL";
        case luna::RHI::BackendType::OpenGLES:
            return "OpenGLES";
        case luna::RHI::BackendType::WebGPU:
            return "WebGPU";
        default:
            return "Unknown";
    }
}

inline bool usesSceneRenderer(luna::RHI::BackendType type)
{
    return type == luna::RHI::BackendType::Vulkan || type == luna::RHI::BackendType::DirectX11 ||
           type == luna::RHI::BackendType::DirectX12;
}

inline bool isPresentModeSupported(const std::vector<luna::RHI::PresentMode>& supported_modes,
                                   luna::RHI::PresentMode mode)
{
    return std::find(supported_modes.begin(), supported_modes.end(), mode) != supported_modes.end();
}

inline std::string describePresentModes(const std::vector<luna::RHI::PresentMode>& supported_modes)
{
    if (supported_modes.empty()) {
        return "<none>";
    }

    std::string result;
    for (size_t i = 0; i < supported_modes.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }
        result += presentModeToString(supported_modes[i]);
    }
    return result;
}

inline luna::RHI::PresentMode choosePresentMode(const std::vector<luna::RHI::PresentMode>& supported_modes,
                                                luna::RHI::PresentMode requested_mode)
{
    if (isPresentModeSupported(supported_modes, requested_mode)) {
        return requested_mode;
    }

    switch (requested_mode) {
        case luna::RHI::PresentMode::Mailbox:
            for (const auto fallback_mode : std::array{luna::RHI::PresentMode::Immediate,
                                                       luna::RHI::PresentMode::FifoRelaxed,
                                                       luna::RHI::PresentMode::Fifo}) {
                if (isPresentModeSupported(supported_modes, fallback_mode)) {
                    return fallback_mode;
                }
            }
            break;
        case luna::RHI::PresentMode::Immediate:
            for (const auto fallback_mode : std::array{luna::RHI::PresentMode::Mailbox,
                                                       luna::RHI::PresentMode::FifoRelaxed,
                                                       luna::RHI::PresentMode::Fifo}) {
                if (isPresentModeSupported(supported_modes, fallback_mode)) {
                    return fallback_mode;
                }
            }
            break;
        case luna::RHI::PresentMode::FifoRelaxed:
            for (const auto fallback_mode : std::array{luna::RHI::PresentMode::Fifo,
                                                       luna::RHI::PresentMode::Immediate,
                                                       luna::RHI::PresentMode::Mailbox}) {
                if (isPresentModeSupported(supported_modes, fallback_mode)) {
                    return fallback_mode;
                }
            }
            break;
        case luna::RHI::PresentMode::Fifo:
            if (isPresentModeSupported(supported_modes, luna::RHI::PresentMode::FifoRelaxed)) {
                return luna::RHI::PresentMode::FifoRelaxed;
            }
            break;
        default:
            break;
    }

    return supported_modes.empty() ? luna::RHI::PresentMode::Fifo : supported_modes.front();
}

} // namespace luna::renderer_detail
