#pragma once

#include <cstdint>
#include <string_view>

namespace luna {

struct ClearColorValue {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
};

struct DrawArguments {
    uint32_t vertexCount = 0;
    uint32_t instanceCount = 1;
    uint32_t firstVertex = 0;
    uint32_t firstInstance = 0;
};

enum class IndexFormat : uint32_t {
    UInt16 = 0,
    UInt32
};

struct IndexedDrawArguments {
    uint32_t indexCount = 0;
    uint32_t instanceCount = 1;
    uint32_t firstIndex = 0;
    int32_t vertexOffset = 0;
    uint32_t firstInstance = 0;
};

enum class AttachmentLoadOp : uint32_t {
    Load = 0,
    Clear,
    Discard
};

constexpr std::string_view to_string(AttachmentLoadOp op) noexcept
{
    switch (op) {
        case AttachmentLoadOp::Load:
            return "Load";
        case AttachmentLoadOp::Clear:
            return "Clear";
        case AttachmentLoadOp::Discard:
            return "Discard";
        default:
            return "Unknown";
    }
}

enum class AttachmentStoreOp : uint32_t {
    Store = 0,
    Discard
};

constexpr std::string_view to_string(AttachmentStoreOp op) noexcept
{
    switch (op) {
        case AttachmentStoreOp::Store:
            return "Store";
        case AttachmentStoreOp::Discard:
            return "Discard";
        default:
            return "Unknown";
    }
}

struct Viewport {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float minDepth = 0.0f;
    float maxDepth = 1.0f;
};

struct ScissorRect {
    int32_t x = 0;
    int32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

} // namespace luna
