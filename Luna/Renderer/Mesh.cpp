#include "Renderer/Mesh.h"

#include <utility>

namespace luna {

Mesh::Mesh() = default;

Mesh::Mesh(std::string name, std::vector<StaticMeshVertex> vertices, std::vector<uint32_t> indices)
    : m_name(std::move(name)),
      m_vertices(std::move(vertices)),
      m_indices(std::move(indices))
{}

std::shared_ptr<Mesh>
    Mesh::create(std::string name, std::vector<StaticMeshVertex> vertices, std::vector<uint32_t> indices)
{
    return std::make_shared<Mesh>(std::move(name), std::move(vertices), std::move(indices));
}

std::shared_ptr<Mesh> Mesh::createFromModelShape(const rhi::ModelData::Shape& shape)
{
    std::vector<StaticMeshVertex> vertices;
    vertices.reserve(shape.Vertices.size());

    for (const auto& vertex : shape.Vertices) {
        vertices.push_back(StaticMeshVertex{
            .position = vertex.Position,
            .uv = vertex.TexCoord,
            .normal = vertex.Normal,
            .tangent = vertex.Tangent,
            .bitangent = vertex.Bitangent,
        });
    }

    return Mesh::create(shape.Name, vertices, std::vector<uint32_t>{shape.Indices.begin(), shape.Indices.end()});
}

const std::string& Mesh::getName() const
{
    return m_name;
}

const std::vector<StaticMeshVertex>& Mesh::getVertices() const
{
    return m_vertices;
}

const std::vector<uint32_t>& Mesh::getIndices() const
{
    return m_indices;
}

bool Mesh::isValid() const
{
    return !m_vertices.empty() && !m_indices.empty();
}

} // namespace luna
