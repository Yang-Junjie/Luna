#ifndef CACAO_RAYTRACING_H
#define CACAO_RAYTRACING_H
#include "Core.h"
#include "Buffer.h"
#include "Texture.h"
#include "CommandBufferEncoder.h"
#include <memory>
#include <vector>

namespace Cacao
{
    enum class AccelerationStructureType { TopLevel, BottomLevel };

    struct AccelerationStructureGeometryDesc
    {
        Ref<Buffer> VertexBuffer;
        uint64_t VertexOffset = 0;
        uint32_t VertexStride = 0;
        uint32_t VertexCount = 0;
        Format VertexFormat = Format::UNDEFINED;
        Ref<Buffer> IndexBuffer;
        uint64_t IndexOffset = 0;
        uint32_t IndexCount = 0;
        IndexType IndexFormat = IndexType::UInt32;
        Ref<Buffer> TransformBuffer;
        uint64_t TransformOffset = 0;
        bool Opaque = true;
    };

    struct AccelerationStructureInstance
    {
        float Transform[3][4] = {};
        uint32_t InstanceID = 0;
        uint32_t Mask = 0xFF;
        uint32_t ShaderBindingTableOffset = 0;
        uint32_t Flags = 0;
        uint64_t AccelerationStructureAddress = 0;
    };

    struct AccelerationStructureCreateInfo
    {
        AccelerationStructureType Type = AccelerationStructureType::BottomLevel;
        std::vector<AccelerationStructureGeometryDesc> Geometries;
        std::vector<AccelerationStructureInstance> Instances;
        bool AllowUpdate = false;
        bool PreferFastTrace = true;
    };

    class CACAO_API AccelerationStructure
    {
    public:
        virtual ~AccelerationStructure() = default;
        virtual AccelerationStructureType GetType() const = 0;
        virtual uint64_t GetDeviceAddress() const = 0;
        virtual uint64_t GetScratchSize() const = 0;
    };

    struct ShaderBindingTableCreateInfo
    {
        Ref<class GraphicsPipeline> RayTracingPipeline;
        uint32_t RayGenCount = 1;
        uint32_t MissCount = 1;
        uint32_t HitGroupCount = 1;
        uint32_t CallableCount = 0;
    };

    class CACAO_API ShaderBindingTable
    {
    public:
        virtual ~ShaderBindingTable() = default;
        virtual Ref<Buffer> GetBuffer() const = 0;
        virtual uint64_t GetRayGenOffset() const = 0;
        virtual uint64_t GetMissOffset() const = 0;
        virtual uint64_t GetHitGroupOffset() const = 0;
        virtual uint64_t GetCallableOffset() const = 0;
        virtual uint32_t GetEntrySize() const = 0;
    };
}

#endif
