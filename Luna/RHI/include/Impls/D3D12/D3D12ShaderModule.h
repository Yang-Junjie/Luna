#ifndef CACAO_D3D12SHADERMODULE_H
#define CACAO_D3D12SHADERMODULE_H
#include "D3D12Common.h"
#include "ShaderModule.h"

namespace Cacao
{
    class CACAO_API D3D12ShaderModule final : public ShaderModule
    {
    private:
        ShaderBlob m_blob;
        ShaderCreateInfo m_info;

    public:
        D3D12ShaderModule(const ShaderBlob& blob, const ShaderCreateInfo& info)
            : m_blob(blob), m_info(info) {}

        const std::string& GetEntryPoint() const override { return m_info.EntryPoint; }
        ShaderStage GetStage() const override { return m_info.Stage; }
        const ShaderBlob& GetBlob() const override { return m_blob; }
    };
}

#endif
