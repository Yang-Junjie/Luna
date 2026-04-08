#include "VkInitializers.h"
#include "VkPipelines.h"

#include <fstream>

bool vkutil::loadShaderModule(const char* file_path, vk::Device device, vk::ShaderModule* out_shader_module)
{
    std::ifstream file(file_path, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        return false;
    }

    const size_t file_size = static_cast<size_t>(file.tellg());
    std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));

    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(file_size));
    file.close();

    vk::ShaderModuleCreateInfo create_info{};
    create_info.pCode = buffer.data();
    create_info.codeSize = buffer.size() * sizeof(uint32_t);

    vk::ShaderModule shader_module{};
    if (device.createShaderModule(&create_info, nullptr, &shader_module) != vk::Result::eSuccess) {
        return false;
    }

    *out_shader_module = shader_module;
    return true;
}

void PipelineBuilder::clear()
{
    m_input_assembly = {};
    m_rasterizer = {};
    m_color_blend_attachment = {};
    m_multisampling = {};
    m_pipeline_layout = nullptr;
    m_depth_stencil = {};
    m_render_info = {};
    m_color_attachmentformat = {};
    m_shader_stages.clear();
}

vk::Pipeline PipelineBuilder::buildPipeline(vk::Device device)
{
    vk::PipelineViewportStateCreateInfo viewport_state{};
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    vk::PipelineColorBlendStateCreateInfo color_blending{};
    color_blending.logicOpEnable = false;
    color_blending.logicOp = vk::LogicOp::eCopy;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &m_color_blend_attachment;

    vk::PipelineVertexInputStateCreateInfo vertex_input_info{};

    vk::GraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.pNext = &m_render_info;
    pipeline_info.stageCount = static_cast<uint32_t>(m_shader_stages.size());
    pipeline_info.pStages = m_shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &m_input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &m_rasterizer;
    pipeline_info.pMultisampleState = &m_multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDepthStencilState = &m_depth_stencil;
    pipeline_info.layout = m_pipeline_layout;

    std::array<vk::DynamicState, 2> states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamic_info{};
    dynamic_info.pDynamicStates = states.data();
    dynamic_info.dynamicStateCount = static_cast<uint32_t>(states.size());
    pipeline_info.pDynamicState = &dynamic_info;

    vk::Pipeline new_pipeline{};
    if (device.createGraphicsPipelines({}, 1, &pipeline_info, nullptr, &new_pipeline) != vk::Result::eSuccess) {
        fmt::println("failed to create pipeline");
        return {};
    }

    return new_pipeline;
}

void PipelineBuilder::setShaders(vk::ShaderModule vertex_shader, vk::ShaderModule fragment_shader)
{
    m_shader_stages.clear();

    m_shader_stages.push_back(vkinit::pipelineShaderStageCreateInfo(vk::ShaderStageFlagBits::eVertex, vertex_shader));
    m_shader_stages.push_back(
        vkinit::pipelineShaderStageCreateInfo(vk::ShaderStageFlagBits::eFragment, fragment_shader));
}

void PipelineBuilder::setInputTopology(vk::PrimitiveTopology topology)
{
    m_input_assembly.topology = topology;
    m_input_assembly.primitiveRestartEnable = false;
}

void PipelineBuilder::setPolygonMode(vk::PolygonMode mode)
{
    m_rasterizer.polygonMode = mode;
    m_rasterizer.lineWidth = 1.0f;
}

void PipelineBuilder::setCullMode(vk::CullModeFlags cull_mode, vk::FrontFace front_face)
{
    m_rasterizer.cullMode = cull_mode;
    m_rasterizer.frontFace = front_face;
}

void PipelineBuilder::setMultisamplingNone()
{
    m_multisampling.sampleShadingEnable = false;
    m_multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    m_multisampling.minSampleShading = 1.0f;
    m_multisampling.pSampleMask = nullptr;
    m_multisampling.alphaToCoverageEnable = false;
    m_multisampling.alphaToOneEnable = false;
}

void PipelineBuilder::disableBlending()
{
    m_color_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                           vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    m_color_blend_attachment.blendEnable = false;
}

void PipelineBuilder::setColorAttachmentFormat(vk::Format format)
{
    m_color_attachmentformat = format;
    m_render_info.colorAttachmentCount = 1;
    m_render_info.pColorAttachmentFormats = &m_color_attachmentformat;
}

void PipelineBuilder::setDepthFormat(vk::Format format)
{
    m_render_info.depthAttachmentFormat = format;
}

void PipelineBuilder::enableDepthtest(bool depth_write_enable, vk::CompareOp compare_op)
{
    m_depth_stencil.depthTestEnable = true;
    m_depth_stencil.depthWriteEnable = depth_write_enable;
    m_depth_stencil.depthCompareOp = compare_op;
    m_depth_stencil.depthBoundsTestEnable = false;
    m_depth_stencil.stencilTestEnable = false;
    m_depth_stencil.front = {};
    m_depth_stencil.back = {};
    m_depth_stencil.minDepthBounds = 0.0f;
    m_depth_stencil.maxDepthBounds = 1.0f;
}

void PipelineBuilder::enableBlendingAdditive()
{
    m_color_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                           vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    m_color_blend_attachment.blendEnable = true;
    m_color_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    m_color_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eOne;
    m_color_blend_attachment.colorBlendOp = vk::BlendOp::eAdd;
    m_color_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    m_color_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    m_color_blend_attachment.alphaBlendOp = vk::BlendOp::eAdd;
}

void PipelineBuilder::enableBlendingAlphablend()
{
    m_color_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                           vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    m_color_blend_attachment.blendEnable = true;
    m_color_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    m_color_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
    m_color_blend_attachment.colorBlendOp = vk::BlendOp::eAdd;
    m_color_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    m_color_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    m_color_blend_attachment.alphaBlendOp = vk::BlendOp::eAdd;
}

void PipelineBuilder::disableDepthtest()
{
    m_depth_stencil.depthTestEnable = false;
    m_depth_stencil.depthWriteEnable = false;
    m_depth_stencil.depthCompareOp = vk::CompareOp::eNever;
    m_depth_stencil.depthBoundsTestEnable = false;
    m_depth_stencil.stencilTestEnable = false;
    m_depth_stencil.front = {};
    m_depth_stencil.back = {};
    m_depth_stencil.minDepthBounds = 0.0f;
    m_depth_stencil.maxDepthBounds = 1.0f;
}

