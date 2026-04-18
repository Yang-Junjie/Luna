#include "Impls/D3D12/D3D12Device.h"
#include "Impls/D3D12/D3D12PipelineLayout.h"
#include "Impls/D3D12/D3D12RayTracingPipeline.h"
#include "Impls/D3D12/D3D12ShaderModule.h"

#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace luna::RHI {
D3D12RayTracingPipeline::D3D12RayTracingPipeline(const Ref<Device>& device, const RayTracingPipelineCreateInfo& info)
    : m_layout(info.Layout)
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(device);
    auto* dev = d3dDevice->GetHandle();

    const size_t shaderCount = info.Shaders.size();
    const size_t maxSubobjects = shaderCount + 4; // libraries + hitgroup + shaderconfig + rootsig + pipelineconfig

    std::vector<D3D12_DXIL_LIBRARY_DESC> libDescs(shaderCount);
    std::vector<D3D12_STATE_SUBOBJECT> subobjects;
    subobjects.reserve(maxSubobjects);

    for (size_t i = 0; i < shaderCount; i++) {
        auto& blob = info.Shaders[i]->GetBlob();
        libDescs[i] = {};
        libDescs[i].DXILLibrary.pShaderBytecode = blob.Data.data();
        libDescs[i].DXILLibrary.BytecodeLength = blob.Data.size();

        D3D12_STATE_SUBOBJECT subObj = {};
        subObj.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
        subObj.pDesc = &libDescs[i];
        subobjects.push_back(subObj);
    }

    D3D12_HIT_GROUP_DESC hitGroupDesc = {};
    hitGroupDesc.HitGroupExport = L"HitGroup";
    hitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
    hitGroupDesc.ClosestHitShaderImport = L"ClosestHit";

    D3D12_STATE_SUBOBJECT hitGroupSubObj = {};
    hitGroupSubObj.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
    hitGroupSubObj.pDesc = &hitGroupDesc;
    subobjects.push_back(hitGroupSubObj);

    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
    shaderConfig.MaxPayloadSizeInBytes = 32;
    shaderConfig.MaxAttributeSizeInBytes = 8;

    D3D12_STATE_SUBOBJECT shaderConfigSubObj = {};
    shaderConfigSubObj.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    shaderConfigSubObj.pDesc = &shaderConfig;
    subobjects.push_back(shaderConfigSubObj);

    auto* d3dLayout = static_cast<D3D12PipelineLayout*>(info.Layout.get());
    D3D12_GLOBAL_ROOT_SIGNATURE globalRootSig = {};
    globalRootSig.pGlobalRootSignature = d3dLayout->GetHandle();

    D3D12_STATE_SUBOBJECT globalRootSigSubObj = {};
    globalRootSigSubObj.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    globalRootSigSubObj.pDesc = &globalRootSig;
    subobjects.push_back(globalRootSigSubObj);

    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
    pipelineConfig.MaxTraceRecursionDepth = info.MaxRecursionDepth;

    D3D12_STATE_SUBOBJECT pipelineConfigSubObj = {};
    pipelineConfigSubObj.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
    pipelineConfigSubObj.pDesc = &pipelineConfig;
    subobjects.push_back(pipelineConfigSubObj);

    D3D12_STATE_OBJECT_DESC stateObjDesc = {};
    stateObjDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    stateObjDesc.NumSubobjects = static_cast<UINT>(subobjects.size());
    stateObjDesc.pSubobjects = subobjects.data();

    HRESULT hr = dev->CreateStateObject(&stateObjDesc, IID_PPV_ARGS(&m_stateObject));
    if (FAILED(hr)) {
        std::ostringstream error;
        error << "Failed to create D3D12 RT pipeline: HRESULT=0x";
        char hrBuffer[16];
        snprintf(hrBuffer, sizeof(hrBuffer), "%08lX", hr);
        error << hrBuffer << ", subobjects=" << stateObjDesc.NumSubobjects;

        ComPtr<ID3D12InfoQueue> infoQueue;
        if (SUCCEEDED(dev->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
            UINT64 msgCount = infoQueue->GetNumStoredMessages();
            bool appendedMessage = false;
            for (UINT64 i = 0; i < msgCount; i++) {
                SIZE_T msgSize = 0;
                infoQueue->GetMessage(i, nullptr, &msgSize);
                auto* msg = (D3D12_MESSAGE*) malloc(msgSize);
                if (msg) {
                    infoQueue->GetMessage(i, msg, &msgSize);
                    error << (appendedMessage ? " | " : " [");
                    error << msg->pDescription;
                    appendedMessage = true;
                    free(msg);
                }
            }
            if (appendedMessage) {
                error << "]";
            }
            infoQueue->ClearStoredMessages();
        }
        throw std::runtime_error(error.str());
    }
}
} // namespace luna::RHI
