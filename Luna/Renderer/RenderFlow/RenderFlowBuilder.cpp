#include "Renderer/RenderFlow/RenderFlowBuilder.h"

#include <algorithm>
#include <queue>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace luna::render_flow {

namespace {

using NodeIndex = size_t;

struct ReadyNode {
    NodeIndex index{0};
    int32_t priority{0};
    uint64_t registration_index{0};
};

struct ReadyNodeCompare {
    bool operator()(const ReadyNode& lhs, const ReadyNode& rhs) const noexcept
    {
        if (lhs.priority != rhs.priority) {
            return lhs.priority > rhs.priority;
        }
        return lhs.registration_index > rhs.registration_index;
    }
};

enum class VisitState : uint8_t {
    Unvisited,
    Visiting,
    Visited,
};

bool findCycleDfs(NodeIndex node,
                  const std::vector<std::vector<NodeIndex>>& outgoing,
                  std::vector<VisitState>& states,
                  std::vector<NodeIndex>& stack,
                  std::vector<NodeIndex>& cycle)
{
    states[node] = VisitState::Visiting;
    stack.push_back(node);

    for (const NodeIndex next : outgoing[node]) {
        if (states[next] == VisitState::Unvisited) {
            if (findCycleDfs(next, outgoing, states, stack, cycle)) {
                return true;
            }
            continue;
        }

        if (states[next] == VisitState::Visiting) {
            const auto cycle_begin = std::find(stack.begin(), stack.end(), next);
            if (cycle_begin != stack.end()) {
                cycle.assign(cycle_begin, stack.end());
                cycle.push_back(next);
            }
            return true;
        }
    }

    stack.pop_back();
    states[node] = VisitState::Visited;
    return false;
}

std::vector<NodeIndex> findCycle(const std::vector<std::vector<NodeIndex>>& outgoing)
{
    std::vector<VisitState> states(outgoing.size(), VisitState::Unvisited);
    std::vector<NodeIndex> stack;
    std::vector<NodeIndex> cycle;

    for (NodeIndex index = 0; index < outgoing.size(); ++index) {
        if (states[index] == VisitState::Unvisited && findCycleDfs(index, outgoing, states, stack, cycle)) {
            return cycle;
        }
    }

    return cycle;
}

std::string formatCycle(const std::vector<NodeIndex>& cycle, const std::vector<std::string_view>& names)
{
    if (cycle.empty()) {
        return "RenderFlow dependency cycle detected";
    }

    std::ostringstream stream;
    stream << "RenderFlow dependency cycle detected: ";
    for (size_t index = 0; index < cycle.size(); ++index) {
        if (index > 0) {
            stream << " -> ";
        }
        stream << names[cycle[index]];
    }
    return stream.str();
}

} // namespace

bool RenderFlowBuilder::contains(std::string_view name) const noexcept
{
    return findNode(name) != m_nodes.end();
}

IRenderPass* RenderFlowBuilder::find(std::string_view name) const noexcept
{
    const auto node = findNode(name);
    return node != m_nodes.end() ? node->pass.get() : nullptr;
}

std::vector<std::string_view> RenderFlowBuilder::passNames() const
{
    std::vector<std::string_view> names;
    names.reserve(m_nodes.size());
    for (const Node& node : m_nodes) {
        names.push_back(node.name);
    }
    return names;
}

std::vector<std::string_view> RenderFlowBuilder::passesForFeature(std::string_view feature_name) const
{
    std::vector<std::string_view> names;
    for (const Node& node : m_nodes) {
        if (node.owner_feature == feature_name) {
            names.push_back(node.name);
        }
    }
    return names;
}

std::string_view RenderFlowBuilder::passOwner(std::string_view name) const noexcept
{
    const auto node = findNode(name);
    return node != m_nodes.end() ? std::string_view(node->owner_feature) : std::string_view{};
}

std::string_view RenderFlowBuilder::lastError() const noexcept
{
    return m_last_error;
}

RenderFlowBuilder::CompileResult RenderFlowBuilder::compile() const
{
    clearError();
    CompileResult result;
    result.passes.reserve(m_nodes.size());

    std::unordered_map<std::string_view, NodeIndex> indices;
    indices.reserve(m_nodes.size());
    std::vector<std::string_view> names;
    names.reserve(m_nodes.size());
    for (NodeIndex index = 0; index < m_nodes.size(); ++index) {
        const std::string_view name = m_nodes[index].name;
        indices.emplace(name, index);
        names.push_back(name);
    }

    std::vector<std::vector<NodeIndex>> outgoing(m_nodes.size());
    std::vector<uint32_t> indegrees(m_nodes.size(), 0);

    for (const auto& edge : m_edges) {
        const auto before = indices.find(edge.before);
        const auto after = indices.find(edge.after);
        if (before == indices.end() || after == indices.end()) {
            result.error =
                "RenderFlow dependency references a missing pass: '" + edge.before + "' -> '" + edge.after + "'";
            setError(result.error);
            return result;
        }

        outgoing[before->second].push_back(after->second);
        ++indegrees[after->second];
    }

    std::priority_queue<ReadyNode, std::vector<ReadyNode>, ReadyNodeCompare> ready;
    for (NodeIndex index = 0; index < m_nodes.size(); ++index) {
        if (indegrees[index] == 0) {
            ready.push(ReadyNode{
                .index = index,
                .priority = m_nodes[index].priority,
                .registration_index = m_nodes[index].registration_index,
            });
        }
    }

    while (!ready.empty()) {
        const ReadyNode current = ready.top();
        ready.pop();

        const Node& node = m_nodes[current.index];
        result.passes.push_back(OrderedPass{
            .name = node.name,
            .pass = node.pass.get(),
        });

        for (const NodeIndex next : outgoing[current.index]) {
            --indegrees[next];
            if (indegrees[next] == 0) {
                ready.push(ReadyNode{
                    .index = next,
                    .priority = m_nodes[next].priority,
                    .registration_index = m_nodes[next].registration_index,
                });
            }
        }
    }

    if (result.passes.size() != m_nodes.size()) {
        result.error = formatCycle(findCycle(outgoing), names);
        setError(result.error);
        return result;
    }

    result.success = true;
    return result;
}

bool RenderFlowBuilder::addPass(std::string name, std::unique_ptr<IRenderPass> pass, int32_t priority)
{
    return addPassImpl({}, std::move(name), std::move(pass), priority);
}

bool RenderFlowBuilder::addFeaturePass(std::string_view feature_name,
                                       std::string name,
                                       std::unique_ptr<IRenderPass> pass,
                                       int32_t priority)
{
    if (feature_name.empty()) {
        clearError();
        setError("RenderFlow feature pass owner must not be empty");
        return false;
    }

    return addPassImpl(std::string(feature_name), std::move(name), std::move(pass), priority);
}

bool RenderFlowBuilder::addPassImpl(std::string owner_feature,
                                    std::string name,
                                    std::unique_ptr<IRenderPass> pass,
                                    int32_t priority)
{
    clearError();
    const std::string pass_name = name;
    if (name.empty()) {
        setError("RenderFlow pass name must not be empty");
        return false;
    }
    if (!pass) {
        setError("RenderFlow pass '" + pass_name + "' is null");
        return false;
    }
    if (contains(name)) {
        setError("RenderFlow pass '" + pass_name + "' already exists");
        return false;
    }

    m_nodes.push_back(Node{
        .name = std::move(name),
        .owner_feature = std::move(owner_feature),
        .pass = std::move(pass),
        .priority = priority,
        .registration_index = m_next_registration_index++,
    });
    return true;
}

bool RenderFlowBuilder::addDependency(std::string_view before_name, std::string_view after_name)
{
    clearError();
    if (before_name.empty() || after_name.empty()) {
        setError("RenderFlow dependency pass names must not be empty");
        return false;
    }
    if (before_name == after_name) {
        setError("RenderFlow pass '" + std::string(before_name) + "' cannot depend on itself");
        return false;
    }
    if (!contains(before_name)) {
        setError("RenderFlow dependency source pass '" + std::string(before_name) + "' does not exist");
        return false;
    }
    if (!contains(after_name)) {
        setError("RenderFlow dependency target pass '" + std::string(after_name) + "' does not exist");
        return false;
    }
    if (hasDependency(before_name, after_name)) {
        setError("RenderFlow dependency already exists: '" + std::string(before_name) + "' -> '" +
                 std::string(after_name) + "'");
        return false;
    }

    m_edges.push_back(Edge{
        .before = std::string(before_name),
        .after = std::string(after_name),
    });
    return true;
}

bool RenderFlowBuilder::insertPassBefore(std::string_view anchor_name,
                                         std::string name,
                                         std::unique_ptr<IRenderPass> pass,
                                         int32_t priority)
{
    clearError();
    if (!contains(anchor_name)) {
        setError("RenderFlow anchor pass '" + std::string(anchor_name) + "' does not exist");
        return false;
    }

    const std::string inserted_name = name;
    if (!addPass(name, std::move(pass), priority)) {
        return false;
    }

    if (!addDependency(inserted_name, anchor_name)) {
        removePass(inserted_name);
        return false;
    }
    return true;
}

bool RenderFlowBuilder::insertPassAfter(std::string_view anchor_name,
                                        std::string name,
                                        std::unique_ptr<IRenderPass> pass,
                                        int32_t priority)
{
    clearError();
    if (!contains(anchor_name)) {
        setError("RenderFlow anchor pass '" + std::string(anchor_name) + "' does not exist");
        return false;
    }

    const std::string inserted_name = name;
    if (!addPass(name, std::move(pass), priority)) {
        return false;
    }

    if (!addDependency(anchor_name, inserted_name)) {
        removePass(inserted_name);
        return false;
    }
    return true;
}

bool RenderFlowBuilder::insertPassBetween(std::string_view after_name,
                                          std::string_view before_name,
                                          std::string name,
                                          std::unique_ptr<IRenderPass> pass,
                                          int32_t priority)
{
    clearError();
    if (!contains(after_name)) {
        setError("RenderFlow after-anchor pass '" + std::string(after_name) + "' does not exist");
        return false;
    }
    if (!contains(before_name)) {
        setError("RenderFlow before-anchor pass '" + std::string(before_name) + "' does not exist");
        return false;
    }

    const std::string inserted_name = name;
    if (!addPass(name, std::move(pass), priority)) {
        return false;
    }

    if (!addDependency(after_name, inserted_name) || !addDependency(inserted_name, before_name)) {
        removePass(inserted_name);
        return false;
    }
    return true;
}

bool RenderFlowBuilder::insertFeaturePassBefore(std::string_view feature_name,
                                                std::string_view anchor_name,
                                                std::string name,
                                                std::unique_ptr<IRenderPass> pass,
                                                int32_t priority)
{
    clearError();
    if (!contains(anchor_name)) {
        setError("RenderFlow anchor pass '" + std::string(anchor_name) + "' does not exist");
        return false;
    }

    const std::string inserted_name = name;
    if (!addFeaturePass(feature_name, name, std::move(pass), priority)) {
        return false;
    }

    if (!addDependency(inserted_name, anchor_name)) {
        removePass(inserted_name);
        return false;
    }
    return true;
}

bool RenderFlowBuilder::insertFeaturePassAfter(std::string_view feature_name,
                                               std::string_view anchor_name,
                                               std::string name,
                                               std::unique_ptr<IRenderPass> pass,
                                               int32_t priority)
{
    clearError();
    if (!contains(anchor_name)) {
        setError("RenderFlow anchor pass '" + std::string(anchor_name) + "' does not exist");
        return false;
    }

    const std::string inserted_name = name;
    if (!addFeaturePass(feature_name, name, std::move(pass), priority)) {
        return false;
    }

    if (!addDependency(anchor_name, inserted_name)) {
        removePass(inserted_name);
        return false;
    }
    return true;
}

bool RenderFlowBuilder::insertFeaturePassBetween(std::string_view feature_name,
                                                 std::string_view after_name,
                                                 std::string_view before_name,
                                                 std::string name,
                                                 std::unique_ptr<IRenderPass> pass,
                                                 int32_t priority)
{
    clearError();
    if (!contains(after_name)) {
        setError("RenderFlow after-anchor pass '" + std::string(after_name) + "' does not exist");
        return false;
    }
    if (!contains(before_name)) {
        setError("RenderFlow before-anchor pass '" + std::string(before_name) + "' does not exist");
        return false;
    }

    const std::string inserted_name = name;
    if (!addFeaturePass(feature_name, name, std::move(pass), priority)) {
        return false;
    }

    if (!addDependency(after_name, inserted_name) || !addDependency(inserted_name, before_name)) {
        removePass(inserted_name);
        return false;
    }
    return true;
}

bool RenderFlowBuilder::replacePass(std::string_view name, std::unique_ptr<IRenderPass> pass)
{
    clearError();
    if (name.empty()) {
        setError("RenderFlow replacement pass name must not be empty");
        return false;
    }
    if (!pass) {
        setError("RenderFlow replacement pass for '" + std::string(name) + "' is null");
        return false;
    }

    auto node = findNode(name);
    if (node == m_nodes.end()) {
        setError("RenderFlow pass '" + std::string(name) + "' does not exist");
        return false;
    }

    node->pass = std::move(pass);
    return true;
}

bool RenderFlowBuilder::removePass(std::string_view name)
{
    clearError();
    if (name.empty()) {
        setError("RenderFlow pass name must not be empty");
        return false;
    }

    auto node = findNode(name);
    if (node == m_nodes.end()) {
        setError("RenderFlow pass '" + std::string(name) + "' does not exist");
        return false;
    }

    removeDependenciesFor(name);
    m_nodes.erase(node);
    return true;
}

void RenderFlowBuilder::clear() noexcept
{
    m_nodes.clear();
    m_edges.clear();
    clearError();
    m_next_registration_index = 0;
}

RenderFlowBuilder::NodeList::iterator RenderFlowBuilder::findNode(std::string_view name) noexcept
{
    return std::find_if(m_nodes.begin(), m_nodes.end(), [name](const Node& node) {
        return node.name == name;
    });
}

RenderFlowBuilder::NodeList::const_iterator RenderFlowBuilder::findNode(std::string_view name) const noexcept
{
    return std::find_if(m_nodes.begin(), m_nodes.end(), [name](const Node& node) {
        return node.name == name;
    });
}

bool RenderFlowBuilder::canInsert(std::string_view name, const std::unique_ptr<IRenderPass>& pass) const noexcept
{
    return pass && !name.empty() && !contains(name);
}

bool RenderFlowBuilder::hasDependency(std::string_view before_name, std::string_view after_name) const noexcept
{
    return std::any_of(m_edges.begin(), m_edges.end(), [before_name, after_name](const Edge& edge) {
        return edge.before == before_name && edge.after == after_name;
    });
}

void RenderFlowBuilder::removeDependenciesFor(std::string_view name) noexcept
{
    std::erase_if(m_edges, [name](const Edge& edge) {
        return edge.before == name || edge.after == name;
    });
}

void RenderFlowBuilder::clearError() const noexcept
{
    m_last_error.clear();
}

void RenderFlowBuilder::setError(std::string error) const
{
    m_last_error = std::move(error);
}

} // namespace luna::render_flow
