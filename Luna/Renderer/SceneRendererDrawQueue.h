#pragma once

#include "Renderer/Camera.h"

#include <cstdint>

#include <glm/mat4x4.hpp>
#include <memory>
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
};

class DrawQueue final {
public:
    void beginScene(const Camera& camera);
    void clear() noexcept;

    void submitStaticMesh(const glm::mat4& transform,
                          std::shared_ptr<Mesh> mesh,
                          std::shared_ptr<Material> material = {});
    void submitStaticMesh(const glm::mat4& transform,
                          std::shared_ptr<Mesh> mesh,
                          const std::vector<std::shared_ptr<Material>>& submesh_materials);

    void sortTransparentBackToFront();

    [[nodiscard]] const Camera& camera() const noexcept
    {
        return m_camera;
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
    std::vector<DrawCommand> m_opaque_draw_commands;
    std::vector<DrawCommand> m_transparent_draw_commands;
};

} // namespace luna::scene_renderer
