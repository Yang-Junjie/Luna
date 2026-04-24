#pragma once

// Stores scene draw submissions for the current frame.
// Splits work into opaque and transparent queues and keeps the camera snapshot
// needed for sorting and later pass execution.

#include "Renderer/Camera.h"

#include <cstdint>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <optional>
#include <vector>

namespace luna {
class Material;
class Mesh;
} // namespace luna

namespace luna::scene_renderer {

struct DrawCommand {
    glm::mat4 transform{1.0f};
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    uint32_t sub_mesh_index{UINT32_MAX};
    uint32_t picking_id{0};
};

struct DirectionalLightSubmission {
    glm::vec3 direction{0.0f, 1.0f, 0.0f};
    float intensity{0.0f};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
};

struct PointLightSubmission {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    float intensity{0.0f};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float range{10.0f};
};

struct SpotLightSubmission {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    float intensity{0.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    float range{10.0f};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float innerConeCos{0.0f};
    float outerConeCos{0.0f};
};

class DrawQueue final {
public:
    void beginScene(const Camera& camera);
    void clear() noexcept;

    void submitStaticMesh(const glm::mat4& transform,
                          std::shared_ptr<Mesh> mesh,
                          std::shared_ptr<Material> material = {},
                          uint32_t picking_id = 0);
    void submitStaticMesh(const glm::mat4& transform,
                          std::shared_ptr<Mesh> mesh,
                          const std::vector<std::shared_ptr<Material>>& submesh_materials,
                          uint32_t picking_id = 0);
    void submitDirectionalLight(const DirectionalLightSubmission& light);
    void submitPointLight(const PointLightSubmission& light);
    void submitSpotLight(const SpotLightSubmission& light);

    void sortTransparentBackToFront();

    [[nodiscard]] const Camera& camera() const noexcept
    {
        return m_camera;
    }

    [[nodiscard]] const std::optional<DirectionalLightSubmission>& directionalLight() const noexcept
    {
        return m_directional_light;
    }

    [[nodiscard]] const std::vector<PointLightSubmission>& pointLights() const noexcept
    {
        return m_point_lights;
    }

    [[nodiscard]] const std::vector<SpotLightSubmission>& spotLights() const noexcept
    {
        return m_spot_lights;
    }

    [[nodiscard]] const std::vector<DrawCommand>& opaqueDrawCommands() const noexcept
    {
        return m_opaque_draw_commands;
    }

    [[nodiscard]] const std::vector<DrawCommand>& transparentDrawCommands() const noexcept
    {
        return m_transparent_draw_commands;
    }

    [[nodiscard]] bool hasTransparentDrawCommands() const noexcept
    {
        return !m_transparent_draw_commands.empty();
    }

private:
    Camera m_camera{};
    std::optional<DirectionalLightSubmission> m_directional_light;
    std::vector<PointLightSubmission> m_point_lights;
    std::vector<SpotLightSubmission> m_spot_lights;
    std::vector<DrawCommand> m_opaque_draw_commands;
    std::vector<DrawCommand> m_transparent_draw_commands;
};

} // namespace luna::scene_renderer
