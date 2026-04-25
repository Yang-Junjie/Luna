#include "Renderer/RenderWorld/RenderWorld.h"

#include <utility>

namespace luna {

void RenderWorld::begin(const Camera& camera)
{
    clear();
    m_camera = camera;
    m_has_camera = true;
}

void RenderWorld::clear()
{
    m_has_camera = false;
    m_directional_lights.clear();
    m_point_lights.clear();
    m_spot_lights.clear();
    m_mesh_instances.clear();
    m_draw_packets.clear();
}

const Camera& RenderWorld::camera() const
{
    return m_camera;
}

bool RenderWorld::hasCamera() const
{
    return m_has_camera;
}

void RenderWorld::addDirectionalLight(const RenderDirectionalLight& light)
{
    m_directional_lights.push_back(light);
}

void RenderWorld::addPointLight(const RenderPointLight& light)
{
    m_point_lights.push_back(light);
}

void RenderWorld::addSpotLight(const RenderSpotLight& light)
{
    m_spot_lights.push_back(light);
}

void RenderWorld::addMeshInstance(RenderMeshInstance instance)
{
    m_mesh_instances.push_back(std::move(instance));
}

void RenderWorld::addDrawPacket(RenderDrawPacket packet)
{
    m_draw_packets.push_back(std::move(packet));
}

const std::vector<RenderDirectionalLight>& RenderWorld::directionalLights() const
{
    return m_directional_lights;
}

const std::vector<RenderPointLight>& RenderWorld::pointLights() const
{
    return m_point_lights;
}

const std::vector<RenderSpotLight>& RenderWorld::spotLights() const
{
    return m_spot_lights;
}

const std::vector<RenderMeshInstance>& RenderWorld::meshInstances() const
{
    return m_mesh_instances;
}

const std::vector<RenderDrawPacket>& RenderWorld::drawPackets() const
{
    return m_draw_packets;
}

std::vector<RenderDrawPacket> RenderWorld::drawPackets(RenderPhase phase) const
{
    std::vector<RenderDrawPacket> packets;
    for (const auto& packet : m_draw_packets) {
        if (hasRenderPhase(packet.phases, phase)) {
            packets.push_back(packet);
        }
    }
    return packets;
}

} // namespace luna




