#include "RHI/RHIDevice.h"

#include "Vulkan/vk_rhi_device.h"

namespace luna {

RHICapabilities QueryRHICapabilities(RHIBackend backend) noexcept
{
    switch (backend) {
        case RHIBackend::Vulkan:
            return {
                .backend = backend,
                .implemented = true,
                .supportsGraphics = true,
                .supportsPresent = true,
                .supportsDynamicRendering = true,
                .supportsIndexedDraw = true,
                .supportsResourceSets = true,
                .framesInFlight = 2,
            };
        case RHIBackend::D3D12:
        case RHIBackend::Metal:
            return {
                .backend = backend,
                .implemented = false,
                .supportsGraphics = true,
                .supportsPresent = true,
                .supportsDynamicRendering = true,
                .supportsIndexedDraw = true,
                .supportsResourceSets = true,
                .framesInFlight = 2,
            };
        default:
            return {
                .backend = backend,
                .implemented = false,
            };
    }
}

bool IsBackendImplemented(RHIBackend backend) noexcept
{
    return QueryRHICapabilities(backend).implemented;
}

std::unique_ptr<IRHIDevice> CreateRHIDevice(RHIBackend backend)
{
    switch (backend) {
        case RHIBackend::Vulkan:
            return std::make_unique<VulkanRHIDevice>();
        case RHIBackend::D3D12:
        case RHIBackend::Metal:
        default:
            return nullptr;
    }
}

std::vector<std::unique_ptr<IRHIAdapter>> EnumerateRHIAdapters(RHIBackend backend)
{
    switch (backend) {
        case RHIBackend::Vulkan:
            return EnumerateVulkanAdapters();
        case RHIBackend::D3D12:
        case RHIBackend::Metal:
        default:
            return {};
    }
}

} // namespace luna
