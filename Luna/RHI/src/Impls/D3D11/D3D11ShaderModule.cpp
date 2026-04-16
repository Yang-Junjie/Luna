#include "Impls/D3D11/D3D11ShaderModule.h"

namespace Cacao {
D3D11ShaderModule::D3D11ShaderModule(const ShaderBlob& blob, const ShaderCreateInfo& info)
    : m_stage(info.Stage),
      m_entryPoint(info.EntryPoint),
      m_blob(blob)
{}
} // namespace Cacao
