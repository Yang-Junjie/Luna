#include "Renderer/RendererUtilities.h"

#include <algorithm>
#include <array>
#include <Capabilities.h>

namespace luna::renderer_detail {

RHI::Ref<RHI::Adapter> selectAdapter(const std::vector<RHI::Ref<RHI::Adapter>>& adapters)
{
    if (adapters.empty()) {
        return {};
    }

    const auto discrete_adapter =
        std::find_if(adapters.begin(), adapters.end(), [](const RHI::Ref<RHI::Adapter>& adapter) {
            return adapter && adapter->GetAdapterType() == RHI::AdapterType::Discrete;
        });
    return discrete_adapter != adapters.end() ? *discrete_adapter : adapters.front();
}

RHI::SurfaceFormat chooseSurfaceFormat(const std::vector<RHI::SurfaceFormat>& formats)
{
    const auto preferred = std::find_if(formats.begin(), formats.end(), [](const RHI::SurfaceFormat& format) {
        return format.format == RHI::Format::BGRA8_UNORM &&
               format.colorSpace == RHI::ColorSpace::SRGB_NONLINEAR;
    });
    if (preferred != formats.end()) {
        return *preferred;
    }

    const auto fallback = std::find_if(formats.begin(), formats.end(), [](const RHI::SurfaceFormat& format) {
        return format.format == RHI::Format::RGBA8_UNORM || format.format == RHI::Format::BGRA8_UNORM;
    });
    if (fallback != formats.end()) {
        return *fallback;
    }

    return formats.empty()
               ? RHI::SurfaceFormat{RHI::Format::BGRA8_UNORM, RHI::ColorSpace::SRGB_NONLINEAR}
               : formats.front();
}

const char* presentModeToString(RHI::PresentMode mode)
{
    switch (mode) {
        case RHI::PresentMode::Immediate:
            return "Immediate";
        case RHI::PresentMode::Mailbox:
            return "Mailbox";
        case RHI::PresentMode::Fifo:
            return "Fifo";
        case RHI::PresentMode::FifoRelaxed:
            return "FifoRelaxed";
        default:
            return "Unknown";
    }
}

const char* swapchainResultToString(RHI::Result result)
{
    switch (result) {
        case RHI::Result::Success:
            return "Success";
        case RHI::Result::Timeout:
            return "Timeout";
        case RHI::Result::NotReady:
            return "NotReady";
        case RHI::Result::Suboptimal:
            return "Suboptimal";
        case RHI::Result::OutOfDate:
            return "OutOfDate";
        case RHI::Result::DeviceLost:
            return "DeviceLost";
        case RHI::Result::Error:
        default:
            return "Error";
    }
}

const char* adapterTypeToString(RHI::AdapterType type)
{
    switch (type) {
        case RHI::AdapterType::Discrete:
            return "Discrete";
        case RHI::AdapterType::Integrated:
            return "Integrated";
        case RHI::AdapterType::Software:
            return "Software";
        case RHI::AdapterType::Unknown:
        default:
            return "Unknown";
    }
}

const char* formatToString(RHI::Format format)
{
    switch (format) {
        case RHI::Format::RGBA8_UNORM:
            return "RGBA8_UNORM";
        case RHI::Format::RGBA8_SRGB:
            return "RGBA8_SRGB";
        case RHI::Format::BGRA8_UNORM:
            return "BGRA8_UNORM";
        case RHI::Format::BGRA8_SRGB:
            return "BGRA8_SRGB";
        case RHI::Format::R32_UINT:
            return "R32_UINT";
        case RHI::Format::D32_FLOAT:
            return "D32_FLOAT";
        case RHI::Format::RGBA16_FLOAT:
            return "RGBA16_FLOAT";
        case RHI::Format::RGBA32_FLOAT:
            return "RGBA32_FLOAT";
        case RHI::Format::UNDEFINED:
            return "UNDEFINED";
        default:
            return "Unknown";
    }
}

bool supportsDefaultRenderFlow(RHI::BackendType type)
{
    return RHI::makeCapabilitiesForBackend(type).supports_default_render_flow;
}

bool isPresentModeSupported(const std::vector<RHI::PresentMode>& supported_modes, RHI::PresentMode mode)
{
    return std::find(supported_modes.begin(), supported_modes.end(), mode) != supported_modes.end();
}

std::string describePresentModes(const std::vector<RHI::PresentMode>& supported_modes)
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

RHI::PresentMode choosePresentMode(const std::vector<RHI::PresentMode>& supported_modes,
                                         RHI::PresentMode requested_mode)
{
    if (isPresentModeSupported(supported_modes, requested_mode)) {
        return requested_mode;
    }

    switch (requested_mode) {
        case RHI::PresentMode::Mailbox:
            for (const auto fallback_mode : std::array{RHI::PresentMode::Immediate,
                                                       RHI::PresentMode::FifoRelaxed,
                                                       RHI::PresentMode::Fifo}) {
                if (isPresentModeSupported(supported_modes, fallback_mode)) {
                    return fallback_mode;
                }
            }
            break;
        case RHI::PresentMode::Immediate:
            for (const auto fallback_mode : std::array{RHI::PresentMode::Mailbox,
                                                       RHI::PresentMode::FifoRelaxed,
                                                       RHI::PresentMode::Fifo}) {
                if (isPresentModeSupported(supported_modes, fallback_mode)) {
                    return fallback_mode;
                }
            }
            break;
        case RHI::PresentMode::FifoRelaxed:
            for (const auto fallback_mode : std::array{RHI::PresentMode::Fifo,
                                                       RHI::PresentMode::Immediate,
                                                       RHI::PresentMode::Mailbox}) {
                if (isPresentModeSupported(supported_modes, fallback_mode)) {
                    return fallback_mode;
                }
            }
            break;
        case RHI::PresentMode::Fifo:
            if (isPresentModeSupported(supported_modes, RHI::PresentMode::FifoRelaxed)) {
                return RHI::PresentMode::FifoRelaxed;
            }
            break;
        default:
            break;
    }

    return supported_modes.empty() ? RHI::PresentMode::Fifo : supported_modes.front();
}

} // namespace luna::renderer_detail




