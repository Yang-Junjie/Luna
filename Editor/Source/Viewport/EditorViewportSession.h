#pragma once

#include "Protocol/ViewProtocol.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace luna {

class EditorCamera;
class Renderer;

class EditorViewportSession final {
public:
    explicit EditorViewportSession(editor::EditorViewportId viewport_id = 1,
                                   editor::EditorRenderPlaneId plane_id = 1);

    void configureRenderer(Renderer& renderer, bool imgui_overlay_enabled) const;
    void resetRenderer(Renderer& renderer);

    void setPickDebugVisualization(Renderer& renderer, bool enabled) const;
    void setEditorGrid(Renderer& renderer, bool enabled, bool runtime_viewport_enabled) const;

    [[nodiscard]] editor::EditorViewportCommandResult applyCommand(const editor::EditorViewportCommand& command);

    const editor::EditorViewportState& sync(Renderer& renderer,
                                            EditorCamera& camera,
                                            uint32_t width,
                                            uint32_t height,
                                            bool focused,
                                            bool hovered,
                                            bool input_enabled,
                                            bool mouse_captured,
                                            bool runtime_viewport_enabled,
                                            editor::EditorTransformTool transform_tool,
                                            editor::EditorTransformSpace transform_space,
                                            std::string_view title = "Viewport",
                                            editor::EditorViewportKind kind = editor::EditorViewportKind::Scene);

    bool requestScenePick(Renderer& renderer, uint32_t pixel_x, uint32_t pixel_y) const;
    std::optional<uint32_t> consumeScenePickResult(Renderer& renderer) const;

    const editor::EditorViewportState& state() const noexcept;

private:
    editor::EditorViewportState m_state;
    uint64_t m_frame_sequence{0};
};

} // namespace luna
