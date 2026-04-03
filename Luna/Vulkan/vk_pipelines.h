#pragma once
#include "vk_types.h"

namespace vkutil {
bool load_shader_module(const char* filePath, vk::Device device, vk::ShaderModule* outShaderModule);
inline bool load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outShaderModule)
{
    vk::ShaderModule shaderModule{};
    const bool loaded = load_shader_module(filePath, vk::Device(device), &shaderModule);
    if (loaded) {
        *outShaderModule = static_cast<VkShaderModule>(shaderModule);
    }
    return loaded;
}

} // namespace vkutil

class PipelineBuilder {
public:
    std::vector<vk::PipelineShaderStageCreateInfo> _shaderStages;

    vk::PipelineInputAssemblyStateCreateInfo _inputAssembly;
    vk::PipelineRasterizationStateCreateInfo _rasterizer;
    vk::PipelineColorBlendAttachmentState _colorBlendAttachment;
    vk::PipelineMultisampleStateCreateInfo _multisampling;
    vk::PipelineLayout _pipelineLayout;
    vk::PipelineDepthStencilStateCreateInfo _depthStencil;
    vk::PipelineRenderingCreateInfo _renderInfo;
    vk::Format _colorAttachmentformat;

    PipelineBuilder()
    {
        clear();
    }

    void clear();

    void set_shaders(vk::ShaderModule vertexShader, vk::ShaderModule fragmentShader);
    void set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
    {
        set_shaders(vk::ShaderModule(vertexShader), vk::ShaderModule(fragmentShader));
    }
    void set_input_topology(vk::PrimitiveTopology topology);
    void set_input_topology(VkPrimitiveTopology topology)
    {
        set_input_topology(static_cast<vk::PrimitiveTopology>(topology));
    }
    void set_polygon_mode(vk::PolygonMode mode);
    void set_polygon_mode(VkPolygonMode mode)
    {
        set_polygon_mode(static_cast<vk::PolygonMode>(mode));
    }
    void set_cull_mode(vk::CullModeFlags cullMode, vk::FrontFace frontFace);
    void set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace)
    {
        set_cull_mode(static_cast<vk::CullModeFlags>(cullMode), static_cast<vk::FrontFace>(frontFace));
    }
    void set_multisampling_none();
    void disable_blending();
    void set_color_attachment_format(vk::Format format);
    void set_color_attachment_format(VkFormat format)
    {
        set_color_attachment_format(static_cast<vk::Format>(format));
    }
    void set_depth_format(vk::Format format);
    void set_depth_format(VkFormat format)
    {
        set_depth_format(static_cast<vk::Format>(format));
    }
    void enable_depthtest(bool depthWriteEnable, vk::CompareOp compareOp);
    void enable_depthtest(bool depthWriteEnable, VkCompareOp compareOp)
    {
        enable_depthtest(depthWriteEnable, static_cast<vk::CompareOp>(compareOp));
    }
    void enable_blending_additive();
    void enable_blending_alphablend();
    void disable_depthtest();

    vk::Pipeline build_pipeline(vk::Device device);
};
