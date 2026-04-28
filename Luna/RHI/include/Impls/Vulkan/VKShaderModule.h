#ifndef LUNA_RHI_VKSHADERMODULE_H
#define LUNA_RHI_VKSHADERMODULE_H
#include "ShaderModule.h"

#include <vulkan/vulkan.hpp>

namespace luna::RHI {
class Device;
class VKDevice;

class LUNA_RHI_API VKShaderModule : public ShaderModule {
    Ref<VKDevice> m_device;
    vk::ShaderModule m_module;
    ShaderBlob m_shaderBlob;
    ShaderCreateInfo m_createInfo;

public:
    VKShaderModule(const Ref<Device>& device, const ShaderCreateInfo& info, const ShaderBlob& blob);
    static Ref<VKShaderModule> Create(const Ref<Device>& device, const ShaderCreateInfo& info, const ShaderBlob& blob);
    const std::string& GetEntryPoint() const override;
    ShaderStage GetStage() const override;
    const ShaderBlob& GetBlob() const override;
    const ShaderReflectionData& GetReflection() const override;

    vk::ShaderModule& GetHandle()
    {
        return m_module;
    }

    ~VKShaderModule() override;
};
} // namespace luna::RHI
#endif
