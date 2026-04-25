#include "Renderer/RenderFlow/RenderFlowBuilder.h"

#include <algorithm>
#include <utility>

namespace luna::render_flow {

bool RenderFlowBuilder::contains(std::string_view name) const noexcept
{
    return findEntry(name) != m_passes.end();
}

IRenderPass* RenderFlowBuilder::find(std::string_view name) const noexcept
{
    const auto entry = findEntry(name);
    return entry != m_passes.end() ? entry->pass.get() : nullptr;
}

std::span<const RenderFlowBuilder::Entry> RenderFlowBuilder::passes() const noexcept
{
    return m_passes;
}

bool RenderFlowBuilder::addPass(std::string name, std::unique_ptr<IRenderPass> pass)
{
    if (!canInsert(name, pass)) {
        return false;
    }

    m_passes.push_back(Entry{std::move(name), std::move(pass)});
    return true;
}

bool RenderFlowBuilder::insertPassBefore(std::string_view anchor_name, std::string name, std::unique_ptr<IRenderPass> pass)
{
    if (!canInsert(name, pass)) {
        return false;
    }

    auto anchor = findEntry(anchor_name);
    if (anchor == m_passes.end()) {
        return false;
    }

    m_passes.insert(anchor, Entry{std::move(name), std::move(pass)});
    return true;
}

bool RenderFlowBuilder::insertPassAfter(std::string_view anchor_name, std::string name, std::unique_ptr<IRenderPass> pass)
{
    if (!canInsert(name, pass)) {
        return false;
    }

    auto anchor = findEntry(anchor_name);
    if (anchor == m_passes.end()) {
        return false;
    }

    m_passes.insert(std::next(anchor), Entry{std::move(name), std::move(pass)});
    return true;
}

bool RenderFlowBuilder::replacePass(std::string_view name, std::unique_ptr<IRenderPass> pass)
{
    if (!pass) {
        return false;
    }

    auto entry = findEntry(name);
    if (entry == m_passes.end()) {
        return false;
    }

    entry->pass = std::move(pass);
    return true;
}

bool RenderFlowBuilder::removePass(std::string_view name) noexcept
{
    auto entry = findEntry(name);
    if (entry == m_passes.end()) {
        return false;
    }

    m_passes.erase(entry);
    return true;
}

void RenderFlowBuilder::clear() noexcept
{
    m_passes.clear();
}

RenderFlowBuilder::PassList::iterator RenderFlowBuilder::findEntry(std::string_view name) noexcept
{
    return std::find_if(m_passes.begin(), m_passes.end(), [name](const Entry& entry) {
        return entry.name == name;
    });
}

RenderFlowBuilder::PassList::const_iterator RenderFlowBuilder::findEntry(std::string_view name) const noexcept
{
    return std::find_if(m_passes.begin(), m_passes.end(), [name](const Entry& entry) {
        return entry.name == name;
    });
}

bool RenderFlowBuilder::canInsert(std::string_view name, const std::unique_ptr<IRenderPass>& pass) const noexcept
{
    return pass && !name.empty() && !contains(name);
}

} // namespace luna::render_flow
