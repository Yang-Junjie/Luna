#pragma once

#include <Capabilities.h>
#include <Core.h>

#include <cstdint>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

namespace luna {

class Camera;

struct RenderViewMatrices {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::mat4 view_projection{1.0f};
    glm::mat4 inverse_view{1.0f};
    glm::mat4 inverse_projection{1.0f};
    glm::mat4 inverse_view_projection{1.0f};
};

struct RenderViewFrameState {
    RenderViewMatrices current;
    RenderViewMatrices previous;
    RenderViewMatrices current_jittered;
    RenderViewMatrices previous_jittered;
    glm::uvec2 viewport_size{0, 0};
    glm::uvec2 previous_viewport_size{0, 0};
    glm::vec2 jitter_pixels{0.0f};
    glm::vec2 previous_jitter_pixels{0.0f};
    glm::vec2 jitter_ndc{0.0f};
    glm::vec2 previous_jitter_ndc{0.0f};
    uint64_t frame_index{0};
    uint64_t previous_frame_index{0};
    uint64_t temporal_frame_index{0};
    uint64_t previous_temporal_frame_index{0};
    uint32_t jitter_sample_index{0};
    uint32_t previous_jitter_sample_index{0};
    bool history_valid{false};
};

class RenderViewHistory final {
public:
    [[nodiscard]] RenderViewFrameState beginFrame(const Camera& camera,
                                                  const luna::RHI::RHICapabilities& capabilities,
                                                  uint64_t frame_index,
                                                  uint32_t framebuffer_width,
                                                  uint32_t framebuffer_height,
                                                  bool invalidate_history);
    void commitFrame() noexcept;
    void reset() noexcept;

private:
    RenderViewFrameState m_pending_frame{};
    RenderViewMatrices m_previous_matrices{};
    RenderViewMatrices m_previous_jittered_matrices{};
    glm::uvec2 m_previous_viewport_size{0, 0};
    glm::vec2 m_previous_jitter_pixels{0.0f};
    glm::vec2 m_previous_jitter_ndc{0.0f};
    uint64_t m_previous_frame_index{0};
    uint64_t m_previous_temporal_frame_index{0};
    uint64_t m_next_temporal_frame_index{0};
    uint32_t m_previous_jitter_sample_index{0};
    bool m_has_previous_frame{false};
    bool m_has_pending_frame{false};
};

} // namespace luna
