#ifndef CACAO_D3D11SHADERMODULE_H
#define CACAO_D3D11SHADERMODULE_H
#include "D3D11Common.h"

#include <ShaderModule.h>

namespace Cacao {
class CACAO_API D3D11ShaderModule : public ShaderModule {
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

    std::span<const uint8_t> GetBytecode() const override
    {
        return {m_blob.Data.data(), m_blob.Data.size()};
    }

private:
    ShaderStage m_stage;
    std::string m_entryPoint;
    ShaderBlob m_blob;
};
} // namespace Cacao
#endif
