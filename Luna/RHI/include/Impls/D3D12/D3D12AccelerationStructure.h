#ifndef LUNA_RHI_D3D12ACCELERATIONSTRUCTURE_H
#define LUNA_RHI_D3D12ACCELERATIONSTRUCTURE_H
#include "D3D12Common.h"
#include "RayTracing.h"

namespace luna::RHI {
class LUNA_RHI_API D3D12AccelerationStructure final : public AccelerationStructure {
public:
    D3D12AccelerationStructure(const Ref<Device>& device, const AccelerationStructureCreateInfo& info);
    ~D3D12AccelerationStructure() override;

    AccelerationStructureType GetType() const override
    {
        return m_type;
    }

    uint64_t GetDeviceAddress() const override;

    uint64_t GetScratchSize() const override
    {
        return m_scratchSize;
    }

    ID3D12Resource* GetResultBuffer() const
    {
        return m_resultBuffer.Get();
    }

    ID3D12Resource* GetScratchBuffer() const
    {
        return m_scratchBuffer.Get();
    }

    ID3D12Resource* GetInstanceBuffer() const
    {
        return m_instanceBuffer.Get();
    }

    const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& GetInputs() const
    {
        return m_inputs;
    }

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC GetBuildDesc() const;

private:
    AccelerationStructureType m_type;
    ComPtr<ID3D12Resource> m_resultBuffer;
    ComPtr<ID3D12Resource> m_scratchBuffer;
    ComPtr<ID3D12Resource> m_instanceBuffer;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS m_inputs = {};
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> m_geomDescs;
    uint64_t m_scratchSize = 0;
    uint64_t m_resultSize = 0;
    Ref<Device> m_device;
};
} // namespace luna::RHI

#endif
