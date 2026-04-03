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

vk::Format to_vulkan_format(luna::PixelFormat format)
{
    switch (format) {
        case luna::PixelFormat::BGRA8Unorm:
            return vk::Format::eB8G8R8A8Unorm;
        case luna::PixelFormat::RGBA8Unorm:
            return vk::Format::eR8G8B8A8Unorm;
        case luna::PixelFormat::RGBA8Srgb:
            return vk::Format::eR8G8B8A8Srgb;
        case luna::PixelFormat::RGBA16Float:
            return vk::Format::eR16G16B16A16Sfloat;
        case luna::PixelFormat::D32Float:
            return vk::Format::eD32Sfloat;
        default:
            return vk::Format::eUndefined;
    }
}

vk::PrimitiveTopology to_vulkan_topology(luna::PrimitiveTopology topology)
{
    switch (topology) {
        case luna::PrimitiveTopology::TriangleList:
            return vk::PrimitiveTopology::eTriangleList;
        case luna::PrimitiveTopology::TriangleStrip:
            return vk::PrimitiveTopology::eTriangleStrip;
        case luna::PrimitiveTopology::LineList:
            return vk::PrimitiveTopology::eLineList;
        default:
            return vk::PrimitiveTopology::eTriangleList;
    }
}

vk::PolygonMode to_vulkan_polygon_mode(luna::PolygonMode mode)
{
    switch (mode) {
        case luna::PolygonMode::Fill:
            return vk::PolygonMode::eFill;
        case luna::PolygonMode::Line:
            return vk::PolygonMode::eLine;
        default:
            return vk::PolygonMode::eFill;
    }
}

vk::CullModeFlags to_vulkan_cull_mode(luna::CullMode mode)
{
    switch (mode) {
        case luna::CullMode::None:
            return vk::CullModeFlagBits::eNone;
        case luna::CullMode::Front:
            return vk::CullModeFlagBits::eFront;
        case luna::CullMode::Back:
            return vk::CullModeFlagBits::eBack;
        default:
            return vk::CullModeFlagBits::eBack;
    }
}

vk::FrontFace to_vulkan_front_face(luna::FrontFace frontFace)
{
    switch (frontFace) {
        case luna::FrontFace::CounterClockwise:
            return vk::FrontFace::eCounterClockwise;
        case luna::FrontFace::Clockwise:
            return vk::FrontFace::eClockwise;
        default:
            return vk::FrontFace::eCounterClockwise;
    }
}

vk::CompareOp to_vulkan_compare_op(luna::CompareOp compareOp)
{
    switch (compareOp) {
        case luna::CompareOp::Never:
            return vk::CompareOp::eNever;
        case luna::CompareOp::Less:
            return vk::CompareOp::eLess;
        case luna::CompareOp::Equal:
            return vk::CompareOp::eEqual;
        case luna::CompareOp::LessOrEqual:
            return vk::CompareOp::eLessOrEqual;
        case luna::CompareOp::Greater:
            return vk::CompareOp::eGreater;
        case luna::CompareOp::Always:
            return vk::CompareOp::eAlways;
        default:
            return vk::CompareOp::eLessOrEqual;
    }
}

vk::Format to_vulkan_vertex_format(luna::VertexFormat format)
{
    switch (format) {
        case luna::VertexFormat::Float2:
            return vk::Format::eR32G32Sfloat;
        case luna::VertexFormat::Float3:
            return vk::Format::eR32G32B32Sfloat;
        case luna::VertexFormat::Float4:
            return vk::Format::eR32G32B32A32Sfloat;
        case luna::VertexFormat::UByte4Norm:
            return vk::Format::eR8G8B8A8Unorm;
        default:
            return vk::Format::eUndefined;
    }
}

vk::Pipeline build_graphics_pipeline(vk::Device device, const luna::GraphicsPipelineDesc& desc, vk::PipelineLayout layout)
{
    if (desc.vertexShader.filePath.empty() || desc.fragmentShader.filePath.empty()) {
        LUNA_CORE_ERROR("GraphicsPipelineDesc requires both vertex and fragment shaders");
        return {};
    }

    if (desc.colorAttachments.empty()) {
        LUNA_CORE_ERROR("GraphicsPipelineDesc requires at least one color attachment");
        return {};
    }

    const std::string vertexShaderPath{desc.vertexShader.filePath};
    const std::string fragmentShaderPath{desc.fragmentShader.filePath};

    vk::ShaderModule vertexShader{};
    if (!vkutil::load_shader_module(vertexShaderPath.c_str(), device, &vertexShader)) {
        LUNA_CORE_ERROR("Failed to load pipeline vertex shader '{}'", vertexShaderPath);
        return {};
    }

    vk::ShaderModule fragmentShader{};
    if (!vkutil::load_shader_module(fragmentShaderPath.c_str(), device, &fragmentShader)) {
        device.destroyShaderModule(vertexShader, nullptr);
        LUNA_CORE_ERROR("Failed to load pipeline fragment shader '{}'", fragmentShaderPath);
        return {};
    }

    PipelineBuilder pipelineBuilder;
    pipelineBuilder._pipelineLayout = layout;
    pipelineBuilder.set_shaders(vertexShader, fragmentShader);
    pipelineBuilder.set_input_topology(to_vulkan_topology(desc.topology));
    pipelineBuilder.set_polygon_mode(to_vulkan_polygon_mode(desc.polygonMode));
    pipelineBuilder.set_cull_mode(to_vulkan_cull_mode(desc.cullMode), to_vulkan_front_face(desc.frontFace));
    pipelineBuilder.set_multisampling_none();

    if (!desc.vertexLayout.attributes.empty() && desc.vertexLayout.stride > 0) {
        std::vector<vk::VertexInputAttributeDescription> vertexAttributes;
        vertexAttributes.reserve(desc.vertexLayout.attributes.size());
        for (const luna::VertexAttributeDesc& attribute : desc.vertexLayout.attributes) {
            vertexAttributes.push_back(
                vk::VertexInputAttributeDescription{attribute.location,
                                                    0,
                                                    to_vulkan_vertex_format(attribute.format),
                                                    attribute.offset});
        }

        const vk::VertexInputBindingDescription vertexBinding{
            0, desc.vertexLayout.stride, vk::VertexInputRate::eVertex};
        pipelineBuilder.set_vertex_input(std::span<const vk::VertexInputBindingDescription>(&vertexBinding, 1),
                                         std::span<const vk::VertexInputAttributeDescription>(vertexAttributes));
    }

    if (desc.colorAttachments.front().blendEnabled) {
        pipelineBuilder.enable_blending_alphablend();
    } else {
        pipelineBuilder.disable_blending();
    }

    if (desc.depthStencil.depthTestEnabled || desc.depthStencil.depthWriteEnabled) {
        pipelineBuilder.enable_depthtest(desc.depthStencil.depthWriteEnabled,
                                         to_vulkan_compare_op(desc.depthStencil.depthCompareOp));
    } else {
        pipelineBuilder.disable_depthtest();
    }

    pipelineBuilder.set_color_attachment_format(to_vulkan_format(desc.colorAttachments.front().format));
    pipelineBuilder.set_depth_format(to_vulkan_format(desc.depthStencil.format));

    vk::Pipeline pipeline = pipelineBuilder.build_pipeline(device);

    device.destroyShaderModule(vertexShader, nullptr);
    device.destroyShaderModule(fragmentShader, nullptr);

    if (pipeline && !desc.debugName.empty()) {
        LUNA_CORE_INFO("{} created via RHI", desc.debugName);
    }

    return pipeline;
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
    _vertexInputBindings.clear();
    _vertexInputAttributes.clear();
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
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(_vertexInputBindings.size());
    vertexInputInfo.pVertexBindingDescriptions = _vertexInputBindings.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(_vertexInputAttributes.size());
    vertexInputInfo.pVertexAttributeDescriptions = _vertexInputAttributes.data();

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

void PipelineBuilder::set_vertex_input(std::span<const vk::VertexInputBindingDescription> bindings,
                                       std::span<const vk::VertexInputAttributeDescription> attributes)
{
    _vertexInputBindings.assign(bindings.begin(), bindings.end());
    _vertexInputAttributes.assign(attributes.begin(), attributes.end());
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
