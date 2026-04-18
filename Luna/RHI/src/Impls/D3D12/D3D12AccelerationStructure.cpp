#include "Impls/D3D12/D3D12AccelerationStructure.h"
#include "Impls/D3D12/D3D12Buffer.h"
#include "Impls/D3D12/D3D12Device.h"

namespace luna::RHI {
D3D12AccelerationStructure::D3D12AccelerationStructure(const Ref<Device>& device,
                                                       const AccelerationStructureCreateInfo& info)
    : m_type(info.Type),
      m_device(device)
{
    auto d3dDevice = std::dynamic_pointer_cast<D3D12Device>(device);
    auto* dev5 = d3dDevice->GetHandle();

    m_inputs.Type = (info.Type == AccelerationStructureType::BottomLevel)
                        ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL
                        : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    m_inputs.Flags = info.PreferFastTrace ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE
                                          : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
    if (info.AllowUpdate) {
        m_inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
    }

    if (info.Type == AccelerationStructureType::BottomLevel) {
        for (auto& g : info.Geometries) {
            D3D12_RAYTRACING_GEOMETRY_DESC gd = {};
            gd.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            gd.Flags = g.Opaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;

            auto* vb = static_cast<D3D12Buffer*>(g.VertexBuffer.get());
            gd.Triangles.VertexBuffer.StartAddress = vb->GetHandle()->GetGPUVirtualAddress() + g.VertexOffset;
            gd.Triangles.VertexBuffer.StrideInBytes = g.VertexStride;
            gd.Triangles.VertexCount = g.VertexCount;
            gd.Triangles.VertexFormat =
                (g.VertexFormat != Format::UNDEFINED) ? ToDXGIFormat(g.VertexFormat) : DXGI_FORMAT_R32G32B32_FLOAT;

            if (g.IndexBuffer) {
                auto* ib = static_cast<D3D12Buffer*>(g.IndexBuffer.get());
                gd.Triangles.IndexBuffer = ib->GetHandle()->GetGPUVirtualAddress() + g.IndexOffset;
                gd.Triangles.IndexCount = g.IndexCount;
                gd.Triangles.IndexFormat =
                    (g.IndexFormat == IndexType::UInt16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
            }

            if (g.TransformBuffer) {
                auto* tb = static_cast<D3D12Buffer*>(g.TransformBuffer.get());
                gd.Triangles.Transform3x4 = tb->GetHandle()->GetGPUVirtualAddress() + g.TransformOffset;
            }
            m_geomDescs.push_back(gd);
        }
        m_inputs.NumDescs = static_cast<UINT>(m_geomDescs.size());
        m_inputs.pGeometryDescs = m_geomDescs.data();
    } else {
        m_inputs.NumDescs = static_cast<UINT>(info.Instances.size());
        m_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    }

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
    dev5->GetRaytracingAccelerationStructurePrebuildInfo(&m_inputs, &prebuild);

    m_resultSize = prebuild.ResultDataMaxSizeInBytes;
    m_scratchSize = prebuild.ScratchDataSizeInBytes;

    D3D12_HEAP_PROPERTIES hp = {};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Height = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_UNKNOWN;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    rd.Width = m_resultSize;
    rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    dev5->CreateCommittedResource(&hp,
                                  D3D12_HEAP_FLAG_NONE,
                                  &rd,
                                  D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
                                  nullptr,
                                  IID_PPV_ARGS(&m_resultBuffer));

    rd.Width = m_scratchSize;
    dev5->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_scratchBuffer));

    if (info.Type == AccelerationStructureType::TopLevel && !info.Instances.empty()) {
        static_assert(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) == 64);
        uint64_t instSize = info.Instances.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);

        D3D12_HEAP_PROPERTIES uhp = {};
        uhp.Type = D3D12_HEAP_TYPE_UPLOAD;
        rd.Width = instSize;
        rd.Flags = D3D12_RESOURCE_FLAG_NONE;
        dev5->CreateCommittedResource(&uhp,
                                      D3D12_HEAP_FLAG_NONE,
                                      &rd,
                                      D3D12_RESOURCE_STATE_GENERIC_READ,
                                      nullptr,
                                      IID_PPV_ARGS(&m_instanceBuffer));

        D3D12_RAYTRACING_INSTANCE_DESC* dst = nullptr;
        m_instanceBuffer->Map(0, nullptr, reinterpret_cast<void**>(&dst));
        for (size_t i = 0; i < info.Instances.size(); i++) {
            auto& src = info.Instances[i];
            memcpy(dst[i].Transform, src.Transform, sizeof(src.Transform));
            dst[i].InstanceID = src.InstanceID;
            dst[i].InstanceMask = static_cast<UINT8>(src.Mask);
            dst[i].InstanceContributionToHitGroupIndex = src.ShaderBindingTableOffset;
            dst[i].Flags = static_cast<UINT8>(src.Flags);
            dst[i].AccelerationStructure = src.AccelerationStructureAddress;
        }
        m_instanceBuffer->Unmap(0, nullptr);
    }
}

D3D12AccelerationStructure::~D3D12AccelerationStructure() = default;

uint64_t D3D12AccelerationStructure::GetDeviceAddress() const
{
    return m_resultBuffer ? m_resultBuffer->GetGPUVirtualAddress() : 0;
}

D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC D3D12AccelerationStructure::GetBuildDesc() const
{
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
    desc.DestAccelerationStructureData = m_resultBuffer->GetGPUVirtualAddress();
    desc.Inputs = m_inputs;
    desc.ScratchAccelerationStructureData = m_scratchBuffer->GetGPUVirtualAddress();
    if (m_instanceBuffer) {
        desc.Inputs.InstanceDescs = m_instanceBuffer->GetGPUVirtualAddress();
    }
    return desc;
}
} // namespace luna::RHI
