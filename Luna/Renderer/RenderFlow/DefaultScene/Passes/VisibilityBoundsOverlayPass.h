#pragma once

#include "Renderer/RenderFlow/DefaultScene/DrawQueue.h"
#include "Renderer/RenderFlow/RenderPass.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace luna::render_flow::default_scene {

class PassSharedState;

inline constexpr float kVisibilityBoundsInvalidMarkerHalfExtent = 0.25f;

struct VisibilityBoundsOverlayVertex {
    glm::vec3 world_position{0.0f};
    glm::vec4 color{1.0f};
};

struct VisibilityBoundsOverlayBuildStats {
    uint32_t items{0};
    uint32_t bounds{0};
    uint32_t invalid_markers{0};
    uint32_t frustums{0};
    uint32_t vertices{0};
};

struct VisibilityBoundsOverlayStats {
    VisibilityBoundsOverlayBuildStats build{};
    uint64_t vertex_buffer_bytes{0};
    bool resources_ready{false};
};

[[nodiscard]] glm::vec4 visibilityBoundsOverlayColor(VisibilityDebugClassification classification) noexcept;

[[nodiscard]] VisibilityBoundsOverlayBuildStats buildVisibilityBoundsOverlayVertices(
    std::span<const VisibilityDebugItem> items,
    std::span<const VisibilityDebugFrustumItem> frustums,
    std::vector<VisibilityBoundsOverlayVertex>& vertices);

class VisibilityBoundsOverlayResources final {
public:
    VisibilityBoundsOverlayResources();
    ~VisibilityBoundsOverlayResources();

    void resetFrameStats() noexcept;
    void shutdown();
    [[nodiscard]] bool upload(const SceneRenderContext& context,
                              std::span<const VisibilityDebugItem> items,
                              std::span<const VisibilityDebugFrustumItem> frustums,
                              const glm::mat4& view_projection);
    void draw(RenderGraphRasterPassContext& pass_context, uint32_t vertex_count) const;

    [[nodiscard]] uint32_t vertexCount() const noexcept;
    [[nodiscard]] VisibilityBoundsOverlayStats stats() const noexcept;

private:
    struct State;

    [[nodiscard]] bool ensurePipeline(const SceneRenderContext& context);
    [[nodiscard]] bool ensureVertexBuffer(const SceneRenderContext& context, uint64_t required_bytes);

    std::unique_ptr<State> m_state;
};

class VisibilityBoundsOverlayPass final : public IRenderPass {
public:
    VisibilityBoundsOverlayPass(PassSharedState& state, VisibilityBoundsOverlayResources& resources);

    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::span<const RenderPassResourceUsage> resourceUsages() const noexcept override;
    void setup(RenderPassContext& context) override;

private:
    PassSharedState* m_state{nullptr};
    VisibilityBoundsOverlayResources* m_resources{nullptr};
};

} // namespace luna::render_flow::default_scene
