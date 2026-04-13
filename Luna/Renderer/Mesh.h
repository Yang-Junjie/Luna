#pragma once

#include "Renderer/ModelLoader.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace luna {

struct StaticMeshVertex {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec2 uv{0.0f, 0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
};

class Mesh {
public:
    Mesh() = default;
    Mesh(std::string name, std::vector<StaticMeshVertex> vertices, std::vector<uint32_t> indices);

    static std::shared_ptr<Mesh>
        create(std::string name, std::vector<StaticMeshVertex> vertices, std::vector<uint32_t> indices);
    static std::shared_ptr<Mesh> createFromModelShape(const val::ModelData::Shape& shape);

    const std::string& getName() const
    {
        return m_name;
    }

    const std::vector<StaticMeshVertex>& getVertices() const
    {
        return m_vertices;
    }

    const std::vector<uint32_t>& getIndices() const
    {
        return m_indices;
    }

    bool isValid() const
    {
        return !m_vertices.empty() && !m_indices.empty();
    }

private:
    std::string m_name;
    std::vector<StaticMeshVertex> m_vertices;
    std::vector<uint32_t> m_indices;
};

} // namespace luna
