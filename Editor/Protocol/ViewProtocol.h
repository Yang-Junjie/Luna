#pragma once

#include "Protocol/EditorProtocol.h"
#include "Protocol/RenderDataPlane.h"
#include "Renderer/Camera.h"
#include "Renderer/RenderFlow/RenderFlowTypes.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace luna::editor {

enum class EditorViewportKind : uint8_t {
    Scene,
    Debug,
    Preview,
    Custom,
};

enum class EditorTransformTool : uint8_t {
    Translate,
    Rotate,
    Scale,
};

enum class EditorTransformSpace : uint8_t {
    Local,
    World,
};

enum class EditorViewportCommandKind : uint8_t {
    CreateViewport,
    DestroyViewport,
    ResizeViewport,
    SetCamera,
    SetInteractionState,
    SetTransformState,
    SetDebugViewMode,
    SetDebugVelocityScale,
    RequestPick,
    CaptureFrame,
};

enum class EditorViewportCommandEffect : uint8_t {
    None = 0,
    MutatesViewportState = 1 << 0,
    RequestsPick = 1 << 1,
    RequestsCapture = 1 << 2,
};

[[nodiscard]] constexpr EditorViewportCommandEffect operator|(EditorViewportCommandEffect lhs,
                                                               EditorViewportCommandEffect rhs) noexcept
{
    return static_cast<EditorViewportCommandEffect>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

[[nodiscard]] constexpr bool hasEditorViewportCommandEffect(EditorViewportCommandEffect effects,
                                                            EditorViewportCommandEffect effect) noexcept
{
    return (static_cast<uint8_t>(effects) & static_cast<uint8_t>(effect)) != 0;
}

[[nodiscard]] constexpr std::string_view editorViewportKindName(EditorViewportKind kind) noexcept
{
    switch (kind) {
        case EditorViewportKind::Scene:
            return "scene";
        case EditorViewportKind::Debug:
            return "debug";
        case EditorViewportKind::Preview:
            return "preview";
        case EditorViewportKind::Custom:
            return "custom";
    }

    return "unknown";
}

[[nodiscard]] constexpr std::string_view editorTransformToolName(EditorTransformTool tool) noexcept
{
    switch (tool) {
        case EditorTransformTool::Translate:
            return "translate";
        case EditorTransformTool::Rotate:
            return "rotate";
        case EditorTransformTool::Scale:
            return "scale";
    }

    return "unknown";
}

[[nodiscard]] constexpr std::string_view editorTransformSpaceName(EditorTransformSpace space) noexcept
{
    switch (space) {
        case EditorTransformSpace::Local:
            return "local";
        case EditorTransformSpace::World:
            return "world";
    }

    return "unknown";
}

[[nodiscard]] constexpr std::string_view editorViewportCommandName(EditorViewportCommandKind kind) noexcept
{
    switch (kind) {
        case EditorViewportCommandKind::CreateViewport:
            return "createViewport";
        case EditorViewportCommandKind::DestroyViewport:
            return "destroyViewport";
        case EditorViewportCommandKind::ResizeViewport:
            return "resizeViewport";
        case EditorViewportCommandKind::SetCamera:
            return "setCamera";
        case EditorViewportCommandKind::SetInteractionState:
            return "setInteractionState";
        case EditorViewportCommandKind::SetTransformState:
            return "setTransformState";
        case EditorViewportCommandKind::SetDebugViewMode:
            return "setDebugViewMode";
        case EditorViewportCommandKind::SetDebugVelocityScale:
            return "setDebugVelocityScale";
        case EditorViewportCommandKind::RequestPick:
            return "requestPick";
        case EditorViewportCommandKind::CaptureFrame:
            return "captureFrame";
    }

    return "unknown";
}

[[nodiscard]] constexpr EditorViewportCommandEffect editorViewportCommandEffects(EditorViewportCommandKind kind) noexcept
{
    switch (kind) {
        case EditorViewportCommandKind::CreateViewport:
        case EditorViewportCommandKind::DestroyViewport:
        case EditorViewportCommandKind::ResizeViewport:
        case EditorViewportCommandKind::SetCamera:
        case EditorViewportCommandKind::SetInteractionState:
        case EditorViewportCommandKind::SetTransformState:
        case EditorViewportCommandKind::SetDebugViewMode:
        case EditorViewportCommandKind::SetDebugVelocityScale:
            return EditorViewportCommandEffect::MutatesViewportState;
        case EditorViewportCommandKind::RequestPick:
            return EditorViewportCommandEffect::RequestsPick;
        case EditorViewportCommandKind::CaptureFrame:
            return EditorViewportCommandEffect::RequestsCapture;
    }

    return EditorViewportCommandEffect::None;
}

[[nodiscard]] constexpr bool editorViewportCommandMutatesState(EditorViewportCommandKind kind) noexcept
{
    return hasEditorViewportCommandEffect(editorViewportCommandEffects(kind),
                                          EditorViewportCommandEffect::MutatesViewportState);
}

[[nodiscard]] constexpr bool editorViewportCommandRequestsPick(EditorViewportCommandKind kind) noexcept
{
    return hasEditorViewportCommandEffect(editorViewportCommandEffects(kind), EditorViewportCommandEffect::RequestsPick);
}

[[nodiscard]] constexpr bool editorViewportCommandRequestsCapture(EditorViewportCommandKind kind) noexcept
{
    return hasEditorViewportCommandEffect(editorViewportCommandEffects(kind),
                                          EditorViewportCommandEffect::RequestsCapture);
}

[[nodiscard]] constexpr std::string_view editorRenderDebugViewModeName(RenderDebugViewMode mode) noexcept
{
    switch (mode) {
        case RenderDebugViewMode::None:
            return "none";
        case RenderDebugViewMode::Velocity:
            return "velocity";
        case RenderDebugViewMode::HistoryValidity:
            return "historyValidity";
        case RenderDebugViewMode::ShadowCascades:
            return "shadowCascades";
        case RenderDebugViewMode::BaseColor:
            return "baseColor";
        case RenderDebugViewMode::Normal:
            return "normal";
        case RenderDebugViewMode::Metallic:
            return "metallic";
        case RenderDebugViewMode::Roughness:
            return "roughness";
        case RenderDebugViewMode::DirectLighting:
            return "directLighting";
        case RenderDebugViewMode::SpecularIbl:
            return "specularIbl";
        case RenderDebugViewMode::BloomInput:
            return "bloomInput";
        case RenderDebugViewMode::BloomPrefilter:
            return "bloomPrefilter";
        case RenderDebugViewMode::BloomMip0:
            return "bloomMip0";
        case RenderDebugViewMode::BloomMip1:
            return "bloomMip1";
        case RenderDebugViewMode::BloomMip2:
            return "bloomMip2";
        case RenderDebugViewMode::BloomMip3:
            return "bloomMip3";
        case RenderDebugViewMode::BloomMip4:
            return "bloomMip4";
        case RenderDebugViewMode::BloomMip5:
            return "bloomMip5";
        case RenderDebugViewMode::BloomComposite:
            return "bloomComposite";
    }

    return "unknown";
}

struct EditorViewportSize {
    uint32_t width{0};
    uint32_t height{0};

    [[nodiscard]] bool isValid() const noexcept
    {
        return width > 0 && height > 0;
    }
};

struct EditorViewportCameraState {
    Camera::ProjectionType projection_type{Camera::ProjectionType::Perspective};
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 orientation_euler_radians{0.0f, 0.0f, 0.0f};
    float perspective_vertical_fov_radians{glm::radians(50.0f)};
    float perspective_near{0.05f};
    float perspective_far{500.0f};
    float orthographic_size{10.0f};
    float orthographic_near{-100.0f};
    float orthographic_far{100.0f};

    [[nodiscard]] Camera toCamera() const
    {
        Camera camera;
        if (projection_type == Camera::ProjectionType::Orthographic) {
            camera.setOrthographic(orthographic_size, orthographic_near, orthographic_far);
        } else {
            camera.setPerspective(perspective_vertical_fov_radians, perspective_near, perspective_far);
        }
        camera.setPosition(position);
        camera.setOrientationEuler(orientation_euler_radians);
        return camera;
    }

    [[nodiscard]] static EditorViewportCameraState fromCamera(const Camera& camera)
    {
        EditorViewportCameraState state;
        state.projection_type = camera.getProjectionType();
        state.position = camera.getPosition();
        state.orientation_euler_radians = camera.getOrientationEuler();

        const auto& perspective = camera.getPerspectiveSettings();
        state.perspective_vertical_fov_radians = perspective.vertical_fov_radians;
        state.perspective_near = perspective.near_clip;
        state.perspective_far = perspective.far_clip;

        const auto& orthographic = camera.getOrthographicSettings();
        state.orthographic_size = orthographic.vertical_size;
        state.orthographic_near = orthographic.near_clip;
        state.orthographic_far = orthographic.far_clip;
        return state;
    }
};

struct EditorViewportInteractionState {
    bool visible{true};
    bool focused{false};
    bool hovered{false};
    bool input_enabled{false};
    bool mouse_captured{false};
    bool runtime_viewport{false};
    bool pick_debug_enabled{false};
};

struct EditorViewportState {
    EditorViewportId viewport_id{0};
    std::string title{"Viewport"};
    EditorViewportKind kind{EditorViewportKind::Scene};
    EditorViewportSize size;
    EditorViewportCameraState camera;
    EditorViewportInteractionState interaction;
    EditorTransformTool transform_tool{EditorTransformTool::Translate};
    EditorTransformSpace transform_space{EditorTransformSpace::Local};
    RenderDebugViewMode debug_view_mode{RenderDebugViewMode::None};
    float debug_velocity_scale{20.0f};
    EditorRenderPlaneState render_plane;

    [[nodiscard]] bool isValid() const noexcept
    {
        return viewport_id != 0 && size.isValid();
    }

    [[nodiscard]] bool hasRealtimeRenderPlane() const noexcept
    {
        return render_plane.active && render_plane.descriptor.isRealtime();
    }
};

struct EditorViewportCommand {
    EditorViewportCommandKind kind{EditorViewportCommandKind::CreateViewport};
    EditorViewportId viewport_id{0};
    std::string title{"Viewport"};
    EditorViewportKind viewport_kind{EditorViewportKind::Scene};
    EditorViewportSize size;
    EditorViewportCameraState camera;
    EditorViewportInteractionState interaction;
    EditorTransformTool transform_tool{EditorTransformTool::Translate};
    EditorTransformSpace transform_space{EditorTransformSpace::Local};
    RenderDebugViewMode debug_view_mode{RenderDebugViewMode::None};
    float debug_velocity_scale{20.0f};
    uint32_t pick_x{0};
    uint32_t pick_y{0};
};

struct EditorViewportCommandResult {
    bool accepted{false};
    bool changed{false};
    bool request_pick{false};
    bool request_capture{false};
};

[[nodiscard]] inline bool sameEditorViewportCameraState(const EditorViewportCameraState& lhs,
                                                        const EditorViewportCameraState& rhs) noexcept
{
    return lhs.projection_type == rhs.projection_type && lhs.position.x == rhs.position.x &&
           lhs.position.y == rhs.position.y && lhs.position.z == rhs.position.z &&
           lhs.orientation_euler_radians.x == rhs.orientation_euler_radians.x &&
           lhs.orientation_euler_radians.y == rhs.orientation_euler_radians.y &&
           lhs.orientation_euler_radians.z == rhs.orientation_euler_radians.z &&
           lhs.perspective_vertical_fov_radians == rhs.perspective_vertical_fov_radians &&
           lhs.perspective_near == rhs.perspective_near && lhs.perspective_far == rhs.perspective_far &&
           lhs.orthographic_size == rhs.orthographic_size && lhs.orthographic_near == rhs.orthographic_near &&
           lhs.orthographic_far == rhs.orthographic_far;
}

[[nodiscard]] inline bool sameEditorViewportInteractionState(const EditorViewportInteractionState& lhs,
                                                             const EditorViewportInteractionState& rhs) noexcept
{
    return lhs.visible == rhs.visible && lhs.focused == rhs.focused && lhs.hovered == rhs.hovered &&
           lhs.input_enabled == rhs.input_enabled && lhs.mouse_captured == rhs.mouse_captured &&
           lhs.runtime_viewport == rhs.runtime_viewport && lhs.pick_debug_enabled == rhs.pick_debug_enabled;
}

[[nodiscard]] inline EditorViewportCommandResult applyEditorViewportCommand(EditorViewportState& state,
                                                                             const EditorViewportCommand& command)
{
    EditorViewportCommandResult result;
    result.accepted = true;

    switch (command.kind) {
        case EditorViewportCommandKind::CreateViewport: {
            const EditorViewportId viewport_id = command.viewport_id != 0 ? command.viewport_id
                                                                          : (state.viewport_id != 0 ? state.viewport_id
                                                                                                    : EditorViewportId{1});
            const std::string title = command.title.empty() ? std::string("Viewport") : command.title;
            const bool changed = state.viewport_id != viewport_id || state.title != title ||
                                 state.kind != command.viewport_kind || !state.size.isValid() ||
                                 state.size.width != command.size.width || state.size.height != command.size.height ||
                                 !sameEditorViewportCameraState(state.camera, command.camera) ||
                                 !sameEditorViewportInteractionState(state.interaction, command.interaction) ||
                                 state.transform_tool != command.transform_tool ||
                                 state.transform_space != command.transform_space ||
                                 state.debug_view_mode != command.debug_view_mode ||
                                 state.debug_velocity_scale != command.debug_velocity_scale;

            state.viewport_id = viewport_id;
            state.title = std::move(title);
            state.kind = command.viewport_kind;
            state.size = command.size;
            state.camera = command.camera;
            state.interaction = command.interaction;
            state.transform_tool = command.transform_tool;
            state.transform_space = command.transform_space;
            state.debug_view_mode = command.debug_view_mode;
            state.debug_velocity_scale = command.debug_velocity_scale;
            if (!state.render_plane.descriptor.label.empty() && state.render_plane.descriptor.label != state.title) {
                state.render_plane.descriptor.label = state.title;
            }
            result.changed = changed;
            break;
        }
        case EditorViewportCommandKind::DestroyViewport:
            result.changed = state.viewport_id != 0 || state.size.isValid() || !state.title.empty() ||
                             state.kind != EditorViewportKind::Scene || !sameEditorViewportCameraState(state.camera,
                                                                                                         EditorViewportCameraState{}) ||
                             !sameEditorViewportInteractionState(state.interaction, EditorViewportInteractionState{}) ||
                             state.transform_tool != EditorTransformTool::Translate ||
                             state.transform_space != EditorTransformSpace::Local ||
                             state.debug_view_mode != RenderDebugViewMode::None ||
                             state.debug_velocity_scale != 20.0f || state.render_plane.active;
            state = EditorViewportState{};
            break;
        case EditorViewportCommandKind::ResizeViewport:
            result.changed = state.size.width != command.size.width || state.size.height != command.size.height;
            state.size = command.size;
            break;
        case EditorViewportCommandKind::SetCamera:
            result.changed = !sameEditorViewportCameraState(state.camera, command.camera);
            state.camera = command.camera;
            break;
        case EditorViewportCommandKind::SetInteractionState:
            result.changed = !sameEditorViewportInteractionState(state.interaction, command.interaction);
            state.interaction = command.interaction;
            break;
        case EditorViewportCommandKind::SetTransformState:
            result.changed = state.transform_tool != command.transform_tool ||
                             state.transform_space != command.transform_space;
            state.transform_tool = command.transform_tool;
            state.transform_space = command.transform_space;
            break;
        case EditorViewportCommandKind::SetDebugViewMode:
            result.changed = state.debug_view_mode != command.debug_view_mode;
            state.debug_view_mode = command.debug_view_mode;
            break;
        case EditorViewportCommandKind::SetDebugVelocityScale:
            result.changed = state.debug_velocity_scale != command.debug_velocity_scale;
            state.debug_velocity_scale = command.debug_velocity_scale;
            break;
        case EditorViewportCommandKind::RequestPick:
            result.request_pick = true;
            result.changed = false;
            break;
        case EditorViewportCommandKind::CaptureFrame:
            result.request_capture = true;
            result.changed = false;
            break;
    }

    return result;
}

} // namespace luna::editor
