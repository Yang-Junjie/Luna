#pragma once

#include "Renderer/RenderFlow/RenderFlowTypes.h"
#include "Renderer/RenderWorld/RenderWorld.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace luna {
class RenderGraphBuilder;
} // namespace luna

namespace luna::render_flow {

class RenderPassBlackboard final {
public:
    void setTexture(std::string_view name, RenderGraphTextureHandle handle);
    [[nodiscard]] std::optional<RenderGraphTextureHandle> getTexture(std::string_view name) const;
    [[nodiscard]] bool hasTexture(std::string_view name) const;
    void clear() noexcept;

private:
    std::unordered_map<std::string, RenderGraphTextureHandle> m_textures;
};

class RenderPassContext {
public:
    RenderPassContext(RenderGraphBuilder& graph,
                      const RenderWorld& world,
                      const SceneRenderContext& scene_context,
                      RenderPassBlackboard& blackboard);

    [[nodiscard]] RenderGraphBuilder& graph() const noexcept;
    [[nodiscard]] const RenderWorld& world() const noexcept;
    [[nodiscard]] const SceneRenderContext& sceneContext() const noexcept;
    [[nodiscard]] RenderPassBlackboard& blackboard() const noexcept;

private:
    RenderGraphBuilder* m_graph{nullptr};
    const RenderWorld* m_world{nullptr};
    const SceneRenderContext* m_scene_context{nullptr};
    RenderPassBlackboard* m_blackboard{nullptr};
};

class IRenderPass {
public:
    virtual ~IRenderPass() = default;

    [[nodiscard]] virtual const char* name() const noexcept = 0;
    virtual void setup(RenderPassContext& context) = 0;
};

} // namespace luna::render_flow



