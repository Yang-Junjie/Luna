#include "Core/Log.h"
#include "Math/Math.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/RendererUtilities.h"
#include "Renderer/RenderFlow/DefaultScene/DrawQueue.h"

#include <algorithm>

namespace luna::render_flow::default_scene {

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
    for (auto& draw_commands : m_draw_commands_by_phase) {
        draw_commands.clear();
    }
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
        cacheDrawCommandForPhases(packet, true);
        ++m_stats.camera_visible;
        captureVisibilityDebugItem(packet,
                                   invalid_bounds ? VisibilityDebugClassification::InvalidBounds
                                                  : VisibilityDebugClassification::CameraVisible);
        return;
    }

    ++m_stats.camera_culled;
    cacheDrawCommandForPhases(packet, false);
    captureVisibilityDebugItem(packet, VisibilityDebugClassification::CameraCulled);
}

const std::vector<DrawCommand>& DrawQueue::drawCommands() const noexcept
{
    return m_camera_visible_draw_commands;
}

const std::vector<DrawCommand>& DrawQueue::drawCommands(RenderPhase phase) const noexcept
{
    return m_draw_commands_by_phase[static_cast<std::size_t>(phase)];
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

void DrawQueue::cacheDrawCommandForPhases(const RenderDrawPacket& packet, bool camera_visible)
{
    for (std::size_t phase_index = 0; phase_index < m_draw_commands_by_phase.size(); ++phase_index) {
        const RenderPhase phase = static_cast<RenderPhase>(phase_index);
        if (!hasRenderPhase(packet.phases, phase)) {
            continue;
        }
        if (!camera_visible && phase != RenderPhase::ShadowCaster) {
            continue;
        }
        m_draw_commands_by_phase[phase_index].push_back(packet);
        switch (phase) {
            case RenderPhase::DepthOnly:
                ++m_stats.phase_depth_only;
                break;
            case RenderPhase::GBuffer:
                ++m_stats.phase_gbuffer;
                break;
            case RenderPhase::ForwardOpaque:
                ++m_stats.phase_forward_opaque;
                break;
            case RenderPhase::Transparent:
                ++m_stats.phase_transparent;
                break;
            case RenderPhase::ShadowCaster:
                ++m_stats.phase_shadow_caster;
                break;
            case RenderPhase::Picking:
                ++m_stats.phase_picking;
                break;
        }
    }
}

void DrawQueue::captureVisibilityDebugFrustum(const Camera& camera, float aspect_ratio)
{
    if (!m_visibility_debug_options.capture_culling_frustum) {
        return;
    }

    m_visibility_debug_frustums.push_back(VisibilityDebugFrustumItem{
        .corners = cameraFrustumCorners(camera, aspect_ratio),
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
