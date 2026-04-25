#pragma once

#include "Renderer/RenderFlow/RenderPass.h"

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace luna::render_flow {

class RenderFlowBuilder final {
public:
    struct Entry {
        std::string name;
        std::unique_ptr<IRenderPass> pass;
    };

    using PassList = std::vector<Entry>;

    [[nodiscard]] bool contains(std::string_view name) const noexcept;
    [[nodiscard]] IRenderPass* find(std::string_view name) const noexcept;
    [[nodiscard]] std::span<const Entry> passes() const noexcept;

    bool addPass(std::string name, std::unique_ptr<IRenderPass> pass);
    bool insertPassBefore(std::string_view anchor_name, std::string name, std::unique_ptr<IRenderPass> pass);
    bool insertPassAfter(std::string_view anchor_name, std::string name, std::unique_ptr<IRenderPass> pass);
    bool replacePass(std::string_view name, std::unique_ptr<IRenderPass> pass);
    bool removePass(std::string_view name) noexcept;
    void clear() noexcept;

private:
    [[nodiscard]] PassList::iterator findEntry(std::string_view name) noexcept;
    [[nodiscard]] PassList::const_iterator findEntry(std::string_view name) const noexcept;
    [[nodiscard]] bool canInsert(std::string_view name, const std::unique_ptr<IRenderPass>& pass) const noexcept;

private:
    PassList m_passes;
};

} // namespace luna::render_flow
