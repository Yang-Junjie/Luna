#pragma once

#include "RHI/RHIDevice.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

namespace ibl_lab {

enum class Page : uint8_t {
    Overview = 0,
    SourceHdr,
    EnvCube,
    Irradiance,
    Prefilter,
    BrdfLut,
    Skybox,
    Inspector
};

enum class ProbeKind : uint8_t {
    None = 0,
    CubeFacesDistinct,
    SkyboxRotation,
    FaceMipPreview,
    FaceIsolation,
    EnvMips,
    IrradianceVsEnv,
    PrefilterMips,
    BrdfLutPreview
};

inline uint32_t calculate_mip_count(uint32_t width, uint32_t height)
{
    uint32_t maxDimension = std::max(width, height);
    uint32_t mipCount = 0;
    do {
        ++mipCount;
        maxDimension >>= 1u;
    } while (maxDimension > 0);
    return mipCount;
}

struct FormatRequirementStatus {
    luna::PixelFormat format = luna::PixelFormat::Undefined;
    const char* purpose = "";
    luna::RHIFormatSupport support{};
    bool ready = false;
    bool requireSampled = true;
    bool requireColorAttachment = true;
};

struct ProbeState {
    ProbeKind request = ProbeKind::None;
    ProbeKind completed = ProbeKind::None;
    bool ready = false;
    bool passed = false;
    std::string summary;
};

struct State {
    Page page = Page::Overview;
    bool pipelineReady = false;
    bool sourceHdrReady = false;
    bool envCubeReady = false;
    bool irradianceReady = false;
    bool prefilterReady = false;
    bool prefilterGenerated = false;
    bool brdfLutReady = false;
    bool anyBlockedFormats = false;
    uint64_t frameCounter = 0;
    uint32_t sourceWidth = 0;
    uint32_t sourceHeight = 0;
    uint32_t sourceChannels = 0;
    uint32_t cubeSize = 256;
    uint32_t cubeMipLevels = calculate_mip_count(256, 256);
    uint32_t cubeFaceCount = 6;
    uint32_t irradianceSize = 32;
    uint32_t irradianceMipLevels = 1;
    uint32_t prefilterSize = 256;
    uint32_t prefilterMipLevels = calculate_mip_count(256, 256);
    uint32_t brdfLutWidth = 0;
    uint32_t brdfLutHeight = 0;
    float envPreviewLod = 0.0f;
    int selectedFace = 0;
    int selectedMip = 0;
    int irradianceFace = 0;
    int prefilterFace = 0;
    int prefilterMip = 0;
    bool stampRequested = false;
    uint32_t stampSerial = 0;
    bool regenerateEnvCubeRequested = false;
    bool regenerateIrradianceRequested = false;
    bool regeneratePrefilterRequested = false;
    float skyYaw = 0.0f;
    float skyPitch = 0.0f;
    std::string shaderRoot;
    std::string sourcePath;
    std::string brdfLutPath;
    std::string renderStatus;
    std::string capabilityStatus;
    std::string sourceHdrStatus;
    std::string envCubeStatus;
    std::string irradianceStatus;
    std::string prefilterStatus;
    std::string brdfLutStatus;
    std::string skyboxStatus;
    std::string inspectorStatus;
    ProbeState probe;
    std::array<FormatRequirementStatus, 2> requiredFormats = {{
        {.format = luna::PixelFormat::RGBA16Float,
         .purpose = "HDR Env / Irradiance / Prefilter",
         .support = {},
         .ready = false,
         .requireSampled = true,
         .requireColorAttachment = true},
        {.format = luna::PixelFormat::RG16Float,
         .purpose = "Offline BRDF LUT",
         .support = {},
         .ready = false,
         .requireSampled = true,
         .requireColorAttachment = false},
    }};
};

} // namespace ibl_lab
