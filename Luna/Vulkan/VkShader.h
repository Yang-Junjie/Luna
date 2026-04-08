#pragma once
#include "Renderer/Shader.h"

#include <vulkan/vulkan.hpp>

namespace luna {
class VulkanShader : public Shader {
public:
    ~VulkanShader() override = default;
    VulkanShader(const std::vector<uint32_t>& spv_code, ShaderType type);

    ShaderType getType() const override;

private:
    ShaderType m_type;
    vk::ShaderModule m_shader_module{};
};
} // namespace luna

