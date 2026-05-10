#include "Protocol/RenderDataPlane.h"
#include "Protocol/ViewProtocol.h"
#include "Protocol/ViewProtocolJson.h"

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

class TestContext {
public:
    bool expect(bool condition, std::string_view message)
    {
        if (condition) {
            return true;
        }

        ++m_failures;
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }

    [[nodiscard]] int result() const noexcept
    {
        return m_failures == 0 ? 0 : 1;
    }

private:
    int m_failures{0};
};

bool sameVec3(const glm::vec3& lhs, const glm::vec3& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool sameCamera(const luna::editor::EditorViewportCameraState& lhs,
                const luna::editor::EditorViewportCameraState& rhs)
{
    return lhs.projection_type == rhs.projection_type && sameVec3(lhs.position, rhs.position) &&
           sameVec3(lhs.orientation_euler_radians, rhs.orientation_euler_radians) &&
           lhs.perspective_vertical_fov_radians == rhs.perspective_vertical_fov_radians &&
           lhs.perspective_near == rhs.perspective_near && lhs.perspective_far == rhs.perspective_far &&
           lhs.orthographic_size == rhs.orthographic_size && lhs.orthographic_near == rhs.orthographic_near &&
           lhs.orthographic_far == rhs.orthographic_far;
}

bool sameSize(const luna::editor::EditorViewportSize& lhs, const luna::editor::EditorViewportSize& rhs)
{
    return lhs.width == rhs.width && lhs.height == rhs.height;
}

bool sameRenderPlaneDescriptor(const luna::editor::EditorRenderPlaneDescriptor& lhs,
                               const luna::editor::EditorRenderPlaneDescriptor& rhs)
{
    return lhs.plane_id == rhs.plane_id && lhs.viewport_id == rhs.viewport_id && lhs.kind == rhs.kind &&
           lhs.transport == rhs.transport && lhs.format == rhs.format && lhs.width == rhs.width &&
           lhs.height == rhs.height && lhs.y_flip == rhs.y_flip && lhs.presentable == rhs.presentable &&
           lhs.generation == rhs.generation && lhs.binding_token == rhs.binding_token && lhs.label == rhs.label;
}

bool sameRenderFrame(const luna::editor::EditorRenderFrameInfo& lhs, const luna::editor::EditorRenderFrameInfo& rhs)
{
    return lhs.frame_id == rhs.frame_id && lhs.plane_id == rhs.plane_id && lhs.sequence == rhs.sequence &&
           lhs.timestamp_ns == rhs.timestamp_ns && lhs.width == rhs.width && lhs.height == rhs.height &&
           lhs.ready == rhs.ready;
}

bool sameRenderPlaneState(const luna::editor::EditorRenderPlaneState& lhs,
                          const luna::editor::EditorRenderPlaneState& rhs)
{
    return lhs.active == rhs.active && sameRenderPlaneDescriptor(lhs.descriptor, rhs.descriptor) &&
           sameRenderFrame(lhs.frame, rhs.frame);
}

bool sameViewportState(const luna::editor::EditorViewportState& lhs, const luna::editor::EditorViewportState& rhs)
{
    return lhs.viewport_id == rhs.viewport_id && lhs.title == rhs.title && lhs.kind == rhs.kind &&
           sameSize(lhs.size, rhs.size) && sameCamera(lhs.camera, rhs.camera) &&
           luna::editor::sameEditorViewportInteractionState(lhs.interaction, rhs.interaction) &&
           lhs.transform_tool == rhs.transform_tool && lhs.transform_space == rhs.transform_space &&
           lhs.debug_view_mode == rhs.debug_view_mode && lhs.debug_velocity_scale == rhs.debug_velocity_scale &&
           sameRenderPlaneState(lhs.render_plane, rhs.render_plane);
}

bool sameViewportCommand(const luna::editor::EditorViewportCommand& lhs,
                         const luna::editor::EditorViewportCommand& rhs)
{
    return lhs.kind == rhs.kind && lhs.viewport_id == rhs.viewport_id && lhs.title == rhs.title &&
           lhs.viewport_kind == rhs.viewport_kind && sameSize(lhs.size, rhs.size) &&
           sameCamera(lhs.camera, rhs.camera) &&
           luna::editor::sameEditorViewportInteractionState(lhs.interaction, rhs.interaction) &&
           lhs.transform_tool == rhs.transform_tool && lhs.transform_space == rhs.transform_space &&
           lhs.debug_view_mode == rhs.debug_view_mode && lhs.debug_velocity_scale == rhs.debug_velocity_scale &&
           lhs.pick_x == rhs.pick_x && lhs.pick_y == rhs.pick_y;
}

std::filesystem::path editorSchemaPath(std::string_view filename)
{
    return std::filesystem::path{LUNA_EDITOR_SCHEMA_DIR} / std::string(filename);
}

std::set<std::string> schemaEnumValues(const YAML::Node& root, std::string_view def_name)
{
    std::set<std::string> values;
    const YAML::Node enum_node = root["$defs"][std::string(def_name)]["enum"];
    if (!enum_node.IsSequence()) {
        return values;
    }

    for (const YAML::Node value : enum_node) {
        if (value.IsScalar()) {
            values.insert(value.as<std::string>());
        }
    }
    return values;
}

std::vector<std::string> schemaViewportCommandKindOrder(const YAML::Node& root)
{
    std::vector<std::string> names;
    const YAML::Node command_refs = root["$defs"]["viewportCommand"]["oneOf"];
    if (!command_refs.IsSequence()) {
        return names;
    }

    for (const YAML::Node ref : command_refs) {
        const std::string ref_path = ref["$ref"].as<std::string>();
        const size_t def_begin = ref_path.rfind('/');
        if (def_begin == std::string::npos || def_begin + 1 >= ref_path.size()) {
            continue;
        }

        const std::string def_name = ref_path.substr(def_begin + 1);
        const YAML::Node kind = root["$defs"][def_name]["properties"]["kind"]["const"];
        if (kind.IsScalar()) {
            names.push_back(kind.as<std::string>());
        }
    }
    return names;
}

std::set<std::string> stringSet(const std::vector<std::string>& values)
{
    return {values.begin(), values.end()};
}

std::vector<std::string> expectedViewportCommandNames()
{
    using luna::editor::EditorViewportCommandKind;
    return {
        std::string(luna::editor::editorViewportCommandName(EditorViewportCommandKind::CreateViewport)),
        std::string(luna::editor::editorViewportCommandName(EditorViewportCommandKind::DestroyViewport)),
        std::string(luna::editor::editorViewportCommandName(EditorViewportCommandKind::ResizeViewport)),
        std::string(luna::editor::editorViewportCommandName(EditorViewportCommandKind::SetCamera)),
        std::string(luna::editor::editorViewportCommandName(EditorViewportCommandKind::SetInteractionState)),
        std::string(luna::editor::editorViewportCommandName(EditorViewportCommandKind::SetTransformState)),
        std::string(luna::editor::editorViewportCommandName(EditorViewportCommandKind::SetDebugViewMode)),
        std::string(luna::editor::editorViewportCommandName(EditorViewportCommandKind::SetDebugVelocityScale)),
        std::string(luna::editor::editorViewportCommandName(EditorViewportCommandKind::RequestPick)),
        std::string(luna::editor::editorViewportCommandName(EditorViewportCommandKind::CaptureFrame)),
    };
}

std::set<std::string> expectedViewportKinds()
{
    using luna::editor::EditorViewportKind;
    return {
        std::string(luna::editor::editorViewportKindName(EditorViewportKind::Scene)),
        std::string(luna::editor::editorViewportKindName(EditorViewportKind::Debug)),
        std::string(luna::editor::editorViewportKindName(EditorViewportKind::Preview)),
        std::string(luna::editor::editorViewportKindName(EditorViewportKind::Custom)),
    };
}

std::set<std::string> expectedTransformTools()
{
    using luna::editor::EditorTransformTool;
    return {
        std::string(luna::editor::editorTransformToolName(EditorTransformTool::Translate)),
        std::string(luna::editor::editorTransformToolName(EditorTransformTool::Rotate)),
        std::string(luna::editor::editorTransformToolName(EditorTransformTool::Scale)),
    };
}

std::set<std::string> expectedTransformSpaces()
{
    using luna::editor::EditorTransformSpace;
    return {
        std::string(luna::editor::editorTransformSpaceName(EditorTransformSpace::Local)),
        std::string(luna::editor::editorTransformSpaceName(EditorTransformSpace::World)),
    };
}

std::set<std::string> expectedRenderPlaneKinds()
{
    using luna::editor::EditorRenderPlaneKind;
    return {
        std::string(luna::editor::editorRenderPlaneKindName(EditorRenderPlaneKind::SceneViewport)),
        std::string(luna::editor::editorRenderPlaneKindName(EditorRenderPlaneKind::DebugViewport)),
        std::string(luna::editor::editorRenderPlaneKindName(EditorRenderPlaneKind::Preview)),
        std::string(luna::editor::editorRenderPlaneKindName(EditorRenderPlaneKind::Capture)),
    };
}

std::set<std::string> expectedRenderTransportKinds()
{
    using luna::editor::EditorRenderTransportKind;
    return {
        std::string(luna::editor::editorRenderTransportKindName(EditorRenderTransportKind::None)),
        std::string(luna::editor::editorRenderTransportKindName(EditorRenderTransportKind::NativeSurface)),
        std::string(luna::editor::editorRenderTransportKindName(EditorRenderTransportKind::SharedTexture)),
        std::string(luna::editor::editorRenderTransportKindName(EditorRenderTransportKind::CpuImage)),
    };
}

std::set<std::string> expectedDebugViewModes()
{
    return {
        std::string(luna::editor::editorRenderDebugViewModeName(luna::RenderDebugViewMode::None)),
        std::string(luna::editor::editorRenderDebugViewModeName(luna::RenderDebugViewMode::Velocity)),
        std::string(luna::editor::editorRenderDebugViewModeName(luna::RenderDebugViewMode::HistoryValidity)),
        std::string(luna::editor::editorRenderDebugViewModeName(luna::RenderDebugViewMode::ShadowCascades)),
        std::string(luna::editor::editorRenderDebugViewModeName(luna::RenderDebugViewMode::BaseColor)),
        std::string(luna::editor::editorRenderDebugViewModeName(luna::RenderDebugViewMode::Normal)),
        std::string(luna::editor::editorRenderDebugViewModeName(luna::RenderDebugViewMode::Metallic)),
        std::string(luna::editor::editorRenderDebugViewModeName(luna::RenderDebugViewMode::Roughness)),
        std::string(luna::editor::editorRenderDebugViewModeName(luna::RenderDebugViewMode::DirectLighting)),
        std::string(luna::editor::editorRenderDebugViewModeName(luna::RenderDebugViewMode::SpecularIbl)),
        std::string(luna::editor::editorRenderDebugViewModeName(luna::RenderDebugViewMode::BloomInput)),
        std::string(luna::editor::editorRenderDebugViewModeName(luna::RenderDebugViewMode::BloomPrefilter)),
        std::string(luna::editor::editorRenderDebugViewModeName(luna::RenderDebugViewMode::BloomMip0)),
        std::string(luna::editor::editorRenderDebugViewModeName(luna::RenderDebugViewMode::BloomMip1)),
        std::string(luna::editor::editorRenderDebugViewModeName(luna::RenderDebugViewMode::BloomMip2)),
        std::string(luna::editor::editorRenderDebugViewModeName(luna::RenderDebugViewMode::BloomMip3)),
        std::string(luna::editor::editorRenderDebugViewModeName(luna::RenderDebugViewMode::BloomMip4)),
        std::string(luna::editor::editorRenderDebugViewModeName(luna::RenderDebugViewMode::BloomMip5)),
        std::string(luna::editor::editorRenderDebugViewModeName(luna::RenderDebugViewMode::BloomComposite)),
    };
}

void testProtocolInfo(TestContext& context)
{
    context.expect(luna::editor::kEditorProtocolName == "luna.editor", "editor protocol should have a stable name");
    context.expect(luna::editor::kEditorProtocolVersion == 1, "editor protocol should start at version 1");
    context.expect(luna::editor::isEditorProtocolCompatible({}), "default protocol info should be compatible");

    luna::editor::EditorProtocolInfo info;
    info.name = "luna.editor";
    info.version = 1;
    context.expect(luna::editor::isEditorProtocolCompatible(info), "matching protocol info should be compatible");

    info.version = 2;
    context.expect(!luna::editor::isEditorProtocolCompatible(info), "version mismatch should be incompatible");
}

void testViewProtocolSchemaContract(TestContext& context)
{
    YAML::Node root;
    try {
        root = YAML::LoadFile(editorSchemaPath("editor-view-protocol.schema.json").string());
    } catch (const YAML::Exception& error) {
        context.expect(false, std::string("editor view protocol schema should parse: ") + error.what());
        return;
    }

    context.expect(root.IsMap(), "editor view protocol schema should parse as an object");
    context.expect(root["$schema"].as<std::string>() == "https://json-schema.org/draft/2020-12/schema",
                   "editor view protocol schema should declare draft 2020-12");
    context.expect(root["$id"].as<std::string>() == "https://luna.local/schemas/editor-view-protocol.schema.json",
                   "editor view protocol schema should declare a stable schema id");
    context.expect(root["oneOf"].IsSequence() && root["oneOf"].size() == 3,
                   "editor view protocol schema should expose command, state, and response envelopes");
    context.expect(root["$defs"]["protocol"]["properties"]["name"]["const"].as<std::string>() ==
                       std::string(luna::editor::kEditorProtocolName),
                   "editor view protocol schema should declare the C++ protocol name");
    context.expect(root["$defs"]["protocol"]["properties"]["version"]["const"].as<uint32_t>() ==
                       luna::editor::kEditorProtocolVersion,
                   "editor view protocol schema should declare the C++ protocol version");

    context.expect(schemaEnumValues(root, "viewportKind") == expectedViewportKinds(),
                   "viewport kind schema enum should match C++ names");
    context.expect(schemaEnumValues(root, "transformTool") == expectedTransformTools(),
                   "transform tool schema enum should match C++ names");
    context.expect(schemaEnumValues(root, "transformSpace") == expectedTransformSpaces(),
                   "transform space schema enum should match C++ names");
    context.expect(schemaEnumValues(root, "renderPlaneKind") == expectedRenderPlaneKinds(),
                   "render plane kind schema enum should match C++ names");
    context.expect(schemaEnumValues(root, "renderTransportKind") == expectedRenderTransportKinds(),
                   "render transport schema enum should match C++ names");
    context.expect(schemaEnumValues(root, "debugViewMode") == expectedDebugViewModes(),
                   "debug view mode schema enum should match C++ names");

    const std::vector<std::string> command_names = schemaViewportCommandKindOrder(root);
    const std::vector<std::string> expected_commands = expectedViewportCommandNames();
    context.expect(command_names == expected_commands,
                   "viewport command schema order should match C++ command names");
    context.expect(stringSet(command_names).size() == expected_commands.size(),
                   "viewport command schema should not contain duplicate command kinds");
}

void testViewportCameraRoundTrip(TestContext& context)
{
    luna::Camera camera;
    camera.setPerspective(glm::radians(60.0f), 0.1f, 1000.0f);
    camera.setPosition({3.0f, 4.0f, 5.0f});
    camera.setOrientationEuler(glm::radians(glm::vec3{10.0f, 20.0f, 30.0f}));

    const luna::editor::EditorViewportCameraState state =
        luna::editor::EditorViewportCameraState::fromCamera(camera);
    const luna::Camera restored = state.toCamera();

    context.expect(restored.getProjectionType() == camera.getProjectionType(),
                   "camera projection type should round-trip");
    context.expect(sameVec3(restored.getPosition(), camera.getPosition()), "camera position should round-trip");
    context.expect(sameVec3(restored.getOrientationEuler(), camera.getOrientationEuler()),
                   "camera orientation should round-trip");
    context.expect(restored.getPerspectiveSettings().vertical_fov_radians ==
                       camera.getPerspectiveSettings().vertical_fov_radians,
                   "perspective fov should round-trip");
    context.expect(restored.getPerspectiveSettings().near_clip == camera.getPerspectiveSettings().near_clip,
                   "perspective near plane should round-trip");
    context.expect(restored.getPerspectiveSettings().far_clip == camera.getPerspectiveSettings().far_clip,
                   "perspective far plane should round-trip");
}

void testRenderDataPlane(TestContext& context)
{
    using luna::editor::EditorRenderPlaneKind;
    using luna::editor::EditorRenderTransportKind;

    context.expect(luna::editor::editorRenderPlaneKindName(EditorRenderPlaneKind::SceneViewport) == "sceneViewport",
                   "scene viewport plane should have a stable name");
    context.expect(luna::editor::editorRenderTransportKindName(EditorRenderTransportKind::NativeSurface) ==
                       "nativeSurface",
                   "native surface transport should have a stable name");
    context.expect(luna::editor::editorRenderTransportIsRealtime(EditorRenderTransportKind::NativeSurface),
                   "native surface should be considered realtime");
    context.expect(luna::editor::editorRenderTransportIsRealtime(EditorRenderTransportKind::SharedTexture),
                   "shared texture should be considered realtime");
    context.expect(!luna::editor::editorRenderTransportIsRealtime(EditorRenderTransportKind::CpuImage),
                   "cpu image transport should not be considered realtime");

    luna::editor::EditorRenderPlaneDescriptor descriptor;
    context.expect(!descriptor.isValid(), "default render plane descriptor should be invalid");
    descriptor.plane_id = 1;
    descriptor.viewport_id = 2;
    descriptor.kind = EditorRenderPlaneKind::SceneViewport;
    descriptor.transport = EditorRenderTransportKind::SharedTexture;
    descriptor.format = luna::RHI::Format::RGBA8_UNORM;
    descriptor.width = 1280;
    descriptor.height = 720;
    descriptor.presentable = true;
    descriptor.binding_token = "shared-texture-0";
    context.expect(descriptor.isValid(), "configured render plane descriptor should be valid");
    context.expect(descriptor.isRealtime(), "shared texture descriptor should be realtime");

    luna::editor::EditorRenderFrameInfo frame;
    context.expect(!frame.isValid(), "default render plane frame should be invalid");
    frame.frame_id = 7;
    frame.plane_id = 1;
    frame.width = 1280;
    frame.height = 720;
    frame.ready = true;
    context.expect(frame.isValid(), "configured render plane frame should be valid");

    luna::editor::EditorRenderPlaneState state;
    state.active = true;
    state.descriptor = descriptor;
    state.frame = frame;
    context.expect(state.canPresent(), "active presentable plane should be presentable");

    luna::editor::EditorRenderPlaneState runtime_state;
    context.expect(!runtime_state.canPresent(), "default render plane state should not be presentable");
    context.expect(luna::editor::bindEditorRenderPlane(runtime_state, descriptor),
                   "binding the render plane should report a change");
    context.expect(runtime_state.active, "bound render plane should be active");
    context.expect(runtime_state.descriptor.generation == 1, "first binding should bump the generation");
    context.expect(runtime_state.descriptor.binding_token == "shared-texture-0",
                   "binding token should be preserved on bind");

    const uint64_t timestamp_ns = 987654321;
    luna::editor::presentEditorRenderPlaneFrame(runtime_state, 7, 11, timestamp_ns);
    context.expect(runtime_state.frame.frame_id == 7 && runtime_state.frame.sequence == 11 &&
                       runtime_state.frame.timestamp_ns == timestamp_ns,
                   "present should stamp frame metadata");
    context.expect(runtime_state.frame.ready, "presented shared texture should be ready");
    context.expect(runtime_state.canPresent(), "presented render plane should remain presentable");

    context.expect(!luna::editor::bindEditorRenderPlane(runtime_state, descriptor),
                   "rebinding the same descriptor should be stable");
    context.expect(runtime_state.descriptor.generation == 1, "stable rebind should keep the generation");

    luna::editor::EditorRenderPlaneDescriptor resized_descriptor = descriptor;
    resized_descriptor.width = 1920;
    resized_descriptor.height = 1080;
    resized_descriptor.label = "Scene Viewport";
    context.expect(luna::editor::bindEditorRenderPlane(runtime_state, resized_descriptor),
                   "resizing the render plane should report a change");
    context.expect(runtime_state.descriptor.generation == 2, "resizing should bump the generation");
    context.expect(runtime_state.descriptor.width == 1920 && runtime_state.descriptor.height == 1080,
                   "resizing should update the descriptor extent");

    context.expect(luna::editor::releaseEditorRenderPlane(runtime_state),
                   "releasing the render plane should report a change");
    context.expect(!runtime_state.active, "released render plane should be inactive");
    context.expect(!runtime_state.canPresent(), "released render plane should not be presentable");
    context.expect(runtime_state.descriptor.transport == EditorRenderTransportKind::None,
                   "released render plane should clear transport");
    context.expect(runtime_state.descriptor.width == 0 && runtime_state.descriptor.height == 0,
                   "released render plane should clear extent");
    context.expect(luna::editor::bindEditorRenderPlane(runtime_state, descriptor),
                   "rebinding after release should still work");
    context.expect(runtime_state.descriptor.generation == 4, "rebind after release should advance generation");
}

void testViewportCommands(TestContext& context)
{
    using luna::editor::EditorViewportCommand;
    using luna::editor::EditorViewportCommandEffect;
    using luna::editor::EditorViewportCommandKind;

    context.expect(luna::editor::editorViewportCommandName(EditorViewportCommandKind::CreateViewport) ==
                       "createViewport",
                   "create viewport command should have a stable name");
    context.expect(luna::editor::editorViewportCommandName(EditorViewportCommandKind::CaptureFrame) ==
                       "captureFrame",
                   "capture frame command should have a stable name");
    context.expect(luna::editor::editorViewportCommandMutatesState(EditorViewportCommandKind::ResizeViewport),
                   "resize viewport should mutate state");
    context.expect(luna::editor::hasEditorViewportCommandEffect(
                       luna::editor::editorViewportCommandEffects(EditorViewportCommandKind::RequestPick),
                       EditorViewportCommandEffect::RequestsPick),
                   "request pick should advertise pick effect");
    context.expect(luna::editor::editorRenderDebugViewModeName(luna::RenderDebugViewMode::BloomComposite) ==
                       "bloomComposite",
                   "debug view mode should have a stable protocol name");

    luna::editor::EditorViewportState state;
    EditorViewportCommand create_command;
    create_command.kind = EditorViewportCommandKind::CreateViewport;
    create_command.viewport_id = 7;
    create_command.title = "Scene Viewport";
    create_command.viewport_kind = luna::editor::EditorViewportKind::Scene;
    create_command.size = {1920, 1080};
    create_command.camera.position = {3.0f, 4.0f, 5.0f};
    create_command.camera.orientation_euler_radians = glm::radians(glm::vec3{10.0f, 20.0f, 30.0f});
    create_command.interaction.visible = true;
    create_command.interaction.focused = true;
    create_command.interaction.hovered = true;
    create_command.interaction.input_enabled = true;
    create_command.interaction.mouse_captured = false;
    create_command.interaction.runtime_viewport = false;
    create_command.interaction.pick_debug_enabled = true;
    create_command.transform_tool = luna::editor::EditorTransformTool::Rotate;
    create_command.transform_space = luna::editor::EditorTransformSpace::World;
    create_command.debug_view_mode = luna::RenderDebugViewMode::Normal;
    create_command.debug_velocity_scale = 24.0f;

    const luna::editor::EditorViewportCommandResult create_result = luna::editor::applyEditorViewportCommand(
        state, create_command);
    context.expect(create_result.accepted, "create viewport command should be accepted");
    context.expect(create_result.changed, "create viewport command should change state");
    context.expect(state.isValid(), "created viewport state should be valid");
    context.expect(state.viewport_id == 7, "created viewport should preserve its id");
    context.expect(state.title == "Scene Viewport", "created viewport should preserve its title");
    context.expect(state.kind == luna::editor::EditorViewportKind::Scene, "created viewport kind should be scene");
    context.expect(state.size.width == 1920 && state.size.height == 1080, "created viewport should carry size");
    context.expect(sameCamera(state.camera, create_command.camera), "created viewport should carry camera state");
    context.expect(state.interaction.focused && state.interaction.hovered && state.interaction.input_enabled,
                   "created viewport should carry interaction flags");
    context.expect(state.transform_tool == luna::editor::EditorTransformTool::Rotate,
                   "created viewport should carry transform tool");
    context.expect(state.transform_space == luna::editor::EditorTransformSpace::World,
                   "created viewport should carry transform space");
    context.expect(state.debug_view_mode == luna::RenderDebugViewMode::Normal,
                   "created viewport should carry debug view mode");
    context.expect(state.debug_velocity_scale == 24.0f, "created viewport should carry debug velocity scale");

    EditorViewportCommand resize_command;
    resize_command.kind = EditorViewportCommandKind::ResizeViewport;
    resize_command.size = {2560, 1440};
    const luna::editor::EditorViewportCommandResult resize_result =
        luna::editor::applyEditorViewportCommand(state, resize_command);
    context.expect(resize_result.accepted, "resize viewport command should be accepted");
    context.expect(resize_result.changed, "resize viewport command should change state");
    context.expect(state.size.width == 2560 && state.size.height == 1440, "resize command should update size");

    EditorViewportCommand camera_command;
    camera_command.kind = EditorViewportCommandKind::SetCamera;
    camera_command.camera.projection_type = luna::Camera::ProjectionType::Orthographic;
    camera_command.camera.position = {1.0f, 2.0f, 3.0f};
    camera_command.camera.orientation_euler_radians = glm::radians(glm::vec3{0.0f, 90.0f, 0.0f});
    camera_command.camera.orthographic_size = 32.0f;
    const luna::editor::EditorViewportCommandResult camera_result =
        luna::editor::applyEditorViewportCommand(state, camera_command);
    context.expect(camera_result.changed, "camera command should change state");
    context.expect(sameCamera(state.camera, camera_command.camera), "camera command should replace camera state");

    EditorViewportCommand interaction_command;
    interaction_command.kind = EditorViewportCommandKind::SetInteractionState;
    interaction_command.interaction.visible = false;
    interaction_command.interaction.focused = false;
    interaction_command.interaction.hovered = false;
    interaction_command.interaction.input_enabled = false;
    interaction_command.interaction.mouse_captured = true;
    interaction_command.interaction.runtime_viewport = true;
    interaction_command.interaction.pick_debug_enabled = false;
    const luna::editor::EditorViewportCommandResult interaction_result =
        luna::editor::applyEditorViewportCommand(state, interaction_command);
    context.expect(interaction_result.changed, "interaction command should change state");
    context.expect(!state.interaction.visible && state.interaction.mouse_captured && state.interaction.runtime_viewport,
                   "interaction command should update interaction state");

    EditorViewportCommand tool_command;
    tool_command.kind = EditorViewportCommandKind::SetTransformState;
    tool_command.transform_tool = luna::editor::EditorTransformTool::Scale;
    tool_command.transform_space = luna::editor::EditorTransformSpace::Local;
    const luna::editor::EditorViewportCommandResult tool_result =
        luna::editor::applyEditorViewportCommand(state, tool_command);
    context.expect(tool_result.changed, "transform command should change state");
    context.expect(state.transform_tool == luna::editor::EditorTransformTool::Scale &&
                       state.transform_space == luna::editor::EditorTransformSpace::Local,
                   "transform command should update tool and space");

    EditorViewportCommand debug_command;
    debug_command.kind = EditorViewportCommandKind::SetDebugViewMode;
    debug_command.debug_view_mode = luna::RenderDebugViewMode::BloomComposite;
    const luna::editor::EditorViewportCommandResult debug_result =
        luna::editor::applyEditorViewportCommand(state, debug_command);
    context.expect(debug_result.changed, "debug command should change state");
    context.expect(state.debug_view_mode == luna::RenderDebugViewMode::BloomComposite,
                   "debug command should update debug view mode");

    EditorViewportCommand velocity_command;
    velocity_command.kind = EditorViewportCommandKind::SetDebugVelocityScale;
    velocity_command.debug_velocity_scale = 42.0f;
    const luna::editor::EditorViewportCommandResult velocity_result =
        luna::editor::applyEditorViewportCommand(state, velocity_command);
    context.expect(velocity_result.changed, "debug velocity command should change state");
    context.expect(state.debug_velocity_scale == 42.0f, "debug velocity command should update scale");

    EditorViewportCommand pick_command;
    pick_command.kind = EditorViewportCommandKind::RequestPick;
    pick_command.pick_x = 123;
    pick_command.pick_y = 456;
    const luna::editor::EditorViewportCommandResult pick_result =
        luna::editor::applyEditorViewportCommand(state, pick_command);
    context.expect(pick_result.request_pick, "pick command should request a pick");
    context.expect(!pick_result.request_capture, "pick command should not request capture");

    EditorViewportCommand capture_command;
    capture_command.kind = EditorViewportCommandKind::CaptureFrame;
    const luna::editor::EditorViewportCommandResult capture_result =
        luna::editor::applyEditorViewportCommand(state, capture_command);
    context.expect(capture_result.request_capture, "capture command should request capture");

    EditorViewportCommand destroy_command;
    destroy_command.kind = EditorViewportCommandKind::DestroyViewport;
    const luna::editor::EditorViewportCommandResult destroy_result =
        luna::editor::applyEditorViewportCommand(state, destroy_command);
    context.expect(destroy_result.changed, "destroy command should change state");
    context.expect(!state.isValid(), "destroy command should invalidate the viewport state");
}

void testViewportState(TestContext& context)
{
    luna::editor::EditorViewportState viewport_state;
    context.expect(!viewport_state.isValid(), "default viewport state should be incomplete");
    viewport_state.viewport_id = 42;
    viewport_state.title = "Scene Viewport";
    viewport_state.kind = luna::editor::EditorViewportKind::Scene;
    viewport_state.size = {1920, 1080};
    viewport_state.interaction.focused = true;
    viewport_state.interaction.hovered = true;
    viewport_state.interaction.input_enabled = true;
    viewport_state.render_plane.descriptor.plane_id = 7;
    viewport_state.render_plane.descriptor.viewport_id = 42;
    viewport_state.render_plane.descriptor.transport = luna::editor::EditorRenderTransportKind::NativeSurface;
    viewport_state.render_plane.descriptor.format = luna::RHI::Format::RGBA8_UNORM;
    viewport_state.render_plane.descriptor.width = 1920;
    viewport_state.render_plane.descriptor.height = 1080;
    viewport_state.render_plane.descriptor.presentable = true;
    viewport_state.render_plane.active = true;
    viewport_state.render_plane.frame.frame_id = 1;
    viewport_state.render_plane.frame.plane_id = 7;
    viewport_state.render_plane.frame.width = 1920;
    viewport_state.render_plane.frame.height = 1080;
    viewport_state.render_plane.frame.ready = true;
    context.expect(viewport_state.isValid(), "configured viewport state should be valid");
    context.expect(viewport_state.hasRealtimeRenderPlane(), "scene viewport should expose a realtime render plane");
    context.expect(viewport_state.interaction.focused && viewport_state.interaction.hovered &&
                       viewport_state.interaction.input_enabled,
                   "interaction state should preserve focus and input flags");
}

void testViewportJsonWire(TestContext& context)
{
    using luna::editor::EditorViewportCommand;
    using luna::editor::EditorViewportCommandKind;

    EditorViewportCommand create_command;
    create_command.kind = EditorViewportCommandKind::CreateViewport;
    create_command.viewport_id = 9;
    create_command.title = "Viewport Wire";
    create_command.viewport_kind = luna::editor::EditorViewportKind::Debug;
    create_command.size = {1024, 768};
    create_command.camera.projection_type = luna::Camera::ProjectionType::Orthographic;
    create_command.camera.position = {2.0f, 3.0f, 4.0f};
    create_command.camera.orientation_euler_radians = glm::radians(glm::vec3{15.0f, 25.0f, 35.0f});
    create_command.camera.orthographic_size = 18.0f;
    create_command.interaction.visible = true;
    create_command.interaction.focused = true;
    create_command.interaction.hovered = true;
    create_command.interaction.input_enabled = true;
    create_command.interaction.mouse_captured = false;
    create_command.interaction.runtime_viewport = false;
    create_command.interaction.pick_debug_enabled = true;
    create_command.transform_tool = luna::editor::EditorTransformTool::Rotate;
    create_command.transform_space = luna::editor::EditorTransformSpace::World;
    create_command.debug_view_mode = luna::RenderDebugViewMode::Normal;
    create_command.debug_velocity_scale = 12.5f;

    const luna::editor::Json command_json = luna::editor::editorViewportCommandJson(create_command);
    luna::editor::EditorViewportCommand parsed_command;
    std::vector<std::string> errors;
    context.expect(luna::editor::editorViewportCommandFromJson(command_json, parsed_command, &errors),
                   "viewport command JSON should parse");
    context.expect(errors.empty(), "viewport command JSON parse should not report errors");
    context.expect(sameViewportCommand(create_command, parsed_command), "viewport command should round-trip");

    const luna::editor::Json command_envelope = luna::editor::editorViewportCommandEnvelopeJson(create_command);
    luna::editor::EditorViewportCommand envelope_command;
    errors.clear();
    context.expect(luna::editor::editorViewportCommandEnvelopeFromJson(command_envelope, envelope_command, &errors),
                   "viewport command envelope should parse");
    context.expect(errors.empty(), "viewport command envelope parse should not report errors");
    context.expect(sameViewportCommand(create_command, envelope_command), "viewport command envelope should round-trip");

    luna::editor::EditorViewportState viewport_state;
    viewport_state.viewport_id = 42;
    viewport_state.title = "Scene Viewport";
    viewport_state.kind = luna::editor::EditorViewportKind::Scene;
    viewport_state.size = {1920, 1080};
    viewport_state.camera = create_command.camera;
    viewport_state.interaction = create_command.interaction;
    viewport_state.transform_tool = create_command.transform_tool;
    viewport_state.transform_space = create_command.transform_space;
    viewport_state.debug_view_mode = create_command.debug_view_mode;
    viewport_state.debug_velocity_scale = create_command.debug_velocity_scale;
    viewport_state.render_plane.descriptor.plane_id = 7;
    viewport_state.render_plane.descriptor.viewport_id = 42;
    viewport_state.render_plane.descriptor.kind = luna::editor::EditorRenderPlaneKind::SceneViewport;
    viewport_state.render_plane.descriptor.transport = luna::editor::EditorRenderTransportKind::SharedTexture;
    viewport_state.render_plane.descriptor.format = luna::RHI::Format::RGBA8_UNORM;
    viewport_state.render_plane.descriptor.width = 1920;
    viewport_state.render_plane.descriptor.height = 1080;
    viewport_state.render_plane.descriptor.y_flip = false;
    viewport_state.render_plane.descriptor.presentable = true;
    viewport_state.render_plane.descriptor.generation = 3;
    viewport_state.render_plane.descriptor.binding_token = "renderer.scene_output.texture";
    viewport_state.render_plane.descriptor.label = "Scene Viewport";
    viewport_state.render_plane.frame.frame_id = 88;
    viewport_state.render_plane.frame.plane_id = 7;
    viewport_state.render_plane.frame.sequence = 91;
    viewport_state.render_plane.frame.timestamp_ns = 123456789;
    viewport_state.render_plane.frame.width = 1920;
    viewport_state.render_plane.frame.height = 1080;
    viewport_state.render_plane.frame.ready = true;
    viewport_state.render_plane.active = true;

    const luna::editor::Json state_json = luna::editor::editorViewportStateJson(viewport_state);
    luna::editor::EditorViewportState parsed_state;
    errors.clear();
    context.expect(luna::editor::editorViewportStateFromJson(state_json, parsed_state, &errors),
                   "viewport state JSON should parse");
    context.expect(errors.empty(), "viewport state JSON parse should not report errors");
    context.expect(sameViewportState(viewport_state, parsed_state), "viewport state should round-trip");

    const luna::editor::Json state_envelope = luna::editor::editorViewportStateEnvelopeJson(viewport_state);
    luna::editor::EditorViewportState envelope_state;
    errors.clear();
    context.expect(luna::editor::editorViewportStateEnvelopeFromJson(state_envelope, envelope_state, &errors),
                   "viewport state envelope should parse");
    context.expect(errors.empty(), "viewport state envelope parse should not report errors");
    context.expect(sameViewportState(viewport_state, envelope_state), "viewport state envelope should round-trip");

    luna::editor::EditorViewportState applied_state;
    luna::editor::Json response_json;
    errors.clear();
    context.expect(luna::editor::applyEditorViewportCommandEnvelopeJson(applied_state, command_envelope, response_json, &errors),
                   "viewport command envelope should apply");
    context.expect(errors.empty(), "viewport command application should not report errors");

    luna::editor::EditorViewportState response_state;
    luna::editor::EditorViewportCommandResult response_result;
    errors.clear();
    context.expect(luna::editor::editorViewportResponseEnvelopeFromJson(response_json,
                                                                        response_state,
                                                                        response_result,
                                                                        &errors),
                   "viewport response envelope should parse");
    context.expect(errors.empty(), "viewport response envelope parse should not report errors");
    context.expect(response_result.accepted, "viewport response should accept the command");
    context.expect(response_result.changed, "viewport response should report state changes");
    context.expect(sameViewportState(applied_state, response_state), "viewport response should carry the updated state");

    EditorViewportCommand pick_command;
    pick_command.kind = EditorViewportCommandKind::RequestPick;
    pick_command.viewport_id = viewport_state.viewport_id;
    pick_command.pick_x = 111;
    pick_command.pick_y = 222;
    const luna::editor::Json pick_envelope = luna::editor::editorViewportCommandEnvelopeJson(pick_command);
    luna::editor::EditorViewportCommand parsed_pick_command;
    errors.clear();
    context.expect(luna::editor::editorViewportCommandEnvelopeFromJson(pick_envelope, parsed_pick_command, &errors),
                   "request-pick envelope should parse");
    context.expect(errors.empty(), "request-pick envelope parse should not report errors");
    context.expect(sameViewportCommand(pick_command, parsed_pick_command), "request-pick command should round-trip");

    luna::editor::Json pick_response_json;
    errors.clear();
    context.expect(luna::editor::applyEditorViewportCommandEnvelopeJson(applied_state,
                                                                        pick_envelope,
                                                                        pick_response_json,
                                                                        &errors),
                   "request-pick envelope should apply");
    context.expect(errors.empty(), "request-pick application should not report errors");
    luna::editor::EditorViewportState pick_response_state;
    luna::editor::EditorViewportCommandResult pick_response_result;
    errors.clear();
    context.expect(luna::editor::editorViewportResponseEnvelopeFromJson(pick_response_json,
                                                                        pick_response_state,
                                                                        pick_response_result,
                                                                        &errors),
                   "request-pick response should parse");
    context.expect(errors.empty(), "request-pick response parse should not report errors");
    context.expect(pick_response_result.request_pick && !pick_response_result.changed &&
                       !pick_response_result.request_capture,
                   "request-pick should only request a pick");
    context.expect(sameViewportState(applied_state, pick_response_state),
                   "request-pick should not mutate viewport state");

    EditorViewportCommand capture_command;
    capture_command.kind = EditorViewportCommandKind::CaptureFrame;
    capture_command.viewport_id = viewport_state.viewport_id;
    const luna::editor::Json capture_envelope = luna::editor::editorViewportCommandEnvelopeJson(capture_command);
    luna::editor::EditorViewportCommand parsed_capture_command;
    errors.clear();
    context.expect(luna::editor::editorViewportCommandEnvelopeFromJson(capture_envelope,
                                                                       parsed_capture_command,
                                                                       &errors),
                   "capture envelope should parse");
    context.expect(errors.empty(), "capture envelope parse should not report errors");
    context.expect(sameViewportCommand(capture_command, parsed_capture_command), "capture command should round-trip");

    luna::editor::Json capture_response_json;
    errors.clear();
    context.expect(luna::editor::applyEditorViewportCommandEnvelopeJson(applied_state,
                                                                        capture_envelope,
                                                                        capture_response_json,
                                                                        &errors),
                   "capture envelope should apply");
    context.expect(errors.empty(), "capture application should not report errors");
    luna::editor::EditorViewportState capture_response_state;
    luna::editor::EditorViewportCommandResult capture_response_result;
    errors.clear();
    context.expect(luna::editor::editorViewportResponseEnvelopeFromJson(capture_response_json,
                                                                        capture_response_state,
                                                                        capture_response_result,
                                                                        &errors),
                   "capture response should parse");
    context.expect(errors.empty(), "capture response parse should not report errors");
    context.expect(capture_response_result.request_capture && !capture_response_result.changed &&
                       !capture_response_result.request_pick,
                   "capture should only request a capture");
    context.expect(sameViewportState(applied_state, capture_response_state),
                   "capture should not mutate viewport state");

    luna::editor::EditorViewportCommandResult result;
    result.accepted = true;
    result.changed = true;
    result.request_pick = true;
    result.request_capture = false;
    const luna::editor::Json result_json = luna::editor::editorViewportCommandResultJson(result);
    luna::editor::EditorViewportCommandResult parsed_result;
    errors.clear();
    context.expect(luna::editor::editorViewportCommandResultFromJson(result_json, parsed_result, &errors),
                   "viewport command result JSON should parse");
    context.expect(errors.empty(), "viewport command result parse should not report errors");
    context.expect(parsed_result.accepted && parsed_result.changed && parsed_result.request_pick &&
                       !parsed_result.request_capture,
                   "viewport command result should round-trip");
}

} // namespace

int main()
{
    TestContext context;
    testProtocolInfo(context);
    testViewProtocolSchemaContract(context);
    testViewportCameraRoundTrip(context);
    testRenderDataPlane(context);
    testViewportCommands(context);
    testViewportState(context);
    testViewportJsonWire(context);
    return context.result();
}
