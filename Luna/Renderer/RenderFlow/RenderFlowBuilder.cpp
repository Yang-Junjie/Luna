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

RenderFlowBuilder::CompileResult RenderFlowBuilder::compile() const
{
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
            result.error = "RenderFlow dependency references a missing pass: '" + edge.before + "' -> '" + edge.after + "'";
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
        return result;
    }

    result.success = true;
    return result;
}

bool RenderFlowBuilder::addPass(std::string name, std::unique_ptr<IRenderPass> pass, int32_t priority)
{
    if (!canInsert(name, pass)) {
        return false;
    }

    m_nodes.push_back(Node{
        .name = std::move(name),
        .pass = std::move(pass),
        .priority = priority,
        .registration_index = m_next_registration_index++,
    });
    return true;
}

bool RenderFlowBuilder::addDependency(std::string_view before_name, std::string_view after_name)
{
    if (before_name.empty() || after_name.empty() || before_name == after_name || !contains(before_name) ||
        !contains(after_name) || hasDependency(before_name, after_name)) {
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
    if (!contains(anchor_name) || !addPass(name, std::move(pass), priority)) {
        return false;
    }

    return addDependency(m_nodes.back().name, anchor_name);
}

bool RenderFlowBuilder::insertPassAfter(std::string_view anchor_name,
                                        std::string name,
                                        std::unique_ptr<IRenderPass> pass,
                                        int32_t priority)
{
    if (!contains(anchor_name) || !addPass(name, std::move(pass), priority)) {
        return false;
    }

    return addDependency(anchor_name, m_nodes.back().name);
}

bool RenderFlowBuilder::replacePass(std::string_view name, std::unique_ptr<IRenderPass> pass)
{
    if (!pass) {
        return false;
    }

    auto node = findNode(name);
    if (node == m_nodes.end()) {
        return false;
    }

    node->pass = std::move(pass);
    return true;
}

bool RenderFlowBuilder::removePass(std::string_view name) noexcept
{
    auto node = findNode(name);
    if (node == m_nodes.end()) {
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

} // namespace luna::render_flow
