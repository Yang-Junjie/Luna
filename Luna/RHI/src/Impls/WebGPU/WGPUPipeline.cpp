#include "Impls/WebGPU/WGPUCommon.h"
#include "Impls/WebGPU/WGPUDevice.h"
#include "Impls/WebGPU/WGPUPipeline.h"

#include <stdexcept>

namespace luna::RHI {
WGPUGraphicsPipeline::WGPUGraphicsPipeline(const Ref<Device>& device, const GraphicsPipelineCreateInfo& info)
    : m_layout(info.Layout),
      m_createInfo(info)
{
    m_device = std::dynamic_pointer_cast<WGPUDevice>(device);
    if (!m_device || !m_device->GetHandle()) {
        throw std::runtime_error("WGPUGraphicsPipeline requires a valid WGPUDevice");
    }

    auto wgpuDevice = m_device->GetHandle();

    // --- Vertex buffer layouts ---
    std::vector<WGPUVertexBufferLayout> vbLayouts(info.VertexBindings.size());
    std::vector<std::vector<WGPUVertexAttribute>> attrStorage(info.VertexBindings.size());

    for (size_t b = 0; b < info.VertexBindings.size(); b++) {
        for (auto& attr : info.VertexAttributes) {
            if (attr.Binding == info.VertexBindings[b].Binding) {
                WGPUVertexAttribute wa = {};
                wa.format = ToWGPUVertexFormat(attr.Format);
                wa.offset = attr.Offset;
                wa.shaderLocation = attr.Location;
                attrStorage[b].push_back(wa);
            }
        }
        vbLayouts[b].arrayStride = info.VertexBindings[b].Stride;
        vbLayouts[b].stepMode = (info.VertexBindings[b].InputRate == VertexInputRate::Instance)
                                    ? WGPUVertexStepMode_Instance
                                    : WGPUVertexStepMode_Vertex;
        vbLayouts[b].attributeCount = static_cast<uint32_t>(attrStorage[b].size());
        vbLayouts[b].attributes = attrStorage[b].data();
    }

    // --- Find vertex and fragment shaders ---
    ::WGPUShaderModule vsModule = nullptr;
    ::WGPUShaderModule fsModule = nullptr;
    for (auto& shader : info.Shaders) {
        if (!shader) {
            continue;
        }
        // WebGPU uses SPIRV or WGSL modules created via wgpuDeviceCreateShaderModule.
        // The native handle is stored on the WGPUShaderModule subclass.
        // For now, extract bytecode and create inline modules.
    }

    // --- Vertex state ---
    WGPUVertexState vertexState = {};
    vertexState.entryPoint = "main";
    vertexState.bufferCount = static_cast<uint32_t>(vbLayouts.size());
    vertexState.buffers = vbLayouts.data();

    // Create shader modules from SPIRV bytecode
    std::vector<::WGPUShaderModule> createdModules;
    for (auto& shader : info.Shaders) {
        if (!shader) {
            continue;
        }
        auto& blob = shader->GetBlob();
        WGPUShaderModuleDescriptor smDesc = {};
        WGPUShaderModuleSPIRVDescriptor spirvDesc = {};
        spirvDesc.chain.sType = WGPUSType_ShaderModuleSPIRVDescriptor;
        spirvDesc.codeSize = static_cast<uint32_t>(blob.Data.size() / 4);
        spirvDesc.code = reinterpret_cast<const uint32_t*>(blob.Data.data());
        smDesc.nextInChain = &spirvDesc.chain;
        auto mod = wgpuDeviceCreateShaderModule(wgpuDevice, &smDesc);
        createdModules.push_back(mod);

        if (shader->GetStage() == ShaderStage::Vertex) {
            vsModule = mod;
        } else if (shader->GetStage() == ShaderStage::Fragment) {
            fsModule = mod;
        }
    }

    if (vsModule) {
        vertexState.module = vsModule;
    }

    // --- Primitive state ---
    WGPUPrimitiveState primitiveState = {};
    primitiveState.topology = ToWGPUTopology(info.InputAssembly.Topology);
    primitiveState.frontFace = ToWGPUFrontFace(info.Rasterizer.FrontFace);
    primitiveState.cullMode = ToWGPUCullMode(info.Rasterizer.CullMode);
    primitiveState.stripIndexFormat = WGPUIndexFormat_Undefined;

    // --- Depth/stencil state ---
    WGPUDepthStencilState depthStencilState = {};
    bool hasDepthStencil = (info.DepthStencilFormat != Format::UNDEFINED);
    if (hasDepthStencil) {
        depthStencilState.format = ToWGPUFormat(info.DepthStencilFormat);
        depthStencilState.depthWriteEnabled = info.DepthStencil.DepthWriteEnable;
        depthStencilState.depthCompare = info.DepthStencil.DepthTestEnable
                                             ? ToWGPUCompareFunction(info.DepthStencil.DepthCompareOp)
                                             : WGPUCompareFunction_Always;
        depthStencilState.depthBiasConstant = static_cast<int32_t>(info.Rasterizer.DepthBiasConstantFactor);
        depthStencilState.depthBiasSlopeScale = info.Rasterizer.DepthBiasSlopeFactor;
        depthStencilState.depthBiasClamp = info.Rasterizer.DepthBiasClamp;

        depthStencilState.stencilFront.compare = info.DepthStencil.StencilTestEnable
                                                     ? ToWGPUCompareFunction(info.DepthStencil.Front.CompareOp)
                                                     : WGPUCompareFunction_Always;
        depthStencilState.stencilFront.failOp = ToWGPUStencilOp(info.DepthStencil.Front.FailOp);
        depthStencilState.stencilFront.depthFailOp = ToWGPUStencilOp(info.DepthStencil.Front.DepthFailOp);
        depthStencilState.stencilFront.passOp = ToWGPUStencilOp(info.DepthStencil.Front.PassOp);
        depthStencilState.stencilBack.compare = info.DepthStencil.StencilTestEnable
                                                    ? ToWGPUCompareFunction(info.DepthStencil.Back.CompareOp)
                                                    : WGPUCompareFunction_Always;
        depthStencilState.stencilBack.failOp = ToWGPUStencilOp(info.DepthStencil.Back.FailOp);
        depthStencilState.stencilBack.depthFailOp = ToWGPUStencilOp(info.DepthStencil.Back.DepthFailOp);
        depthStencilState.stencilBack.passOp = ToWGPUStencilOp(info.DepthStencil.Back.PassOp);
        depthStencilState.stencilReadMask = info.DepthStencil.StencilReadMask;
        depthStencilState.stencilWriteMask = info.DepthStencil.StencilWriteMask;
    }

    // --- Multisample state ---
    WGPUMultisampleState multisampleState = {};
    multisampleState.count = std::max(1u, info.Multisample.RasterizationSamples);
    multisampleState.mask = 0xFF'FF'FF'FF;
    multisampleState.alphaToCoverageEnabled = info.Multisample.AlphaToCoverageEnable;

    // --- Fragment state + color targets ---
    std::vector<WGPUColorTargetState> colorTargets(info.ColorAttachmentFormats.size());
    std::vector<WGPUBlendState> blendStates(info.ColorAttachmentFormats.size());

    for (size_t i = 0; i < info.ColorAttachmentFormats.size(); i++) {
        colorTargets[i].format = ToWGPUFormat(info.ColorAttachmentFormats[i]);

        if (i < info.ColorBlend.Attachments.size()) {
            auto& att = info.ColorBlend.Attachments[i];
            colorTargets[i].writeMask = ToWGPUColorWriteMask(att.ColorWriteMask);
            if (att.BlendEnable) {
                blendStates[i].color.srcFactor = ToWGPUBlendFactor(att.SrcColorBlendFactor);
                blendStates[i].color.dstFactor = ToWGPUBlendFactor(att.DstColorBlendFactor);
                blendStates[i].color.operation = ToWGPUBlendOp(att.ColorBlendOp);
                blendStates[i].alpha.srcFactor = ToWGPUBlendFactor(att.SrcAlphaBlendFactor);
                blendStates[i].alpha.dstFactor = ToWGPUBlendFactor(att.DstAlphaBlendFactor);
                blendStates[i].alpha.operation = ToWGPUBlendOp(att.AlphaBlendOp);
                colorTargets[i].blend = &blendStates[i];
            } else {
                colorTargets[i].blend = nullptr;
            }
        } else {
            colorTargets[i].writeMask = WGPUColorWriteMask_All;
            colorTargets[i].blend = nullptr;
        }
    }

    WGPUFragmentState fragmentState = {};
    bool hasFragment = (fsModule != nullptr && !info.ColorAttachmentFormats.empty());
    if (hasFragment) {
        fragmentState.module = fsModule;
        fragmentState.entryPoint = "main";
        fragmentState.targetCount = static_cast<uint32_t>(colorTargets.size());
        fragmentState.targets = colorTargets.data();
    }

    // --- Pipeline descriptor ---
    WGPURenderPipelineDescriptor desc = {};
    desc.vertex = vertexState;
    desc.primitive = primitiveState;
    desc.depthStencil = hasDepthStencil ? &depthStencilState : nullptr;
    desc.multisample = multisampleState;
    desc.fragment = hasFragment ? &fragmentState : nullptr;
    desc.layout = nullptr; // auto layout

    m_pipeline = wgpuDeviceCreateRenderPipeline(wgpuDevice, &desc);
    if (!m_pipeline) {
        throw std::runtime_error("Failed to create WebGPU render pipeline");
    }

    for (auto mod : createdModules) {
        wgpuShaderModuleRelease(mod);
    }
}

WGPUGraphicsPipeline::~WGPUGraphicsPipeline()
{
    if (m_pipeline) {
        wgpuRenderPipelineRelease(m_pipeline);
        m_pipeline = nullptr;
    }
}

WGPUComputePipelineImpl::WGPUComputePipelineImpl(const Ref<Device>& device, const ComputePipelineCreateInfo& info)
    : m_layout(info.Layout),
      m_createInfo(info)
{
    m_device = std::dynamic_pointer_cast<WGPUDevice>(device);
    if (!m_device || !m_device->GetHandle()) {
        throw std::runtime_error("WGPUComputePipelineImpl requires a valid WGPUDevice");
    }

    if (!info.ComputeShader) {
        throw std::runtime_error("WGPUComputePipelineImpl requires a compute shader");
    }

    auto wgpuDevice = m_device->GetHandle();

    auto& blob = info.ComputeShader->GetBlob();
    WGPUShaderModuleDescriptor smDesc = {};
    WGPUShaderModuleSPIRVDescriptor spirvDesc = {};
    spirvDesc.chain.sType = WGPUSType_ShaderModuleSPIRVDescriptor;
    spirvDesc.codeSize = static_cast<uint32_t>(blob.Data.size() / 4);
    spirvDesc.code = reinterpret_cast<const uint32_t*>(blob.Data.data());
    smDesc.nextInChain = &spirvDesc.chain;
    auto csModule = wgpuDeviceCreateShaderModule(wgpuDevice, &smDesc);

    WGPUComputePipelineDescriptor desc = {};
    desc.compute.module = csModule;
    desc.compute.entryPoint = "main";
    desc.layout = nullptr; // auto layout

    m_pipeline = wgpuDeviceCreateComputePipeline(wgpuDevice, &desc);

    wgpuShaderModuleRelease(csModule);

    if (!m_pipeline) {
        throw std::runtime_error("Failed to create WebGPU compute pipeline");
    }
}

WGPUComputePipelineImpl::~WGPUComputePipelineImpl()
{
    if (m_pipeline) {
        wgpuComputePipelineRelease(m_pipeline);
        m_pipeline = nullptr;
    }
}
} // namespace luna::RHI
