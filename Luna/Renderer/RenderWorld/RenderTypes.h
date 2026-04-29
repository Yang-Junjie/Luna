#pragma once

#include <cstdint>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <vector>

namespace luna {

class Material;
class Mesh;

enum class RenderPhase : uint8_t {
    DepthOnly,
    GBuffer,
    ForwardOpaque,
    Transparent,
    ShadowCaster,
    Picking,
};

using RenderPhaseMask = uint32_t;

constexpr RenderPhaseMask renderPhaseBit(RenderPhase phase)
{
    return 1u << static_cast<uint32_t>(phase);
}

constexpr bool hasRenderPhase(RenderPhaseMask mask, RenderPhase phase)
{
    return (mask & renderPhaseBit(phase)) != 0;
}

struct RenderMeshInstance {
    glm::mat4 transform{1.0f};
    std::shared_ptr<Mesh> mesh;
    std::vector<std::shared_ptr<Material>> submesh_materials;
    uint32_t picking_id{0};
};

struct RenderDrawPacket {
    glm::mat4 transform{1.0f};
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    uint32_t submesh_index{UINT32_MAX};
    uint32_t picking_id{0};
    RenderPhaseMask phases{0};
};

struct RenderDirectionalLight {
    glm::vec3 direction{0.0f, 1.0f, 0.0f};
    float intensity{0.0f};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
};

struct RenderPointLight {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    float intensity{0.0f};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float range{10.0f};
};

struct RenderSpotLight {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    float intensity{0.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    float range{10.0f};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float innerConeCos{0.0f};
    float outerConeCos{0.0f};
};

} // namespace luna
