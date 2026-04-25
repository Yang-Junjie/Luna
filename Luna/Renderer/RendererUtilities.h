#pragma once

// Shared helper functions for renderer setup and diagnostics.
// Keeps small backend-independent decisions such as adapter selection,
// surface format choice, and logging-friendly enum formatting out of core classes.

#include <Adapter.h>
#include <Instance.h>
#include <Surface.h>

#include <string>
#include <vector>

namespace luna::renderer_detail {

luna::RHI::Ref<luna::RHI::Adapter> selectAdapter(const std::vector<luna::RHI::Ref<luna::RHI::Adapter>>& adapters);
luna::RHI::SurfaceFormat chooseSurfaceFormat(const std::vector<luna::RHI::SurfaceFormat>& formats);
const char* presentModeToString(luna::RHI::PresentMode mode);
const char* adapterTypeToString(luna::RHI::AdapterType type);
const char* backendTypeToString(luna::RHI::BackendType type);
const char* formatToString(luna::RHI::Format format);
bool supportsDefaultRenderFlow(luna::RHI::BackendType type);
bool isPresentModeSupported(const std::vector<luna::RHI::PresentMode>& supported_modes, luna::RHI::PresentMode mode);
std::string describePresentModes(const std::vector<luna::RHI::PresentMode>& supported_modes);
luna::RHI::PresentMode choosePresentMode(const std::vector<luna::RHI::PresentMode>& supported_modes,
                                         luna::RHI::PresentMode requested_mode);

} // namespace luna::renderer_detail

#if defined(LUNA_RENDERER_ENABLE_FRAME_LOGS)
#define LUNA_RENDERER_FRAME_TRACE(...) LUNA_RENDERER_TRACE(__VA_ARGS__)
#define LUNA_RENDERER_FRAME_DEBUG(...) LUNA_RENDERER_DEBUG(__VA_ARGS__)
#else
#define LUNA_RENDERER_FRAME_TRACE(...) ((void) 0)
#define LUNA_RENDERER_FRAME_DEBUG(...) ((void) 0)
#endif




