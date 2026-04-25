#pragma once

#include "Renderer/Mesh.h"

#include <memory>

namespace luna {

class PrimitiveMeshFactory final {
public:
    static std::shared_ptr<Mesh> createCube();
    static std::shared_ptr<Mesh> createPlane();
    static std::shared_ptr<Mesh> createSphere(uint32_t segments = 32, uint32_t rings = 16);
    static std::shared_ptr<Mesh> createCylinder(uint32_t segments = 32);
    static std::shared_ptr<Mesh> createCone(uint32_t segments = 32);
};

} // namespace luna




