#pragma once

#include "Protocol/EditorProtocol.h"

#include <Core.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace luna::editor {

enum class EditorRenderPlaneKind : uint8_t {
    SceneViewport,
    DebugViewport,
    Preview,
    Capture,
};

enum class EditorRenderTransportKind : uint8_t {
    None,
    NativeSurface,
    SharedTexture,
    CpuImage,
};

[[nodiscard]] constexpr std::string_view editorRenderPlaneKindName(EditorRenderPlaneKind kind) noexcept
{
    switch (kind) {
        case EditorRenderPlaneKind::SceneViewport:
            return "sceneViewport";
        case EditorRenderPlaneKind::DebugViewport:
            return "debugViewport";
        case EditorRenderPlaneKind::Preview:
            return "preview";
        case EditorRenderPlaneKind::Capture:
            return "capture";
    }

    return "unknown";
}

[[nodiscard]] constexpr std::string_view editorRenderTransportKindName(EditorRenderTransportKind kind) noexcept
{
    switch (kind) {
        case EditorRenderTransportKind::None:
            return "none";
        case EditorRenderTransportKind::NativeSurface:
            return "nativeSurface";
        case EditorRenderTransportKind::SharedTexture:
            return "sharedTexture";
        case EditorRenderTransportKind::CpuImage:
            return "cpuImage";
    }

    return "unknown";
}

[[nodiscard]] constexpr bool editorRenderTransportIsRealtime(EditorRenderTransportKind kind) noexcept
{
    return kind == EditorRenderTransportKind::NativeSurface || kind == EditorRenderTransportKind::SharedTexture;
}

struct EditorRenderPlaneDescriptor {
    EditorRenderPlaneId plane_id{0};
    EditorViewportId viewport_id{0};
    EditorRenderPlaneKind kind{EditorRenderPlaneKind::SceneViewport};
    EditorRenderTransportKind transport{EditorRenderTransportKind::None};
    luna::RHI::Format format{luna::RHI::Format::UNDEFINED};
    uint32_t width{0};
    uint32_t height{0};
    bool y_flip{false};
    bool presentable{false};
    uint64_t generation{0};
    std::string binding_token;
    std::string label;

    [[nodiscard]] bool isValid() const noexcept
    {
        return plane_id != 0 && viewport_id != 0 && transport != EditorRenderTransportKind::None && width > 0 &&
               height > 0 && format != luna::RHI::Format::UNDEFINED;
    }

    [[nodiscard]] bool isRealtime() const noexcept
    {
        return editorRenderTransportIsRealtime(transport);
    }
};

struct EditorRenderFrameInfo {
    EditorFrameId frame_id{0};
    EditorRenderPlaneId plane_id{0};
    uint64_t sequence{0};
    uint64_t timestamp_ns{0};
    uint32_t width{0};
    uint32_t height{0};
    bool ready{false};

    [[nodiscard]] bool isValid() const noexcept
    {
        return frame_id != 0 && plane_id != 0 && width > 0 && height > 0;
    }
};

struct EditorRenderPlaneState {
    EditorRenderPlaneDescriptor descriptor;
    EditorRenderFrameInfo frame;
    bool active{false};

    [[nodiscard]] bool canPresent() const noexcept
    {
        return active && descriptor.isValid() && descriptor.presentable;
    }
};

[[nodiscard]] inline bool sameEditorRenderPlaneBinding(const EditorRenderPlaneDescriptor& lhs,
                                                       const EditorRenderPlaneDescriptor& rhs)
{
    return lhs.plane_id == rhs.plane_id && lhs.viewport_id == rhs.viewport_id && lhs.kind == rhs.kind &&
           lhs.transport == rhs.transport && lhs.format == rhs.format && lhs.width == rhs.width &&
           lhs.height == rhs.height && lhs.y_flip == rhs.y_flip && lhs.presentable == rhs.presentable &&
           lhs.binding_token == rhs.binding_token && lhs.label == rhs.label;
}

[[nodiscard]] inline bool bindEditorRenderPlane(EditorRenderPlaneState& state,
                                                EditorRenderPlaneDescriptor descriptor)
{
    const bool changed = !sameEditorRenderPlaneBinding(state.descriptor, descriptor);
    descriptor.generation = changed ? state.descriptor.generation + 1 : state.descriptor.generation;

    state.descriptor = std::move(descriptor);
    state.active = state.descriptor.transport != EditorRenderTransportKind::None && state.descriptor.width > 0 &&
                   state.descriptor.height > 0;
    state.frame.plane_id = state.descriptor.plane_id;
    if (changed) {
        state.frame.ready = false;
    }
    return changed;
}

inline void presentEditorRenderPlaneFrame(EditorRenderPlaneState& state,
                                          EditorFrameId frame_id,
                                          uint64_t sequence,
                                          uint64_t timestamp_ns)
{
    state.frame.frame_id = frame_id;
    state.frame.plane_id = state.descriptor.plane_id;
    state.frame.sequence = sequence;
    state.frame.timestamp_ns = timestamp_ns;
    state.frame.width = state.descriptor.width;
    state.frame.height = state.descriptor.height;
    state.frame.ready = state.canPresent();
}

[[nodiscard]] inline bool releaseEditorRenderPlane(EditorRenderPlaneState& state)
{
    if (state.descriptor.transport == EditorRenderTransportKind::None && state.descriptor.width == 0 &&
        state.descriptor.height == 0 && state.descriptor.binding_token.empty() && !state.active &&
        !state.frame.isValid()) {
        return false;
    }

    EditorRenderPlaneDescriptor released_descriptor;
    released_descriptor.plane_id = state.descriptor.plane_id;
    released_descriptor.viewport_id = state.descriptor.viewport_id;
    released_descriptor.kind = state.descriptor.kind;
    released_descriptor.generation = state.descriptor.generation + 1;
    released_descriptor.label = std::move(state.descriptor.label);

    state.descriptor = std::move(released_descriptor);
    state.frame = {};
    state.frame.plane_id = state.descriptor.plane_id;
    state.active = false;
    return true;
}

} // namespace luna::editor
