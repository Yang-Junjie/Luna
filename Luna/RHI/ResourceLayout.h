#pragma once

#include "Types.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace luna {

struct ResourceBindingDesc {
    uint32_t binding = 0;
    ResourceType type = ResourceType::UniformBuffer;
    uint32_t count = 1;
    ShaderType visibility = ShaderType::AllGraphics;
};

struct ResourceLayoutDesc {
    std::string_view debugName;
    uint32_t setIndex = 0;
    std::vector<ResourceBindingDesc> bindings;
};

struct BufferBindingElementWriteDesc {
    BufferHandle buffer{};
    uint64_t offset = 0;
    uint64_t size = 0;
};

struct BufferBindingWriteDesc {
    uint32_t binding = 0;
    BufferHandle buffer{};
    uint64_t offset = 0;
    uint64_t size = 0;
    ResourceType type = ResourceType::UniformBuffer;
    uint32_t firstArrayElement = 0;
    std::vector<BufferBindingElementWriteDesc> elements;
};

struct ImageBindingElementWriteDesc {
    ImageViewHandle imageView{};
    ImageHandle image{};
    SamplerHandle sampler{};
};

struct ImageBindingWriteDesc {
    uint32_t binding = 0;
    ImageViewHandle imageView{};
    ImageHandle image{};
    SamplerHandle sampler{};
    ResourceType type = ResourceType::CombinedImageSampler;
    uint32_t firstArrayElement = 0;
    std::vector<ImageBindingElementWriteDesc> elements;
};

struct ResourceSetWriteDesc {
    std::vector<BufferBindingWriteDesc> buffers;
    std::vector<ImageBindingWriteDesc> images;
};

} // namespace luna
