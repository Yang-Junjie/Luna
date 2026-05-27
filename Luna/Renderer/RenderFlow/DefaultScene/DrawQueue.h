#pragma once

// Stores scene draw submissions for the current frame.
// Stores draw packets and light submissions for the current frame.
// Passes query packets by RenderPhase and can sort their local lists as needed.

#include "Renderer/Camera.h"
#include "Renderer/RenderWorld/RenderTypes.h"
#include "Renderer/Visibility/Frustum.h"

#include <cstddef>
#include <cstdint>

#include <array>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <optional>
#include <vector>

namespace luna {
class Material;
class Mesh;
} // namespace luna

namespace luna::render_flow::default_scene {

using DrawCommand = RenderDrawPacket;

struct DrawQueueStats {
    uint32_t submitted{0};
    uint32_t camera_visible{0};
    uint32_t camera_culled{0};
    uint32_t invalid_bounds{0};
    uint32_t shadow_unculled{0};
    uint32_t phase_depth_only{0};
    uint32_t phase_gbuffer{0};
    uint32_t phase_forward_opaque{0};
    uint32_t phase_transparent{0};
    uint32_t phase_shadow_caster{0};
    uint32_t phase_picking{0};
};

enum class VisibilityDebugClassification : uint8_t {
    CameraVisible,
    CameraCulled,
    InvalidBounds,
};

enum class DrawPacketVisibility : uint8_t {
    CameraVisible,
    CameraCulled,
};

struct DrawPacketVisibilityResult {
    DrawPacketVisibility visibility{DrawPacketVisibility::CameraVisible};
    bool invalid_bounds{false};

    [[nodiscard]] bool cameraVisible() const noexcept
    {
        return visibility == DrawPacketVisibility::CameraVisible;
    }
};

struct VisibilityDebugCaptureOptions {
    bool capture_visible_bounds{false};
    bool capture_culled_bounds{false};
    bool capture_culling_frustum{false};
    bool culling_frustum_frozen{false};

    [[nodiscard]] bool enabled() const noexcept
    {
        return capture_visible_bounds || capture_culled_bounds || capture_culling_frustum;
    }
};

struct VisibilityDebugItem {
    MeshBounds world_bounds{};
    glm::vec3 world_origin{0.0f};
    uint32_t picking_id{0};
    uint32_t submesh_index{UINT32_MAX};
    RenderPhaseMask phases{0};
    VisibilityDebugClassification classification{VisibilityDebugClassification::CameraVisible};
};

struct VisibilityDebugStats {
    uint32_t captured{0};
    uint32_t camera_visible{0};
    uint32_t camera_culled{0};
    uint32_t invalid_bounds{0};
    uint32_t culling_frustums{0};
};

struct VisibilityDebugFrustumItem {
    std::array<glm::vec3, 8> corners{};
    glm::vec4 color{0.20f, 0.62f, 1.0f, 0.95f};
    bool frozen{false};
};

class DrawQueue final {
public:
    void beginScene(const Camera& camera,
                    float aspect_ratio,
                    VisibilityDebugCaptureOptions visibility_debug_options = {});
    void beginScene(const Camera& camera,
                    float aspect_ratio,
                    const Camera& culling_camera,
                    float culling_aspect_ratio,
                    VisibilityDebugCaptureOptions visibility_debug_options = {});
    void clear() noexcept;
    void reserveForFrame(const DrawQueueStats& previous_stats);

    void submitDrawPacket(const RenderDrawPacket& packet);

    void sortBackToFront(std::vector<DrawCommand>& draw_commands) const;

    [[nodiscard]] const Camera& camera() const noexcept
    {
        return m_camera;
    }

    [[nodiscard]] const std::vector<DrawCommand>& drawCommands() const noexcept;
    [[nodiscard]] const std::vector<DrawCommand>& drawCommands(RenderPhase phase) const noexcept;

    [[nodiscard]] const DrawQueueStats& stats() const noexcept
    {
        return m_stats;
    }

    [[nodiscard]] const std::vector<VisibilityDebugItem>& visibilityDebugItems() const noexcept
    {
        return m_visibility_debug_items;
    }

    [[nodiscard]] const VisibilityDebugStats& visibilityDebugStats() const noexcept
    {
        return m_visibility_debug_stats;
    }

    [[nodiscard]] const std::vector<VisibilityDebugFrustumItem>& visibilityDebugFrustums() const noexcept
    {
        return m_visibility_debug_frustums;
    }

private:
    static constexpr std::size_t kRenderPhaseCount = static_cast<std::size_t>(RenderPhase::Picking) + 1u;

    [[nodiscard]] bool validateDrawPacket(const RenderDrawPacket& packet) const;
    [[nodiscard]] DrawPacketVisibilityResult classifyDrawPacketVisibility(const RenderDrawPacket& packet) const;
    void recordDrawPacketVisibility(const RenderDrawPacket& packet, const DrawPacketVisibilityResult& visibility);
    void cacheDrawCommandForPhases(const RenderDrawPacket& packet, bool camera_visible);
    void captureVisibilityDebugFrustum(const Camera& camera, float aspect_ratio);
    void captureVisibilityDebugItem(const RenderDrawPacket& packet, VisibilityDebugClassification classification);

    Camera m_camera{};
    Camera m_culling_camera{};
    Frustum m_culling_frustum{};
    std::vector<DrawCommand> m_camera_visible_draw_commands;
    std::array<std::vector<DrawCommand>, kRenderPhaseCount> m_draw_commands_by_phase;
    std::vector<VisibilityDebugItem> m_visibility_debug_items;
    std::vector<VisibilityDebugFrustumItem> m_visibility_debug_frustums;
    VisibilityDebugCaptureOptions m_visibility_debug_options{};
    VisibilityDebugStats m_visibility_debug_stats{};
    DrawQueueStats m_stats{};
};

} // namespace luna::render_flow::default_scene
