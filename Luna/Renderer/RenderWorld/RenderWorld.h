#pragma once

#include "Renderer/Camera.h"
#include "Renderer/RenderWorld/RenderTypes.h"

#include <vector>

namespace luna {

class RenderWorld {
public:
    void begin(const Camera& camera);
    void clear();

    const Camera& camera() const;
    bool hasCamera() const;

    void addDirectionalLight(const RenderDirectionalLight& light);
    void addPointLight(const RenderPointLight& light);
    void addSpotLight(const RenderSpotLight& light);
    void addMeshInstance(RenderMeshInstance instance);
    void addDrawPacket(RenderDrawPacket packet);

    const std::vector<RenderDirectionalLight>& directionalLights() const;
    const std::vector<RenderPointLight>& pointLights() const;
    const std::vector<RenderSpotLight>& spotLights() const;
    const std::vector<RenderMeshInstance>& meshInstances() const;
    const std::vector<RenderDrawPacket>& drawPackets() const;
    std::vector<RenderDrawPacket> drawPackets(RenderPhase phase) const;

private:
    Camera m_camera;
    bool m_has_camera{false};
    std::vector<RenderDirectionalLight> m_directional_lights;
    std::vector<RenderPointLight> m_point_lights;
    std::vector<RenderSpotLight> m_spot_lights;
    std::vector<RenderMeshInstance> m_mesh_instances;
    std::vector<RenderDrawPacket> m_draw_packets;
};

} // namespace luna
