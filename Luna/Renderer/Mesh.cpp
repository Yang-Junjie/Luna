#include "Renderer/Mesh.h"

#include <utility>

namespace luna {
Mesh::Mesh(std::string name, std::vector<SubMesh> subMeshes)
    : m_name(std::move(name)),
      m_subMeshes(std::move(subMeshes))
{}

std::shared_ptr<Mesh> Mesh::create(std::string name, std::vector<SubMesh> subMeshes)
{
    return std::make_shared<Mesh>(std::move(name), std::move(subMeshes));
}

const std::string& Mesh::getName() const
{
    return m_name;
}

const std::vector<SubMesh>& Mesh::getSubMeshes() const
{
    return m_subMeshes;
}

bool Mesh::isValid() const
{
    if (m_subMeshes.empty()) {
        return false;
    }

    for (const auto& sm : m_subMeshes) {
        if (sm.Vertices.empty() || sm.Indices.empty()) {
            return false;
        }
    }
    return true;
}

} // namespace luna
