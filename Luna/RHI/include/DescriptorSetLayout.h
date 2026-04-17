#ifndef LUNA_RHI_DESCRIPTOR_H
#define LUNA_RHI_DESCRIPTOR_H
#include "Core.h"

#include <memory>
#include <vector>

namespace luna::RHI {
class Buffer;
class Texture;
class Sampler;
enum class DescriptorType {
    Sampler,
    CombinedImageSampler,
    SampledImage,
    StorageImage,
    UniformBuffer,
    StorageBuffer,
    UniformBufferDynamic,
    StorageBufferDynamic,
    InputAttachment,
    AccelerationStructure
};

struct DescriptorSetLayoutBinding {
    uint32_t Binding = 0;
    DescriptorType Type = DescriptorType::UniformBuffer;
    uint32_t Count = 1;
    ShaderStage StageFlags = ShaderStage::AllGraphics;
    std::vector<std::shared_ptr<Sampler>> ImmutableSamplers;
};

struct DescriptorSetLayoutCreateInfo {
    std::vector<DescriptorSetLayoutBinding> Bindings;
    bool SupportBindless = false;
};

class LUNA_RHI_API DescriptorSetLayout : public std::enable_shared_from_this<DescriptorSetLayout> {
public:
    virtual ~DescriptorSetLayout() = default;
};

using BindingType = DescriptorType;

struct BindingSlot {
    uint32_t Slot = 0;
    BindingType Type = BindingType::UniformBuffer;
    uint32_t Count = 1;
    ShaderStage Visibility = ShaderStage::AllGraphics;
};

struct BindingLayoutCreateInfo {
    std::vector<BindingSlot> Slots;
    bool SupportBindless = false;
};

using BindingLayout = DescriptorSetLayout;
using BindingLayoutBinding = DescriptorSetLayoutBinding;
using BindingLayoutCreateInfoLegacy = DescriptorSetLayoutCreateInfo;
} // namespace luna::RHI
#endif
