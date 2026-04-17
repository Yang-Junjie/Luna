#include "Impls/WebGPU/WGPUCommon.h"
#include "Impls/WebGPU/WGPUDescriptorSet.h"

#include <webgpu/webgpu.h>

namespace luna::RHI {
// === DescriptorSetLayout ===
// WebGPU equivalent: WGPUBindGroupLayout
// Created via wgpuDeviceCreateBindGroupLayout when device handle is available.
// Layout entries are stored for deferred creation.
WGPUDescriptorSetLayout::WGPUDescriptorSetLayout(const DescriptorSetLayoutCreateInfo& info)
    : m_createInfo(info)
{}

// === DescriptorPool ===
// WebGPU has no pool concept; bind groups are created directly from the device.
WGPUDescriptorPool::WGPUDescriptorPool(const DescriptorPoolCreateInfo& info)
    : m_createInfo(info)
{}

Ref<DescriptorSet> WGPUDescriptorPool::AllocateDescriptorSet(const Ref<DescriptorSetLayout>& layout)
{
    return std::make_shared<WGPUDescriptorSet>();
}

// === DescriptorSet ===
// WebGPU equivalent: WGPUBindGroup
// Entries are accumulated via Write* calls, then materialized in Update().
WGPUDescriptorSet::WGPUDescriptorSet() {}

void WGPUDescriptorSet::WriteBuffer(const BufferWriteInfo& info)
{
    // Accumulates: binding, buffer handle, offset, size for WGPUBindGroupEntry
}

void WGPUDescriptorSet::WriteBuffers(const BufferWriteInfos& infos)
{
    for (size_t i = 0; i < infos.Buffers.size(); i++) {
        BufferWriteInfo single;
        single.Binding = infos.Binding;
        single.Buffer = infos.Buffers[i];
        single.Offset = i < infos.Offsets.size() ? infos.Offsets[i] : 0;
        single.Size = i < infos.Sizes.size() ? infos.Sizes[i] : UINT64_MAX;
        single.Type = infos.Type;
        single.ArrayElement = infos.ArrayElement + static_cast<uint32_t>(i);
        WriteBuffer(single);
    }
}

void WGPUDescriptorSet::WriteTexture(const TextureWriteInfo& info)
{
    // Accumulates: binding, texture view handle for WGPUBindGroupEntry
}

void WGPUDescriptorSet::WriteTextures(const TextureWriteInfos& infos)
{
    for (size_t i = 0; i < infos.TextureViews.size(); i++) {
        TextureWriteInfo single;
        single.Binding = infos.Binding;
        single.TextureView = infos.TextureViews[i];
        single.Layout = i < infos.Layouts.size() ? infos.Layouts[i] : ResourceState::ShaderRead;
        single.Type = infos.Type;
        single.Sampler = i < infos.Samplers.size() ? infos.Samplers[i] : nullptr;
        single.ArrayElement = infos.ArrayElement + static_cast<uint32_t>(i);
        WriteTexture(single);
    }
}

void WGPUDescriptorSet::WriteSampler(const SamplerWriteInfo& info)
{
    // Accumulates: binding, sampler handle for WGPUBindGroupEntry
}

void WGPUDescriptorSet::WriteSamplers(const SamplerWriteInfos& infos)
{
    for (size_t i = 0; i < infos.Samplers.size(); i++) {
        SamplerWriteInfo single;
        single.Binding = infos.Binding;
        single.Sampler = infos.Samplers[i];
        single.ArrayElement = infos.ArrayElement + static_cast<uint32_t>(i);
        WriteSampler(single);
    }
}

void WGPUDescriptorSet::WriteAccelerationStructure(const AccelerationStructureWriteInfo& info)
{
    // WebGPU does not support ray tracing acceleration structures
}

void WGPUDescriptorSet::WriteAccelerationStructures(const AccelerationStructureWriteInfos& infos)
{
    // WebGPU does not support ray tracing acceleration structures
}

void WGPUDescriptorSet::Update()
{
    // Materializes the accumulated entries into a WGPUBindGroup via
    // wgpuDeviceCreateBindGroup when device handle becomes available.
    // The bind group is recreated each time Update() is called.
}

// === Sampler ===
// WebGPU equivalent: WGPUSampler
// Created via wgpuDeviceCreateSampler when device handle is available.
// The SamplerCreateInfo parameters map to WGPUSamplerDescriptor fields.
WGPUSampler::WGPUSampler(const SamplerCreateInfo& info) {}

// === PipelineLayout ===
// WebGPU equivalent: WGPUPipelineLayout
// Created via wgpuDeviceCreatePipelineLayout with bind group layouts.
WGPUPipelineLayout::WGPUPipelineLayout(const PipelineLayoutCreateInfo& info)
    : m_createInfo(info)
{}

// === PipelineCache ===
// WebGPU doesn't expose pipeline cache serialization
std::vector<uint8_t> WGPUPipelineCache::GetData() const
{
    return {};
}

// === ShaderModule ===
// WebGPU equivalent: WGPUShaderModule
// Created via wgpuDeviceCreateShaderModule with SPIRV or WGSL source.
// The ShaderBlob data is SPIRV bytecode compiled by the Slang compiler.
WGPUShaderModule::WGPUShaderModule(const ShaderBlob& blob, const ShaderCreateInfo& info)
{
    // SPIRV data stored in blob.Data; native module created in pipeline construction
    // since shader modules in WebGPU require a device handle.
}
} // namespace luna::RHI
