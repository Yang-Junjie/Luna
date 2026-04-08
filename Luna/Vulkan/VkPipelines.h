#pragma once
#include "VkTypes.h"

namespace vkutil {
bool loadShaderModule(const char* file_path, vk::Device device, vk::ShaderModule* out_shader_module);
inline bool loadShaderModule(const char* file_path, VkDevice device, VkShaderModule* out_shader_module)
{
    vk::ShaderModule shader_module{};
    const bool loaded = loadShaderModule(file_path, vk::Device(device), &shader_module);
    if (loaded) {
        *out_shader_module = static_cast<VkShaderModule>(shader_module);
    }
    return loaded;
}

} // namespace vkutil

class PipelineBuilder {
public:
    std::vector<vk::PipelineShaderStageCreateInfo> m_shader_stages;

    vk::PipelineInputAssemblyStateCreateInfo m_input_assembly;
    vk::PipelineRasterizationStateCreateInfo m_rasterizer;
    vk::PipelineColorBlendAttachmentState m_color_blend_attachment;
    vk::PipelineMultisampleStateCreateInfo m_multisampling;
    vk::PipelineLayout m_pipeline_layout;
    vk::PipelineDepthStencilStateCreateInfo m_depth_stencil;
    vk::PipelineRenderingCreateInfo m_render_info;
    vk::Format m_color_attachmentformat;

    PipelineBuilder()
    {
        clear();
    }

    void clear();

    void setShaders(vk::ShaderModule vertex_shader, vk::ShaderModule fragment_shader);
    void setShaders(VkShaderModule vertex_shader, VkShaderModule fragment_shader)
    {
        setShaders(vk::ShaderModule(vertex_shader), vk::ShaderModule(fragment_shader));
    }
    void setInputTopology(vk::PrimitiveTopology topology);
    void setInputTopology(VkPrimitiveTopology topology)
    {
        setInputTopology(static_cast<vk::PrimitiveTopology>(topology));
    }
    void setPolygonMode(vk::PolygonMode mode);
    void setPolygonMode(VkPolygonMode mode)
    {
        setPolygonMode(static_cast<vk::PolygonMode>(mode));
    }
    void setCullMode(vk::CullModeFlags cull_mode, vk::FrontFace front_face);
    void setCullMode(VkCullModeFlags cull_mode, VkFrontFace front_face)
    {
        setCullMode(static_cast<vk::CullModeFlags>(cull_mode), static_cast<vk::FrontFace>(front_face));
    }
    void setMultisamplingNone();
    void disableBlending();
    void setColorAttachmentFormat(vk::Format format);
    void setColorAttachmentFormat(VkFormat format)
    {
        setColorAttachmentFormat(static_cast<vk::Format>(format));
    }
    void setDepthFormat(vk::Format format);
    void setDepthFormat(VkFormat format)
    {
        setDepthFormat(static_cast<vk::Format>(format));
    }
    void enableDepthtest(bool depth_write_enable, vk::CompareOp compare_op);
    void enableDepthtest(bool depth_write_enable, VkCompareOp compare_op)
    {
        enableDepthtest(depth_write_enable, static_cast<vk::CompareOp>(compare_op));
    }
    void enableBlendingAdditive();
    void enableBlendingAlphablend();
    void disableDepthtest();

    vk::Pipeline buildPipeline(vk::Device device);
};

