#include "Asset/Model.h"

#include <utility>

namespace luna {

Model::Model(std::string name, std::filesystem::path source, std::vector<ModelNode> nodes)
    : m_name(std::move(name)),
      m_source(std::move(source)),
      m_nodes(std::move(nodes))
{}

std::shared_ptr<Model> Model::create(std::string name,
                                     std::filesystem::path source,
                                     std::vector<ModelNode> nodes)
{
    return std::make_shared<Model>(std::move(name), std::move(source), std::move(nodes));
}

const std::string& Model::getName() const
{
    return m_name;
}

const std::filesystem::path& Model::getSourcePath() const
{
    return m_source;
}

const std::vector<ModelNode>& Model::getNodes() const
{
    return m_nodes;
}

bool Model::isValid() const
{
    return !m_nodes.empty();
}

} // namespace luna
