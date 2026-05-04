#pragma once

#include "Asset/Asset.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
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

enum class RenderBackgroundMode : uint8_t {
    SolidColor,
    ProceduralSky,
    EnvironmentMap,
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

struct RenderEnvironment {
    bool enabled{true};
    bool ibl_enabled{true};
    RenderBackgroundMode background_mode{RenderBackgroundMode::ProceduralSky};
    glm::vec3 background_color{0.10f, 0.10f, 0.12f};
    AssetHandle environment_map_handle{AssetHandle(0)};
    float intensity{1.0f};
    float sky_intensity{1.0f};
    float diffuse_intensity{1.0f};
    float specular_intensity{1.0f};
    bool allow_async_load{false};

    glm::vec3 procedural_sun_direction{0.51214755f, 0.76822126f, 0.38411063f};
    float procedural_sun_intensity{20.0f};
    float procedural_sun_angular_radius{0.02f};
    glm::vec3 procedural_sky_color_zenith{0.15f, 0.30f, 0.60f};
    glm::vec3 procedural_sky_color_horizon{0.60f, 0.50f, 0.40f};
    glm::vec3 procedural_ground_color{0.10f, 0.08f, 0.06f};
    float procedural_sky_exposure{1.5f};
};

} // namespace luna




