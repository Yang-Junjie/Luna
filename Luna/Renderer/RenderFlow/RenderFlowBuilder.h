#pragma once

#include "Renderer/RenderFlow/RenderPass.h"

#include <cstdint>

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace luna::render_flow {

class RenderFlowBuilder final {
public:
    struct OrderedPass {
        std::string_view name;
        IRenderPass* pass{nullptr};
    };

    struct CompileResult {
        bool success{false};
        std::string error;
        std::vector<OrderedPass> passes;
    };

    [[nodiscard]] bool contains(std::string_view name) const noexcept;
    [[nodiscard]] IRenderPass* find(std::string_view name) const noexcept;
    [[nodiscard]] std::vector<std::string_view> passNames() const;
    [[nodiscard]] std::vector<std::string_view> passesForFeature(std::string_view feature_name) const;
    [[nodiscard]] std::string_view passOwner(std::string_view name) const noexcept;
    [[nodiscard]] std::string_view lastError() const noexcept;
    [[nodiscard]] CompileResult compile() const;

    bool addPass(std::string name, std::unique_ptr<IRenderPass> pass, int32_t priority = 0);
    bool addFeaturePass(std::string_view feature_name,
                        std::string name,
                        std::unique_ptr<IRenderPass> pass,
                        int32_t priority = 0);
    bool addDependency(std::string_view before_name, std::string_view after_name);
    bool insertPassBefore(std::string_view anchor_name,
                          std::string name,
                          std::unique_ptr<IRenderPass> pass,
                          int32_t priority = 0);
    bool insertPassAfter(std::string_view anchor_name,
                         std::string name,
                         std::unique_ptr<IRenderPass> pass,
                         int32_t priority = 0);
    bool insertPassBetween(std::string_view after_name,
                           std::string_view before_name,
                           std::string name,
                           std::unique_ptr<IRenderPass> pass,
                           int32_t priority = 0);
    bool insertFeaturePassBefore(std::string_view feature_name,
                                 std::string_view anchor_name,
                                 std::string name,
                                 std::unique_ptr<IRenderPass> pass,
                                 int32_t priority = 0);
    bool insertFeaturePassAfter(std::string_view feature_name,
                                std::string_view anchor_name,
                                std::string name,
                                std::unique_ptr<IRenderPass> pass,
                                int32_t priority = 0);
    bool insertFeaturePassBetween(std::string_view feature_name,
                                  std::string_view after_name,
                                  std::string_view before_name,
                                  std::string name,
                                  std::unique_ptr<IRenderPass> pass,
                                  int32_t priority = 0);
    bool replacePass(std::string_view name, std::unique_ptr<IRenderPass> pass);
    bool removePass(std::string_view name);
    void clear() noexcept;

private:
    struct Node {
        std::string name;
        std::string owner_feature;
        std::unique_ptr<IRenderPass> pass;
        int32_t priority{0};
        uint64_t registration_index{0};
    };

    struct Edge {
        std::string before;
        std::string after;
    };

    using NodeList = std::vector<Node>;

    [[nodiscard]] NodeList::iterator findNode(std::string_view name) noexcept;
    [[nodiscard]] NodeList::const_iterator findNode(std::string_view name) const noexcept;
    [[nodiscard]] bool canInsert(std::string_view name, const std::unique_ptr<IRenderPass>& pass) const noexcept;
    bool addPassImpl(std::string owner_feature, std::string name, std::unique_ptr<IRenderPass> pass, int32_t priority);
    [[nodiscard]] bool hasDependency(std::string_view before_name, std::string_view after_name) const noexcept;
    void removeDependenciesFor(std::string_view name) noexcept;
    void clearError() const noexcept;
    void setError(std::string error) const;

private:
    NodeList m_nodes;
    std::vector<Edge> m_edges;
    mutable std::string m_last_error;
    uint64_t m_next_registration_index{0};
};

} // namespace luna::render_flow
