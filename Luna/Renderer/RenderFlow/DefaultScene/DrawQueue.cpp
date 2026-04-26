#include "Core/Log.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/RendererUtilities.h"
#include "Renderer/RenderFlow/DefaultScene/DrawQueue.h"
#include "Math/Math.h"

#include <algorithm>

namespace luna::render_flow::default_scene {

void DrawQueue::beginScene(const Camera& camera)
{
    m_camera = camera;
    LUNA_RENDERER_FRAME_TRACE("Beginning scene draw submission");
    clear();
}

void DrawQueue::clear() noexcept
{
    if (!m_draw_commands.empty()) {
        LUNA_RENDERER_FRAME_TRACE("Clearing draw queue: draws={}", m_draw_commands.size());
    }
    m_draw_commands.clear();
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
        LUNA_RENDERER_FRAME_TRACE("Skipping empty draw packet submesh {} from mesh '{}'",
                                  packet.submesh_index,
                                  packet.mesh->getName());
        return;
    }

    if (hasRenderPhase(packet.phases, RenderPhase::Transparent)) {
        m_draw_commands.push_back(packet);
        return;
    }

    m_draw_commands.push_back(packet);
}

const std::vector<DrawCommand>& DrawQueue::drawCommands() const noexcept
{
    return m_draw_commands;
}

std::vector<DrawCommand> DrawQueue::drawCommands(RenderPhase phase) const
{
    std::vector<DrawCommand> commands;
    for (const auto& command : m_draw_commands) {
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
    std::sort(draw_commands.begin(),
              draw_commands.end(),
              [camera_position](const DrawCommand& lhs, const DrawCommand& rhs) {
                  return luna::translationDistanceSquared(lhs.transform, camera_position) >
                         luna::translationDistanceSquared(rhs.transform, camera_position);
              });
}

} // namespace luna::render_flow::default_scene





