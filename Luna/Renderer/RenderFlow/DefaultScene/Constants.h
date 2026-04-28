#pragma once

#include <Core.h>

#include <cstdint>

namespace luna::render_flow::default_scene_detail {

inline constexpr luna::RHI::Format kGBufferBaseColorFormat = luna::RHI::Format::RGBA8_UNORM;
inline constexpr luna::RHI::Format kGBufferLightingFormat = luna::RHI::Format::RGBA16_FLOAT;
inline constexpr luna::RHI::Format kVelocityFormat = luna::RHI::Format::RG16_FLOAT;
inline constexpr luna::RHI::Format kScenePickingFormat = luna::RHI::Format::R32_UINT;
inline constexpr luna::RHI::Format kShadowMapFormat = luna::RHI::Format::R32_FLOAT;
inline constexpr uint32_t kShadowMapSize = 2048;

inline constexpr luna::RHI::Format kEnvironmentFormat = luna::RHI::Format::RGBA32_FLOAT;
inline constexpr luna::RHI::Format kEnvironmentIblFormat = luna::RHI::Format::RGBA16_FLOAT;
inline constexpr luna::RHI::Format kEnvironmentBrdfLutFormat = luna::RHI::Format::RGBA16_FLOAT;
inline constexpr uint32_t kEnvironmentCubeSize = 512;
inline constexpr uint32_t kEnvironmentIrradianceCubeSize = 32;
inline constexpr uint32_t kEnvironmentPrefilterCubeSize = 128;
inline constexpr uint32_t kEnvironmentPrefilterMipLevels = 5;
inline constexpr uint32_t kEnvironmentBrdfLutSize = 256;
inline constexpr uint32_t kEnvironmentIrradianceSampleCount = 64;
inline constexpr uint32_t kEnvironmentPrefilterSampleCount = 128;
inline constexpr uint32_t kEnvironmentBrdfSampleCount = 128;
inline constexpr float kEnvironmentFallbackValue = 0.08f;

inline constexpr float kDefaultMaterialAlphaCutoff = 0.5f;
inline constexpr uint32_t kMaxPointLights = 32;
inline constexpr uint32_t kMaxSpotLights = 32;

} // namespace luna::render_flow::default_scene_detail
