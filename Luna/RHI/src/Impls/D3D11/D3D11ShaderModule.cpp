#include "Impls/D3D11/D3D11ShaderModule.h"

namespace luna::RHI {
D3D11ShaderModule::D3D11ShaderModule(const ShaderBlob& blob, const ShaderCreateInfo& info)
    : m_stage(info.Stage),
      m_entryPoint(info.EntryPoint),
      m_blob(blob)
{}
} // namespace luna::RHI
