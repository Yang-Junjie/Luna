#pragma once

#include <Core.h>

#include <cstdint>

namespace luna::render_flow::default_scene_detail {

inline constexpr RHI::Format kGBufferBaseColorFormat = RHI::Format::RGBA8_UNORM;
inline constexpr RHI::Format kGBufferLightingFormat = RHI::Format::RGBA16_FLOAT;
inline constexpr RHI::Format kSceneHdrColorFormat = RHI::Format::RGBA16_FLOAT;
inline constexpr RHI::Format kVelocityFormat = RHI::Format::RG16_FLOAT;
inline constexpr RHI::Format kScenePickingFormat = RHI::Format::R32_UINT;
inline constexpr RHI::Format kShadowMapFormat = RHI::Format::R32_FLOAT;
inline constexpr uint32_t kShadowCascadeCount = 4;
inline constexpr uint32_t kShadowCascadeAtlasColumns = 2;
inline constexpr uint32_t kShadowCascadeAtlasRows = 2;
inline constexpr uint32_t kShadowCascadeTileSize = 2048;
inline constexpr uint32_t kShadowCascadeAtlasSize = kShadowCascadeTileSize * kShadowCascadeAtlasColumns;

inline constexpr RHI::Format kEnvironmentFormat = RHI::Format::RGBA32_FLOAT;
inline constexpr RHI::Format kEnvironmentIblFormat = RHI::Format::RGBA16_FLOAT;
inline constexpr RHI::Format kEnvironmentBrdfLutFormat = RHI::Format::RGBA16_FLOAT;
inline constexpr uint32_t kEnvironmentCubeSize = 512;
inline constexpr uint32_t kEnvironmentIrradianceCubeSize = 32;
inline constexpr uint32_t kEnvironmentPrefilterCubeSize = 128;
inline constexpr uint32_t kEnvironmentPrefilterMipLevels = 8;
inline constexpr uint32_t kEnvironmentBrdfLutSize = 256;
inline constexpr uint32_t kEnvironmentIrradianceSampleCount = 64;
inline constexpr uint32_t kEnvironmentPrefilterSampleCount = 1024;
inline constexpr uint32_t kEnvironmentBrdfSampleCount = 128;
inline constexpr float kEnvironmentFallbackValue = 0.08f;

inline constexpr float kDefaultMaterialAlphaCutoff = 0.5f;
inline constexpr uint32_t kMaxPointLights = 32;
inline constexpr uint32_t kMaxSpotLights = 32;

} // namespace luna::render_flow::default_scene_detail
