#pragma once

#include "Renderer/Mesh.h"

#include <cstddef>

#include <array>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace luna {

class Camera;

struct Plane {
    glm::vec3 Normal{0.0f, 1.0f, 0.0f};
    float Distance{0.0f};
};

using FrustumCorners = std::array<glm::vec3, 8>;

class Frustum {
public:
    enum class PlaneIndex : size_t {
        Left = 0,
        Right,
        Bottom,
        Top,
        Near,
        Far,
        Count,
    };

    static Frustum fromViewProjection(const glm::mat4& view_projection);

    [[nodiscard]] bool intersects(const MeshBounds& bounds) const noexcept;
    [[nodiscard]] const std::array<Plane, static_cast<size_t>(PlaneIndex::Count)>& planes() const noexcept;

private:
    std::array<Plane, static_cast<size_t>(PlaneIndex::Count)> m_planes{};
};

[[nodiscard]] FrustumCorners cameraFrustumCorners(const Camera& camera, float aspect_ratio);

} // namespace luna
