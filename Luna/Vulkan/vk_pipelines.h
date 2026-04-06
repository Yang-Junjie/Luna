#pragma once
#include "RHI/Descriptors.h"
#include "vk_types.h"

namespace vkutil {
bool load_shader_module(const char* filePath, vk::Device device, vk::ShaderModule* outShaderModule);
bool create_shader_module(std::span<const uint32_t> code, vk::Device device, vk::ShaderModule* outShaderModule);
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
    std::vector<vk::VertexInputBindingDescription> _vertexInputBindings;
    std::vector<vk::VertexInputAttributeDescription> _vertexInputAttributes;
    std::vector<vk::PipelineColorBlendAttachmentState> _colorBlendAttachments;
    std::vector<vk::Format> _colorAttachmentFormats;

    vk::PipelineInputAssemblyStateCreateInfo _inputAssembly;
    vk::PipelineRasterizationStateCreateInfo _rasterizer;
    vk::PipelineMultisampleStateCreateInfo _multisampling;
    vk::PipelineLayout _pipelineLayout;
    vk::PipelineDepthStencilStateCreateInfo _depthStencil;
    vk::PipelineRenderingCreateInfo _renderInfo;

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
    void set_vertex_input(std::span<const vk::VertexInputBindingDescription> bindings,
                          std::span<const vk::VertexInputAttributeDescription> attributes);
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
    void disable_blending(uint32_t attachmentCount);
    void set_color_attachment_format(vk::Format format);
    void set_color_attachment_formats(std::span<const vk::Format> formats);
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

vk::Format to_vulkan_format(luna::PixelFormat format);
vk::PrimitiveTopology to_vulkan_topology(luna::PrimitiveTopology topology);
vk::PolygonMode to_vulkan_polygon_mode(luna::PolygonMode mode);
vk::CullModeFlags to_vulkan_cull_mode(luna::CullMode mode);
vk::FrontFace to_vulkan_front_face(luna::FrontFace frontFace);
vk::CompareOp to_vulkan_compare_op(luna::CompareOp compareOp);
vk::Format to_vulkan_vertex_format(luna::VertexFormat format);

vk::Pipeline build_graphics_pipeline(vk::Device device,
                                     const luna::GraphicsPipelineDesc& desc,
                                     vk::PipelineLayout layout);
vk::Pipeline build_graphics_pipeline(vk::Device device,
                                     const luna::GraphicsPipelineDesc& desc,
                                     vk::PipelineLayout layout,
                                     vk::ShaderModule vertexShader,
                                     vk::ShaderModule fragmentShader);
