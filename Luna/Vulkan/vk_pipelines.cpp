#include "vk_initializers.h"
#include "vk_pipelines.h"

#include <fstream>

bool vkutil::load_shader_module(const char* filePath, vk::Device device, vk::ShaderModule* outShaderModule)
{
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        return false;
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(fileSize));
    file.close();

    vk::ShaderModuleCreateInfo createInfo{};
    createInfo.pCode = buffer.data();
    createInfo.codeSize = buffer.size() * sizeof(uint32_t);

    vk::ShaderModule shaderModule{};
    if (device.createShaderModule(&createInfo, nullptr, &shaderModule) != vk::Result::eSuccess) {
        return false;
    }

    *outShaderModule = shaderModule;
    return true;
}

void PipelineBuilder::clear()
{
    _inputAssembly = {};
    _rasterizer = {};
    _colorBlendAttachment = {};
    _multisampling = {};
    _pipelineLayout = nullptr;
    _depthStencil = {};
    _renderInfo = {};
    _colorAttachmentformat = {};
    _shaderStages.clear();
}

vk::Pipeline PipelineBuilder::build_pipeline(vk::Device device)
{
    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.logicOpEnable = false;
    colorBlending.logicOp = vk::LogicOp::eCopy;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &_colorBlendAttachment;

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.pNext = &_renderInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(_shaderStages.size());
    pipelineInfo.pStages = _shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &_inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &_rasterizer;
    pipelineInfo.pMultisampleState = &_multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &_depthStencil;
    pipelineInfo.layout = _pipelineLayout;

    std::array<vk::DynamicState, 2> states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicInfo{};
    dynamicInfo.pDynamicStates = states.data();
    dynamicInfo.dynamicStateCount = static_cast<uint32_t>(states.size());
    pipelineInfo.pDynamicState = &dynamicInfo;

    vk::Pipeline newPipeline{};
    if (device.createGraphicsPipelines({}, 1, &pipelineInfo, nullptr, &newPipeline) != vk::Result::eSuccess) {
        fmt::println("failed to create pipeline");
        return {};
    }

    return newPipeline;
}

void PipelineBuilder::set_shaders(vk::ShaderModule vertexShader, vk::ShaderModule fragmentShader)
{
    _shaderStages.clear();

    _shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eVertex, vertexShader));
    _shaderStages.push_back(
        vkinit::pipeline_shader_stage_create_info(vk::ShaderStageFlagBits::eFragment, fragmentShader));
}

void PipelineBuilder::set_input_topology(vk::PrimitiveTopology topology)
{
    _inputAssembly.topology = topology;
    _inputAssembly.primitiveRestartEnable = false;
}

void PipelineBuilder::set_polygon_mode(vk::PolygonMode mode)
{
    _rasterizer.polygonMode = mode;
    _rasterizer.lineWidth = 1.0f;
}

void PipelineBuilder::set_cull_mode(vk::CullModeFlags cullMode, vk::FrontFace frontFace)
{
    _rasterizer.cullMode = cullMode;
    _rasterizer.frontFace = frontFace;
}

void PipelineBuilder::set_multisampling_none()
{
    _multisampling.sampleShadingEnable = false;
    _multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    _multisampling.minSampleShading = 1.0f;
    _multisampling.pSampleMask = nullptr;
    _multisampling.alphaToCoverageEnable = false;
    _multisampling.alphaToOneEnable = false;
}

void PipelineBuilder::disable_blending()
{
    _colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                           vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    _colorBlendAttachment.blendEnable = false;
}

void PipelineBuilder::set_color_attachment_format(vk::Format format)
{
    _colorAttachmentformat = format;
    _renderInfo.colorAttachmentCount = 1;
    _renderInfo.pColorAttachmentFormats = &_colorAttachmentformat;
}

void PipelineBuilder::set_depth_format(vk::Format format)
{
    _renderInfo.depthAttachmentFormat = format;
}

void PipelineBuilder::enable_depthtest(bool depthWriteEnable, vk::CompareOp compareOp)
{
    _depthStencil.depthTestEnable = true;
    _depthStencil.depthWriteEnable = depthWriteEnable;
    _depthStencil.depthCompareOp = compareOp;
    _depthStencil.depthBoundsTestEnable = false;
    _depthStencil.stencilTestEnable = false;
    _depthStencil.front = {};
    _depthStencil.back = {};
    _depthStencil.minDepthBounds = 0.0f;
    _depthStencil.maxDepthBounds = 1.0f;
}

void PipelineBuilder::enable_blending_additive()
{
    _colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                           vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    _colorBlendAttachment.blendEnable = true;
    _colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    _colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOne;
    _colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
    _colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    _colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    _colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;
}

void PipelineBuilder::enable_blending_alphablend()
{
    _colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                           vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    _colorBlendAttachment.blendEnable = true;
    _colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    _colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    _colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
    _colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    _colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    _colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;
}

void PipelineBuilder::disable_depthtest()
{
    _depthStencil.depthTestEnable = false;
    _depthStencil.depthWriteEnable = false;
    _depthStencil.depthCompareOp = vk::CompareOp::eNever;
    _depthStencil.depthBoundsTestEnable = false;
    _depthStencil.stencilTestEnable = false;
    _depthStencil.front = {};
    _depthStencil.back = {};
    _depthStencil.minDepthBounds = 0.0f;
    _depthStencil.maxDepthBounds = 1.0f;
}
