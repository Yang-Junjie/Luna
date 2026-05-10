#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace luna::editor {

inline constexpr std::string_view kEditorProtocolName = "luna.editor";
inline constexpr uint32_t kEditorProtocolVersion = 1;

using EditorSessionId = uint64_t;
using EditorViewportId = uint64_t;
using EditorRenderPlaneId = uint64_t;
using EditorFrameId = uint64_t;

struct EditorProtocolInfo {
    std::string name{std::string(kEditorProtocolName)};
    uint32_t version{kEditorProtocolVersion};
};

[[nodiscard]] inline bool isEditorProtocolCompatible(const EditorProtocolInfo& info) noexcept
{
    return info.name == kEditorProtocolName && info.version == kEditorProtocolVersion;
}

} // namespace luna::editor

