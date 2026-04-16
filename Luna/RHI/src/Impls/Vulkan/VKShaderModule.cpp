#include "Impls/Vulkan/VKShaderModule.h"
#include "Impls/Vulkan/VKDevice.h"
Cacao::VKShaderModule::VKShaderModule(const Ref<Device>& device, const ShaderCreateInfo& info,
                                      const ShaderBlob& blob) : m_shaderBlob(blob), m_createInfo(info)
{
    if (!device)
    {
        throw std::runtime_error("VKShaderModule created with null device");
    }
    m_device = std::dynamic_pointer_cast<VKDevice>(device);
    if (m_shaderBlob.Data.empty())
        throw std::runtime_error("ShaderBlob is empty!");
    if (m_shaderBlob.Data.size() % 4 != 0)
        throw std::runtime_error("SPIR-V blob size must be a multiple of 4!");
    vk::ShaderModuleCreateInfo createInfo = {};
    createInfo.codeSize = m_shaderBlob.Data.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(m_shaderBlob.Data.data());
    m_module = m_device->GetHandle().createShaderModule(createInfo);
    if (!m_module)
    {
        throw std::runtime_error("Failed to create Vulkan Shader Module");
    }
}
Cacao::Ref<Cacao::VKShaderModule> Cacao::VKShaderModule::Create(const Ref<Device>& device,
                                                                const ShaderCreateInfo& info, const ShaderBlob& blob)
{
    return CreateRef<VKShaderModule>(device, info, blob);
}
const std::string& Cacao::VKShaderModule::GetEntryPoint() const
{
    return m_createInfo.EntryPoint;
}
Cacao::ShaderStage Cacao::VKShaderModule::GetStage() const
{
    return m_createInfo.Stage;
}
const Cacao::ShaderBlob& Cacao::VKShaderModule::GetBlob() const
{
    return m_shaderBlob;
}
Cacao::VKShaderModule::~VKShaderModule()
{
    if (m_module && m_device)
    {
        m_device->GetHandle().destroyShaderModule(m_module);
        m_module = VK_NULL_HANDLE;
    }
}
