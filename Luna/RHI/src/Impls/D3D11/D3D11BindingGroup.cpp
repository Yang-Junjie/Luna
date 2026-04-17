#include "Impls/D3D11/D3D11BindingGroup.h"
#include "Impls/D3D11/D3D11Buffer.h"
#include "Impls/D3D11/D3D11Device.h"
#include "Impls/D3D11/D3D11Sampler.h"
#include "Impls/D3D11/D3D11Texture.h"

namespace luna::RHI {
D3D11DescriptorSet::D3D11DescriptorSet(Ref<D3D11Device> device, Ref<D3D11DescriptorSetLayout> layout)
    : m_device(std::move(device)),
      m_layout(std::move(layout))
{
    auto& bindings = m_layout->GetBindings();
    m_resources.resize(bindings.size());
    for (size_t i = 0; i < bindings.size(); ++i) {
        m_resources[i].slot = bindings[i].Binding;
        m_resources[i].type = bindings[i].Type;
    }
}

void D3D11DescriptorSet::WriteBuffer(const BufferWriteInfo& info)
{
    for (auto& res : m_resources) {
        if (res.slot == info.Binding) {
            auto* d3dBuf = static_cast<D3D11Buffer*>(info.Buffer.get());
            if (info.Type == DescriptorType::StorageBuffer || info.Type == DescriptorType::StorageBufferDynamic) {
                uint64_t stride = info.Stride > 0 ? info.Stride : 1;
                d3dBuf->EnsureStructuredSRV(static_cast<uint32_t>(stride));
                res.srv = d3dBuf->GetStructuredSRV();
            } else {
                res.constantBuffer = d3dBuf->GetNativeBuffer();
            }
            break;
        }
    }
}

void D3D11DescriptorSet::WriteBuffers(const BufferWriteInfos& infos)
{
    for (size_t i = 0; i < infos.Buffers.size(); ++i) {
        BufferWriteInfo single{};
        single.Binding = infos.Binding;
        single.Buffer = infos.Buffers[i];
        single.Type = infos.Type;
        single.ArrayElement = infos.ArrayElement + static_cast<uint32_t>(i);
        WriteBuffer(single);
    }
}

void D3D11DescriptorSet::WriteTexture(const TextureWriteInfo& info)
{
    for (auto& res : m_resources) {
        if (res.slot == info.Binding) {
            if (info.TextureView) {
                auto tex = info.TextureView->GetTexture();
                auto* d3dTex = static_cast<D3D11Texture*>(tex.get());
                if (d3dTex && d3dTex->GetSRV()) {
                    res.srv = d3dTex->GetSRV();
                }
            }
            if (info.Sampler) {
                auto* d3dSampler = static_cast<D3D11Sampler*>(info.Sampler.get());
                if (d3dSampler) {
                    res.sampler = d3dSampler->GetNativeSampler();
                }
            }
            break;
        }
    }
}

void D3D11DescriptorSet::WriteTextures(const TextureWriteInfos& infos)
{
    for (size_t i = 0; i < infos.TextureViews.size(); ++i) {
        TextureWriteInfo single{};
        single.Binding = infos.Binding;
        single.TextureView = infos.TextureViews[i];
        single.Type = infos.Type;
        single.ArrayElement = infos.ArrayElement + static_cast<uint32_t>(i);
        if (i < infos.Samplers.size()) {
            single.Sampler = infos.Samplers[i];
        }
        WriteTexture(single);
    }
}

void D3D11DescriptorSet::WriteSampler(const SamplerWriteInfo& info)
{
    for (auto& res : m_resources) {
        if (res.slot == info.Binding) {
            auto* d3dSampler = static_cast<D3D11Sampler*>(info.Sampler.get());
            if (d3dSampler) {
                res.sampler = d3dSampler->GetNativeSampler();
            }
            break;
        }
    }
}

void D3D11DescriptorSet::WriteSamplers(const SamplerWriteInfos& infos)
{
    for (size_t i = 0; i < infos.Samplers.size(); ++i) {
        SamplerWriteInfo single{};
        single.Binding = infos.Binding;
        single.Sampler = infos.Samplers[i];
        single.ArrayElement = infos.ArrayElement + static_cast<uint32_t>(i);
        WriteSampler(single);
    }
}

void D3D11DescriptorSet::Bind(ID3D11DeviceContext* ctx, ShaderStage stages) const
{
    for (auto& res : m_resources) {
        switch (res.type) {
            case DescriptorType::UniformBuffer:
            case DescriptorType::UniformBufferDynamic: {
                ID3D11Buffer* cb = res.constantBuffer.Get();
                if (!cb) {
                    break;
                }
                if (stages & ShaderStage::Vertex) {
                    ctx->VSSetConstantBuffers(res.slot, 1, &cb);
                }
                if (stages & ShaderStage::Fragment) {
                    ctx->PSSetConstantBuffers(res.slot, 1, &cb);
                }
                if (stages & ShaderStage::Compute) {
                    ctx->CSSetConstantBuffers(res.slot, 1, &cb);
                }
            } break;
            case DescriptorType::SampledImage:
            case DescriptorType::CombinedImageSampler: {
                ID3D11ShaderResourceView* srv = res.srv.Get();
                if (!srv) {
                    break;
                }
                if (stages & ShaderStage::Vertex) {
                    ctx->VSSetShaderResources(res.slot, 1, &srv);
                }
                if (stages & ShaderStage::Fragment) {
                    ctx->PSSetShaderResources(res.slot, 1, &srv);
                }
                if (stages & ShaderStage::Compute) {
                    ctx->CSSetShaderResources(res.slot, 1, &srv);
                }

                ID3D11SamplerState* sampler = res.sampler.Get();
                if (sampler) {
                    if (stages & ShaderStage::Vertex) {
                        ctx->VSSetSamplers(res.slot, 1, &sampler);
                    }
                    if (stages & ShaderStage::Fragment) {
                        ctx->PSSetSamplers(res.slot, 1, &sampler);
                    }
                    if (stages & ShaderStage::Compute) {
                        ctx->CSSetSamplers(res.slot, 1, &sampler);
                    }
                }
            } break;
            case DescriptorType::StorageBuffer:
            case DescriptorType::StorageBufferDynamic: {
                ID3D11ShaderResourceView* srv = res.srv.Get();
                if (!srv) {
                    break;
                }
                if (stages & ShaderStage::Vertex) {
                    ctx->VSSetShaderResources(res.slot, 1, &srv);
                }
                if (stages & ShaderStage::Fragment) {
                    ctx->PSSetShaderResources(res.slot, 1, &srv);
                }
                if (stages & ShaderStage::Compute) {
                    ctx->CSSetShaderResources(res.slot, 1, &srv);
                }
            } break;
            case DescriptorType::StorageImage: {
                ID3D11UnorderedAccessView* uav = res.uav.Get();
                if (!uav) {
                    break;
                }
                if (stages & ShaderStage::Compute) {
                    ctx->CSSetUnorderedAccessViews(res.slot, 1, &uav, nullptr);
                }
            } break;
            case DescriptorType::Sampler: {
                ID3D11SamplerState* sampler = res.sampler.Get();
                if (!sampler) {
                    break;
                }
                if (stages & ShaderStage::Vertex) {
                    ctx->VSSetSamplers(res.slot, 1, &sampler);
                }
                if (stages & ShaderStage::Fragment) {
                    ctx->PSSetSamplers(res.slot, 1, &sampler);
                }
                if (stages & ShaderStage::Compute) {
                    ctx->CSSetSamplers(res.slot, 1, &sampler);
                }
            } break;
            default:
                break;
        }
    }
}

D3D11DescriptorPool::D3D11DescriptorPool(Ref<D3D11Device> device, const DescriptorPoolCreateInfo& info)
    : m_device(std::move(device))
{}

void D3D11DescriptorPool::Reset()
{ /* DX11: no-op, resources are individually managed */
}

Ref<DescriptorSet> D3D11DescriptorPool::AllocateDescriptorSet(const Ref<DescriptorSetLayout>& layout)
{
    auto d3dLayout = std::static_pointer_cast<D3D11DescriptorSetLayout>(layout);
    return CreateRef<D3D11DescriptorSet>(m_device, d3dLayout);
}
} // namespace luna::RHI
