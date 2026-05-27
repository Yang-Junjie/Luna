#pragma once

// Shared helper functions for renderer setup and diagnostics.
// Keeps small backend-independent decisions such as adapter selection,
// surface format choice, and logging-friendly enum formatting out of core classes.

#include <Adapter.h>
#include <Instance.h>
#include <string>
#include <Surface.h>
#include <Swapchain.h>
#include <vector>

namespace luna::renderer_detail {

RHI::Ref<RHI::Adapter> selectAdapter(const std::vector<RHI::Ref<RHI::Adapter>>& adapters);
RHI::SurfaceFormat chooseSurfaceFormat(const std::vector<RHI::SurfaceFormat>& formats);
const char* presentModeToString(RHI::PresentMode mode);
const char* swapchainResultToString(RHI::Result result);
const char* adapterTypeToString(RHI::AdapterType type);
const char* formatToString(RHI::Format format);
bool supportsDefaultRenderFlow(RHI::BackendType type);
bool isPresentModeSupported(const std::vector<RHI::PresentMode>& supported_modes, RHI::PresentMode mode);
std::string describePresentModes(const std::vector<RHI::PresentMode>& supported_modes);
RHI::PresentMode choosePresentMode(const std::vector<RHI::PresentMode>& supported_modes,
                                   RHI::PresentMode requested_mode);

} // namespace luna::renderer_detail

#if defined(LUNA_RENDERER_ENABLE_FRAME_LOGS)
#define LUNA_RENDERER_FRAME_TRACE(...) LUNA_RENDERER_TRACE(__VA_ARGS__)
#define LUNA_RENDERER_FRAME_DEBUG(...) LUNA_RENDERER_DEBUG(__VA_ARGS__)
#else
#define LUNA_RENDERER_FRAME_TRACE(...) ((void) 0)
#define LUNA_RENDERER_FRAME_DEBUG(...) ((void) 0)
#endif
