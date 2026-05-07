#include "Core/Log.h"
#include "Renderer/Mesh.h"

#include <numeric>
#include <utility>

namespace luna {
Mesh::Mesh(std::string name, std::vector<SubMesh> subMeshes)
    : m_name(std::move(name)),
      m_subMeshes(std::move(subMeshes))
{
    const size_t vertex_count =
        std::accumulate(m_subMeshes.begin(), m_subMeshes.end(), size_t{0}, [](size_t total, const SubMesh& sub_mesh) {
            return total + sub_mesh.Vertices.size();
        });
    const size_t index_count =
        std::accumulate(m_subMeshes.begin(), m_subMeshes.end(), size_t{0}, [](size_t total, const SubMesh& sub_mesh) {
            return total + sub_mesh.Indices.size();
        });
    LUNA_RENDERER_DEBUG("Created mesh '{}' with {} submesh(es), {} vertex/vertices, {} index/indices",
                        m_name.empty() ? "<unnamed>" : m_name,
                        m_subMeshes.size(),
                        vertex_count,
                        index_count);
}

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
