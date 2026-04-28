#ifndef LUNA_RHI_D3D11SHADERMODULE_H
#define LUNA_RHI_D3D11SHADERMODULE_H
#include "D3D11Common.h"

#include <ShaderModule.h>

namespace luna::RHI {
class LUNA_RHI_API D3D11ShaderModule : public ShaderModule {
public:
    D3D11ShaderModule(const ShaderBlob& blob, const ShaderCreateInfo& info);

    ShaderStage GetStage() const override
    {
        return m_stage;
    }

    const std::string& GetEntryPoint() const override
    {
        return m_entryPoint;
    }

    const ShaderBlob& GetBlob() const override
    {
        return m_blob;
    }

    const ShaderReflectionData& GetReflection() const override
    {
        return m_reflection;
    }

    std::span<const uint8_t> GetBytecode() const override
    {
        return {m_blob.Data.data(), m_blob.Data.size()};
    }

private:
    ShaderStage m_stage;
    std::string m_entryPoint;
    ShaderBlob m_blob;
    ShaderReflectionData m_reflection;
};
} // namespace luna::RHI
#endif
