#include "Core/Log.h"
#include "Math/Math.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/RendererUtilities.h"
#include "Renderer/RenderFlow/DefaultScene/DrawQueue.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace luna::render_flow::default_scene {
namespace {

std::array<glm::vec3, 8> perspectiveFrustumCorners(const Camera& camera, float aspect_ratio)
{
    const Camera::PerspectiveSettings& settings = camera.getPerspectiveSettings();
    const float near_distance = settings.near_clip;
    const float far_distance = settings.far_clip;
    const float tan_half_fov = std::tan(settings.vertical_fov_radians * 0.5f);
    const float near_half_height = tan_half_fov * near_distance;
    const float near_half_width = near_half_height * aspect_ratio;
    const float far_half_height = tan_half_fov * far_distance;
    const float far_half_width = far_half_height * aspect_ratio;

    const glm::vec3 position = camera.getPosition();
    const glm::vec3 forward = camera.getForwardDirection();
    const glm::vec3 right = camera.getRightDirection();
    const glm::vec3 up = camera.getUpDirection();
    const glm::vec3 near_center = position + forward * near_distance;
    const glm::vec3 far_center = position + forward * far_distance;

    return {{
        near_center - right * near_half_width - up * near_half_height,
        near_center + right * near_half_width - up * near_half_height,
        near_center + right * near_half_width + up * near_half_height,
        near_center - right * near_half_width + up * near_half_height,
        far_center - right * far_half_width - up * far_half_height,
        far_center + right * far_half_width - up * far_half_height,
        far_center + right * far_half_width + up * far_half_height,
        far_center - right * far_half_width + up * far_half_height,
    }};
}

std::array<glm::vec3, 8> orthographicFrustumCorners(const Camera& camera, float aspect_ratio)
{
    const Camera::OrthographicSettings& settings = camera.getOrthographicSettings();
    const float half_height = settings.vertical_size * 0.5f;
    const float half_width = half_height * aspect_ratio;

    const glm::vec3 position = camera.getPosition();
    const glm::vec3 forward = camera.getForwardDirection();
    const glm::vec3 right = camera.getRightDirection();
    const glm::vec3 up = camera.getUpDirection();
    const glm::vec3 near_center = position + forward * settings.near_clip;
    const glm::vec3 far_center = position + forward * settings.far_clip;

    return {{
        near_center - right * half_width - up * half_height,
        near_center + right * half_width - up * half_height,
        near_center + right * half_width + up * half_height,
        near_center - right * half_width + up * half_height,
        far_center - right * half_width - up * half_height,
        far_center + right * half_width - up * half_height,
        far_center + right * half_width + up * half_height,
        far_center - right * half_width + up * half_height,
    }};
}

std::array<glm::vec3, 8> frustumCorners(const Camera& camera, float aspect_ratio)
{
    const float clamped_aspect_ratio = std::max(aspect_ratio, 0.001f);
    if (camera.getProjectionType() == Camera::ProjectionType::Orthographic) {
        return orthographicFrustumCorners(camera, clamped_aspect_ratio);
    }
    return perspectiveFrustumCorners(camera, clamped_aspect_ratio);
}

} // namespace

void DrawQueue::beginScene(const Camera& camera,
                           float aspect_ratio,
                           VisibilityDebugCaptureOptions visibility_debug_options)
{
    beginScene(camera, aspect_ratio, camera, aspect_ratio, visibility_debug_options);
}

void DrawQueue::beginScene(const Camera& camera,
                           float aspect_ratio,
                           const Camera& culling_camera,
                           float culling_aspect_ratio,
                           VisibilityDebugCaptureOptions visibility_debug_options)
{
    m_camera = camera;
    m_culling_camera = culling_camera;
    m_culling_frustum = Frustum::fromViewProjection(culling_camera.getViewProjectionMatrix(culling_aspect_ratio));
    m_visibility_debug_options = visibility_debug_options;
    LUNA_RENDERER_FRAME_TRACE("Beginning scene draw submission");
    clear();
    captureVisibilityDebugFrustum(culling_camera, culling_aspect_ratio);
}

void DrawQueue::clear() noexcept
{
    if (!m_all_draw_commands.empty() || !m_camera_visible_draw_commands.empty() || !m_visibility_debug_items.empty() ||
        !m_visibility_debug_frustums.empty()) {
        LUNA_RENDERER_FRAME_TRACE(
            "Clearing draw queue: all_draws={} camera_visible_draws={} visibility_debug_items={} visibility_debug_frustums={}",
            m_all_draw_commands.size(),
            m_camera_visible_draw_commands.size(),
            m_visibility_debug_items.size(),
            m_visibility_debug_frustums.size());
    }
    m_all_draw_commands.clear();
    m_camera_visible_draw_commands.clear();
    m_visibility_debug_items.clear();
    m_visibility_debug_frustums.clear();
    m_visibility_debug_stats = {};
    m_stats = {};
}

void DrawQueue::submitDrawPacket(const RenderDrawPacket& packet)
{
    if (!packet.mesh || !packet.mesh->isValid()) {
        LUNA_RENDERER_WARN("Ignoring draw packet because mesh is null or invalid");
        return;
    }

    const auto& sub_meshes = packet.mesh->getSubMeshes();
    if (packet.submesh_index >= sub_meshes.size()) {
        LUNA_RENDERER_WARN("Ignoring draw packet because submesh {} is out of range for mesh '{}'",
                           packet.submesh_index,
                           packet.mesh->getName());
        return;
    }

    const auto& sub_mesh = sub_meshes[packet.submesh_index];
    if (sub_mesh.Vertices.empty() || sub_mesh.Indices.empty()) {
        LUNA_RENDERER_FRAME_TRACE(
            "Skipping empty draw packet submesh {} from mesh '{}'", packet.submesh_index, packet.mesh->getName());
        return;
    }

    m_all_draw_commands.push_back(packet);
    ++m_stats.submitted;
    if (hasRenderPhase(packet.phases, RenderPhase::ShadowCaster)) {
        ++m_stats.shadow_unculled;
    }

    const bool invalid_bounds = !packet.world_bounds.isValid();
    if (invalid_bounds) {
        ++m_stats.invalid_bounds;
    }

    if (m_culling_frustum.intersects(packet.world_bounds)) {
        m_camera_visible_draw_commands.push_back(packet);
        ++m_stats.camera_visible;
        captureVisibilityDebugItem(packet,
                                   invalid_bounds ? VisibilityDebugClassification::InvalidBounds
                                                  : VisibilityDebugClassification::CameraVisible);
        return;
    }

    ++m_stats.camera_culled;
    captureVisibilityDebugItem(packet, VisibilityDebugClassification::CameraCulled);
}

const std::vector<DrawCommand>& DrawQueue::drawCommands() const noexcept
{
    return m_camera_visible_draw_commands;
}

std::vector<DrawCommand> DrawQueue::drawCommands(RenderPhase phase) const
{
    const std::vector<DrawCommand>& source =
        phase == RenderPhase::ShadowCaster ? m_all_draw_commands : m_camera_visible_draw_commands;
    std::vector<DrawCommand> commands;
    for (const auto& command : source) {
        if (hasRenderPhase(command.phases, phase)) {
            commands.push_back(command);
        }
    }
    return commands;
}

void DrawQueue::sortBackToFront(std::vector<DrawCommand>& draw_commands) const
{
    LUNA_RENDERER_FRAME_TRACE("Sorting {} draw command(s) back-to-front", draw_commands.size());
    const glm::vec3 camera_position = m_camera.getPosition();
    std::sort(
        draw_commands.begin(), draw_commands.end(), [camera_position](const DrawCommand& lhs, const DrawCommand& rhs) {
            return luna::translationDistanceSquared(lhs.transform, camera_position) >
                   luna::translationDistanceSquared(rhs.transform, camera_position);
        });
}

void DrawQueue::captureVisibilityDebugFrustum(const Camera& camera, float aspect_ratio)
{
    if (!m_visibility_debug_options.capture_culling_frustum) {
        return;
    }

    m_visibility_debug_frustums.push_back(VisibilityDebugFrustumItem{
        .corners = frustumCorners(camera, aspect_ratio),
        .color = m_visibility_debug_options.culling_frustum_frozen ? glm::vec4{0.35f, 0.72f, 1.0f, 0.96f}
                                                                   : glm::vec4{0.12f, 0.58f, 1.0f, 0.82f},
        .frozen = m_visibility_debug_options.culling_frustum_frozen,
    });
    ++m_visibility_debug_stats.culling_frustums;
}

void DrawQueue::captureVisibilityDebugItem(const RenderDrawPacket& packet,
                                           VisibilityDebugClassification classification)
{
    const bool capture_item = classification == VisibilityDebugClassification::CameraCulled
                                  ? m_visibility_debug_options.capture_culled_bounds
                                  : m_visibility_debug_options.capture_visible_bounds;
    if (!capture_item) {
        return;
    }

    m_visibility_debug_items.push_back(VisibilityDebugItem{
        .world_bounds = packet.world_bounds,
        .world_origin = glm::vec3(packet.transform[3]),
        .picking_id = packet.picking_id,
        .submesh_index = packet.submesh_index,
        .phases = packet.phases,
        .classification = classification,
    });
    ++m_visibility_debug_stats.captured;
    switch (classification) {
        case VisibilityDebugClassification::CameraVisible:
            ++m_visibility_debug_stats.camera_visible;
            break;
        case VisibilityDebugClassification::CameraCulled:
            ++m_visibility_debug_stats.camera_culled;
            break;
        case VisibilityDebugClassification::InvalidBounds:
            ++m_visibility_debug_stats.invalid_bounds;
            break;
    }
}

} // namespace luna::render_flow::default_scene
