#include "Impls/Vulkan/VKCommon.h"
#include "Impls/Vulkan/VKDevice.h"
#include "Impls/Vulkan/VKPipeline.h"
#include "Impls/Vulkan/VKPipelineLayout.h"
#include "Impls/Vulkan/VKShaderModule.h"

namespace luna::RHI {
static void ConvertShaderStages(std::span<const Ref<ShaderModule>> shaderModules,
                                std::vector<vk::PipelineShaderStageCreateInfo>& outShaderStages)
{
    outShaderStages.resize(shaderModules.size());
    for (size_t i = 0; i < shaderModules.size(); i++) {
        if (!shaderModules[i]) {
            throw std::runtime_error("Null shader module in shader stages");
        }
        auto* vkShaderModule = static_cast<VKShaderModule*>(shaderModules[i].get());
        outShaderStages[i].stage = VKConverter::ConvertShaderStageBits(vkShaderModule->GetStage());
        outShaderStages[i].module = vkShaderModule->GetHandle();
        // Slang-generated SPIR-V entry point is typically "main".
        outShaderStages[i].pName = "main";
    }
}

static vk::PipelineVertexInputStateCreateInfo
    ConvertVertexInputState(std::span<const VertexInputBinding> bindings,
                            std::span<const VertexInputAttribute> attributes,
                            std::vector<vk::VertexInputBindingDescription>& outBindings,
                            std::vector<vk::VertexInputAttributeDescription>& outAttributes)
{
    outBindings.resize(bindings.size());
    for (size_t i = 0; i < bindings.size(); i++) {
        outBindings[i].binding = bindings[i].Binding;
        outBindings[i].stride = bindings[i].Stride;
        outBindings[i].inputRate = (bindings[i].InputRate == VertexInputRate::Vertex) ? vk::VertexInputRate::eVertex
                                                                                      : vk::VertexInputRate::eInstance;
    }
    outAttributes.resize(attributes.size());
    for (size_t i = 0; i < attributes.size(); i++) {
        outAttributes[i].location = attributes[i].Location;
        outAttributes[i].binding = attributes[i].Binding;
        outAttributes[i].format = VKConverter::Convert(attributes[i].Format);
        outAttributes[i].offset = attributes[i].Offset;
    }
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(outBindings.size());
    vertexInputInfo.pVertexBindingDescriptions = outBindings.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(outAttributes.size());
    vertexInputInfo.pVertexAttributeDescriptions = outAttributes.data();
    return vertexInputInfo;
}

static vk::PipelineInputAssemblyStateCreateInfo ConvertInputAssemblyState(const InputAssemblyState& inputAssembly)
{
    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {};
    inputAssemblyInfo.topology = VKConverter::Convert(inputAssembly.Topology);
    inputAssemblyInfo.primitiveRestartEnable = inputAssembly.PrimitiveRestartEnable;
    return inputAssemblyInfo;
}

static vk::PipelineRasterizationStateCreateInfo ConvertRasterizationState(const RasterizationState& rasterizer)
{
    vk::PipelineRasterizationStateCreateInfo rasterizerInfo = {};
    rasterizerInfo.depthClampEnable = rasterizer.DepthClampEnable;
    rasterizerInfo.rasterizerDiscardEnable = rasterizer.RasterizerDiscardEnable;
    rasterizerInfo.polygonMode = VKConverter::Convert(rasterizer.PolygonMode);
    rasterizerInfo.cullMode = VKConverter::Convert(rasterizer.CullMode);
    rasterizerInfo.frontFace = VKConverter::Convert(rasterizer.FrontFace);
    rasterizerInfo.depthBiasEnable = rasterizer.DepthBiasEnable;
    rasterizerInfo.depthBiasConstantFactor = rasterizer.DepthBiasConstantFactor;
    rasterizerInfo.depthBiasClamp = rasterizer.DepthBiasClamp;
    rasterizerInfo.depthBiasSlopeFactor = rasterizer.DepthBiasSlopeFactor;
    rasterizerInfo.lineWidth = rasterizer.LineWidth;
    return rasterizerInfo;
}

static vk::PipelineColorBlendAttachmentState
    ConvertColorBlendAttachmentState(const ColorBlendAttachmentState& attachment)
{
    vk::PipelineColorBlendAttachmentState vkAttachment = {};
    vkAttachment.blendEnable = attachment.BlendEnable;
    vkAttachment.srcColorBlendFactor = VKConverter::Convert(attachment.SrcColorBlendFactor);
    vkAttachment.dstColorBlendFactor = VKConverter::Convert(attachment.DstColorBlendFactor);
    vkAttachment.colorBlendOp = VKConverter::Convert(attachment.ColorBlendOp);
    vkAttachment.srcAlphaBlendFactor = VKConverter::Convert(attachment.SrcAlphaBlendFactor);
    vkAttachment.dstAlphaBlendFactor = VKConverter::Convert(attachment.DstAlphaBlendFactor);
    vkAttachment.alphaBlendOp = VKConverter::Convert(attachment.AlphaBlendOp);
    vkAttachment.colorWriteMask = VKConverter::Convert(attachment.ColorWriteMask);
    return vkAttachment;
}

static vk::StencilOpState ConvertStencilOpState(const StencilOpState& stencilOpState)
{
    vk::StencilOpState vkStencilOpState = {};
    vkStencilOpState.failOp = VKConverter::Convert(stencilOpState.FailOp);
    vkStencilOpState.passOp = VKConverter::Convert(stencilOpState.PassOp);
    vkStencilOpState.depthFailOp = VKConverter::Convert(stencilOpState.DepthFailOp);
    vkStencilOpState.compareOp = VKConverter::Convert(stencilOpState.CompareOp);
    vkStencilOpState.compareMask = stencilOpState.CompareMask;
    vkStencilOpState.writeMask = stencilOpState.WriteMask;
    vkStencilOpState.reference = stencilOpState.Reference;
    return vkStencilOpState;
}

static vk::PipelineDepthStencilStateCreateInfo ConvertDepthStencilState(const DepthStencilState& depthStencil)
{
    vk::PipelineDepthStencilStateCreateInfo depthStencilInfo = {};
    depthStencilInfo.depthTestEnable = depthStencil.DepthTestEnable;
    depthStencilInfo.depthWriteEnable = depthStencil.DepthWriteEnable;
    depthStencilInfo.depthCompareOp = VKConverter::Convert(depthStencil.DepthCompareOp);
    depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilInfo.stencilTestEnable = depthStencil.StencilTestEnable;
    depthStencilInfo.front = ConvertStencilOpState(depthStencil.Front);
    depthStencilInfo.back = ConvertStencilOpState(depthStencil.Back);
    depthStencilInfo.minDepthBounds = 0.0f;
    depthStencilInfo.maxDepthBounds = 1.0f;
    return depthStencilInfo;
}

static vk::PipelineMultisampleStateCreateInfo ConvertMultisampleState(const MultisampleState& multisample,
                                                                      std::vector<vk::SampleMask>& outSampleMask)
{
    vk::PipelineMultisampleStateCreateInfo multisampleInfo = {};
    multisampleInfo.rasterizationSamples = VKConverter::ConvertSampleCount(multisample.RasterizationSamples);
    multisampleInfo.sampleShadingEnable = multisample.SampleShadingEnable;
    multisampleInfo.minSampleShading = multisample.MinSampleShading;
    multisampleInfo.alphaToCoverageEnable = multisample.AlphaToCoverageEnable;
    multisampleInfo.alphaToOneEnable = multisample.AlphaToOneEnable;
    if (!multisample.SampleMask.empty()) {
        outSampleMask.resize(multisample.SampleMask.size());
        for (size_t i = 0; i < multisample.SampleMask.size(); i++) {
            outSampleMask[i] = multisample.SampleMask[i];
        }
        multisampleInfo.pSampleMask = outSampleMask.data();
    } else {
        multisampleInfo.pSampleMask = nullptr;
    }
    return multisampleInfo;
}

static vk::PipelineColorBlendStateCreateInfo
    ConvertColorBlendState(const ColorBlendState& colorBlend,
                           std::vector<vk::PipelineColorBlendAttachmentState>& outAttachments)
{
    outAttachments.resize(colorBlend.Attachments.size());
    for (size_t i = 0; i < colorBlend.Attachments.size(); i++) {
        outAttachments[i] = ConvertColorBlendAttachmentState(colorBlend.Attachments[i]);
    }
    vk::PipelineColorBlendStateCreateInfo colorBlendInfo = {};
    colorBlendInfo.logicOpEnable = colorBlend.LogicOpEnable;
    colorBlendInfo.logicOp = VKConverter::Convert(colorBlend.LogicOp);
    colorBlendInfo.attachmentCount = static_cast<uint32_t>(outAttachments.size());
    colorBlendInfo.pAttachments = outAttachments.data();
    colorBlendInfo.blendConstants[0] = colorBlend.BlendConstants[0];
    colorBlendInfo.blendConstants[1] = colorBlend.BlendConstants[1];
    colorBlendInfo.blendConstants[2] = colorBlend.BlendConstants[2];
    colorBlendInfo.blendConstants[3] = colorBlend.BlendConstants[3];
    return colorBlendInfo;
}

static std::vector<vk::Format> ConvertColorAttachmentFormats(std::span<const Format> formats)
{
    std::vector<vk::Format> vkFormats(formats.size());
    for (size_t i = 0; i < formats.size(); i++) {
        vkFormats[i] = VKConverter::Convert(formats[i]);
    }
    return vkFormats;
}

static vk::PipelineRenderingCreateInfo
    ConvertPipelineRenderingCreateInfo(std::span<const vk::Format> colorAttachmentFormats,
                                       vk::Format depthStencilFormat)
{
    vk::PipelineRenderingCreateInfo renderingInfo = {};
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentFormats.size());
    renderingInfo.pColorAttachmentFormats = colorAttachmentFormats.data();
    if (depthStencilFormat != vk::Format::eUndefined) {
        bool hasDepth =
            (depthStencilFormat == vk::Format::eD16Unorm || depthStencilFormat == vk::Format::eD32Sfloat ||
             depthStencilFormat == vk::Format::eD16UnormS8Uint || depthStencilFormat == vk::Format::eD24UnormS8Uint ||
             depthStencilFormat == vk::Format::eD32SfloatS8Uint);
        bool hasStencil =
            (depthStencilFormat == vk::Format::eS8Uint || depthStencilFormat == vk::Format::eD16UnormS8Uint ||
             depthStencilFormat == vk::Format::eD24UnormS8Uint || depthStencilFormat == vk::Format::eD32SfloatS8Uint);
        renderingInfo.depthAttachmentFormat = hasDepth ? depthStencilFormat : vk::Format::eUndefined;
        renderingInfo.stencilAttachmentFormat = hasStencil ? depthStencilFormat : vk::Format::eUndefined;
    } else {
        renderingInfo.depthAttachmentFormat = vk::Format::eUndefined;
        renderingInfo.stencilAttachmentFormat = vk::Format::eUndefined;
    }
    return renderingInfo;
}

VKGraphicsPipeline::VKGraphicsPipeline(const Ref<Device>& device, const GraphicsPipelineCreateInfo& createInfo)
{
    if (!device) {
        throw std::runtime_error("VKGraphicsPipeline created with null device");
    }
    m_device = std::dynamic_pointer_cast<VKDevice>(device);
    if (!m_device) {
        throw std::runtime_error("VKGraphicsPipeline requires a VKDevice");
    }
    m_pipelineInfo = createInfo;
    vk::GraphicsPipelineCreateInfo pipelineInfo = {};
    ConvertShaderStages(createInfo.Shaders, m_shaderStages);
    pipelineInfo.stageCount = static_cast<uint32_t>(m_shaderStages.size());
    pipelineInfo.pStages = m_shaderStages.data();
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo = ConvertVertexInputState(
        createInfo.VertexBindings, createInfo.VertexAttributes, m_vertexBindings, m_vertexAttributes);
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo = ConvertInputAssemblyState(createInfo.InputAssembly);
    pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
    vk::PipelineTessellationStateCreateInfo tessellationInfo{};
    if (createInfo.InputAssembly.Topology == PrimitiveTopology::PatchList) {
        tessellationInfo.patchControlPoints = createInfo.InputAssembly.PatchControlPoints;
        pipelineInfo.pTessellationState = &tessellationInfo;
    } else {
        pipelineInfo.pTessellationState = nullptr;
    }
    vk::PipelineViewportStateCreateInfo viewportState = {};
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;
    pipelineInfo.pViewportState = &viewportState;
    vk::PipelineRasterizationStateCreateInfo rasterizerInfo = ConvertRasterizationState(createInfo.Rasterizer);
    pipelineInfo.pRasterizationState = &rasterizerInfo;
    vk::PipelineMultisampleStateCreateInfo multisampleInfo =
        ConvertMultisampleState(createInfo.Multisample, m_sampleMask);
    pipelineInfo.pMultisampleState = &multisampleInfo;
    vk::PipelineDepthStencilStateCreateInfo depthStencilInfo = ConvertDepthStencilState(createInfo.DepthStencil);
    pipelineInfo.pDepthStencilState = &depthStencilInfo;
    vk::PipelineColorBlendStateCreateInfo colorBlendInfo =
        ConvertColorBlendState(createInfo.ColorBlend, m_colorBlendAttachments);
    pipelineInfo.pColorBlendState = &colorBlendInfo;
    m_dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicStateInfo = {};
    dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(m_dynamicStates.size());
    dynamicStateInfo.pDynamicStates = m_dynamicStates.data();
    pipelineInfo.pDynamicState = &dynamicStateInfo;
    if (!createInfo.Layout) {
        throw std::runtime_error("VKGraphicsPipeline requires a valid PipelineLayout");
    }
    Ref<VKPipelineLayout> vkLayout = std::dynamic_pointer_cast<VKPipelineLayout>(createInfo.Layout);
    if (!vkLayout) {
        throw std::runtime_error("VKGraphicsPipeline requires a VKPipelineLayout");
    }
    pipelineInfo.layout = vkLayout->GetHandle();
    m_colorAttachmentFormats = ConvertColorAttachmentFormats(createInfo.ColorAttachmentFormats);
    vk::Format depthStencilVkFormat = (createInfo.DepthStencilFormat != Format::UNDEFINED)
                                          ? VKConverter::Convert(createInfo.DepthStencilFormat)
                                          : vk::Format::eUndefined;
    vk::PipelineRenderingCreateInfo renderingInfo =
        ConvertPipelineRenderingCreateInfo(m_colorAttachmentFormats, depthStencilVkFormat);
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    vk::PipelineCache pipelineCache = VK_NULL_HANDLE;
    if (createInfo.Cache) {
        Ref<VKPipelineCache> vkCache = std::dynamic_pointer_cast<VKPipelineCache>(createInfo.Cache);
        if (vkCache) {
            pipelineCache = vkCache->GetHandle();
        }
    }
    auto result = m_device->GetHandle().createGraphicsPipeline(pipelineCache, pipelineInfo);
    if (result.result != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to create graphics pipeline: " + vk::to_string(result.result));
    }
    m_pipeline = result.value;
}

VKGraphicsPipeline::~VKGraphicsPipeline()
{
    if (m_pipeline && m_device) {
        m_device->GetHandle().destroyPipeline(m_pipeline);
        m_pipeline = VK_NULL_HANDLE;
    }
}

Ref<VKGraphicsPipeline> VKGraphicsPipeline::Create(const Ref<Device>& device,
                                                   const GraphicsPipelineCreateInfo& createInfo)
{
    return std::make_shared<VKGraphicsPipeline>(device, createInfo);
}

Ref<PipelineLayout> VKGraphicsPipeline::GetLayout() const
{
    return m_pipelineInfo.Layout;
}

VKComputePipeline::VKComputePipeline(const Ref<Device>& device, const ComputePipelineCreateInfo& createInfo)
{
    if (!device) {
        throw std::runtime_error("VKComputePipeline created with null device");
    }
    m_device = std::dynamic_pointer_cast<VKDevice>(device);
    if (!m_device) {
        throw std::runtime_error("VKComputePipeline requires a VKDevice");
    }
    m_pipelineInfo = createInfo;
    if (!createInfo.ComputeShader) {
        throw std::runtime_error("VKComputePipeline requires a valid compute shader");
    }
    Ref<VKShaderModule> vkShaderModule = std::dynamic_pointer_cast<VKShaderModule>(createInfo.ComputeShader);
    if (!vkShaderModule) {
        throw std::runtime_error("VKComputePipeline requires a VKShaderModule");
    }
    if (vkShaderModule->GetStage() != ShaderStage::Compute) {
        throw std::runtime_error("VKComputePipeline requires a compute shader, not " +
                                 std::to_string(static_cast<int>(vkShaderModule->GetStage())));
    }
    if (!createInfo.Layout) {
        throw std::runtime_error("VKComputePipeline requires a valid PipelineLayout");
    }
    Ref<VKPipelineLayout> vkLayout = std::dynamic_pointer_cast<VKPipelineLayout>(createInfo.Layout);
    if (!vkLayout) {
        throw std::runtime_error("VKComputePipeline requires a VKPipelineLayout");
    }
    vk::ComputePipelineCreateInfo pipelineInfo = {};
    vk::PipelineShaderStageCreateInfo shaderStageInfo = {};
    shaderStageInfo.stage = vk::ShaderStageFlagBits::eCompute;
    shaderStageInfo.module = vkShaderModule->GetHandle();
    shaderStageInfo.pName = "main";
    pipelineInfo.stage = shaderStageInfo;
    pipelineInfo.layout = vkLayout->GetHandle();
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;
    vk::PipelineCache pipelineCache = VK_NULL_HANDLE;
    if (createInfo.Cache) {
        Ref<VKPipelineCache> vkCache = std::dynamic_pointer_cast<VKPipelineCache>(createInfo.Cache);
        if (vkCache) {
            pipelineCache = vkCache->GetHandle();
        }
    }
    auto result = m_device->GetHandle().createComputePipeline(pipelineCache, pipelineInfo);
    if (result.result != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to create compute pipeline: " + vk::to_string(result.result));
    }
    m_pipeline = result.value;
}

VKComputePipeline::~VKComputePipeline()
{
    if (m_pipeline && m_device) {
        m_device->GetHandle().destroyPipeline(m_pipeline);
        m_pipeline = VK_NULL_HANDLE;
    }
}

Ref<VKComputePipeline> VKComputePipeline::Create(const Ref<Device>& device, const ComputePipelineCreateInfo& createInfo)
{
    return std::make_shared<VKComputePipeline>(device, createInfo);
}

Ref<PipelineLayout> VKComputePipeline::GetLayout() const
{
    return m_pipelineInfo.Layout;
}
} // namespace luna::RHI
