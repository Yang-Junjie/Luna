#include "IblLabPipeline.h"

#include "Core/log.h"
#include "RHI/CommandContext.h"
#include "RHI/ResourceLayout.h"

#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace ibl_lab {
namespace {

struct alignas(16) PreviewPushConstants {
    float params[4] = {};
};

constexpr uint32_t kCubeFaceCount = 6;
constexpr uint32_t kProbeWidth = 384;
constexpr uint32_t kProbeHeight = 256;
constexpr float kHalfPi = 1.57079632679f;
constexpr float kPi = 3.14159265359f;
constexpr luna::PixelFormat kEnvCubeFormat = luna::PixelFormat::RGBA16Float;
constexpr luna::PixelFormat kBrdfLutFormat = luna::PixelFormat::RG16Float;
constexpr uint32_t kIrradianceCubeSize = 32u;
constexpr size_t kProbeTexelBytesRGBA16F = 8u;
constexpr size_t kCubeDistinctProbeSampleCount = 8u;
constexpr size_t kProbeBufferBytes = 512u;
constexpr uint32_t kBrdfLutFileMagic = 0x544C4442u;
constexpr uint32_t kBrdfLutFileVersion = 1u;
constexpr uint32_t kBrdfLutFileFormatRG16Float = 1u;

const std::array<const char*, kCubeFaceCount> kFaceLabels = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
static_assert(kProbeBufferBytes >= kCubeFaceCount * kCubeDistinctProbeSampleCount * kProbeTexelBytesRGBA16F);

struct BrdfLutFileHeader {
    uint32_t magic = 0;
    uint32_t version = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;
};

std::string shader_path(const std::string& root, std::string_view fileName)
{
    return (std::filesystem::path(root) / fileName).lexically_normal().generic_string();
}

size_t frame_slot(uint32_t frameIndex, size_t framesInFlight)
{
    return framesInFlight == 0 ? 0 : static_cast<size_t>(frameIndex % static_cast<uint32_t>(framesInFlight));
}

uint8_t to_u8(float value)
{
    return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

glm::vec3 tone_map_reinhard(glm::vec3 color)
{
    const glm::vec3 mapped = color / (glm::vec3(1.0f) + glm::max(color, glm::vec3(0.0f)));
    return glm::pow(glm::clamp(mapped, glm::vec3(0.0f), glm::vec3(1.0f)), glm::vec3(1.0f / 2.2f));
}

void write_rgba(std::vector<uint8_t>& pixels, size_t offset, float r, float g, float b, float a)
{
    pixels[offset + 0] = to_u8(r);
    pixels[offset + 1] = to_u8(g);
    pixels[offset + 2] = to_u8(b);
    pixels[offset + 3] = to_u8(a);
}

uint32_t pack_rgba_u32(const uint8_t* bytes)
{
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8u) |
           (static_cast<uint32_t>(bytes[2]) << 16u) | (static_cast<uint32_t>(bytes[3]) << 24u);
}

std::string hex_u32(uint32_t value)
{
    std::ostringstream builder;
    builder << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << value;
    return builder.str();
}

uint32_t hash_bytes_fnv1a(const uint8_t* bytes, size_t size)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) {
        hash ^= static_cast<uint32_t>(bytes[i]);
        hash *= 16777619u;
    }
    return hash;
}

std::array<float, 4> unpack_rgba16f(const uint8_t* bytes)
{
    const auto read_half = [&](size_t component) {
        const uint16_t packed =
            static_cast<uint16_t>(bytes[component * 2u]) | (static_cast<uint16_t>(bytes[component * 2u + 1u]) << 8u);
        return glm::unpackHalf1x16(packed);
    };

    return {read_half(0), read_half(1), read_half(2), read_half(3)};
}

std::string format_float4(const std::array<float, 4>& value)
{
    std::ostringstream builder;
    builder << std::fixed << std::setprecision(3) << "(" << value[0] << ", " << value[1] << ", " << value[2] << ", "
            << value[3] << ")";
    return builder.str();
}

const char* probe_kind_label(ProbeKind kind)
{
    switch (kind) {
        case ProbeKind::CubeFacesDistinct:
            return "CubeFacesDistinct";
        case ProbeKind::SkyboxRotation:
            return "SkyboxRotation";
        case ProbeKind::FaceMipPreview:
            return "FaceMipPreview";
        case ProbeKind::FaceIsolation:
            return "FaceIsolation";
        case ProbeKind::EnvMips:
            return "EnvMips";
        case ProbeKind::IrradianceVsEnv:
            return "IrradianceVsEnv";
        case ProbeKind::PrefilterMips:
            return "PrefilterMips";
        case ProbeKind::BrdfLutPreview:
            return "BrdfLutPreview";
        case ProbeKind::None:
        default:
            return "None";
    }
}

int clamp_face_index(int faceIndex)
{
    return std::clamp(faceIndex, 0, static_cast<int>(kCubeFaceCount - 1u));
}

luna::ClearColorValue stamp_color(uint32_t faceIndex, uint32_t mipLevel, uint32_t serial)
{
    const std::array<luna::ClearColorValue, kCubeFaceCount> palette = {{
        {.r = 0.96f, .g = 0.20f, .b = 0.26f, .a = 1.0f},
        {.r = 0.14f, .g = 0.80f, .b = 0.32f, .a = 1.0f},
        {.r = 0.18f, .g = 0.42f, .b = 0.98f, .a = 1.0f},
        {.r = 0.98f, .g = 0.82f, .b = 0.22f, .a = 1.0f},
        {.r = 0.88f, .g = 0.30f, .b = 0.88f, .a = 1.0f},
        {.r = 0.18f, .g = 0.90f, .b = 0.92f, .a = 1.0f},
    }};

    luna::ClearColorValue color = palette[faceIndex % palette.size()];
    const float mipFade = 0.08f * static_cast<float>(mipLevel % 4u);
    const float serialShift = 0.04f * static_cast<float>(serial % 3u);
    color.r = std::clamp(color.r - mipFade + serialShift, 0.0f, 1.0f);
    color.g = std::clamp(color.g - serialShift, 0.0f, 1.0f);
    color.b = std::clamp(color.b - 0.5f * mipFade + serialShift, 0.0f, 1.0f);
    return color;
}

bool all_unique(const std::array<uint32_t, kCubeFaceCount>& values)
{
    for (size_t i = 0; i < values.size(); ++i) {
        for (size_t j = i + 1; j < values.size(); ++j) {
            if (values[i] == values[j]) {
                return false;
            }
        }
    }
    return true;
}

uint32_t normalized_probe_coord(uint32_t extent, float normalized)
{
    if (extent <= 1u) {
        return 0;
    }
    const float clamped = std::clamp(normalized, 0.0f, 1.0f);
    return std::min(extent - 1u, static_cast<uint32_t>(clamped * static_cast<float>(extent - 1u) + 0.5f));
}

std::array<float, 3> face_direction(uint32_t faceIndex, float u, float v)
{
    float px = u * 2.0f - 1.0f;
    float py = -(v * 2.0f - 1.0f);
    glm::vec3 direction{0.0f};
    switch (faceIndex) {
        case 0:
            direction = glm::normalize(glm::vec3(1.0f, py, -px));
            break;
        case 1:
            direction = glm::normalize(glm::vec3(-1.0f, py, px));
            break;
        case 2:
            direction = glm::normalize(glm::vec3(px, 1.0f, -py));
            break;
        case 3:
            direction = glm::normalize(glm::vec3(px, -1.0f, py));
            break;
        case 4:
            direction = glm::normalize(glm::vec3(px, py, 1.0f));
            break;
        case 5:
        default:
            direction = glm::normalize(glm::vec3(-px, py, -1.0f));
            break;
    }
    return {direction.x, direction.y, direction.z};
}

} // namespace

RhiIblLabRenderPipeline::RhiIblLabRenderPipeline(std::shared_ptr<State> state)
    : m_state(std::move(state))
{}

bool RhiIblLabRenderPipeline::init(luna::IRHIDevice& device)
{
#ifndef RHI_IBL_LAB_SHADER_ROOT
#error "RHI_IBL_LAB_SHADER_ROOT must be defined for RhiIblLab"
#endif
    m_device = &device;
    m_shaderRoot = std::filesystem::path{RHI_IBL_LAB_SHADER_ROOT}.lexically_normal().generic_string();
    if (m_state != nullptr) {
        m_state->shaderRoot = m_shaderRoot;
        m_state->renderStatus = "Waiting for first frame.";
        m_state->sourceHdrStatus = "Waiting to load assets/newport_loft.hdr.";
        m_state->envCubeStatus = "Waiting for HDR source before generating the environment cube.";
        m_state->irradianceStatus = "Waiting for the environment cube before generating irradiance.";
        m_state->prefilterStatus = "Waiting for the environment cube before allocating the prefilter cubemap.";
        m_state->brdfLutStatus = "Waiting to load the offline BRDF LUT from disk.";
        m_state->skyboxStatus = "Drag with left mouse button in Skybox mode to orbit the generated environment.";
        m_state->inspectorStatus = "Select a face and mip, then stamp the selected subresource.";
        m_state->irradianceSize = kIrradianceCubeSize;
        m_state->irradianceMipLevels = 1u;
        m_state->prefilterSize = std::max(32u, m_state->cubeSize);
        m_state->prefilterMipLevels = calculate_mip_count(m_state->prefilterSize, m_state->prefilterSize);
        m_state->anyBlockedFormats = false;
        for (auto& requirement : m_state->requiredFormats) {
            requirement.support = device.queryFormatSupport(requirement.format);
            requirement.ready = (!requirement.requireSampled || requirement.support.sampled) &&
                                (!requirement.requireColorAttachment || requirement.support.colorAttachment);
            m_state->anyBlockedFormats = m_state->anyBlockedFormats || !requirement.ready;
        }
        m_state->capabilityStatus = m_state->anyBlockedFormats
                                        ? "Some stage 6 resource formats are still blocked on this GPU."
                                        : "Stage 6 HDR cubes and the offline BRDF LUT format are ready.";
    }
    return true;
}

void RhiIblLabRenderPipeline::shutdown(luna::IRHIDevice& device)
{
    destroy_probe_resources(device);
    destroy_brdf_lut(device);
    destroy_present_pipelines(device);
    destroy_prefilter_cube(device);
    destroy_irradiance_cube(device);
    destroy_env_cube(device);
    destroy_source_hdr(device);
    destroy_shared_resources(device);
    m_device = nullptr;
}

bool RhiIblLabRenderPipeline::render(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    if (m_state == nullptr || frameContext.commandContext == nullptr || !frameContext.backbuffer.isValid()) {
        return false;
    }
    if (m_state->regenerateEnvCubeRequested) {
        destroy_prefilter_cube(device);
        destroy_irradiance_cube(device);
        destroy_env_cube(device);
        m_state->regenerateEnvCubeRequested = false;
        m_state->regenerateIrradianceRequested = false;
        m_state->regeneratePrefilterRequested = false;
    }
    if (!ensure_shared_resources(device) || !ensure_source_hdr(device) || !ensure_env_cube(device) || !ensure_irradiance_cube(device) ||
        !ensure_prefilter_cube(device) || !ensure_brdf_lut(device) || !ensure_present_pipelines(device, frameContext.backbufferFormat) ||
        !ensure_generation_pipeline(device)) {
        m_state->pipelineReady = false;
        m_state->renderStatus = "Failed to load or build one of the stage 6 IBL resources.";
        return false;
    }

    if (m_state->regenerateIrradianceRequested && !generate_irradiance_cube(device, frameContext)) {
        return false;
    }
    if (m_state->regeneratePrefilterRequested && !generate_prefilter_cube(device, frameContext)) {
        return false;
    }

    ++m_state->frameCounter;
    m_state->pipelineReady = true;
    m_state->selectedFace = clamp_face_index(m_state->selectedFace);
    m_state->selectedMip = std::clamp(m_state->selectedMip, 0, static_cast<int>(std::max(1u, m_state->cubeMipLevels) - 1u));
    m_state->irradianceFace = clamp_face_index(m_state->irradianceFace);
    m_state->prefilterFace = clamp_face_index(m_state->prefilterFace);
    m_state->prefilterMip =
        std::clamp(m_state->prefilterMip, 0, static_cast<int>(std::max(1u, m_state->prefilterMipLevels) - 1u));
    m_state->envPreviewLod = std::clamp(m_state->envPreviewLod, 0.0f, static_cast<float>(std::max(1u, m_state->cubeMipLevels) - 1u));

    const bool probeFrame = m_state->probe.request != ProbeKind::None;
    if (!consume_probe_result(device, frameContext.frameIndex) || !execute_requested_probe(device, frameContext)) {
        return false;
    }
    if (m_state->stampRequested && !stamp_selected_face(device, frameContext)) {
        return false;
    }

    const RenderTarget backbufferTarget{frameContext.backbuffer, {}, frameContext.backbufferFormat, frameContext.renderWidth, frameContext.renderHeight};
    if (probeFrame) {
        m_state->renderStatus = "Probe frame active.";
        return render_placeholder(frameContext, backbufferTarget);
    }
    switch (m_state->page) {
        case Page::Overview:
            m_state->renderStatus = "Overview shows the stage 6 IBL resource status over the environment cubemap atlas.";
            return render_cube_atlas(frameContext, backbufferTarget, m_envCubeView, m_state->selectedFace, m_state->envPreviewLod);
        case Page::SourceHdr:
            m_state->renderStatus = "Source HDR preview active.";
            return m_sourcePreviewView.isValid() ? render_face_preview(frameContext, backbufferTarget, m_sourcePreviewView, 0.0f, false)
                                                 : render_placeholder(frameContext, backbufferTarget);
        case Page::EnvCube:
            {
                std::ostringstream status;
                status << "Env Cube atlas active. selectedFace=" << kFaceLabels[static_cast<size_t>(m_state->selectedFace)]
                       << ", lod=" << std::fixed << std::setprecision(2) << m_state->envPreviewLod;
                m_state->renderStatus = status.str();
                return render_cube_atlas(frameContext, backbufferTarget, m_envCubeView, m_state->selectedFace, m_state->envPreviewLod);
            }
        case Page::Irradiance:
            {
                std::ostringstream status;
                status << "Irradiance atlas active. selectedFace=" << kFaceLabels[static_cast<size_t>(m_state->irradianceFace)];
                m_state->renderStatus = status.str();
                return m_irradianceCube.cubeView.isValid()
                           ? render_cube_atlas(frameContext, backbufferTarget, m_irradianceCube.cubeView, m_state->irradianceFace, 0.0f)
                           : render_placeholder(frameContext, backbufferTarget);
            }
        case Page::Prefilter:
            {
                const float previewLod = static_cast<float>(m_state->prefilterMip);
                const float roughness = m_state->prefilterMipLevels > 1u
                                            ? previewLod / static_cast<float>(m_state->prefilterMipLevels - 1u)
                                            : 0.0f;
                std::ostringstream status;
                status << "Prefilter atlas active. selectedFace=" << kFaceLabels[static_cast<size_t>(m_state->prefilterFace)]
                       << ", mip=" << m_state->prefilterMip << ", roughness=" << std::fixed << std::setprecision(2) << roughness;
                m_state->renderStatus = status.str();
                return (m_prefilterCube.cubeView.isValid() && m_state->prefilterGenerated)
                           ? render_cube_atlas(frameContext, backbufferTarget, m_prefilterCube.cubeView, m_state->prefilterFace, previewLod)
                           : render_placeholder(frameContext, backbufferTarget);
            }
        case Page::BrdfLut:
            m_state->renderStatus = "Offline BRDF LUT preview active.";
            return m_brdfLutView.isValid() ? render_face_preview(frameContext, backbufferTarget, m_brdfLutView, 0.0f, false)
                                           : render_placeholder(frameContext, backbufferTarget);
        case Page::Skybox:
            {
                std::ostringstream status;
                status << "Skybox samplerCube preview active. yaw=" << m_state->skyYaw << ", pitch=" << m_state->skyPitch;
                m_state->renderStatus = status.str();
                return render_skybox(frameContext, backbufferTarget, m_envCubeView, m_state->skyYaw, m_state->skyPitch);
            }
        case Page::Inspector:
            {
                std::ostringstream status;
                status << "Inspector preview face=" << kFaceLabels[static_cast<size_t>(m_state->selectedFace)]
                       << ", mip=" << m_state->selectedMip;
                m_state->renderStatus = status.str();
                return render_face_preview(frameContext,
                                           backbufferTarget,
                                           m_faceViews[static_cast<size_t>(m_state->selectedFace)],
                                           static_cast<float>(m_state->selectedMip),
                                           true);
            }
        default:
            return render_placeholder(frameContext, backbufferTarget);
    }
}

bool RhiIblLabRenderPipeline::ensure_shared_resources(luna::IRHIDevice& device)
{
    const uint32_t desiredFramesInFlight = std::max(1u, device.getCapabilities().framesInFlight);
    bool ready = m_linearSampler.isValid() && m_cubeLayout.isValid() && m_faceLayout.isValid() &&
                 m_framesInFlight == desiredFramesInFlight && m_cubeSets.size() == desiredFramesInFlight &&
                 m_faceSets.size() == desiredFramesInFlight;
    if (ready) {
        for (uint32_t frame = 0; frame < desiredFramesInFlight; ++frame) {
            if (!m_cubeSets[frame].isValid() || !m_faceSets[frame].isValid()) {
                ready = false;
                break;
            }
        }
    }
    if (ready) {
        return true;
    }

    destroy_shared_resources(device);

    luna::SamplerDesc samplerDesc{};
    samplerDesc.debugName = "RhiIblLabLinearSampler";
    samplerDesc.addressModeU = luna::SamplerAddressMode::ClampToEdge;
    samplerDesc.addressModeV = luna::SamplerAddressMode::ClampToEdge;
    samplerDesc.addressModeW = luna::SamplerAddressMode::ClampToEdge;
    if (device.createSampler(samplerDesc, &m_linearSampler) != luna::RHIResult::Success) {
        return false;
    }

    luna::ResourceLayoutDesc layoutDesc{};
    layoutDesc.debugName = "RhiIblLabCubeLayout";
    layoutDesc.bindings.push_back({0, luna::ResourceType::CombinedImageSampler, 1, luna::ShaderType::Fragment});
    if (device.createResourceLayout(layoutDesc, &m_cubeLayout) != luna::RHIResult::Success) {
        destroy_shared_resources(device);
        return false;
    }

    layoutDesc = {};
    layoutDesc.debugName = "RhiIblLabFaceLayout";
    layoutDesc.bindings.push_back({0, luna::ResourceType::CombinedImageSampler, 1, luna::ShaderType::Fragment});
    if (device.createResourceLayout(layoutDesc, &m_faceLayout) != luna::RHIResult::Success) {
        destroy_shared_resources(device);
        return false;
    }

    m_framesInFlight = desiredFramesInFlight;
    m_cubeSets.assign(m_framesInFlight, {});
    m_faceSets.assign(m_framesInFlight, {});
    m_boundCubeViews.assign(m_framesInFlight, {});
    m_boundFaceViews.assign(m_framesInFlight, {});

    for (uint32_t frame = 0; frame < m_framesInFlight; ++frame) {
        if (device.createResourceSet(m_cubeLayout, &m_cubeSets[frame]) != luna::RHIResult::Success ||
            device.createResourceSet(m_faceLayout, &m_faceSets[frame]) != luna::RHIResult::Success) {
            destroy_shared_resources(device);
            return false;
        }
    }
    return true;
}

bool RhiIblLabRenderPipeline::ensure_source_hdr(luna::IRHIDevice& device)
{
    if (!m_sourceHdrPixels.empty() && m_sourcePreviewImage.isValid() && m_sourcePreviewView.isValid()) {
        return true;
    }

    destroy_source_hdr(device);
    if (!load_source_hdr_pixels()) {
        if (m_state != nullptr) {
            m_state->sourceHdrReady = false;
            if (m_state->sourceHdrStatus.empty()) {
                m_state->sourceHdrStatus = "Failed to load the source HDR image.";
            }
        }
        return false;
    }

    const std::vector<uint8_t> previewPixels = build_source_preview_data();
    if (previewPixels.empty()) {
        if (m_state != nullptr) {
            m_state->sourceHdrStatus = "Failed to build the source HDR preview image.";
        }
        return false;
    }

    luna::ImageDesc imageDesc{};
    imageDesc.width = m_sourceHdrWidth;
    imageDesc.height = m_sourceHdrHeight;
    imageDesc.depth = 1;
    imageDesc.mipLevels = 1;
    imageDesc.arrayLayers = 1;
    imageDesc.type = luna::ImageType::Image2D;
    imageDesc.format = luna::PixelFormat::RGBA8Unorm;
    imageDesc.usage = luna::ImageUsage::Sampled;
    imageDesc.debugName = "RhiIblLabSourceHdrPreview";
    if (device.createImage(imageDesc, &m_sourcePreviewImage, previewPixels.data()) != luna::RHIResult::Success) {
        if (m_state != nullptr) {
            m_state->sourceHdrStatus = "Failed to upload the source HDR preview image.";
        }
        destroy_source_hdr(device);
        return false;
    }

    luna::ImageViewDesc viewDesc{};
    viewDesc.image = m_sourcePreviewImage;
    viewDesc.type = luna::ImageViewType::Image2D;
    viewDesc.aspect = luna::ImageAspect::Color;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipCount = 1;
    viewDesc.baseArrayLayer = 0;
    viewDesc.layerCount = 1;
    viewDesc.debugName = "RhiIblLabSourceHdrPreviewView";
    if (device.createImageView(viewDesc, &m_sourcePreviewView) != luna::RHIResult::Success) {
        if (m_state != nullptr) {
            m_state->sourceHdrStatus = "Failed to create the source HDR preview view.";
        }
        destroy_source_hdr(device);
        return false;
    }

    if (m_state != nullptr) {
        m_state->sourceHdrReady = true;
        m_state->sourceWidth = m_sourceHdrWidth;
        m_state->sourceHeight = m_sourceHdrHeight;
        m_state->sourceChannels = m_sourceHdrChannels;
        m_state->sourcePath = m_sourceHdrPath;
        std::ostringstream status;
        status << "Loaded equirect HDR from " << m_sourceHdrPath << " (" << m_sourceHdrWidth << "x" << m_sourceHdrHeight
               << ", channels=" << m_sourceHdrChannels << ").";
        m_state->sourceHdrStatus = status.str();
    }
    return true;
}

bool RhiIblLabRenderPipeline::ensure_env_cube(luna::IRHIDevice& device)
{
    if (m_envCubeImage.isValid() && m_envCubeView.isValid()) {
        return true;
    }
    destroy_env_cube(device);

    if (m_sourceHdrPixels.empty()) {
        if (m_state != nullptr) {
            m_state->envCubeStatus = "Cannot generate the environment cube before the source HDR is loaded.";
        }
        return false;
    }

    const uint32_t cubeSize = m_state != nullptr ? std::max(32u, m_state->cubeSize) : 256u;
    const uint32_t mipLevels = calculate_mip_count(cubeSize, cubeSize);
    const std::vector<uint16_t> pixels = build_env_cube_data(cubeSize);
    if (pixels.empty()) {
        if (m_state != nullptr) {
            m_state->envCubeStatus = "Failed to project the source HDR into cube faces.";
        }
        return false;
    }

    luna::ImageDesc imageDesc{};
    imageDesc.width = cubeSize;
    imageDesc.height = cubeSize;
    imageDesc.depth = 1;
    imageDesc.mipLevels = mipLevels;
    imageDesc.arrayLayers = kCubeFaceCount;
    imageDesc.type = luna::ImageType::Cube;
    imageDesc.format = kEnvCubeFormat;
    imageDesc.usage = luna::ImageUsage::Sampled | luna::ImageUsage::ColorAttachment;
    imageDesc.debugName = "RhiIblLabEnvCube";
    if (device.createImage(imageDesc, &m_envCubeImage, pixels.data()) != luna::RHIResult::Success) {
        m_state->envCubeStatus = "Failed to create the RGBA16Float environment cubemap image.";
        return false;
    }

    luna::ImageViewDesc cubeViewDesc{};
    cubeViewDesc.image = m_envCubeImage;
    cubeViewDesc.type = luna::ImageViewType::Cube;
    cubeViewDesc.aspect = luna::ImageAspect::Color;
    cubeViewDesc.baseMipLevel = 0;
    cubeViewDesc.mipCount = mipLevels;
    cubeViewDesc.baseArrayLayer = 0;
    cubeViewDesc.layerCount = kCubeFaceCount;
    cubeViewDesc.debugName = "RhiIblLabEnvCubeView";
    if (device.createImageView(cubeViewDesc, &m_envCubeView) != luna::RHIResult::Success) {
        destroy_env_cube(device);
        m_state->envCubeStatus = "Failed to create the full cube SRV view.";
        return false;
    }

    for (uint32_t face = 0; face < kCubeFaceCount; ++face) {
        luna::ImageViewDesc faceDesc{};
        faceDesc.image = m_envCubeImage;
        faceDesc.type = luna::ImageViewType::Image2D;
        faceDesc.aspect = luna::ImageAspect::Color;
        faceDesc.baseMipLevel = 0;
        faceDesc.mipCount = mipLevels;
        faceDesc.baseArrayLayer = face;
        faceDesc.layerCount = 1;
        faceDesc.debugName = "RhiIblLabEnvCubeFaceView";
        if (device.createImageView(faceDesc, &m_faceViews[face]) != luna::RHIResult::Success) {
            destroy_env_cube(device);
            m_state->envCubeStatus = "Failed to create one of the face SRV views.";
            return false;
        }
    }

    m_attachmentViews.assign(static_cast<size_t>(mipLevels) * kCubeFaceCount, {});
    std::fill(m_boundCubeViews.begin(), m_boundCubeViews.end(), luna::ImageViewHandle{});
    std::fill(m_boundFaceViews.begin(), m_boundFaceViews.end(), luna::ImageViewHandle{});

    m_state->envCubeReady = true;
    m_state->cubeSize = cubeSize;
    m_state->cubeFaceCount = kCubeFaceCount;
    m_state->cubeMipLevels = mipLevels;
    m_state->selectedMip = std::clamp(m_state->selectedMip, 0, static_cast<int>(mipLevels - 1u));
    m_state->envPreviewLod = std::clamp(m_state->envPreviewLod, 0.0f, static_cast<float>(mipLevels - 1u));

    std::ostringstream status;
    status << "HDR env cube ready. size=" << cubeSize << "x" << cubeSize << ", faces=" << kCubeFaceCount << ", mips=" << mipLevels
           << ", format=" << luna::to_string(kEnvCubeFormat) << ".";
    m_state->envCubeStatus = status.str();
    return true;
}

bool RhiIblLabRenderPipeline::ensure_irradiance_cube(luna::IRHIDevice& device)
{
    if (m_irradianceCube.image.isValid() && m_irradianceCube.cubeView.isValid()) {
        return true;
    }

    destroy_irradiance_cube(device);
    if (!m_envCubeImage.isValid()) {
        if (m_state != nullptr) {
            m_state->irradianceStatus = "Cannot allocate the irradiance cube before the environment cube exists.";
        }
        return false;
    }

    if (!create_cube_texture(device, m_irradianceCube, kIrradianceCubeSize, 1u, "RhiIblLabIrradianceCube")) {
        if (m_state != nullptr) {
            m_state->irradianceStatus = "Failed to allocate the irradiance cubemap.";
        }
        return false;
    }

    if (m_state != nullptr) {
        m_state->irradianceReady = false;
        m_state->irradianceSize = m_irradianceCube.size;
        m_state->irradianceMipLevels = m_irradianceCube.mipLevels;
        std::ostringstream status;
        status << "Irradiance cube allocated. size=" << m_irradianceCube.size << "x" << m_irradianceCube.size
               << ", faces=" << kCubeFaceCount << ", format=" << luna::to_string(kEnvCubeFormat)
               << ". Click Generate Irradiance to populate the low-frequency result.";
        m_state->irradianceStatus = status.str();
    }
    return true;
}

bool RhiIblLabRenderPipeline::ensure_prefilter_cube(luna::IRHIDevice& device)
{
    if (m_prefilterCube.image.isValid() && m_prefilterCube.cubeView.isValid()) {
        return true;
    }

    destroy_prefilter_cube(device);
    if (!m_envCubeImage.isValid()) {
        if (m_state != nullptr) {
            m_state->prefilterStatus = "Cannot allocate the prefilter cube before the environment cube exists.";
        }
        return false;
    }

    const uint32_t cubeSize = std::max(32u, m_state != nullptr ? m_state->cubeSize : 256u);
    const uint32_t mipLevels = calculate_mip_count(cubeSize, cubeSize);
    if (!create_cube_texture(device, m_prefilterCube, cubeSize, mipLevels, "RhiIblLabPrefilterCube")) {
        if (m_state != nullptr) {
            m_state->prefilterStatus = "Failed to allocate the prefilter cubemap.";
        }
        return false;
    }

    if (m_state != nullptr) {
        m_state->prefilterReady = true;
        m_state->prefilterGenerated = false;
        m_state->prefilterSize = cubeSize;
        m_state->prefilterMipLevels = mipLevels;
        m_state->prefilterMip = std::clamp(m_state->prefilterMip, 0, static_cast<int>(mipLevels - 1u));
        std::ostringstream status;
        status << "Prefilter cube ready. size=" << cubeSize << "x" << cubeSize << ", faces=" << kCubeFaceCount
               << ", mips=" << mipLevels << ", format=" << luna::to_string(kEnvCubeFormat)
               << ". Roughness maps to mip 0.." << (mipLevels - 1u) << ". Click Generate Prefilter to populate it.";
        m_state->prefilterStatus = status.str();
    }
    return true;
}

bool RhiIblLabRenderPipeline::ensure_brdf_lut(luna::IRHIDevice& device)
{
    if (m_brdfLutImage.isValid() && m_brdfLutView.isValid()) {
        return true;
    }

    destroy_brdf_lut(device);

    std::vector<uint16_t> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
    std::string resolvedPath;
    if (!load_brdf_lut_pixels(&pixels, &width, &height, &resolvedPath) || pixels.empty() || width == 0 || height == 0) {
        if (m_state != nullptr && m_state->brdfLutStatus.empty()) {
            m_state->brdfLutStatus = "Failed to load the offline BRDF LUT.";
        }
        return false;
    }

    luna::ImageDesc imageDesc{};
    imageDesc.width = width;
    imageDesc.height = height;
    imageDesc.depth = 1;
    imageDesc.mipLevels = 1;
    imageDesc.arrayLayers = 1;
    imageDesc.type = luna::ImageType::Image2D;
    imageDesc.format = kBrdfLutFormat;
    imageDesc.usage = luna::ImageUsage::Sampled;
    imageDesc.debugName = "RhiIblLabBrdfLut";
    if (device.createImage(imageDesc, &m_brdfLutImage, pixels.data()) != luna::RHIResult::Success) {
        if (m_state != nullptr) {
            m_state->brdfLutStatus = "Failed to upload the offline BRDF LUT image.";
        }
        destroy_brdf_lut(device);
        return false;
    }

    luna::ImageViewDesc viewDesc{};
    viewDesc.image = m_brdfLutImage;
    viewDesc.type = luna::ImageViewType::Image2D;
    viewDesc.aspect = luna::ImageAspect::Color;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipCount = 1;
    viewDesc.baseArrayLayer = 0;
    viewDesc.layerCount = 1;
    viewDesc.debugName = "RhiIblLabBrdfLutView";
    if (device.createImageView(viewDesc, &m_brdfLutView) != luna::RHIResult::Success) {
        if (m_state != nullptr) {
            m_state->brdfLutStatus = "Failed to create the BRDF LUT view.";
        }
        destroy_brdf_lut(device);
        return false;
    }

    if (m_state != nullptr) {
        m_state->brdfLutReady = true;
        m_state->brdfLutWidth = width;
        m_state->brdfLutHeight = height;
        m_state->brdfLutPath = resolvedPath;
        std::ostringstream status;
        status << "Loaded From Disk: " << resolvedPath << " (" << width << "x" << height << ", format="
               << luna::to_string(kBrdfLutFormat) << ").";
        m_state->brdfLutStatus = status.str();
    }
    return true;
}

bool RhiIblLabRenderPipeline::ensure_present_pipelines(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat)
{
    if (backbufferFormat == luna::PixelFormat::Undefined) {
        return false;
    }
    if (m_presentBackbufferFormat == backbufferFormat && m_cubeAtlasPipeline.isValid() && m_skyboxPipeline.isValid() &&
        m_facePreviewPipeline.isValid()) {
        return true;
    }

    destroy_present_pipelines(device);
    const std::string vertexShaderPath = shader_path(m_shaderRoot, "ibl_lab_fullscreen.vert.spv");
    const std::string atlasFragmentPath = shader_path(m_shaderRoot, "ibl_lab_cube_atlas.frag.spv");
    const std::string skyboxFragmentPath = shader_path(m_shaderRoot, "ibl_lab_cube_skybox.frag.spv");
    const std::string faceFragmentPath = shader_path(m_shaderRoot, "ibl_lab_face_preview.frag.spv");

    luna::GraphicsPipelineDesc pipelineDesc{};
    pipelineDesc.vertexShader = {.stage = luna::ShaderType::Vertex, .filePath = vertexShaderPath};
    pipelineDesc.cullMode = luna::CullMode::None;
    pipelineDesc.frontFace = luna::FrontFace::Clockwise;
    pipelineDesc.pushConstantSize = sizeof(PreviewPushConstants);
    pipelineDesc.pushConstantVisibility = luna::ShaderType::Fragment;
    pipelineDesc.colorAttachments.push_back({backbufferFormat, false});

    auto create_present_pipeline = [&](std::string_view name,
                                       const std::string& fragmentPath,
                                       luna::ResourceLayoutHandle layout,
                                       luna::PipelineHandle* outHandle) {
        luna::GraphicsPipelineDesc desc = pipelineDesc;
        desc.debugName = name;
        desc.fragmentShader = {.stage = luna::ShaderType::Fragment, .filePath = fragmentPath};
        desc.resourceLayouts.push_back(layout);
        return device.createGraphicsPipeline(desc, outHandle) == luna::RHIResult::Success;
    };

    if (!create_present_pipeline("RhiIblLabCubeAtlas", atlasFragmentPath, m_cubeLayout, &m_cubeAtlasPipeline) ||
        !create_present_pipeline("RhiIblLabCubeSkybox", skyboxFragmentPath, m_cubeLayout, &m_skyboxPipeline) ||
        !create_present_pipeline("RhiIblLabFacePreview", faceFragmentPath, m_faceLayout, &m_facePreviewPipeline)) {
        destroy_present_pipelines(device);
        return false;
    }

    m_presentBackbufferFormat = backbufferFormat;
    return true;
}

bool RhiIblLabRenderPipeline::ensure_generation_pipeline(luna::IRHIDevice& device)
{
    if (m_cubeFilterPipeline.isValid()) {
        return true;
    }

    const std::string vertexShaderPath = shader_path(m_shaderRoot, "ibl_lab_fullscreen.vert.spv");
    const std::string filterFragmentPath = shader_path(m_shaderRoot, "ibl_lab_cube_filter.frag.spv");

    luna::GraphicsPipelineDesc pipelineDesc{};
    pipelineDesc.debugName = "RhiIblLabCubeFilter";
    pipelineDesc.vertexShader = {.stage = luna::ShaderType::Vertex, .filePath = vertexShaderPath};
    pipelineDesc.fragmentShader = {.stage = luna::ShaderType::Fragment, .filePath = filterFragmentPath};
    pipelineDesc.cullMode = luna::CullMode::None;
    pipelineDesc.frontFace = luna::FrontFace::Clockwise;
    pipelineDesc.pushConstantSize = sizeof(PreviewPushConstants);
    pipelineDesc.pushConstantVisibility = luna::ShaderType::Fragment;
    pipelineDesc.colorAttachments.push_back({kEnvCubeFormat, false});
    pipelineDesc.resourceLayouts.push_back(m_cubeLayout);
    return device.createGraphicsPipeline(pipelineDesc, &m_cubeFilterPipeline) == luna::RHIResult::Success;
}

bool RhiIblLabRenderPipeline::ensure_probe_resources(luna::IRHIDevice& device)
{
    if (m_framesInFlight == 0) {
        m_framesInFlight = std::max(1u, device.getCapabilities().framesInFlight);
    }

    bool ready = m_probeImage.isValid() && m_probeBuffers.size() == m_framesInFlight && m_probePending.size() == m_framesInFlight;
    if (ready) {
        for (const luna::BufferHandle buffer : m_probeBuffers) {
            if (!buffer.isValid()) {
                ready = false;
                break;
            }
        }
    }
    if (ready) {
        return true;
    }

    destroy_probe_resources(device);

    luna::ImageDesc probeImageDesc{};
    probeImageDesc.width = kProbeWidth;
    probeImageDesc.height = kProbeHeight;
    probeImageDesc.depth = 1;
    probeImageDesc.mipLevels = 1;
    probeImageDesc.arrayLayers = 1;
    probeImageDesc.type = luna::ImageType::Image2D;
    probeImageDesc.format =
        m_presentBackbufferFormat == luna::PixelFormat::Undefined ? luna::PixelFormat::BGRA8Unorm : m_presentBackbufferFormat;
    probeImageDesc.usage = luna::ImageUsage::ColorAttachment | luna::ImageUsage::TransferSrc;
    probeImageDesc.debugName = "RhiIblLabProbeImage";
    if (device.createImage(probeImageDesc, &m_probeImage) != luna::RHIResult::Success) {
        return false;
    }

    m_probeBuffers.assign(m_framesInFlight, {});
    m_probePending.assign(m_framesInFlight, {});

    luna::BufferDesc bufferDesc{};
    bufferDesc.size = kProbeBufferBytes;
    bufferDesc.usage = luna::BufferUsage::TransferDst;
    bufferDesc.memoryUsage = luna::MemoryUsage::Readback;
    bufferDesc.debugName = "RhiIblLabProbeReadback";
    for (uint32_t frame = 0; frame < m_framesInFlight; ++frame) {
        if (device.createBuffer(bufferDesc, &m_probeBuffers[frame]) != luna::RHIResult::Success) {
            destroy_probe_resources(device);
            return false;
        }
    }

    return true;
}

bool RhiIblLabRenderPipeline::update_cube_texture_set(luna::IRHIDevice& device, uint32_t frameIndex, luna::ImageViewHandle view)
{
    if (!view.isValid() || m_cubeSets.empty()) {
        return false;
    }
    const size_t slot = frame_slot(frameIndex, m_cubeSets.size());
    if (m_boundCubeViews[slot] == view) {
        return true;
    }

    luna::ResourceSetWriteDesc writeDesc{};
    writeDesc.images.push_back({.binding = 0, .imageView = view, .sampler = m_linearSampler, .type = luna::ResourceType::CombinedImageSampler});
    if (device.updateResourceSet(m_cubeSets[slot], writeDesc) != luna::RHIResult::Success) {
        return false;
    }
    m_boundCubeViews[slot] = view;
    return true;
}

bool RhiIblLabRenderPipeline::update_face_texture_set(luna::IRHIDevice& device, uint32_t frameIndex, luna::ImageViewHandle view)
{
    if (!view.isValid() || m_faceSets.empty()) {
        return false;
    }
    const size_t slot = frame_slot(frameIndex, m_faceSets.size());
    if (m_boundFaceViews[slot] == view) {
        return true;
    }

    luna::ResourceSetWriteDesc writeDesc{};
    writeDesc.images.push_back({.binding = 0, .imageView = view, .sampler = m_linearSampler, .type = luna::ResourceType::CombinedImageSampler});
    if (device.updateResourceSet(m_faceSets[slot], writeDesc) != luna::RHIResult::Success) {
        return false;
    }
    m_boundFaceViews[slot] = view;
    return true;
}

bool RhiIblLabRenderPipeline::render_cube_atlas(const luna::FrameContext& frameContext,
                                                const RenderTarget& target,
                                                luna::ImageViewHandle cubeView,
                                                int selectedFace,
                                                float mipLevel)
{
    if (m_device == nullptr || !update_cube_texture_set(*m_device, frameContext.frameIndex, cubeView)) {
        return false;
    }
    PreviewPushConstants pushConstants{};
    pushConstants.params[0] = static_cast<float>(selectedFace);
    pushConstants.params[1] = std::max(0.0f, mipLevel);

    return frameContext.commandContext->beginRendering({.width = target.width,
                                                        .height = target.height,
                                                        .colorAttachments = {{target.image, target.format, {0.04f, 0.05f, 0.06f, 1.0f}, target.view}}}) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->bindGraphicsPipeline(m_cubeAtlasPipeline) == luna::RHIResult::Success &&
           frameContext.commandContext->bindResourceSet(m_cubeSets[frame_slot(frameContext.frameIndex, m_cubeSets.size())]) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->pushConstants(&pushConstants, sizeof(pushConstants), 0, luna::ShaderType::Fragment) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
           frameContext.commandContext->endRendering() == luna::RHIResult::Success;
}

bool RhiIblLabRenderPipeline::render_skybox(const luna::FrameContext& frameContext,
                                            const RenderTarget& target,
                                            luna::ImageViewHandle cubeView,
                                            float yaw,
                                            float pitch)
{
    if (m_device == nullptr || !update_cube_texture_set(*m_device, frameContext.frameIndex, cubeView)) {
        return false;
    }
    PreviewPushConstants pushConstants{};
    pushConstants.params[0] = yaw;
    pushConstants.params[1] = pitch;
    pushConstants.params[2] = target.height == 0 ? 1.0f : static_cast<float>(target.width) / static_cast<float>(target.height);

    return frameContext.commandContext->beginRendering({.width = target.width,
                                                        .height = target.height,
                                                        .colorAttachments = {{target.image, target.format, {0.01f, 0.02f, 0.03f, 1.0f}, target.view}}}) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->bindGraphicsPipeline(m_skyboxPipeline) == luna::RHIResult::Success &&
           frameContext.commandContext->bindResourceSet(m_cubeSets[frame_slot(frameContext.frameIndex, m_cubeSets.size())]) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->pushConstants(&pushConstants, sizeof(pushConstants), 0, luna::ShaderType::Fragment) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
           frameContext.commandContext->endRendering() == luna::RHIResult::Success;
}

bool RhiIblLabRenderPipeline::render_cube_filter(const luna::FrameContext& frameContext,
                                                 const RenderTarget& target,
                                                 luna::ImageViewHandle sourceCubeView,
                                                 uint32_t faceIndex,
                                                 float sourceLod)
{
    if (m_device == nullptr || !update_cube_texture_set(*m_device, frameContext.frameIndex, sourceCubeView)) {
        return false;
    }
    PreviewPushConstants pushConstants{};
    pushConstants.params[0] = static_cast<float>(faceIndex);
    pushConstants.params[1] = std::max(0.0f, sourceLod);

    return frameContext.commandContext->beginRendering({.width = target.width,
                                                        .height = target.height,
                                                        .colorAttachments = {{target.image, target.format, {0.0f, 0.0f, 0.0f, 1.0f}, target.view}}}) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->bindGraphicsPipeline(m_cubeFilterPipeline) == luna::RHIResult::Success &&
           frameContext.commandContext->bindResourceSet(m_cubeSets[frame_slot(frameContext.frameIndex, m_cubeSets.size())]) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->pushConstants(&pushConstants, sizeof(pushConstants), 0, luna::ShaderType::Fragment) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
           frameContext.commandContext->endRendering() == luna::RHIResult::Success;
}

bool RhiIblLabRenderPipeline::render_face_preview(const luna::FrameContext& frameContext,
                                                  const RenderTarget& target,
                                                  luna::ImageViewHandle faceView,
                                                  float mipLevel,
                                                  bool applyTonemap)
{
    if (m_device == nullptr || !update_face_texture_set(*m_device, frameContext.frameIndex, faceView)) {
        return false;
    }
    PreviewPushConstants pushConstants{};
    pushConstants.params[0] = std::max(0.0f, mipLevel);
    pushConstants.params[1] = applyTonemap ? 1.0f : 0.0f;

    return frameContext.commandContext->beginRendering({.width = target.width,
                                                        .height = target.height,
                                                        .colorAttachments = {{target.image, target.format, {0.03f, 0.03f, 0.035f, 1.0f}, target.view}}}) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->bindGraphicsPipeline(m_facePreviewPipeline) == luna::RHIResult::Success &&
           frameContext.commandContext->bindResourceSet(m_faceSets[frame_slot(frameContext.frameIndex, m_faceSets.size())]) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->pushConstants(&pushConstants, sizeof(pushConstants), 0, luna::ShaderType::Fragment) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
           frameContext.commandContext->endRendering() == luna::RHIResult::Success;
}

bool RhiIblLabRenderPipeline::render_placeholder(const luna::FrameContext& frameContext, const RenderTarget& target)
{
    return frameContext.commandContext->beginRendering({.width = target.width,
                                                        .height = target.height,
                                                        .colorAttachments = {{target.image, target.format, {0.05f, 0.05f, 0.07f, 1.0f}, target.view}}}) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->endRendering() == luna::RHIResult::Success;
}

bool RhiIblLabRenderPipeline::generate_irradiance_cube(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    if (!m_envCubeView.isValid() || !m_irradianceCube.image.isValid()) {
        if (m_state != nullptr) {
            m_state->irradianceStatus = "Irradiance generation failed because the source or target cubemap is missing.";
        }
        return false;
    }

    const float sourceLod = static_cast<float>(std::max(1u, m_state->cubeMipLevels) - 1u);
    for (uint32_t face = 0; face < kCubeFaceCount; ++face) {
        const luna::ImageViewHandle attachmentView =
            ensure_attachment_view(device, m_irradianceCube, face, 0, "RhiIblLabIrradianceAttachmentView");
        if (!attachmentView.isValid()) {
            if (m_state != nullptr) {
                m_state->irradianceStatus = "Irradiance generation failed while creating an attachment view.";
            }
            return false;
        }

        const RenderTarget target{m_irradianceCube.image, attachmentView, kEnvCubeFormat, m_irradianceCube.size, m_irradianceCube.size};
        if (!render_cube_filter(frameContext, target, m_envCubeView, face, sourceLod) ||
            frameContext.commandContext->imageBarrier({.image = m_irradianceCube.image,
                                                       .oldLayout = luna::ImageLayout::ColorAttachment,
                                                       .newLayout = luna::ImageLayout::ShaderReadOnly,
                                                       .srcStage = luna::PipelineStage::ColorAttachmentOutput,
                                                       .dstStage = luna::PipelineStage::FragmentShader,
                                                       .srcAccess = luna::ResourceAccess::ColorAttachmentWrite,
                                                       .dstAccess = luna::ResourceAccess::ShaderRead,
                                                       .aspect = luna::ImageAspect::Color,
                                                       .baseMipLevel = 0,
                                                       .mipCount = 1,
                                                       .baseArrayLayer = face,
                                                       .layerCount = 1}) != luna::RHIResult::Success) {
            if (m_state != nullptr) {
                m_state->irradianceStatus = "Irradiance generation failed while recording commands.";
            }
            return false;
        }
    }

    if (m_state != nullptr) {
        m_state->irradianceReady = true;
        m_state->regenerateIrradianceRequested = false;
        std::ostringstream status;
        status << "Irradiance cube generated from Env Cube. size=" << m_irradianceCube.size << "x" << m_irradianceCube.size
               << ", faces=" << kCubeFaceCount << ", sampledLod=" << std::fixed << std::setprecision(2) << sourceLod << ".";
        m_state->irradianceStatus = status.str();
    }
    return true;
}

bool RhiIblLabRenderPipeline::generate_prefilter_cube(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    if (!m_envCubeView.isValid() || !m_prefilterCube.image.isValid()) {
        if (m_state != nullptr) {
            m_state->prefilterStatus = "Prefilter generation failed because the source or target cubemap is missing.";
        }
        return false;
    }

    const uint32_t mipCount = std::max(1u, m_prefilterCube.mipLevels);
    const float maxSourceLod = static_cast<float>(std::max(1u, m_state->cubeMipLevels) - 1u);
    for (uint32_t mip = 0; mip < mipCount; ++mip) {
        const float roughness = mipCount > 1u ? static_cast<float>(mip) / static_cast<float>(mipCount - 1u) : 0.0f;
        const float sourceLod = roughness * maxSourceLod;
        const uint32_t mipExtent = std::max(1u, m_prefilterCube.size >> mip);
        for (uint32_t face = 0; face < kCubeFaceCount; ++face) {
            const luna::ImageViewHandle attachmentView =
                ensure_attachment_view(device, m_prefilterCube, face, mip, "RhiIblLabPrefilterAttachmentView");
            if (!attachmentView.isValid()) {
                if (m_state != nullptr) {
                    m_state->prefilterStatus = "Prefilter generation failed while creating an attachment view.";
                }
                return false;
            }

            const RenderTarget target{m_prefilterCube.image, attachmentView, kEnvCubeFormat, mipExtent, mipExtent};
            if (!render_cube_filter(frameContext, target, m_envCubeView, face, sourceLod) ||
                frameContext.commandContext->imageBarrier({.image = m_prefilterCube.image,
                                                           .oldLayout = luna::ImageLayout::ColorAttachment,
                                                           .newLayout = luna::ImageLayout::ShaderReadOnly,
                                                           .srcStage = luna::PipelineStage::ColorAttachmentOutput,
                                                           .dstStage = luna::PipelineStage::FragmentShader,
                                                           .srcAccess = luna::ResourceAccess::ColorAttachmentWrite,
                                                           .dstAccess = luna::ResourceAccess::ShaderRead,
                                                           .aspect = luna::ImageAspect::Color,
                                                           .baseMipLevel = mip,
                                                           .mipCount = 1,
                                                           .baseArrayLayer = face,
                                                           .layerCount = 1}) != luna::RHIResult::Success) {
                if (m_state != nullptr) {
                    m_state->prefilterStatus = "Prefilter generation failed while recording commands.";
                }
                return false;
            }
        }
    }

    if (m_state != nullptr) {
        m_state->prefilterReady = true;
        m_state->prefilterGenerated = true;
        m_state->regeneratePrefilterRequested = false;
        std::ostringstream status;
        status << "Prefilter cube generated from Env Cube. size=" << m_prefilterCube.size << "x" << m_prefilterCube.size
               << ", faces=" << kCubeFaceCount << ", mips=" << mipCount << ", roughness->mip 0.." << (mipCount - 1u) << ".";
        m_state->prefilterStatus = status.str();
    }
    return true;
}

bool RhiIblLabRenderPipeline::stamp_selected_face(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    const uint32_t faceIndex = static_cast<uint32_t>(clamp_face_index(m_state->selectedFace));
    const uint32_t mipLevel =
        static_cast<uint32_t>(std::clamp(m_state->selectedMip, 0, static_cast<int>(std::max(1u, m_state->cubeMipLevels) - 1u)));
    const luna::ImageViewHandle attachmentView = ensure_attachment_view(device, faceIndex, mipLevel);
    if (!attachmentView.isValid()) {
        m_state->inspectorStatus = "Stamp failed: attachment view creation failed.";
        return false;
    }

    const uint32_t targetSize = std::max(1u, m_state->cubeSize >> mipLevel);
    const luna::ClearColorValue clearColor = stamp_color(faceIndex, mipLevel, m_state->stampSerial);
    const bool ok =
        frameContext.commandContext->beginRendering({.width = targetSize,
                                                    .height = targetSize,
                                                    .colorAttachments = {{m_envCubeImage, kEnvCubeFormat, clearColor, attachmentView}}}) ==
            luna::RHIResult::Success &&
        frameContext.commandContext->endRendering() == luna::RHIResult::Success &&
        frameContext.commandContext->imageBarrier({.image = m_envCubeImage,
                                                   .oldLayout = luna::ImageLayout::ColorAttachment,
                                                   .newLayout = luna::ImageLayout::ShaderReadOnly,
                                                   .srcStage = luna::PipelineStage::ColorAttachmentOutput,
                                                   .dstStage = luna::PipelineStage::FragmentShader,
                                                   .srcAccess = luna::ResourceAccess::ColorAttachmentWrite,
                                                   .dstAccess = luna::ResourceAccess::ShaderRead,
                                                   .aspect = luna::ImageAspect::Color,
                                                   .baseMipLevel = mipLevel,
                                                   .mipCount = 1,
                                                   .baseArrayLayer = faceIndex,
                                                   .layerCount = 1}) == luna::RHIResult::Success;
    if (!ok) {
        m_state->inspectorStatus = "Stamp failed while recording commands.";
        return false;
    }

    ++m_state->stampSerial;
    m_state->stampRequested = false;

    std::ostringstream status;
    status << "Stamped face=" << kFaceLabels[faceIndex] << " mip=" << mipLevel << " with an isolated debug color.";
    m_state->inspectorStatus = status.str();
    LUNA_CORE_INFO("Stamp Selected Face face={} mip={} serial={}", kFaceLabels[faceIndex], mipLevel, m_state->stampSerial);
    return true;
}

bool RhiIblLabRenderPipeline::execute_requested_probe(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    if (m_state->probe.request == ProbeKind::None) {
        return true;
    }
    if (!ensure_probe_resources(device)) {
        return false;
    }

    const size_t slot = frame_slot(frameContext.frameIndex, m_probeBuffers.size());
    if (slot >= m_probePending.size() || m_probePending[slot].kind != ProbeKind::None) {
        return true;
    }

    bool queued = false;
    switch (m_state->probe.request) {
        case ProbeKind::CubeFacesDistinct:
            queued = queue_cube_faces_distinct_probe(device, frameContext, slot);
            break;
        case ProbeKind::SkyboxRotation:
            queued = queue_skybox_rotation_probe(device, frameContext, slot);
            break;
        case ProbeKind::FaceMipPreview:
            queued = queue_face_mip_preview_probe(device, frameContext, slot);
            break;
        case ProbeKind::FaceIsolation:
            queued = queue_face_isolation_probe(device, frameContext, slot);
            break;
        case ProbeKind::EnvMips:
            queued = queue_env_mips_probe(device, frameContext, slot);
            break;
        case ProbeKind::IrradianceVsEnv:
            queued = queue_irradiance_probe(device, frameContext, slot);
            break;
        case ProbeKind::PrefilterMips:
            queued = queue_prefilter_mips_probe(device, frameContext, slot);
            break;
        case ProbeKind::BrdfLutPreview:
            queued = queue_brdf_lut_preview_probe(device, frameContext, slot);
            break;
        case ProbeKind::None:
        default:
            queued = true;
            break;
    }

    if (queued) {
        m_state->probe.ready = false;
        m_state->probe.completed = ProbeKind::None;
        m_state->probe.passed = false;
        m_state->probe.request = ProbeKind::None;
    }
    return queued;
}

bool RhiIblLabRenderPipeline::consume_probe_result(luna::IRHIDevice& device, uint32_t frameIndex)
{
    if (m_probeBuffers.empty() || m_probePending.empty()) {
        return true;
    }

    const size_t slot = frame_slot(frameIndex, m_probeBuffers.size());
    if (slot >= m_probePending.size() || m_probePending[slot].kind == ProbeKind::None) {
        return true;
    }

    std::array<uint8_t, kProbeBufferBytes> bytes{};
    if (device.readBuffer(m_probeBuffers[slot], bytes.data(), bytes.size(), 0) != luna::RHIResult::Success) {
        return false;
    }

    const PendingProbe pending = m_probePending[slot];
    m_probePending[slot] = {};

    auto& probe = m_state->probe;
    probe.completed = pending.kind;
    probe.ready = true;

    std::ostringstream summary;
    bool passed = false;
    const auto approx_equal = [](const std::array<float, 4>& a, const std::array<float, 4>& b) {
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::abs(a[i] - b[i]) > 0.0005f) {
                return false;
            }
        }
        return true;
    };
    switch (pending.kind) {
        case ProbeKind::CubeFacesDistinct:
            {
                const size_t faceStride = kCubeDistinctProbeSampleCount * kProbeTexelBytesRGBA16F;
                std::array<uint32_t, kCubeFaceCount> hashes{};
                passed = true;
                for (size_t face = 0; face < hashes.size(); ++face) {
                    const uint8_t* faceBytes = bytes.data() + face * faceStride;
                    hashes[face] = hash_bytes_fnv1a(faceBytes, faceStride);
                }
                for (size_t i = 0; i < hashes.size(); ++i) {
                    for (size_t j = i + 1; j < hashes.size(); ++j) {
                        if (std::equal(bytes.begin() + static_cast<std::ptrdiff_t>(i * faceStride),
                                       bytes.begin() + static_cast<std::ptrdiff_t>((i + 1u) * faceStride),
                                       bytes.begin() + static_cast<std::ptrdiff_t>(j * faceStride))) {
                            passed = false;
                        }
                    }
                }
                summary << "face hashes";
                for (size_t i = 0; i < hashes.size(); ++i) {
                    summary << " " << kFaceLabels[i] << "=" << hex_u32(hashes[i]);
                }
                if (!passed) {
                    summary << " duplicateFaces=";
                    bool firstDuplicate = true;
                    for (size_t i = 0; i < hashes.size(); ++i) {
                        for (size_t j = i + 1; j < hashes.size(); ++j) {
                            if (std::equal(bytes.begin() + static_cast<std::ptrdiff_t>(i * faceStride),
                                           bytes.begin() + static_cast<std::ptrdiff_t>((i + 1u) * faceStride),
                                           bytes.begin() + static_cast<std::ptrdiff_t>(j * faceStride))) {
                                if (!firstDuplicate) {
                                    summary << ",";
                                }
                                summary << kFaceLabels[i] << "=" << kFaceLabels[j];
                                firstDuplicate = false;
                            }
                        }
                    }
                }
            }
            break;
        case ProbeKind::SkyboxRotation:
            {
                std::array<uint32_t, 3> yaw0{};
                std::array<uint32_t, 3> yaw90{};
                for (size_t i = 0; i < yaw0.size(); ++i) {
                    yaw0[i] = pack_rgba_u32(bytes.data() + i * sizeof(uint32_t));
                    yaw90[i] = pack_rgba_u32(bytes.data() + (i + yaw0.size()) * sizeof(uint32_t));
                }
                passed = yaw0 != yaw90;
                summary << "yaw0=" << hex_u32(yaw0[0]) << "/" << hex_u32(yaw0[1]) << "/" << hex_u32(yaw0[2]) << " yaw90="
                        << hex_u32(yaw90[0]) << "/" << hex_u32(yaw90[1]) << "/" << hex_u32(yaw90[2]);
            }
            break;
        case ProbeKind::FaceMipPreview:
            {
                const uint32_t mip0 = pack_rgba_u32(bytes.data());
                const uint32_t mipLast = pack_rgba_u32(bytes.data() + sizeof(uint32_t));
                passed = mip0 != mipLast;
                summary << "face0m0=" << hex_u32(mip0) << " face0mLast=" << hex_u32(mipLast);
            }
            break;
        case ProbeKind::FaceIsolation:
            {
                const std::array<float, 4> beforeTarget = unpack_rgba16f(bytes.data());
                const std::array<float, 4> beforeControl = unpack_rgba16f(bytes.data() + 8u);
                const std::array<float, 4> afterTarget = unpack_rgba16f(bytes.data() + 16u);
                const std::array<float, 4> afterControl = unpack_rgba16f(bytes.data() + 24u);
                passed = !approx_equal(beforeTarget, afterTarget) && approx_equal(beforeControl, afterControl);
                summary << "targetBefore=" << format_float4(beforeTarget) << " targetAfter=" << format_float4(afterTarget)
                        << " controlBefore=" << format_float4(beforeControl) << " controlAfter=" << format_float4(afterControl);
                if (passed) {
                    summary << " only target subresource modified";
                }
            }
            break;
        case ProbeKind::EnvMips:
            {
                const uint32_t mipCount = std::max(1u, m_state->cubeMipLevels);
                passed = true;
                for (uint32_t mip = 0; mip < mipCount; ++mip) {
                    const std::array<float, 4> value = unpack_rgba16f(bytes.data() + static_cast<size_t>(mip) * 8u);
                    const bool nonEmpty = value[0] > 0.0001f || value[1] > 0.0001f || value[2] > 0.0001f;
                    passed = passed && nonEmpty;
                    summary << "m" << mip << "=" << format_float4(value);
                    if (!nonEmpty) {
                        summary << "[empty]";
                    }
                    if (mip + 1u < mipCount) {
                        summary << " ";
                    }
                }
            }
            break;
        case ProbeKind::IrradianceVsEnv:
            {
                std::array<uint32_t, 3> envPixels{};
                std::array<uint32_t, 3> irradiancePixels{};
                for (size_t i = 0; i < envPixels.size(); ++i) {
                    envPixels[i] = pack_rgba_u32(bytes.data() + i * sizeof(uint32_t));
                    irradiancePixels[i] = pack_rgba_u32(bytes.data() + (i + envPixels.size()) * sizeof(uint32_t));
                }
                passed = irradiancePixels != std::array<uint32_t, 3>{} && envPixels != irradiancePixels;
                summary << "env=" << hex_u32(envPixels[0]) << "/" << hex_u32(envPixels[1]) << "/" << hex_u32(envPixels[2])
                        << " irradiance=" << hex_u32(irradiancePixels[0]) << "/" << hex_u32(irradiancePixels[1]) << "/"
                        << hex_u32(irradiancePixels[2]);
            }
            break;
        case ProbeKind::PrefilterMips:
            {
                const uint32_t mipCount = std::max(1u, m_state->prefilterMipLevels);
                passed = true;
                std::array<float, 4> firstValue{};
                std::array<float, 4> lastValue{};
                for (uint32_t mip = 0; mip < mipCount; ++mip) {
                    const std::array<float, 4> value = unpack_rgba16f(bytes.data() + static_cast<size_t>(mip) * 8u);
                    const bool nonEmpty = value[0] > 0.0001f || value[1] > 0.0001f || value[2] > 0.0001f;
                    passed = passed && nonEmpty;
                    if (mip == 0) {
                        firstValue = value;
                    }
                    if (mip + 1u == mipCount) {
                        lastValue = value;
                    }
                    summary << "m" << mip << "=" << format_float4(value);
                    if (mip + 1u < mipCount) {
                        summary << " ";
                    }
                }
                passed = passed && !approx_equal(firstValue, lastValue);
            }
            break;
        case ProbeKind::BrdfLutPreview:
            {
                std::array<uint32_t, 3> samples{};
                for (size_t i = 0; i < samples.size(); ++i) {
                    samples[i] = pack_rgba_u32(bytes.data() + i * sizeof(uint32_t));
                }
                passed = samples[0] != 0u && (samples[0] != samples[1] || samples[1] != samples[2]);
                summary << "samples=" << hex_u32(samples[0]) << "/" << hex_u32(samples[1]) << "/" << hex_u32(samples[2]);
            }
            break;
        case ProbeKind::None:
        default:
            summary << "unexpected empty probe kind";
            break;
    }

    probe.passed = passed;
    probe.summary = summary.str();
    if (passed) {
        LUNA_CORE_INFO("RhiIblLab probe {} PASS {}", probe_kind_label(pending.kind), probe.summary);
    } else {
        LUNA_CORE_ERROR("RhiIblLab probe {} FAIL {}", probe_kind_label(pending.kind), probe.summary);
    }
    return true;
}

bool RhiIblLabRenderPipeline::queue_cube_faces_distinct_probe(luna::IRHIDevice&,
                                                              const luna::FrameContext& frameContext,
                                                              size_t slot)
{
    constexpr std::array<std::pair<float, float>, kCubeDistinctProbeSampleCount> sampleUv = {{
        {0.13f, 0.17f},
        {0.31f, 0.43f},
        {0.47f, 0.21f},
        {0.62f, 0.74f},
        {0.78f, 0.36f},
        {0.19f, 0.83f},
        {0.71f, 0.58f},
        {0.91f, 0.88f},
    }};

    const uint32_t extent = std::max(1u, m_state->cubeSize);
    if (frameContext.commandContext->imageBarrier({.image = m_envCubeImage,
                                                   .newLayout = luna::ImageLayout::TransferSrc,
                                                   .srcStage = luna::PipelineStage::AllCommands,
                                                   .dstStage = luna::PipelineStage::Transfer,
                                                   .srcAccess = luna::ResourceAccess::ShaderRead |
                                                                luna::ResourceAccess::ColorAttachmentWrite |
                                                                luna::ResourceAccess::TransferWrite,
                                                   .dstAccess = luna::ResourceAccess::TransferRead,
                                                   .aspect = luna::ImageAspect::Color,
                                                   .baseMipLevel = 0,
                                                   .mipCount = 1,
                                                   .baseArrayLayer = 0,
                                                   .layerCount = kCubeFaceCount}) != luna::RHIResult::Success) {
        return false;
    }

    for (uint32_t face = 0; face < kCubeFaceCount; ++face) {
        for (size_t sample = 0; sample < sampleUv.size(); ++sample) {
            const uint32_t sampleX = normalized_probe_coord(extent, sampleUv[sample].first);
            const uint32_t sampleY = normalized_probe_coord(extent, sampleUv[sample].second);
            const uint64_t offset =
                static_cast<uint64_t>(face * sampleUv.size() + sample) * static_cast<uint64_t>(kProbeTexelBytesRGBA16F);
            if (frameContext.commandContext->copyImageToBuffer({.buffer = m_probeBuffers[slot],
                                                                .image = m_envCubeImage,
                                                                .bufferOffset = offset,
                                                                .aspect = luna::ImageAspect::Color,
                                                                .mipLevel = 0,
                                                                .baseArrayLayer = face,
                                                                .layerCount = 1,
                                                                .imageOffsetX = sampleX,
                                                                .imageOffsetY = sampleY,
                                                                .imageExtentWidth = 1,
                                                                .imageExtentHeight = 1,
                                                                .imageExtentDepth = 1}) != luna::RHIResult::Success) {
                return false;
            }
        }
    }

    if (frameContext.commandContext->bufferBarrier({.buffer = m_probeBuffers[slot],
                                                    .srcStage = luna::PipelineStage::Transfer,
                                                    .dstStage = luna::PipelineStage::Host,
                                                    .srcAccess = luna::ResourceAccess::TransferWrite,
                                                    .dstAccess = luna::ResourceAccess::HostRead}) != luna::RHIResult::Success ||
        frameContext.commandContext->imageBarrier({.image = m_envCubeImage,
                                                   .oldLayout = luna::ImageLayout::TransferSrc,
                                                   .newLayout = luna::ImageLayout::ShaderReadOnly,
                                                   .srcStage = luna::PipelineStage::Transfer,
                                                   .dstStage = luna::PipelineStage::FragmentShader,
                                                   .srcAccess = luna::ResourceAccess::TransferRead,
                                                   .dstAccess = luna::ResourceAccess::ShaderRead,
                                                   .aspect = luna::ImageAspect::Color,
                                                   .baseMipLevel = 0,
                                                   .mipCount = 1,
                                                   .baseArrayLayer = 0,
                                                   .layerCount = kCubeFaceCount}) != luna::RHIResult::Success) {
        return false;
    }
    m_probePending[slot] = {.kind = ProbeKind::CubeFacesDistinct};
    return true;
}

bool RhiIblLabRenderPipeline::queue_skybox_rotation_probe(luna::IRHIDevice&,
                                                          const luna::FrameContext& frameContext,
                                                          size_t slot)
{
    const RenderTarget probeTarget{m_probeImage, {}, m_presentBackbufferFormat, kProbeWidth, kProbeHeight};
    const std::array<std::pair<uint32_t, uint32_t>, 3> samplePoints = {{
        {kProbeWidth / 2u, kProbeHeight / 2u},
        {kProbeWidth / 4u, kProbeHeight / 2u},
        {(kProbeWidth * 3u) / 4u, kProbeHeight / 3u},
    }};
    if (!render_skybox(frameContext, probeTarget, m_envCubeView, 0.0f, 0.0f) ||
        frameContext.commandContext->transitionImage(m_probeImage, luna::ImageLayout::TransferSrc) != luna::RHIResult::Success) {
        return false;
    }
    for (size_t i = 0; i < samplePoints.size(); ++i) {
        if (!copy_probe_pixel(frameContext, m_probeBuffers[slot], sizeof(uint32_t) * i, samplePoints[i].first, samplePoints[i].second)) {
            return false;
        }
    }

    if (!render_skybox(frameContext, probeTarget, m_envCubeView, kHalfPi, 0.0f) ||
        frameContext.commandContext->transitionImage(m_probeImage, luna::ImageLayout::TransferSrc) != luna::RHIResult::Success) {
        return false;
    }
    for (size_t i = 0; i < samplePoints.size(); ++i) {
        if (!copy_probe_pixel(frameContext,
                              m_probeBuffers[slot],
                              sizeof(uint32_t) * (i + samplePoints.size()),
                              samplePoints[i].first,
                              samplePoints[i].second)) {
            return false;
        }
    }

    if (frameContext.commandContext->bufferBarrier({.buffer = m_probeBuffers[slot],
                                                    .srcStage = luna::PipelineStage::Transfer,
                                                    .dstStage = luna::PipelineStage::Host,
                                                    .srcAccess = luna::ResourceAccess::TransferWrite,
                                                    .dstAccess = luna::ResourceAccess::HostRead}) != luna::RHIResult::Success) {
        return false;
    }
    m_probePending[slot] = {.kind = ProbeKind::SkyboxRotation};
    return true;
}

bool RhiIblLabRenderPipeline::queue_face_mip_preview_probe(luna::IRHIDevice&,
                                                           const luna::FrameContext& frameContext,
                                                           size_t slot)
{
    const RenderTarget probeTarget{m_probeImage, {}, m_presentBackbufferFormat, kProbeWidth, kProbeHeight};
    const uint32_t sampleX = (kProbeWidth * 3u) / 4u;
    const uint32_t sampleY = kProbeHeight / 4u;
    const uint32_t lastMip = std::max(1u, m_state->cubeMipLevels) - 1u;
    if (!render_face_preview(frameContext, probeTarget, m_faceViews[0], 0.0f, true) ||
        frameContext.commandContext->transitionImage(m_probeImage, luna::ImageLayout::TransferSrc) != luna::RHIResult::Success ||
        !copy_probe_pixel(frameContext, m_probeBuffers[slot], 0, sampleX, sampleY) ||
        !render_face_preview(frameContext, probeTarget, m_faceViews[0], static_cast<float>(lastMip), true) ||
        frameContext.commandContext->transitionImage(m_probeImage, luna::ImageLayout::TransferSrc) != luna::RHIResult::Success ||
        !copy_probe_pixel(frameContext, m_probeBuffers[slot], sizeof(uint32_t), sampleX, sampleY) ||
        frameContext.commandContext->bufferBarrier({.buffer = m_probeBuffers[slot],
                                                    .srcStage = luna::PipelineStage::Transfer,
                                                    .dstStage = luna::PipelineStage::Host,
                                                    .srcAccess = luna::ResourceAccess::TransferWrite,
                                                    .dstAccess = luna::ResourceAccess::HostRead}) != luna::RHIResult::Success) {
        return false;
    }
    m_probePending[slot] = {.kind = ProbeKind::FaceMipPreview};
    return true;
}

bool RhiIblLabRenderPipeline::queue_face_isolation_probe(luna::IRHIDevice& device,
                                                         const luna::FrameContext& frameContext,
                                                         size_t slot)
{
    const uint32_t targetFace = static_cast<uint32_t>(clamp_face_index(std::max(0, m_state->selectedFace)));
    const uint32_t targetMip =
        static_cast<uint32_t>(std::clamp(m_state->selectedMip, 0, static_cast<int>(std::max(1u, m_state->cubeMipLevels) - 1u)));
    const uint32_t controlFace = (targetFace + 1u) % kCubeFaceCount;

    if (frameContext.commandContext->imageBarrier({.image = m_envCubeImage,
                                                   .newLayout = luna::ImageLayout::TransferSrc,
                                                   .srcStage = luna::PipelineStage::AllCommands,
                                                   .dstStage = luna::PipelineStage::Transfer,
                                                   .srcAccess = luna::ResourceAccess::ShaderRead |
                                                                luna::ResourceAccess::ColorAttachmentWrite |
                                                                luna::ResourceAccess::TransferWrite,
                                                   .dstAccess = luna::ResourceAccess::TransferRead,
                                                   .aspect = luna::ImageAspect::Color,
                                                   .baseMipLevel = targetMip,
                                                   .mipCount = 1,
                                                   .baseArrayLayer = targetFace,
                                                   .layerCount = 1}) != luna::RHIResult::Success ||
        frameContext.commandContext->imageBarrier({.image = m_envCubeImage,
                                                   .newLayout = luna::ImageLayout::TransferSrc,
                                                   .srcStage = luna::PipelineStage::AllCommands,
                                                   .dstStage = luna::PipelineStage::Transfer,
                                                   .srcAccess = luna::ResourceAccess::ShaderRead |
                                                                luna::ResourceAccess::ColorAttachmentWrite |
                                                                luna::ResourceAccess::TransferWrite,
                                                   .dstAccess = luna::ResourceAccess::TransferRead,
                                                   .aspect = luna::ImageAspect::Color,
                                                   .baseMipLevel = targetMip,
                                                   .mipCount = 1,
                                                   .baseArrayLayer = controlFace,
                                                   .layerCount = 1}) != luna::RHIResult::Success ||
        frameContext.commandContext->copyImageToBuffer({.buffer = m_probeBuffers[slot],
                                                        .image = m_envCubeImage,
                                                        .bufferOffset = 0,
                                                        .aspect = luna::ImageAspect::Color,
                                                        .mipLevel = targetMip,
                                                        .baseArrayLayer = targetFace,
                                                        .layerCount = 1,
                                                        .imageExtentWidth = 1,
                                                        .imageExtentHeight = 1,
                                                        .imageExtentDepth = 1}) != luna::RHIResult::Success ||
        frameContext.commandContext->copyImageToBuffer({.buffer = m_probeBuffers[slot],
                                                        .image = m_envCubeImage,
                                                        .bufferOffset = 8u,
                                                        .aspect = luna::ImageAspect::Color,
                                                        .mipLevel = targetMip,
                                                        .baseArrayLayer = controlFace,
                                                        .layerCount = 1,
                                                        .imageExtentWidth = 1,
                                                        .imageExtentHeight = 1,
                                                        .imageExtentDepth = 1}) != luna::RHIResult::Success) {
        return false;
    }

    const luna::ImageViewHandle attachmentView = ensure_attachment_view(device, targetFace, targetMip);
    if (!attachmentView.isValid()) {
        return false;
    }

    const uint32_t targetSize = std::max(1u, m_state->cubeSize >> targetMip);
    const luna::ClearColorValue clearColor = {.r = 0.96f, .g = 0.12f, .b = 0.72f, .a = 1.0f};
    if (frameContext.commandContext->beginRendering({.width = targetSize,
                                                    .height = targetSize,
                                                    .colorAttachments = {{m_envCubeImage, kEnvCubeFormat, clearColor, attachmentView}}}) !=
            luna::RHIResult::Success ||
        frameContext.commandContext->endRendering() != luna::RHIResult::Success ||
        frameContext.commandContext->imageBarrier({.image = m_envCubeImage,
                                                   .oldLayout = luna::ImageLayout::ColorAttachment,
                                                   .newLayout = luna::ImageLayout::TransferSrc,
                                                   .srcStage = luna::PipelineStage::ColorAttachmentOutput,
                                                   .dstStage = luna::PipelineStage::Transfer,
                                                   .srcAccess = luna::ResourceAccess::ColorAttachmentWrite,
                                                   .dstAccess = luna::ResourceAccess::TransferRead,
                                                   .aspect = luna::ImageAspect::Color,
                                                   .baseMipLevel = targetMip,
                                                   .mipCount = 1,
                                                   .baseArrayLayer = targetFace,
                                                   .layerCount = 1}) != luna::RHIResult::Success ||
        frameContext.commandContext->copyImageToBuffer({.buffer = m_probeBuffers[slot],
                                                        .image = m_envCubeImage,
                                                        .bufferOffset = 16u,
                                                        .aspect = luna::ImageAspect::Color,
                                                        .mipLevel = targetMip,
                                                        .baseArrayLayer = targetFace,
                                                        .layerCount = 1,
                                                        .imageExtentWidth = 1,
                                                        .imageExtentHeight = 1,
                                                        .imageExtentDepth = 1}) != luna::RHIResult::Success ||
        frameContext.commandContext->copyImageToBuffer({.buffer = m_probeBuffers[slot],
                                                        .image = m_envCubeImage,
                                                        .bufferOffset = 24u,
                                                        .aspect = luna::ImageAspect::Color,
                                                        .mipLevel = targetMip,
                                                        .baseArrayLayer = controlFace,
                                                        .layerCount = 1,
                                                        .imageExtentWidth = 1,
                                                        .imageExtentHeight = 1,
                                                        .imageExtentDepth = 1}) != luna::RHIResult::Success ||
        frameContext.commandContext->bufferBarrier({.buffer = m_probeBuffers[slot],
                                                    .srcStage = luna::PipelineStage::Transfer,
                                                    .dstStage = luna::PipelineStage::Host,
                                                    .srcAccess = luna::ResourceAccess::TransferWrite,
                                                    .dstAccess = luna::ResourceAccess::HostRead}) != luna::RHIResult::Success ||
        frameContext.commandContext->imageBarrier({.image = m_envCubeImage,
                                                   .oldLayout = luna::ImageLayout::TransferSrc,
                                                   .newLayout = luna::ImageLayout::ShaderReadOnly,
                                                   .srcStage = luna::PipelineStage::Transfer,
                                                   .dstStage = luna::PipelineStage::FragmentShader,
                                                   .srcAccess = luna::ResourceAccess::TransferRead,
                                                   .dstAccess = luna::ResourceAccess::ShaderRead,
                                                   .aspect = luna::ImageAspect::Color,
                                                   .baseMipLevel = targetMip,
                                                   .mipCount = 1,
                                                   .baseArrayLayer = targetFace,
                                                   .layerCount = 1}) != luna::RHIResult::Success ||
        frameContext.commandContext->imageBarrier({.image = m_envCubeImage,
                                                   .oldLayout = luna::ImageLayout::TransferSrc,
                                                   .newLayout = luna::ImageLayout::ShaderReadOnly,
                                                   .srcStage = luna::PipelineStage::Transfer,
                                                   .dstStage = luna::PipelineStage::FragmentShader,
                                                   .srcAccess = luna::ResourceAccess::TransferRead,
                                                   .dstAccess = luna::ResourceAccess::ShaderRead,
                                                   .aspect = luna::ImageAspect::Color,
                                                   .baseMipLevel = targetMip,
                                                   .mipCount = 1,
                                                   .baseArrayLayer = controlFace,
                                                   .layerCount = 1}) != luna::RHIResult::Success) {
        return false;
    }

    m_probePending[slot] = {.kind = ProbeKind::FaceIsolation, .face = targetFace, .mip = targetMip, .auxFace = controlFace};
    return true;
}

bool RhiIblLabRenderPipeline::queue_env_mips_probe(luna::IRHIDevice&,
                                                   const luna::FrameContext& frameContext,
                                                   size_t slot)
{
    const uint32_t mipCount = std::max(1u, m_state->cubeMipLevels);
    if (frameContext.commandContext->imageBarrier({.image = m_envCubeImage,
                                                   .newLayout = luna::ImageLayout::TransferSrc,
                                                   .srcStage = luna::PipelineStage::AllCommands,
                                                   .dstStage = luna::PipelineStage::Transfer,
                                                   .srcAccess = luna::ResourceAccess::ShaderRead |
                                                                luna::ResourceAccess::ColorAttachmentWrite |
                                                                luna::ResourceAccess::TransferWrite,
                                                   .dstAccess = luna::ResourceAccess::TransferRead,
                                                   .aspect = luna::ImageAspect::Color,
                                                   .baseMipLevel = 0,
                                                   .mipCount = mipCount,
                                                   .baseArrayLayer = 0,
                                                   .layerCount = 1}) != luna::RHIResult::Success) {
        return false;
    }

    for (uint32_t mip = 0; mip < mipCount; ++mip) {
        const uint32_t mipExtent = std::max(1u, m_state->cubeSize >> mip);
        const uint32_t sampleX = std::min(mipExtent - 1u, std::max(1u, mipExtent / 2u));
        const uint32_t sampleY = std::min(mipExtent - 1u, std::max(1u, mipExtent / 2u));
        if (frameContext.commandContext->copyImageToBuffer({.buffer = m_probeBuffers[slot],
                                                            .image = m_envCubeImage,
                                                            .bufferOffset = static_cast<uint64_t>(mip) * 8u,
                                                            .aspect = luna::ImageAspect::Color,
                                                            .mipLevel = mip,
                                                            .baseArrayLayer = 0,
                                                            .layerCount = 1,
                                                            .imageOffsetX = sampleX,
                                                            .imageOffsetY = sampleY,
                                                            .imageExtentWidth = 1,
                                                            .imageExtentHeight = 1,
                                                            .imageExtentDepth = 1}) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (frameContext.commandContext->bufferBarrier({.buffer = m_probeBuffers[slot],
                                                    .srcStage = luna::PipelineStage::Transfer,
                                                    .dstStage = luna::PipelineStage::Host,
                                                    .srcAccess = luna::ResourceAccess::TransferWrite,
                                                    .dstAccess = luna::ResourceAccess::HostRead}) != luna::RHIResult::Success ||
        frameContext.commandContext->imageBarrier({.image = m_envCubeImage,
                                                   .oldLayout = luna::ImageLayout::TransferSrc,
                                                   .newLayout = luna::ImageLayout::ShaderReadOnly,
                                                   .srcStage = luna::PipelineStage::Transfer,
                                                   .dstStage = luna::PipelineStage::FragmentShader,
                                                   .srcAccess = luna::ResourceAccess::TransferRead,
                                                   .dstAccess = luna::ResourceAccess::ShaderRead,
                                                   .aspect = luna::ImageAspect::Color,
                                                   .baseMipLevel = 0,
                                                   .mipCount = mipCount,
                                                   .baseArrayLayer = 0,
                                                   .layerCount = 1}) != luna::RHIResult::Success) {
        return false;
    }

    m_probePending[slot] = {.kind = ProbeKind::EnvMips};
    return true;
}

bool RhiIblLabRenderPipeline::queue_irradiance_probe(luna::IRHIDevice&,
                                                     const luna::FrameContext& frameContext,
                                                     size_t slot)
{
    const RenderTarget probeTarget{m_probeImage, {}, m_presentBackbufferFormat, kProbeWidth, kProbeHeight};
    const uint32_t tileWidth = kProbeWidth / 3u;
    const uint32_t tileHeight = kProbeHeight / 2u;
    const std::array<std::pair<uint32_t, uint32_t>, 3> envSamplePoints = {{
        {tileWidth / 2u, tileHeight / 2u},
        {tileWidth / 4u, tileHeight / 4u},
        {(tileWidth * 3u) / 4u, (tileHeight * 3u) / 4u},
    }};
    const std::array<std::pair<uint32_t, uint32_t>, 3> irradianceSamplePoints = {{
        {kProbeWidth / 2u, kProbeHeight / 2u},
        {kProbeWidth / 3u, kProbeHeight / 3u},
        {(kProbeWidth * 2u) / 3u, (kProbeHeight * 2u) / 3u},
    }};

    if (!render_cube_atlas(frameContext, probeTarget, m_envCubeView, 0, 0.0f) ||
        frameContext.commandContext->transitionImage(m_probeImage, luna::ImageLayout::TransferSrc) != luna::RHIResult::Success) {
        return false;
    }
    for (size_t i = 0; i < envSamplePoints.size(); ++i) {
        if (!copy_probe_pixel(frameContext,
                              m_probeBuffers[slot],
                              static_cast<uint64_t>(i) * sizeof(uint32_t),
                              envSamplePoints[i].first,
                              envSamplePoints[i].second)) {
            return false;
        }
    }

    if (!render_face_preview(frameContext, probeTarget, m_irradianceCube.faceViews[0], 0.0f, true) ||
        frameContext.commandContext->transitionImage(m_probeImage, luna::ImageLayout::TransferSrc) != luna::RHIResult::Success) {
        return false;
    }
    for (size_t i = 0; i < irradianceSamplePoints.size(); ++i) {
        if (!copy_probe_pixel(frameContext,
                              m_probeBuffers[slot],
                              static_cast<uint64_t>(i + envSamplePoints.size()) * sizeof(uint32_t),
                              irradianceSamplePoints[i].first,
                              irradianceSamplePoints[i].second)) {
            return false;
        }
    }

    if (frameContext.commandContext->bufferBarrier({.buffer = m_probeBuffers[slot],
                                                    .srcStage = luna::PipelineStage::Transfer,
                                                    .dstStage = luna::PipelineStage::Host,
                                                    .srcAccess = luna::ResourceAccess::TransferWrite,
                                                    .dstAccess = luna::ResourceAccess::HostRead}) != luna::RHIResult::Success) {
        return false;
    }

    m_probePending[slot] = {.kind = ProbeKind::IrradianceVsEnv};
    return true;
}

bool RhiIblLabRenderPipeline::queue_prefilter_mips_probe(luna::IRHIDevice&,
                                                         const luna::FrameContext& frameContext,
                                                         size_t slot)
{
    const uint32_t mipCount = std::max(1u, m_prefilterCube.mipLevels);
    if (frameContext.commandContext->imageBarrier({.image = m_prefilterCube.image,
                                                   .newLayout = luna::ImageLayout::TransferSrc,
                                                   .srcStage = luna::PipelineStage::AllCommands,
                                                   .dstStage = luna::PipelineStage::Transfer,
                                                   .srcAccess = luna::ResourceAccess::ShaderRead |
                                                                luna::ResourceAccess::ColorAttachmentWrite |
                                                                luna::ResourceAccess::TransferWrite,
                                                   .dstAccess = luna::ResourceAccess::TransferRead,
                                                   .aspect = luna::ImageAspect::Color,
                                                   .baseMipLevel = 0,
                                                   .mipCount = mipCount,
                                                   .baseArrayLayer = 0,
                                                   .layerCount = 1}) != luna::RHIResult::Success) {
        return false;
    }

    for (uint32_t mip = 0; mip < mipCount; ++mip) {
        const uint32_t mipExtent = std::max(1u, m_prefilterCube.size >> mip);
        const uint32_t sampleX = std::min(mipExtent - 1u, std::max(1u, mipExtent / 2u));
        const uint32_t sampleY = std::min(mipExtent - 1u, std::max(1u, mipExtent / 2u));
        if (frameContext.commandContext->copyImageToBuffer({.buffer = m_probeBuffers[slot],
                                                            .image = m_prefilterCube.image,
                                                            .bufferOffset = static_cast<uint64_t>(mip) * 8u,
                                                            .aspect = luna::ImageAspect::Color,
                                                            .mipLevel = mip,
                                                            .baseArrayLayer = 0,
                                                            .layerCount = 1,
                                                            .imageOffsetX = sampleX,
                                                            .imageOffsetY = sampleY,
                                                            .imageExtentWidth = 1,
                                                            .imageExtentHeight = 1,
                                                            .imageExtentDepth = 1}) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (frameContext.commandContext->bufferBarrier({.buffer = m_probeBuffers[slot],
                                                    .srcStage = luna::PipelineStage::Transfer,
                                                    .dstStage = luna::PipelineStage::Host,
                                                    .srcAccess = luna::ResourceAccess::TransferWrite,
                                                    .dstAccess = luna::ResourceAccess::HostRead}) != luna::RHIResult::Success ||
        frameContext.commandContext->imageBarrier({.image = m_prefilterCube.image,
                                                   .oldLayout = luna::ImageLayout::TransferSrc,
                                                   .newLayout = luna::ImageLayout::ShaderReadOnly,
                                                   .srcStage = luna::PipelineStage::Transfer,
                                                   .dstStage = luna::PipelineStage::FragmentShader,
                                                   .srcAccess = luna::ResourceAccess::TransferRead,
                                                   .dstAccess = luna::ResourceAccess::ShaderRead,
                                                   .aspect = luna::ImageAspect::Color,
                                                   .baseMipLevel = 0,
                                                   .mipCount = mipCount,
                                                   .baseArrayLayer = 0,
                                                   .layerCount = 1}) != luna::RHIResult::Success) {
        return false;
    }

    m_probePending[slot] = {.kind = ProbeKind::PrefilterMips};
    return true;
}

bool RhiIblLabRenderPipeline::queue_brdf_lut_preview_probe(luna::IRHIDevice&,
                                                           const luna::FrameContext& frameContext,
                                                           size_t slot)
{
    const RenderTarget probeTarget{m_probeImage, {}, m_presentBackbufferFormat, kProbeWidth, kProbeHeight};
    const std::array<std::pair<uint32_t, uint32_t>, 3> samplePoints = {{
        {kProbeWidth / 4u, kProbeHeight / 4u},
        {kProbeWidth / 2u, kProbeHeight / 2u},
        {(kProbeWidth * 3u) / 4u, (kProbeHeight * 3u) / 4u},
    }};
    if (!render_face_preview(frameContext, probeTarget, m_brdfLutView, 0.0f, false) ||
        frameContext.commandContext->transitionImage(m_probeImage, luna::ImageLayout::TransferSrc) != luna::RHIResult::Success) {
        return false;
    }
    for (size_t i = 0; i < samplePoints.size(); ++i) {
        if (!copy_probe_pixel(frameContext,
                              m_probeBuffers[slot],
                              static_cast<uint64_t>(i) * sizeof(uint32_t),
                              samplePoints[i].first,
                              samplePoints[i].second)) {
            return false;
        }
    }

    if (frameContext.commandContext->bufferBarrier({.buffer = m_probeBuffers[slot],
                                                    .srcStage = luna::PipelineStage::Transfer,
                                                    .dstStage = luna::PipelineStage::Host,
                                                    .srcAccess = luna::ResourceAccess::TransferWrite,
                                                    .dstAccess = luna::ResourceAccess::HostRead}) != luna::RHIResult::Success) {
        return false;
    }

    m_probePending[slot] = {.kind = ProbeKind::BrdfLutPreview};
    return true;
}

bool RhiIblLabRenderPipeline::create_cube_texture(luna::IRHIDevice& device,
                                                  CubeTexture& cube,
                                                  uint32_t size,
                                                  uint32_t mipLevels,
                                                  std::string_view debugBaseName,
                                                  const void* initialData)
{
    destroy_cube_texture(device, cube);
    const std::string baseName(debugBaseName);
    const std::string cubeViewName = baseName + "View";
    const std::string faceViewName = baseName + "FaceView";

    luna::ImageDesc imageDesc{};
    imageDesc.width = size;
    imageDesc.height = size;
    imageDesc.depth = 1;
    imageDesc.mipLevels = mipLevels;
    imageDesc.arrayLayers = kCubeFaceCount;
    imageDesc.type = luna::ImageType::Cube;
    imageDesc.format = kEnvCubeFormat;
    imageDesc.usage = luna::ImageUsage::Sampled | luna::ImageUsage::ColorAttachment;
    imageDesc.debugName = baseName;
    if (device.createImage(imageDesc, &cube.image, initialData) != luna::RHIResult::Success) {
        destroy_cube_texture(device, cube);
        return false;
    }

    luna::ImageViewDesc cubeViewDesc{};
    cubeViewDesc.image = cube.image;
    cubeViewDesc.type = luna::ImageViewType::Cube;
    cubeViewDesc.aspect = luna::ImageAspect::Color;
    cubeViewDesc.baseMipLevel = 0;
    cubeViewDesc.mipCount = mipLevels;
    cubeViewDesc.baseArrayLayer = 0;
    cubeViewDesc.layerCount = kCubeFaceCount;
    cubeViewDesc.debugName = cubeViewName;
    if (device.createImageView(cubeViewDesc, &cube.cubeView) != luna::RHIResult::Success) {
        destroy_cube_texture(device, cube);
        return false;
    }

    for (uint32_t face = 0; face < kCubeFaceCount; ++face) {
        luna::ImageViewDesc faceDesc{};
        faceDesc.image = cube.image;
        faceDesc.type = luna::ImageViewType::Image2D;
        faceDesc.aspect = luna::ImageAspect::Color;
        faceDesc.baseMipLevel = 0;
        faceDesc.mipCount = mipLevels;
        faceDesc.baseArrayLayer = face;
        faceDesc.layerCount = 1;
        faceDesc.debugName = faceViewName;
        if (device.createImageView(faceDesc, &cube.faceViews[face]) != luna::RHIResult::Success) {
            destroy_cube_texture(device, cube);
            return false;
        }
    }

    cube.attachmentViews.assign(static_cast<size_t>(mipLevels) * kCubeFaceCount, {});
    cube.size = size;
    cube.mipLevels = mipLevels;
    std::fill(m_boundCubeViews.begin(), m_boundCubeViews.end(), luna::ImageViewHandle{});
    std::fill(m_boundFaceViews.begin(), m_boundFaceViews.end(), luna::ImageViewHandle{});
    return true;
}

void RhiIblLabRenderPipeline::destroy_cube_texture(luna::IRHIDevice& device, CubeTexture& cube)
{
    for (luna::ImageViewHandle& view : cube.attachmentViews) {
        if (view.isValid()) {
            device.destroyImageView(view);
            view = {};
        }
    }
    cube.attachmentViews.clear();

    for (luna::ImageViewHandle& view : cube.faceViews) {
        if (view.isValid()) {
            device.destroyImageView(view);
            view = {};
        }
    }

    if (cube.cubeView.isValid()) {
        device.destroyImageView(cube.cubeView);
        cube.cubeView = {};
    }
    if (cube.image.isValid()) {
        device.destroyImage(cube.image);
        cube.image = {};
    }

    cube.size = 0;
    cube.mipLevels = 0;
    std::fill(m_boundCubeViews.begin(), m_boundCubeViews.end(), luna::ImageViewHandle{});
    std::fill(m_boundFaceViews.begin(), m_boundFaceViews.end(), luna::ImageViewHandle{});
}

luna::ImageViewHandle RhiIblLabRenderPipeline::ensure_attachment_view(luna::IRHIDevice& device,
                                                                      uint32_t faceIndex,
                                                                      uint32_t mipLevel)
{
    if (!m_envCubeImage.isValid() || m_attachmentViews.empty()) {
        return {};
    }

    const size_t index = static_cast<size_t>(faceIndex) * std::max(1u, m_state->cubeMipLevels) + mipLevel;
    if (index >= m_attachmentViews.size()) {
        return {};
    }
    if (m_attachmentViews[index].isValid()) {
        return m_attachmentViews[index];
    }

    luna::ImageViewDesc desc{};
    desc.image = m_envCubeImage;
    desc.type = luna::ImageViewType::Image2D;
    desc.aspect = luna::ImageAspect::Color;
    desc.baseMipLevel = mipLevel;
    desc.mipCount = 1;
    desc.baseArrayLayer = faceIndex;
    desc.layerCount = 1;
    desc.debugName = "RhiIblLabCubeAttachmentView";
    if (device.createImageView(desc, &m_attachmentViews[index]) != luna::RHIResult::Success) {
        return {};
    }
    return m_attachmentViews[index];
}

luna::ImageViewHandle RhiIblLabRenderPipeline::ensure_attachment_view(luna::IRHIDevice& device,
                                                                      CubeTexture& cube,
                                                                      uint32_t faceIndex,
                                                                      uint32_t mipLevel,
                                                                      std::string_view debugName)
{
    if (!cube.image.isValid() || cube.attachmentViews.empty()) {
        return {};
    }

    const size_t index = static_cast<size_t>(faceIndex) * std::max(1u, cube.mipLevels) + mipLevel;
    if (index >= cube.attachmentViews.size()) {
        return {};
    }
    if (cube.attachmentViews[index].isValid()) {
        return cube.attachmentViews[index];
    }

    luna::ImageViewDesc desc{};
    desc.image = cube.image;
    desc.type = luna::ImageViewType::Image2D;
    desc.aspect = luna::ImageAspect::Color;
    desc.baseMipLevel = mipLevel;
    desc.mipCount = 1;
    desc.baseArrayLayer = faceIndex;
    desc.layerCount = 1;
    desc.debugName = debugName;
    if (device.createImageView(desc, &cube.attachmentViews[index]) != luna::RHIResult::Success) {
        return {};
    }
    return cube.attachmentViews[index];
}

bool RhiIblLabRenderPipeline::copy_probe_pixel(const luna::FrameContext& frameContext,
                                               luna::BufferHandle buffer,
                                               uint64_t bufferOffset,
                                               uint32_t x,
                                               uint32_t y) const
{
    return frameContext.commandContext->copyImageToBuffer({.buffer = buffer,
                                                           .image = m_probeImage,
                                                           .bufferOffset = bufferOffset,
                                                           .aspect = luna::ImageAspect::Color,
                                                           .mipLevel = 0,
                                                           .baseArrayLayer = 0,
                                                           .layerCount = 1,
                                                           .imageOffsetX = x,
                                                           .imageOffsetY = y,
                                                           .imageExtentWidth = 1,
                                                           .imageExtentHeight = 1,
                                                           .imageExtentDepth = 1}) == luna::RHIResult::Success;
}

void RhiIblLabRenderPipeline::destroy_shared_resources(luna::IRHIDevice& device)
{
    for (luna::ResourceSetHandle& set : m_cubeSets) {
        if (set.isValid()) {
            device.destroyResourceSet(set);
            set = {};
        }
    }
    for (luna::ResourceSetHandle& set : m_faceSets) {
        if (set.isValid()) {
            device.destroyResourceSet(set);
            set = {};
        }
    }
    m_cubeSets.clear();
    m_faceSets.clear();
    m_boundCubeViews.clear();
    m_boundFaceViews.clear();

    if (m_cubeLayout.isValid()) {
        device.destroyResourceLayout(m_cubeLayout);
        m_cubeLayout = {};
    }
    if (m_faceLayout.isValid()) {
        device.destroyResourceLayout(m_faceLayout);
        m_faceLayout = {};
    }
    if (m_linearSampler.isValid()) {
        device.destroySampler(m_linearSampler);
        m_linearSampler = {};
    }

    m_framesInFlight = 0;
}

void RhiIblLabRenderPipeline::destroy_source_hdr(luna::IRHIDevice& device)
{
    if (m_sourcePreviewView.isValid()) {
        device.destroyImageView(m_sourcePreviewView);
        m_sourcePreviewView = {};
    }
    if (m_sourcePreviewImage.isValid()) {
        device.destroyImage(m_sourcePreviewImage);
        m_sourcePreviewImage = {};
    }

    m_sourceHdrPath.clear();
    m_sourceHdrWidth = 0;
    m_sourceHdrHeight = 0;
    m_sourceHdrChannels = 0;
    m_sourceHdrPixels.clear();
    std::fill(m_boundFaceViews.begin(), m_boundFaceViews.end(), luna::ImageViewHandle{});

    if (m_state != nullptr) {
        m_state->sourceHdrReady = false;
        m_state->sourceWidth = 0;
        m_state->sourceHeight = 0;
        m_state->sourceChannels = 0;
    }
}

void RhiIblLabRenderPipeline::destroy_env_cube(luna::IRHIDevice& device)
{
    for (luna::ImageViewHandle& view : m_attachmentViews) {
        if (view.isValid()) {
            device.destroyImageView(view);
            view = {};
        }
    }
    m_attachmentViews.clear();

    for (luna::ImageViewHandle& view : m_faceViews) {
        if (view.isValid()) {
            device.destroyImageView(view);
            view = {};
        }
    }

    if (m_envCubeView.isValid()) {
        device.destroyImageView(m_envCubeView);
        m_envCubeView = {};
    }
    if (m_envCubeImage.isValid()) {
        device.destroyImage(m_envCubeImage);
        m_envCubeImage = {};
    }

    std::fill(m_boundCubeViews.begin(), m_boundCubeViews.end(), luna::ImageViewHandle{});
    std::fill(m_boundFaceViews.begin(), m_boundFaceViews.end(), luna::ImageViewHandle{});

    if (m_state != nullptr) {
        m_state->envCubeReady = false;
    }
}

void RhiIblLabRenderPipeline::destroy_irradiance_cube(luna::IRHIDevice& device)
{
    destroy_cube_texture(device, m_irradianceCube);
    if (m_state != nullptr) {
        m_state->irradianceReady = false;
    }
}

void RhiIblLabRenderPipeline::destroy_prefilter_cube(luna::IRHIDevice& device)
{
    destroy_cube_texture(device, m_prefilterCube);
    if (m_state != nullptr) {
        m_state->prefilterReady = false;
        m_state->prefilterGenerated = false;
    }
}

void RhiIblLabRenderPipeline::destroy_brdf_lut(luna::IRHIDevice& device)
{
    if (m_brdfLutView.isValid()) {
        device.destroyImageView(m_brdfLutView);
        m_brdfLutView = {};
    }
    if (m_brdfLutImage.isValid()) {
        device.destroyImage(m_brdfLutImage);
        m_brdfLutImage = {};
    }

    std::fill(m_boundFaceViews.begin(), m_boundFaceViews.end(), luna::ImageViewHandle{});
    if (m_state != nullptr) {
        m_state->brdfLutReady = false;
        m_state->brdfLutWidth = 0;
        m_state->brdfLutHeight = 0;
        m_state->brdfLutPath.clear();
    }
}

void RhiIblLabRenderPipeline::destroy_present_pipelines(luna::IRHIDevice& device)
{
    if (m_cubeAtlasPipeline.isValid()) {
        device.destroyPipeline(m_cubeAtlasPipeline);
        m_cubeAtlasPipeline = {};
    }
    if (m_skyboxPipeline.isValid()) {
        device.destroyPipeline(m_skyboxPipeline);
        m_skyboxPipeline = {};
    }
    if (m_facePreviewPipeline.isValid()) {
        device.destroyPipeline(m_facePreviewPipeline);
        m_facePreviewPipeline = {};
    }
    if (m_cubeFilterPipeline.isValid()) {
        device.destroyPipeline(m_cubeFilterPipeline);
        m_cubeFilterPipeline = {};
    }
    m_presentBackbufferFormat = luna::PixelFormat::Undefined;
}

void RhiIblLabRenderPipeline::destroy_probe_resources(luna::IRHIDevice& device)
{
    for (luna::BufferHandle& buffer : m_probeBuffers) {
        if (buffer.isValid()) {
            device.destroyBuffer(buffer);
            buffer = {};
        }
    }
    m_probeBuffers.clear();
    m_probePending.clear();

    if (m_probeImage.isValid()) {
        device.destroyImage(m_probeImage);
        m_probeImage = {};
    }
}

std::string RhiIblLabRenderPipeline::resolve_source_hdr_path() const
{
    namespace fs = std::filesystem;
    fs::path base = fs::current_path();
    const fs::path relative = fs::path("assets") / "newport_loft.hdr";
    for (int depth = 0; depth < 6; ++depth) {
        const fs::path candidate = (base / relative).lexically_normal();
        if (fs::exists(candidate)) {
            return candidate.generic_string();
        }
        if (!base.has_parent_path()) {
            break;
        }
        base = base.parent_path();
    }
    return relative.generic_string();
}

std::string RhiIblLabRenderPipeline::resolve_brdf_lut_path() const
{
    namespace fs = std::filesystem;
    fs::path base = fs::current_path();
    const fs::path relative = fs::path("assets") / "brdf_lut_rg16f.bin";
    for (int depth = 0; depth < 6; ++depth) {
        const fs::path candidate = (base / relative).lexically_normal();
        if (fs::exists(candidate)) {
            return candidate.generic_string();
        }
        if (!base.has_parent_path()) {
            break;
        }
        base = base.parent_path();
    }
    return relative.generic_string();
}

bool RhiIblLabRenderPipeline::load_brdf_lut_pixels(std::vector<uint16_t>* outPixels,
                                                   uint32_t* outWidth,
                                                   uint32_t* outHeight,
                                                   std::string* outPath) const
{
    if (outPixels == nullptr || outWidth == nullptr || outHeight == nullptr || outPath == nullptr) {
        return false;
    }

    const std::string resolvedPath = resolve_brdf_lut_path();
    std::ifstream file(resolvedPath, std::ios::binary);
    if (!file) {
        if (m_state != nullptr) {
            m_state->brdfLutStatus = "Failed to open BRDF LUT file from " + resolvedPath + ".";
            m_state->brdfLutPath = resolvedPath;
        }
        return false;
    }

    BrdfLutFileHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file || header.magic != kBrdfLutFileMagic || header.version != kBrdfLutFileVersion ||
        header.format != kBrdfLutFileFormatRG16Float || header.width == 0 || header.height == 0) {
        if (m_state != nullptr) {
            m_state->brdfLutStatus = "BRDF LUT file header is invalid: " + resolvedPath + ".";
            m_state->brdfLutPath = resolvedPath;
        }
        return false;
    }

    const size_t valueCount = static_cast<size_t>(header.width) * static_cast<size_t>(header.height) * 2u;
    outPixels->assign(valueCount, 0u);
    file.read(reinterpret_cast<char*>(outPixels->data()), static_cast<std::streamsize>(valueCount * sizeof(uint16_t)));
    if (!file) {
        if (m_state != nullptr) {
            m_state->brdfLutStatus = "BRDF LUT payload is truncated: " + resolvedPath + ".";
            m_state->brdfLutPath = resolvedPath;
        }
        outPixels->clear();
        return false;
    }

    *outWidth = header.width;
    *outHeight = header.height;
    *outPath = resolvedPath;
    return true;
}

bool RhiIblLabRenderPipeline::load_source_hdr_pixels()
{
    const std::string resolvedPath = resolve_source_hdr_path();
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(0);
    float* pixels = stbi_loadf(resolvedPath.c_str(), &width, &height, &channels, 4);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        const char* failureReason = stbi_failure_reason();
        if (m_state != nullptr) {
            std::ostringstream status;
            status << "Failed to load HDR source from " << resolvedPath;
            if (failureReason != nullptr) {
                status << " (" << failureReason << ")";
            }
            m_state->sourceHdrStatus = status.str();
            m_state->sourcePath = resolvedPath;
        }
        LUNA_CORE_ERROR("RhiIblLab failed to load HDR source '{}' reason='{}'",
                        resolvedPath,
                        failureReason != nullptr ? failureReason : "unknown");
        return false;
    }

    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    m_sourceHdrPixels.assign(pixels, pixels + pixelCount);
    stbi_image_free(pixels);

    m_sourceHdrPath = std::filesystem::path(resolvedPath).lexically_normal().generic_string();
    m_sourceHdrWidth = static_cast<uint32_t>(width);
    m_sourceHdrHeight = static_cast<uint32_t>(height);
    m_sourceHdrChannels = static_cast<uint32_t>(std::max(channels, 0));
    return true;
}

std::vector<uint8_t> RhiIblLabRenderPipeline::build_source_preview_data() const
{
    if (m_sourceHdrPixels.empty() || m_sourceHdrWidth == 0 || m_sourceHdrHeight == 0) {
        return {};
    }

    std::vector<uint8_t> pixels(static_cast<size_t>(m_sourceHdrWidth) * m_sourceHdrHeight * 4u, 0u);
    for (uint32_t y = 0; y < m_sourceHdrHeight; ++y) {
        for (uint32_t x = 0; x < m_sourceHdrWidth; ++x) {
            const size_t index = (static_cast<size_t>(y) * m_sourceHdrWidth + x) * 4u;
            const glm::vec3 hdrColor = glm::max(glm::vec3(m_sourceHdrPixels[index + 0],
                                                          m_sourceHdrPixels[index + 1],
                                                          m_sourceHdrPixels[index + 2]),
                                                glm::vec3(0.0f));
            const glm::vec3 ldrColor = tone_map_reinhard(hdrColor);
            write_rgba(pixels, index, ldrColor.r, ldrColor.g, ldrColor.b, 1.0f);
        }
    }
    return pixels;
}

std::array<float, 4> RhiIblLabRenderPipeline::sample_source_bilinear(float u, float v) const
{
    if (m_sourceHdrPixels.empty() || m_sourceHdrWidth == 0 || m_sourceHdrHeight == 0) {
        return {0.0f, 0.0f, 0.0f, 1.0f};
    }

    u = u - std::floor(u);
    v = std::clamp(v, 0.0f, 1.0f);

    const float fx = u * static_cast<float>(m_sourceHdrWidth - 1u);
    const float fy = v * static_cast<float>(m_sourceHdrHeight - 1u);
    const uint32_t x0 = static_cast<uint32_t>(std::floor(fx));
    const uint32_t y0 = static_cast<uint32_t>(std::floor(fy));
    const uint32_t x1 = (x0 + 1u) % m_sourceHdrWidth;
    const uint32_t y1 = std::min(y0 + 1u, m_sourceHdrHeight - 1u);
    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);

    const auto fetch = [&](uint32_t x, uint32_t y) {
        const size_t index = (static_cast<size_t>(y) * m_sourceHdrWidth + x) * 4u;
        return glm::vec4(m_sourceHdrPixels[index + 0],
                         m_sourceHdrPixels[index + 1],
                         m_sourceHdrPixels[index + 2],
                         m_sourceHdrPixels[index + 3]);
    };

    const glm::vec4 top = glm::mix(fetch(x0, y0), fetch(x1, y0), tx);
    const glm::vec4 bottom = glm::mix(fetch(x0, y1), fetch(x1, y1), tx);
    const glm::vec4 sample = glm::mix(top, bottom, ty);
    return {sample.r, sample.g, sample.b, sample.a};
}

std::array<float, 4> RhiIblLabRenderPipeline::sample_source_direction(uint32_t faceIndex, float u, float v) const
{
    const auto direction = face_direction(faceIndex, u, v);
    const float phi = std::atan2(direction[2], direction[0]);
    const float theta = std::asin(std::clamp(direction[1], -1.0f, 1.0f));
    const float sampleU = phi / (2.0f * kPi) + 0.5f;
    const float sampleV = 0.5f - theta / kPi;
    return sample_source_bilinear(sampleU, sampleV);
}

std::vector<uint16_t> RhiIblLabRenderPipeline::build_env_cube_data(uint32_t size) const
{
    if (m_sourceHdrPixels.empty() || size == 0) {
        return {};
    }

    std::vector<uint16_t> pixels(static_cast<size_t>(size) * size * kCubeFaceCount * 4u, 0u);
    const float invExtent = 1.0f / static_cast<float>(size);
    for (uint32_t face = 0; face < kCubeFaceCount; ++face) {
        for (uint32_t y = 0; y < size; ++y) {
            for (uint32_t x = 0; x < size; ++x) {
                const float u = (static_cast<float>(x) + 0.5f) * invExtent;
                const float v = (static_cast<float>(y) + 0.5f) * invExtent;
                const std::array<float, 4> sample = sample_source_direction(face, u, v);
                const glm::vec4 hdrColor(glm::max(sample[0], 0.0f),
                                         glm::max(sample[1], 0.0f),
                                         glm::max(sample[2], 0.0f),
                                         1.0f);
                const size_t index = (static_cast<size_t>(face) * size * size + static_cast<size_t>(y) * size + x) * 4u;
                pixels[index + 0] = glm::packHalf1x16(hdrColor.r);
                pixels[index + 1] = glm::packHalf1x16(hdrColor.g);
                pixels[index + 2] = glm::packHalf1x16(hdrColor.b);
                pixels[index + 3] = glm::packHalf1x16(hdrColor.a);
            }
        }
    }

    return pixels;
}

} // namespace ibl_lab
