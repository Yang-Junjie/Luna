#include "BindingLabPipeline.h"

#include "Core/log.h"
#include "RHI/CommandContext.h"
#include "Vulkan/vk_rhi_device.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string_view>

namespace binding_lab {
namespace {

struct alignas(16) GlobalData {
    float tint[4] = {};
};

struct alignas(16) MaterialData {
    float color[4] = {};
};

struct alignas(16) ObjectData {
    float offsetScale[4] = {};
};

struct alignas(16) DynamicObjectData {
    float color[4] = {};
    float offsetScale[4] = {};
};

struct alignas(16) ArrayPushConstants {
    int textureIndex = 0;
    int padding[3] = {};
};

std::string shader_path(const std::string& root, std::string_view fileName)
{
    return (std::filesystem::path(root) / fileName).lexically_normal().generic_string();
}

uint8_t to_u8(float value)
{
    return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

void write_rgba(std::vector<uint8_t>& pixels, size_t offset, float r, float g, float b, float a)
{
    pixels[offset + 0] = to_u8(r);
    pixels[offset + 1] = to_u8(g);
    pixels[offset + 2] = to_u8(b);
    pixels[offset + 3] = to_u8(a);
}

uint32_t align_up(uint32_t value, uint32_t alignment)
{
    if (alignment == 0) {
        return value;
    }
    return (value + alignment - 1u) & ~(alignment - 1u);
}

} // namespace

RhiBindingLabRenderPipeline::RhiBindingLabRenderPipeline(std::shared_ptr<State> state)
    : m_state(std::move(state))
{}

bool RhiBindingLabRenderPipeline::init(luna::IRHIDevice& device)
{
    m_vulkanDevice = dynamic_cast<luna::VulkanRHIDevice*>(&device);
    if (m_vulkanDevice == nullptr) {
        LUNA_CORE_ERROR("RhiBindingLab requires the Vulkan RHI backend");
        return false;
    }

    m_shaderRoot = std::filesystem::path{RHI_BINDING_LAB_SHADER_ROOT}.lexically_normal().generic_string();
    return true;
}

void RhiBindingLabRenderPipeline::shutdown(luna::IRHIDevice& device)
{
    destroy_probe_pipeline(device);
    destroy_multi_set_resources(device);
    destroy_descriptor_array_resources(device);
    destroy_dynamic_uniform_resources(device);
    destroy_shared_resources(device);
    m_vulkanDevice = nullptr;
}

bool RhiBindingLabRenderPipeline::render(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    if (m_state == nullptr || frameContext.commandContext == nullptr || !frameContext.backbuffer.isValid()) {
        return false;
    }

    if (!ensure_shared_resources(device)) {
        return false;
    }

    switch (m_state->page) {
        case Page::MultiSet:
            return render_multi_set(device, frameContext);
        case Page::DescriptorArray:
            return render_descriptor_array(device, frameContext);
        case Page::DynamicUniform:
            return render_dynamic_uniform(device, frameContext);
        default:
            return false;
    }
}

bool RhiBindingLabRenderPipeline::ensure_shared_resources(luna::IRHIDevice& device)
{
    if (m_linearSampler.isValid() && m_dummyTexture.isValid()) {
        return true;
    }

    if (!m_linearSampler.isValid()) {
        luna::SamplerDesc samplerDesc{};
        samplerDesc.debugName = "RhiBindingLabLinearSampler";
        if (device.createSampler(samplerDesc, &m_linearSampler) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (!m_dummyTexture.isValid()) {
        const uint8_t whitePixel[4] = {255, 255, 255, 255};
        luna::ImageDesc imageDesc{};
        imageDesc.width = 1;
        imageDesc.height = 1;
        imageDesc.depth = 1;
        imageDesc.mipLevels = 1;
        imageDesc.arrayLayers = 1;
        imageDesc.type = luna::ImageType::Image2D;
        imageDesc.format = luna::PixelFormat::RGBA8Unorm;
        imageDesc.usage = luna::ImageUsage::Sampled;
        imageDesc.debugName = "RhiBindingLabDummyTexture";
        if (device.createImage(imageDesc, &m_dummyTexture, whitePixel) != luna::RHIResult::Success) {
            return false;
        }
    }

    return true;
}

std::vector<uint8_t> RhiBindingLabRenderPipeline::build_texture_pixels(uint32_t width, uint32_t height, int variant) const
{
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 255);

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const float fx = static_cast<float>(x) / static_cast<float>(std::max(1u, width - 1));
            const float fy = static_cast<float>(y) / static_cast<float>(std::max(1u, height - 1));
            const size_t offset = (static_cast<size_t>(y) * width + x) * 4;

            switch (variant) {
                case 0:
                    write_rgba(pixels, offset, 0.92f, fx, 0.18f + 0.45f * fy, 1.0f);
                    break;
                case 1:
                    write_rgba(pixels, offset, 0.10f + 0.75f * fy, 0.90f, fx, 1.0f);
                    break;
                case 2: {
                    const float checker = ((x / 16u) + (y / 16u)) % 2u == 0 ? 1.0f : 0.18f;
                    write_rgba(pixels, offset, checker, 0.24f, 0.96f, 1.0f);
                    break;
                }
                case 3:
                    write_rgba(pixels, offset, fy, 0.22f + 0.68f * fx, 0.18f, 1.0f);
                    break;
                default: {
                    const float stripe = std::fmod((fx * 11.0f + fy * 7.0f) * 3.1415926f, 1.0f);
                    write_rgba(pixels, offset, 0.98f, 0.68f * stripe, 0.10f + 0.50f * fy, 1.0f);
                    break;
                }
            }
        }
    }

    return pixels;
}

bool RhiBindingLabRenderPipeline::build_probe_pipeline(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat)
{
    auto& multiSet = m_state->multiSet;
    multiSet.buildLayoutRequested = false;

    std::vector<luna::ResourceLayoutHandle> selectedLayouts;
    if (multiSet.includeSet0) selectedLayouts.push_back(m_globalLayout);
    if (multiSet.includeSet1) selectedLayouts.push_back(m_materialLayout);
    if (multiSet.includeSet2) selectedLayouts.push_back(m_objectLayout);

    if (m_probePipeline.isValid()) {
        if (device.waitIdle() != luna::RHIResult::Success) {
            return false;
        }
        destroy_probe_pipeline(device);
    }

    const std::string vertexShaderPath = shader_path(m_shaderRoot, "binding_lab_fullscreen.vert.spv");
    const std::string fragmentShaderPath = shader_path(m_shaderRoot, "binding_lab_probe.frag.spv");

    luna::GraphicsPipelineDesc pipelineDesc{};
    pipelineDesc.debugName = "RhiBindingLabProbePipeline";
    pipelineDesc.vertexShader = {.stage = luna::ShaderType::Vertex, .filePath = vertexShaderPath};
    pipelineDesc.fragmentShader = {.stage = luna::ShaderType::Fragment, .filePath = fragmentShaderPath};
    pipelineDesc.resourceLayouts = selectedLayouts;
    pipelineDesc.cullMode = luna::CullMode::None;
    pipelineDesc.frontFace = luna::FrontFace::Clockwise;
    pipelineDesc.colorAttachments.push_back({backbufferFormat, false});

    const luna::RHIResult result = device.createGraphicsPipeline(pipelineDesc, &m_probePipeline);
    std::ostringstream status;
    if (result != luna::RHIResult::Success) {
        status << "Build Pipeline Layout failed: selected set composition is invalid.";
        multiSet.layoutStatus = status.str();
        return false;
    }

    status << "Pipeline layout build success: ";
    if (selectedLayouts.empty()) {
        status << "no descriptor sets";
    } else {
        bool first = true;
        if (multiSet.includeSet0) {
            status << (first ? "" : ", ") << "Set 0";
            first = false;
        }
        if (multiSet.includeSet1) {
            status << (first ? "" : ", ") << "Set 1";
            first = false;
        }
        if (multiSet.includeSet2) {
            status << (first ? "" : ", ") << "Set 2";
        }
    }
    multiSet.layoutStatus = status.str();
    return true;
}

bool RhiBindingLabRenderPipeline::run_conflict_probe(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat)
{
    auto& multiSet = m_state->multiSet;
    multiSet.conflictTestRequested = false;

    luna::ResourceLayoutHandle first{};
    luna::ResourceLayoutHandle second{};

    luna::ResourceLayoutDesc firstDesc{};
    firstDesc.debugName = "RhiBindingLabConflictA";
    firstDesc.setIndex = 1;
    firstDesc.bindings.push_back({0, luna::ResourceType::UniformBuffer, 1, luna::ShaderType::Fragment});

    luna::ResourceLayoutDesc secondDesc{};
    secondDesc.debugName = "RhiBindingLabConflictB";
    secondDesc.setIndex = 1;
    secondDesc.bindings.push_back({1, luna::ResourceType::UniformBuffer, 1, luna::ShaderType::Fragment});

    if (device.createResourceLayout(firstDesc, &first) != luna::RHIResult::Success ||
        device.createResourceLayout(secondDesc, &second) != luna::RHIResult::Success) {
        if (first.isValid()) device.destroyResourceLayout(first);
        if (second.isValid()) device.destroyResourceLayout(second);
        multiSet.layoutStatus = "Conflict probe failed before pipeline build.";
        return false;
    }

    const std::string vertexShaderPath = shader_path(m_shaderRoot, "binding_lab_fullscreen.vert.spv");
    const std::string fragmentShaderPath = shader_path(m_shaderRoot, "binding_lab_probe.frag.spv");

    luna::GraphicsPipelineDesc pipelineDesc{};
    pipelineDesc.debugName = "RhiBindingLabConflictProbe";
    pipelineDesc.vertexShader = {.stage = luna::ShaderType::Vertex, .filePath = vertexShaderPath};
    pipelineDesc.fragmentShader = {.stage = luna::ShaderType::Fragment, .filePath = fragmentShaderPath};
    pipelineDesc.resourceLayouts = {first, second};
    pipelineDesc.cullMode = luna::CullMode::None;
    pipelineDesc.frontFace = luna::FrontFace::Clockwise;
    pipelineDesc.colorAttachments.push_back({backbufferFormat, false});

    luna::PipelineHandle conflictPipeline{};
    const luna::RHIResult result = device.createGraphicsPipeline(pipelineDesc, &conflictPipeline);
    if (conflictPipeline.isValid()) {
        device.destroyPipeline(conflictPipeline);
    }
    device.destroyResourceLayout(second);
    device.destroyResourceLayout(first);

    multiSet.layoutStatus = result == luna::RHIResult::Success
                                ? "Conflict probe unexpectedly succeeded."
                                : "Conflict probe rejected duplicate set index as expected.";
    return result != luna::RHIResult::Success;
}

bool RhiBindingLabRenderPipeline::update_multi_set_buffers(luna::IRHIDevice& device)
{
    GlobalData global{};
    std::copy(m_state->multiSet.globalTint.begin(), m_state->multiSet.globalTint.end(), global.tint);

    MaterialData material{};
    std::copy(m_state->multiSet.materialColor.begin(), m_state->multiSet.materialColor.end(), material.color);

    ObjectData object{};
    object.offsetScale[0] = m_state->multiSet.objectOffset[0];
    object.offsetScale[1] = m_state->multiSet.objectOffset[1];
    object.offsetScale[2] = 0.28f;
    object.offsetScale[3] = 0.0f;

    return device.writeBuffer(m_globalBuffer, &global, sizeof(global), 0) == luna::RHIResult::Success &&
           device.writeBuffer(m_materialBuffer, &material, sizeof(material), 0) == luna::RHIResult::Success &&
           device.writeBuffer(m_objectBuffer, &object, sizeof(object), 0) == luna::RHIResult::Success;
}

bool RhiBindingLabRenderPipeline::ensure_multi_set_resources(luna::IRHIDevice& device, luna::PixelFormat backbufferFormat)
{
    auto& multiSet = m_state->multiSet;
    multiSet.layoutSummaries[0] = "Set 0: b0 UniformBuffer (Global)";
    multiSet.layoutSummaries[1] = "Set 1: b0 UniformBuffer, b1 CombinedImageSampler (Material)";
    multiSet.layoutSummaries[2] = "Set 2: b0 UniformBuffer (Object)";

    if (!m_globalLayout.isValid()) {
        luna::ResourceLayoutDesc desc{};
        desc.debugName = "RhiBindingLabGlobalLayout";
        desc.setIndex = 0;
        desc.bindings.push_back({0, luna::ResourceType::UniformBuffer, 1, luna::ShaderType::Fragment});
        if (device.createResourceLayout(desc, &m_globalLayout) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (!m_materialLayout.isValid()) {
        luna::ResourceLayoutDesc desc{};
        desc.debugName = "RhiBindingLabMaterialLayout";
        desc.setIndex = 1;
        desc.bindings.push_back({0, luna::ResourceType::UniformBuffer, 1, luna::ShaderType::Fragment});
        desc.bindings.push_back({1, luna::ResourceType::CombinedImageSampler, 1, luna::ShaderType::Fragment});
        if (device.createResourceLayout(desc, &m_materialLayout) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (!m_objectLayout.isValid()) {
        luna::ResourceLayoutDesc desc{};
        desc.debugName = "RhiBindingLabObjectLayout";
        desc.setIndex = 2;
        desc.bindings.push_back({0, luna::ResourceType::UniformBuffer, 1, luna::ShaderType::Fragment});
        if (device.createResourceLayout(desc, &m_objectLayout) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (!m_globalBuffer.isValid()) {
        const GlobalData neutralGlobal{{1.0f, 1.0f, 1.0f, 1.0f}};
        const MaterialData neutralMaterial{{1.0f, 1.0f, 1.0f, 1.0f}};
        const ObjectData neutralObject{{0.0f, 0.0f, 0.28f, 0.0f}};

        luna::BufferDesc bufferDesc{};
        bufferDesc.size = sizeof(GlobalData);
        bufferDesc.usage = luna::BufferUsage::Uniform;
        bufferDesc.memoryUsage = luna::MemoryUsage::Upload;
        bufferDesc.debugName = "RhiBindingLabGlobalBuffer";
        if (device.createBuffer(bufferDesc, &m_globalBuffer, &neutralGlobal) != luna::RHIResult::Success) return false;
        bufferDesc.debugName = "RhiBindingLabGlobalNeutralBuffer";
        if (device.createBuffer(bufferDesc, &m_globalNeutralBuffer, &neutralGlobal) != luna::RHIResult::Success) return false;

        bufferDesc.size = sizeof(MaterialData);
        bufferDesc.debugName = "RhiBindingLabMaterialBuffer";
        if (device.createBuffer(bufferDesc, &m_materialBuffer, &neutralMaterial) != luna::RHIResult::Success) return false;
        bufferDesc.debugName = "RhiBindingLabMaterialNeutralBuffer";
        if (device.createBuffer(bufferDesc, &m_materialNeutralBuffer, &neutralMaterial) != luna::RHIResult::Success) return false;

        bufferDesc.size = sizeof(ObjectData);
        bufferDesc.debugName = "RhiBindingLabObjectBuffer";
        if (device.createBuffer(bufferDesc, &m_objectBuffer, &neutralObject) != luna::RHIResult::Success) return false;
        bufferDesc.debugName = "RhiBindingLabObjectNeutralBuffer";
        if (device.createBuffer(bufferDesc, &m_objectNeutralBuffer, &neutralObject) != luna::RHIResult::Success) return false;
    }

    if (!m_globalSet.isValid()) {
        if (device.createResourceSet(m_globalLayout, &m_globalSet) != luna::RHIResult::Success ||
            device.createResourceSet(m_globalLayout, &m_globalNeutralSet) != luna::RHIResult::Success ||
            device.createResourceSet(m_materialLayout, &m_materialSet) != luna::RHIResult::Success ||
            device.createResourceSet(m_materialLayout, &m_materialNeutralSet) != luna::RHIResult::Success ||
            device.createResourceSet(m_objectLayout, &m_objectSet) != luna::RHIResult::Success ||
            device.createResourceSet(m_objectLayout, &m_objectNeutralSet) != luna::RHIResult::Success) {
            return false;
        }

        luna::ResourceSetWriteDesc globalWrite{};
        globalWrite.buffers.push_back({0, m_globalBuffer, 0, sizeof(GlobalData), luna::ResourceType::UniformBuffer});
        luna::ResourceSetWriteDesc globalNeutralWrite{};
        globalNeutralWrite.buffers.push_back(
            {0, m_globalNeutralBuffer, 0, sizeof(GlobalData), luna::ResourceType::UniformBuffer});

        luna::ResourceSetWriteDesc materialWrite{};
        materialWrite.buffers.push_back({0, m_materialBuffer, 0, sizeof(MaterialData), luna::ResourceType::UniformBuffer});
        materialWrite.images.push_back(
            {.binding = 1, .image = m_dummyTexture, .sampler = m_linearSampler, .type = luna::ResourceType::CombinedImageSampler});

        luna::ResourceSetWriteDesc materialNeutralWrite{};
        materialNeutralWrite.buffers.push_back(
            {0, m_materialNeutralBuffer, 0, sizeof(MaterialData), luna::ResourceType::UniformBuffer});
        materialNeutralWrite.images.push_back(
            {.binding = 1, .image = m_dummyTexture, .sampler = m_linearSampler, .type = luna::ResourceType::CombinedImageSampler});

        luna::ResourceSetWriteDesc objectWrite{};
        objectWrite.buffers.push_back({0, m_objectBuffer, 0, sizeof(ObjectData), luna::ResourceType::UniformBuffer});
        luna::ResourceSetWriteDesc objectNeutralWrite{};
        objectNeutralWrite.buffers.push_back(
            {0, m_objectNeutralBuffer, 0, sizeof(ObjectData), luna::ResourceType::UniformBuffer});

        if (device.updateResourceSet(m_globalSet, globalWrite) != luna::RHIResult::Success ||
            device.updateResourceSet(m_globalNeutralSet, globalNeutralWrite) != luna::RHIResult::Success ||
            device.updateResourceSet(m_materialSet, materialWrite) != luna::RHIResult::Success ||
            device.updateResourceSet(m_materialNeutralSet, materialNeutralWrite) != luna::RHIResult::Success ||
            device.updateResourceSet(m_objectSet, objectWrite) != luna::RHIResult::Success ||
            device.updateResourceSet(m_objectNeutralSet, objectNeutralWrite) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (m_multiSetPipeline.isValid() && m_multiSetBackbufferFormat != backbufferFormat) {
        if (device.waitIdle() != luna::RHIResult::Success) {
            return false;
        }
        device.destroyPipeline(m_multiSetPipeline);
        m_multiSetPipeline = {};
        m_multiSetBackbufferFormat = luna::PixelFormat::Undefined;
    }

    if (!m_multiSetPipeline.isValid()) {
        const std::string vertexShaderPath = shader_path(m_shaderRoot, "binding_lab_fullscreen.vert.spv");
        const std::string fragmentShaderPath = shader_path(m_shaderRoot, "binding_lab_multiset.frag.spv");

        luna::GraphicsPipelineDesc pipelineDesc{};
        pipelineDesc.debugName = "RhiBindingLabMultiSet";
        pipelineDesc.vertexShader = {.stage = luna::ShaderType::Vertex, .filePath = vertexShaderPath};
        pipelineDesc.fragmentShader = {.stage = luna::ShaderType::Fragment, .filePath = fragmentShaderPath};
        pipelineDesc.resourceLayouts = {m_globalLayout, m_materialLayout, m_objectLayout};
        pipelineDesc.cullMode = luna::CullMode::None;
        pipelineDesc.frontFace = luna::FrontFace::Clockwise;
        pipelineDesc.colorAttachments.push_back({backbufferFormat, false});
        if (device.createGraphicsPipeline(pipelineDesc, &m_multiSetPipeline) != luna::RHIResult::Success) {
            return false;
        }
        m_multiSetBackbufferFormat = backbufferFormat;
    }

    if (multiSet.conflictTestRequested && !run_conflict_probe(device, backbufferFormat)) {
        return false;
    }
    if (multiSet.buildLayoutRequested && !build_probe_pipeline(device, backbufferFormat)) {
        return false;
    }

    return update_multi_set_buffers(device);
}

bool RhiBindingLabRenderPipeline::ensure_descriptor_array_resources(luna::IRHIDevice& device,
                                                                    luna::PixelFormat backbufferFormat)
{
    auto& descriptorArray = m_state->descriptorArray;
    descriptorArray.textureIndex = std::clamp(descriptorArray.textureIndex, 0, 3);

    if (!m_descriptorArrayLayout.isValid()) {
        luna::ResourceLayoutDesc desc{};
        desc.debugName = "RhiBindingLabDescriptorArrayLayout";
        desc.setIndex = 0;
        desc.bindings.push_back({0, luna::ResourceType::CombinedImageSampler, 4, luna::ShaderType::Fragment});
        if (device.createResourceLayout(desc, &m_descriptorArrayLayout) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (!m_descriptorArrayTextures[0].isValid()) {
        static constexpr std::array<const char*, 4> kTextureNames = {
            "RhiBindingLabArrayTexture0",
            "RhiBindingLabArrayTexture1",
            "RhiBindingLabArrayTexture2",
            "RhiBindingLabArrayTexture3",
        };

        luna::ImageDesc imageDesc{};
        imageDesc.width = 96;
        imageDesc.height = 96;
        imageDesc.depth = 1;
        imageDesc.mipLevels = 1;
        imageDesc.arrayLayers = 1;
        imageDesc.type = luna::ImageType::Image2D;
        imageDesc.format = luna::PixelFormat::RGBA8Unorm;
        imageDesc.usage = luna::ImageUsage::Sampled;

        for (int index = 0; index < 4; ++index) {
            const std::vector<uint8_t> pixels = build_texture_pixels(imageDesc.width, imageDesc.height, index);
            imageDesc.debugName = kTextureNames[static_cast<size_t>(index)];
            if (device.createImage(imageDesc, &m_descriptorArrayTextures[static_cast<size_t>(index)], pixels.data()) !=
                luna::RHIResult::Success) {
                destroy_descriptor_array_resources(device);
                return false;
            }
        }

        const std::vector<uint8_t> alternatePixels = build_texture_pixels(imageDesc.width, imageDesc.height, 4);
        imageDesc.debugName = "RhiBindingLabArrayTextureAlternate";
        if (device.createImage(imageDesc, &m_descriptorArrayAlternateTexture, alternatePixels.data()) !=
            luna::RHIResult::Success) {
            destroy_descriptor_array_resources(device);
            return false;
        }
    }

    if (!m_descriptorArraySet.isValid()) {
        if (device.createResourceSet(m_descriptorArrayLayout, &m_descriptorArraySet) != luna::RHIResult::Success) {
            return false;
        }

        luna::ImageBindingWriteDesc imageWrite{};
        imageWrite.binding = 0;
        imageWrite.type = luna::ResourceType::CombinedImageSampler;
        for (const luna::ImageHandle image : m_descriptorArrayTextures) {
            imageWrite.elements.push_back({.image = image, .sampler = m_linearSampler});
        }

        luna::ResourceSetWriteDesc writeDesc{};
        writeDesc.images.push_back(std::move(imageWrite));
        if (device.updateResourceSet(m_descriptorArraySet, writeDesc) != luna::RHIResult::Success) {
            destroy_descriptor_array_resources(device);
            return false;
        }

        descriptorArray.slot2UsesAlternate = false;
    }

    if (m_descriptorArrayPipeline.isValid() && m_descriptorArrayBackbufferFormat != backbufferFormat) {
        if (device.waitIdle() != luna::RHIResult::Success) {
            return false;
        }
        device.destroyPipeline(m_descriptorArrayPipeline);
        m_descriptorArrayPipeline = {};
        m_descriptorArrayBackbufferFormat = luna::PixelFormat::Undefined;
    }

    if (!m_descriptorArrayPipeline.isValid()) {
        const std::string vertexShaderPath = shader_path(m_shaderRoot, "binding_lab_fullscreen.vert.spv");
        const std::string fragmentShaderPath = shader_path(m_shaderRoot, "binding_lab_descriptor_array.frag.spv");

        luna::GraphicsPipelineDesc pipelineDesc{};
        pipelineDesc.debugName = "RhiBindingLabDescriptorArray";
        pipelineDesc.vertexShader = {.stage = luna::ShaderType::Vertex, .filePath = vertexShaderPath};
        pipelineDesc.fragmentShader = {.stage = luna::ShaderType::Fragment, .filePath = fragmentShaderPath};
        pipelineDesc.resourceLayouts = {m_descriptorArrayLayout};
        pipelineDesc.pushConstantSize = sizeof(ArrayPushConstants);
        pipelineDesc.pushConstantVisibility = luna::ShaderType::Fragment;
        pipelineDesc.cullMode = luna::CullMode::None;
        pipelineDesc.frontFace = luna::FrontFace::Clockwise;
        pipelineDesc.colorAttachments.push_back({backbufferFormat, false});
        if (device.createGraphicsPipeline(pipelineDesc, &m_descriptorArrayPipeline) != luna::RHIResult::Success) {
            return false;
        }
        m_descriptorArrayBackbufferFormat = backbufferFormat;
    }

    return update_descriptor_array_set(device);
}

bool RhiBindingLabRenderPipeline::update_descriptor_array_set(luna::IRHIDevice& device)
{
    static constexpr std::array<const char*, 4> kOriginalLabels = {
        "Warm Gradient",
        "Mint Sweep",
        "Blue Checker",
        "Olive Ramp",
    };

    auto& descriptorArray = m_state->descriptorArray;
    descriptorArray.textureIndex = std::clamp(descriptorArray.textureIndex, 0, 3);

    if (descriptorArray.replaceSlotRequested) {
        descriptorArray.replaceSlotRequested = false;
        descriptorArray.slot2UsesAlternate = !descriptorArray.slot2UsesAlternate;

        if (device.waitIdle() != luna::RHIResult::Success) {
            return false;
        }

        luna::ImageBindingWriteDesc imageWrite{};
        imageWrite.binding = 0;
        imageWrite.type = luna::ResourceType::CombinedImageSampler;
        imageWrite.firstArrayElement = 2;
        imageWrite.elements.push_back(
            {.image = descriptorArray.slot2UsesAlternate ? m_descriptorArrayAlternateTexture : m_descriptorArrayTextures[2],
             .sampler = m_linearSampler});

        luna::ResourceSetWriteDesc writeDesc{};
        writeDesc.images.push_back(std::move(imageWrite));
        if (device.updateResourceSet(m_descriptorArraySet, writeDesc) != luna::RHIResult::Success) {
            return false;
        }
    }

    descriptorArray.slotLabels[0] = kOriginalLabels[0];
    descriptorArray.slotLabels[1] = kOriginalLabels[1];
    descriptorArray.slotLabels[2] = descriptorArray.slot2UsesAlternate ? "Alternate Amber Stripes" : kOriginalLabels[2];
    descriptorArray.slotLabels[3] = kOriginalLabels[3];

    std::ostringstream status;
    status << "binding 0 uploaded with 4 descriptors. ";
    status << "Texture Index=" << descriptorArray.textureIndex << ". ";
    status << "Slot 2 " << (descriptorArray.slot2UsesAlternate ? "was replaced by the alternate texture."
                                                                : "keeps the original texture.");
    descriptorArray.status = status.str();
    return true;
}

bool RhiBindingLabRenderPipeline::ensure_dynamic_uniform_resources(luna::IRHIDevice& device,
                                                                   luna::PixelFormat backbufferFormat)
{
    auto& dynamicUniform = m_state->dynamicUniform;
    dynamicUniform.objectIndex = std::clamp(dynamicUniform.objectIndex, 0, 3);

    if (!m_dynamicUniformLayout.isValid()) {
        luna::ResourceLayoutDesc desc{};
        desc.debugName = "RhiBindingLabDynamicUniformLayout";
        desc.setIndex = 0;
        desc.bindings.push_back({0, luna::ResourceType::DynamicUniformBuffer, 1, luna::ShaderType::Fragment});
        if (device.createResourceLayout(desc, &m_dynamicUniformLayout) != luna::RHIResult::Success) {
            return false;
        }
    }

    if (m_dynamicUniformStride == 0) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(m_vulkanDevice->getEngine()._chosenGPU, &properties);
        m_dynamicUniformStride = align_up(static_cast<uint32_t>(sizeof(DynamicObjectData)),
                                          properties.limits.minUniformBufferOffsetAlignment);
        if (m_dynamicUniformStride == 0) {
            m_dynamicUniformStride = static_cast<uint32_t>(sizeof(DynamicObjectData));
        }
    }

    if (!m_dynamicUniformBuffer.isValid()) {
        std::array<DynamicObjectData, 4> objects{};
        std::memcpy(objects[0].color, std::array<float, 4>{0.94f, 0.22f, 0.20f, 1.0f}.data(), sizeof(objects[0].color));
        std::memcpy(
            objects[0].offsetScale, std::array<float, 4>{-0.42f, -0.18f, 0.18f, 0.0f}.data(), sizeof(objects[0].offsetScale));
        std::memcpy(objects[1].color, std::array<float, 4>{0.18f, 0.82f, 0.34f, 1.0f}.data(), sizeof(objects[1].color));
        std::memcpy(
            objects[1].offsetScale, std::array<float, 4>{0.00f, 0.00f, 0.24f, 0.0f}.data(), sizeof(objects[1].offsetScale));
        std::memcpy(objects[2].color, std::array<float, 4>{0.16f, 0.42f, 0.96f, 1.0f}.data(), sizeof(objects[2].color));
        std::memcpy(
            objects[2].offsetScale, std::array<float, 4>{0.30f, 0.16f, 0.32f, 0.0f}.data(), sizeof(objects[2].offsetScale));
        std::memcpy(objects[3].color, std::array<float, 4>{0.96f, 0.82f, 0.16f, 1.0f}.data(), sizeof(objects[3].color));
        std::memcpy(
            objects[3].offsetScale, std::array<float, 4>{-0.16f, 0.26f, 0.14f, 0.0f}.data(), sizeof(objects[3].offsetScale));

        std::vector<uint8_t> bytes(static_cast<size_t>(m_dynamicUniformStride) * objects.size(), 0);
        for (size_t index = 0; index < objects.size(); ++index) {
            std::memcpy(bytes.data() + static_cast<size_t>(m_dynamicUniformStride) * index,
                        &objects[index],
                        sizeof(DynamicObjectData));
        }

        luna::BufferDesc bufferDesc{};
        bufferDesc.size = bytes.size();
        bufferDesc.usage = luna::BufferUsage::Uniform;
        bufferDesc.memoryUsage = luna::MemoryUsage::Upload;
        bufferDesc.debugName = "RhiBindingLabDynamicUniformBuffer";
        if (device.createBuffer(bufferDesc, &m_dynamicUniformBuffer, bytes.data()) != luna::RHIResult::Success) {
            destroy_dynamic_uniform_resources(device);
            return false;
        }
    }

    if (!m_dynamicUniformSet.isValid()) {
        if (device.createResourceSet(m_dynamicUniformLayout, &m_dynamicUniformSet) != luna::RHIResult::Success) {
            return false;
        }

        luna::ResourceSetWriteDesc writeDesc{};
        writeDesc.buffers.push_back(
            {0, m_dynamicUniformBuffer, 0, sizeof(DynamicObjectData), luna::ResourceType::DynamicUniformBuffer});
        if (device.updateResourceSet(m_dynamicUniformSet, writeDesc) != luna::RHIResult::Success) {
            destroy_dynamic_uniform_resources(device);
            return false;
        }
    }

    if (m_dynamicUniformPipeline.isValid() && m_dynamicUniformBackbufferFormat != backbufferFormat) {
        if (device.waitIdle() != luna::RHIResult::Success) {
            return false;
        }
        device.destroyPipeline(m_dynamicUniformPipeline);
        m_dynamicUniformPipeline = {};
        m_dynamicUniformBackbufferFormat = luna::PixelFormat::Undefined;
    }

    if (!m_dynamicUniformPipeline.isValid()) {
        const std::string vertexShaderPath = shader_path(m_shaderRoot, "binding_lab_fullscreen.vert.spv");
        const std::string fragmentShaderPath = shader_path(m_shaderRoot, "binding_lab_dynamic_uniform.frag.spv");

        luna::GraphicsPipelineDesc pipelineDesc{};
        pipelineDesc.debugName = "RhiBindingLabDynamicUniform";
        pipelineDesc.vertexShader = {.stage = luna::ShaderType::Vertex, .filePath = vertexShaderPath};
        pipelineDesc.fragmentShader = {.stage = luna::ShaderType::Fragment, .filePath = fragmentShaderPath};
        pipelineDesc.resourceLayouts = {m_dynamicUniformLayout};
        pipelineDesc.cullMode = luna::CullMode::None;
        pipelineDesc.frontFace = luna::FrontFace::Clockwise;
        pipelineDesc.colorAttachments.push_back({backbufferFormat, false});
        if (device.createGraphicsPipeline(pipelineDesc, &m_dynamicUniformPipeline) != luna::RHIResult::Success) {
            return false;
        }
        m_dynamicUniformBackbufferFormat = backbufferFormat;
    }

    dynamicUniform.dynamicOffset = static_cast<uint32_t>(dynamicUniform.objectIndex) * m_dynamicUniformStride;

    std::ostringstream status;
    status << "DynamicUniformBuffer stride=" << m_dynamicUniformStride << " bytes. ";
    status << "Object Index=" << dynamicUniform.objectIndex << ", Dynamic Offset=" << dynamicUniform.dynamicOffset
           << " bytes.";
    dynamicUniform.status = status.str();
    return true;
}

bool RhiBindingLabRenderPipeline::render_multi_set(luna::IRHIDevice& device, const luna::FrameContext& frameContext)
{
    if (!ensure_multi_set_resources(device, frameContext.backbufferFormat)) {
        return false;
    }

    auto& multiSet = m_state->multiSet;
    const luna::ResourceSetHandle globalSet = multiSet.bindGlobal ? m_globalSet : m_globalNeutralSet;
    const luna::ResourceSetHandle materialSet = multiSet.bindMaterial ? m_materialSet : m_materialNeutralSet;
    const luna::ResourceSetHandle objectSet = multiSet.bindObject ? m_objectSet : m_objectNeutralSet;

    std::ostringstream status;
    status << "Bound sets: Global=" << (multiSet.bindGlobal ? "active" : "neutral") << ", Material="
           << (multiSet.bindMaterial ? "active" : "neutral") << ", Object="
           << (multiSet.bindObject ? "active" : "neutral") << ".";
    multiSet.sampleStatus = status.str();

    return frameContext.commandContext->beginRendering({.width = frameContext.renderWidth,
                                                        .height = frameContext.renderHeight,
                                                        .colorAttachments = {{frameContext.backbuffer,
                                                                              frameContext.backbufferFormat,
                                                                              {0.04f, 0.05f, 0.07f, 1.0f}}}}) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->bindGraphicsPipeline(m_multiSetPipeline) == luna::RHIResult::Success &&
           frameContext.commandContext->bindResourceSet(globalSet) == luna::RHIResult::Success &&
           frameContext.commandContext->bindResourceSet(materialSet) == luna::RHIResult::Success &&
           frameContext.commandContext->bindResourceSet(objectSet) == luna::RHIResult::Success &&
           frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
           frameContext.commandContext->endRendering() == luna::RHIResult::Success;
}

bool RhiBindingLabRenderPipeline::render_descriptor_array(luna::IRHIDevice& device,
                                                          const luna::FrameContext& frameContext)
{
    if (!ensure_descriptor_array_resources(device, frameContext.backbufferFormat)) {
        return false;
    }

    ArrayPushConstants pushConstants{};
    pushConstants.textureIndex = m_state->descriptorArray.textureIndex;

    return frameContext.commandContext->beginRendering({.width = frameContext.renderWidth,
                                                        .height = frameContext.renderHeight,
                                                        .colorAttachments = {{frameContext.backbuffer,
                                                                              frameContext.backbufferFormat,
                                                                              {0.05f, 0.035f, 0.02f, 1.0f}}}}) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->bindGraphicsPipeline(m_descriptorArrayPipeline) == luna::RHIResult::Success &&
           frameContext.commandContext->bindResourceSet(m_descriptorArraySet) == luna::RHIResult::Success &&
           frameContext.commandContext->pushConstants(
               &pushConstants, sizeof(pushConstants), 0, luna::ShaderType::Fragment) == luna::RHIResult::Success &&
           frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
           frameContext.commandContext->endRendering() == luna::RHIResult::Success;
}

bool RhiBindingLabRenderPipeline::render_dynamic_uniform(luna::IRHIDevice& device,
                                                         const luna::FrameContext& frameContext)
{
    if (!ensure_dynamic_uniform_resources(device, frameContext.backbufferFormat)) {
        return false;
    }

    const std::array<uint32_t, 1> dynamicOffsets = {m_state->dynamicUniform.dynamicOffset};

    return frameContext.commandContext->beginRendering({.width = frameContext.renderWidth,
                                                        .height = frameContext.renderHeight,
                                                        .colorAttachments = {{frameContext.backbuffer,
                                                                              frameContext.backbufferFormat,
                                                                              {0.025f, 0.03f, 0.05f, 1.0f}}}}) ==
               luna::RHIResult::Success &&
           frameContext.commandContext->bindGraphicsPipeline(m_dynamicUniformPipeline) == luna::RHIResult::Success &&
           frameContext.commandContext->bindResourceSet(m_dynamicUniformSet, dynamicOffsets) == luna::RHIResult::Success &&
           frameContext.commandContext->draw({3, 1, 0, 0}) == luna::RHIResult::Success &&
           frameContext.commandContext->endRendering() == luna::RHIResult::Success;
}

void RhiBindingLabRenderPipeline::destroy_probe_pipeline(luna::IRHIDevice& device)
{
    if (m_probePipeline.isValid()) {
        device.destroyPipeline(m_probePipeline);
        m_probePipeline = {};
    }
}

void RhiBindingLabRenderPipeline::destroy_multi_set_resources(luna::IRHIDevice& device)
{
    if (m_multiSetPipeline.isValid()) device.destroyPipeline(m_multiSetPipeline);
    if (m_globalSet.isValid()) device.destroyResourceSet(m_globalSet);
    if (m_globalNeutralSet.isValid()) device.destroyResourceSet(m_globalNeutralSet);
    if (m_materialSet.isValid()) device.destroyResourceSet(m_materialSet);
    if (m_materialNeutralSet.isValid()) device.destroyResourceSet(m_materialNeutralSet);
    if (m_objectSet.isValid()) device.destroyResourceSet(m_objectSet);
    if (m_objectNeutralSet.isValid()) device.destroyResourceSet(m_objectNeutralSet);
    if (m_globalBuffer.isValid()) device.destroyBuffer(m_globalBuffer);
    if (m_globalNeutralBuffer.isValid()) device.destroyBuffer(m_globalNeutralBuffer);
    if (m_materialBuffer.isValid()) device.destroyBuffer(m_materialBuffer);
    if (m_materialNeutralBuffer.isValid()) device.destroyBuffer(m_materialNeutralBuffer);
    if (m_objectBuffer.isValid()) device.destroyBuffer(m_objectBuffer);
    if (m_objectNeutralBuffer.isValid()) device.destroyBuffer(m_objectNeutralBuffer);
    if (m_globalLayout.isValid()) device.destroyResourceLayout(m_globalLayout);
    if (m_materialLayout.isValid()) device.destroyResourceLayout(m_materialLayout);
    if (m_objectLayout.isValid()) device.destroyResourceLayout(m_objectLayout);

    m_multiSetPipeline = {};
    m_multiSetBackbufferFormat = luna::PixelFormat::Undefined;
    m_globalSet = {};
    m_globalNeutralSet = {};
    m_materialSet = {};
    m_materialNeutralSet = {};
    m_objectSet = {};
    m_objectNeutralSet = {};
    m_globalBuffer = {};
    m_globalNeutralBuffer = {};
    m_materialBuffer = {};
    m_materialNeutralBuffer = {};
    m_objectBuffer = {};
    m_objectNeutralBuffer = {};
    m_globalLayout = {};
    m_materialLayout = {};
    m_objectLayout = {};
}

void RhiBindingLabRenderPipeline::destroy_descriptor_array_resources(luna::IRHIDevice& device)
{
    if (m_descriptorArrayPipeline.isValid()) device.destroyPipeline(m_descriptorArrayPipeline);
    if (m_descriptorArraySet.isValid()) device.destroyResourceSet(m_descriptorArraySet);
    if (m_descriptorArrayAlternateTexture.isValid()) device.destroyImage(m_descriptorArrayAlternateTexture);
    for (luna::ImageHandle& image : m_descriptorArrayTextures) {
        if (image.isValid()) {
            device.destroyImage(image);
        }
        image = {};
    }
    if (m_descriptorArrayLayout.isValid()) device.destroyResourceLayout(m_descriptorArrayLayout);

    m_descriptorArrayPipeline = {};
    m_descriptorArrayBackbufferFormat = luna::PixelFormat::Undefined;
    m_descriptorArraySet = {};
    m_descriptorArrayAlternateTexture = {};
    m_descriptorArrayLayout = {};
}

void RhiBindingLabRenderPipeline::destroy_dynamic_uniform_resources(luna::IRHIDevice& device)
{
    if (m_dynamicUniformPipeline.isValid()) device.destroyPipeline(m_dynamicUniformPipeline);
    if (m_dynamicUniformSet.isValid()) device.destroyResourceSet(m_dynamicUniformSet);
    if (m_dynamicUniformBuffer.isValid()) device.destroyBuffer(m_dynamicUniformBuffer);
    if (m_dynamicUniformLayout.isValid()) device.destroyResourceLayout(m_dynamicUniformLayout);

    m_dynamicUniformPipeline = {};
    m_dynamicUniformBackbufferFormat = luna::PixelFormat::Undefined;
    m_dynamicUniformSet = {};
    m_dynamicUniformBuffer = {};
    m_dynamicUniformLayout = {};
    m_dynamicUniformStride = 0;
}

void RhiBindingLabRenderPipeline::destroy_shared_resources(luna::IRHIDevice& device)
{
    if (m_dummyTexture.isValid()) device.destroyImage(m_dummyTexture);
    if (m_linearSampler.isValid()) device.destroySampler(m_linearSampler);

    m_dummyTexture = {};
    m_linearSampler = {};
}

} // namespace binding_lab
