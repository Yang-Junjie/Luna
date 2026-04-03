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
    std::vector<ResourceBindingDesc> bindings;
};

struct BufferBindingWriteDesc {
    uint32_t binding = 0;
    BufferHandle buffer{};
    uint64_t offset = 0;
    uint64_t size = 0;
    ResourceType type = ResourceType::UniformBuffer;
};

struct ImageBindingWriteDesc {
    uint32_t binding = 0;
    ImageHandle image{};
    SamplerHandle sampler{};
    ResourceType type = ResourceType::CombinedImageSampler;
};

struct ResourceSetWriteDesc {
    std::vector<BufferBindingWriteDesc> buffers;
    std::vector<ImageBindingWriteDesc> images;
};

} // namespace luna
