#ifndef LUNA_RHI_DESCRIPTORSET_H
#define LUNA_RHI_DESCRIPTORSET_H
#include "Barrier.h"

namespace luna::RHI {
enum class DescriptorType;
class TextureView;
class Sampler;
class Texture;
class Buffer;
class DescriptorSetLayout;
class DescriptorPool;

struct BufferWriteInfo {
    uint32_t Binding = 0;
    Ref<Buffer> Buffer = nullptr;
    uint64_t Offset = 0;
    uint64_t Stride = 0;
    uint64_t Size = UINT64_MAX;
    DescriptorType Type;
    uint32_t ArrayElement = 0;
};

struct TextureWriteInfo {
    uint32_t Binding = 0;
    Ref<TextureView> TextureView = nullptr;
    ResourceState Layout = ResourceState::ShaderRead;
    DescriptorType Type;
    Ref<Sampler> Sampler = nullptr;
    uint32_t ArrayElement = 0;
};

struct SamplerWriteInfo {
    uint32_t Binding = 0;
    Ref<Sampler> Sampler = nullptr;
    uint32_t ArrayElement = 0;
};

struct AccelerationStructureWriteInfo {
    uint32_t Binding = 0;
    const void* AccelerationStructureHandle = nullptr;
    DescriptorType Type;
};

struct BufferWriteInfos {
    uint32_t Binding = 0;
    std::vector<Ref<Buffer>> Buffers;
    std::vector<uint64_t> Offsets;
    std::vector<uint64_t> Strides;
    std::vector<uint64_t> Sizes;
    DescriptorType Type;
    uint32_t ArrayElement = 0;
};

struct TextureWriteInfos {
    uint32_t Binding = 0;
    std::vector<Ref<TextureView>> TextureViews;
    std::vector<ResourceState> Layouts;
    DescriptorType Type;
    std::vector<Ref<Sampler>> Samplers;
    uint32_t ArrayElement = 0;
};

struct SamplerWriteInfos {
    uint32_t Binding = 0;
    std::vector<Ref<Sampler>> Samplers;
    uint32_t ArrayElement = 0;
};

struct AccelerationStructureWriteInfos {
    uint32_t Binding = 0;
    std::vector<const void*> AccelerationStructureHandles;
    DescriptorType Type;
    uint32_t ArrayElement = 0;
};

class LUNA_RHI_API DescriptorSet : public std::enable_shared_from_this<DescriptorSet> {
public:
    virtual ~DescriptorSet() = default;
    virtual void WriteBuffer(const BufferWriteInfo& info) = 0;
    virtual void WriteBuffers(const BufferWriteInfos& infos) = 0;
    virtual void WriteTexture(const TextureWriteInfo& info) = 0;
    virtual void WriteTextures(const TextureWriteInfos& infos) = 0;
    virtual void WriteSampler(const SamplerWriteInfo& info) = 0;
    virtual void WriteSamplers(const SamplerWriteInfos& infos) = 0;
    virtual void WriteAccelerationStructure(const AccelerationStructureWriteInfo& info) = 0;
    virtual void WriteAccelerationStructures(const AccelerationStructureWriteInfos& infos) = 0;
    virtual void Update() = 0;
};

using BindingGroup = DescriptorSet;
} // namespace luna::RHI
#endif
