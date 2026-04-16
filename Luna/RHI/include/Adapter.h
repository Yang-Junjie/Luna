#ifndef CACAO_CACAOADAPTER_H
#define CACAO_CACAOADAPTER_H
#include "Core.h"

namespace Cacao {
struct DeviceCreateInfo;
class Device;

enum class AdapterType {
    Discrete,
    Integrated,
    Software,
    Unknown
};

struct AdapterProperties {
    uint32_t deviceID;
    uint32_t vendorID;
    std::string name;
    AdapterType type;
    uint64_t dedicatedVideoMemory;
};

enum class QueueType {
    Graphics,
    Compute,
    Transfer,
    Present
};

enum class DeviceFeature : uint32_t {
    MultiViewport,
    DrawIndirectCount,
    GeometryShader,
    TessellationShader,
    MultiDrawIndirect,
    FillModeNonSolid,
    WideLines,
    SamplerAnisotropy,
    TextureCompressionBC,
    TextureCompressionASTC,
    BindlessDescriptors,
    BufferDeviceAddress,
    MeshShader,
    TaskShader,
    RayTracingPipeline,
    RayTracingQuery,
    AccelerationStructure,
    VariableRateShading,
    ConditionalRendering,
    ShaderFloat64,
    ShaderInt16,
    ShaderInt64,
    SubgroupOperations,
    RayTracing,
    ShaderFloat16,
    IndependentBlending,
    PipelineStatistics
};

struct DeviceLimits {
    uint32_t maxTextureSize2D = 16'384;
    uint32_t maxTextureSize3D = 2'048;
    uint32_t maxTextureSizeCube = 16'384;
    uint32_t maxTextureArrayLayers = 2'048;
    uint32_t maxColorAttachments = 8;
    uint32_t maxViewports = 16;
    uint32_t maxComputeWorkGroupCountX = 65'535;
    uint32_t maxComputeWorkGroupCountY = 65'535;
    uint32_t maxComputeWorkGroupCountZ = 65'535;
    uint32_t maxComputeWorkGroupSizeX = 1'024;
    uint32_t maxComputeWorkGroupSizeY = 1'024;
    uint32_t maxComputeWorkGroupSizeZ = 64;
    uint32_t maxComputeSharedMemorySize = 32'768;
    uint32_t maxBoundDescriptorSets = 8;
    uint32_t maxPushConstantsSize = 128;
    uint32_t maxUniformBufferSize = 65'536;
    uint32_t maxStorageBufferSize = 128 * 1'024 * 1'024;
    uint32_t maxSamplerAnisotropy = 16;
    uint32_t maxMSAASamples = 8;
    uint64_t maxBufferSize = 256ULL * 1'024 * 1'024;
    float maxLineWidth = 8.0f;
    bool supportsAsyncCompute = false;
    bool supportsTransferQueue = false;
    bool supportsPipelineCacheSerialization = false;
    bool supportsStorageBufferWriteInGraphics = false;
};

class CACAO_API Adapter : public std::enable_shared_from_this<Adapter> {
public:
    virtual ~Adapter() = default;
    virtual AdapterProperties GetProperties() const = 0;
    virtual AdapterType GetAdapterType() const = 0;
    virtual bool IsFeatureSupported(DeviceFeature feature) const = 0;

    virtual DeviceLimits QueryLimits() const
    {
        return {};
    }

    virtual Ref<Device> CreateDevice(const DeviceCreateInfo& info) = 0;
    virtual uint32_t FindQueueFamilyIndex(QueueType type) const = 0;
};

template <> struct to_string<AdapterType> {
    static std::string convert(AdapterType type)
    {
        switch (type) {
            case AdapterType::Discrete:
                return "Discrete";
            case AdapterType::Integrated:
                return "Integrated";
            case AdapterType::Software:
                return "Software";
            case AdapterType::Unknown:
            default:
                return "Unknown";
        }
    }
};
} // namespace Cacao
#endif
