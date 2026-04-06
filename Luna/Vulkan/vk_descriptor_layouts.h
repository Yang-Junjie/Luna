#pragma once

#include "RHI/ResourceLayout.h"
#include "RHI/Shader.h"
#include "vk_types.h"

#include <cstdint>
#include <vector>

struct DescriptorLayoutBuilder {

    std::vector<vk::DescriptorSetLayoutBinding> bindings;

    void add_binding(uint32_t binding, vk::DescriptorType type);

    void add_binding(uint32_t binding, VkDescriptorType type)
    {
        add_binding(binding, static_cast<vk::DescriptorType>(type));
    }

    void add_binding_from_reflection(const luna::ShaderReflectionData& data, vk::ShaderStageFlags shaderStages);

    void add_binding_from_reflection(const luna::ShaderReflectionData& data, VkShaderStageFlags shaderStages)
    {
        add_binding_from_reflection(data, static_cast<vk::ShaderStageFlags>(shaderStages));
    }

    void add_bindings_from_reflection(const luna::Shader::ReflectionMap& reflectionMap,
                                      uint32_t setIndex,
                                      vk::ShaderStageFlags shaderStages);

    void add_bindings_from_reflection(const luna::Shader::ReflectionMap& reflectionMap,
                                      uint32_t setIndex,
                                      VkShaderStageFlags shaderStages)
    {
        add_bindings_from_reflection(reflectionMap, setIndex, static_cast<vk::ShaderStageFlags>(shaderStages));
    }

    void clear();
    vk::DescriptorSetLayout build(vk::Device device,
                                  vk::ShaderStageFlags shaderStages = {},
                                  const void* pNext = nullptr,
                                  vk::DescriptorSetLayoutCreateFlags flags = {});

    vk::DescriptorSetLayout build(VkDevice device,
                                  VkShaderStageFlags shaderStages = 0,
                                  const void* pNext = nullptr,
                                  VkDescriptorSetLayoutCreateFlags flags = 0)
    {
        return build(vk::Device(device),
                     static_cast<vk::ShaderStageFlags>(shaderStages),
                     pNext,
                     static_cast<vk::DescriptorSetLayoutCreateFlags>(flags));
    }
};

vk::DescriptorType to_vulkan_descriptor_type(luna::ResourceType type);
vk::ShaderStageFlags to_vulkan_shader_stages(luna::ShaderType visibility);
vk::ImageLayout to_vulkan_image_layout(luna::ResourceType type);

vk::DescriptorSetLayout build_resource_layout(vk::Device device,
                                              const luna::ResourceLayoutDesc& desc,
                                              const void* pNext = nullptr,
                                              vk::DescriptorSetLayoutCreateFlags flags = {});
