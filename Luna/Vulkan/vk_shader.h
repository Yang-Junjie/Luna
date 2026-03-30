#pragma once
#include "RHI/shader.h"

#include <vulkan/vulkan.h>

namespace luna {
class VulkanShader : public Shader {
public:
    ~VulkanShader() override = default;
    VulkanShader(const std::vector<uint32_t>& spvCode, ShaderType type);

    ShaderType getType() const override;

private:
    ShaderType m_type;
    VkShaderModule m_shaderModule = VK_NULL_HANDLE;
};
} // namespace luna
