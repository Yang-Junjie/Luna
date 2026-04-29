#pragma once

// Stores scene draw submissions for the current frame.
// Stores draw packets and light submissions for the current frame.
// Passes query packets by RenderPhase and can sort their local lists as needed.

#include "Renderer/Camera.h"
#include "Renderer/RenderWorld/RenderTypes.h"

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

namespace luna::render_flow::default_scene {

using DrawCommand = RenderDrawPacket;

class DrawQueue final {
public:
    void beginScene(const Camera& camera);
    void clear() noexcept;

    void submitDrawPacket(const RenderDrawPacket& packet);

    void sortBackToFront(std::vector<DrawCommand>& draw_commands) const;

    [[nodiscard]] const Camera& camera() const noexcept
    {
        return m_camera;
    }

    [[nodiscard]] const std::vector<DrawCommand>& drawCommands() const noexcept;
    [[nodiscard]] std::vector<DrawCommand> drawCommands(RenderPhase phase) const;

private:
    Camera m_camera{};
    std::vector<DrawCommand> m_draw_commands;
};

} // namespace luna::render_flow::default_scene
