#include "Renderer/RenderViewState.h"

#include "Math/Math.h"
#include "Renderer/Camera.h"

#include <glm/matrix.hpp>

namespace luna {
namespace {

constexpr float kFallbackAspectRatio = 1.0f;
constexpr uint32_t kHaltonJitterSampleCount = 8;

glm::mat4 adjustProjectionForConventions(glm::mat4 projection, const luna::RHI::RHIConventions& conventions)
{
    return conventions.requires_projection_y_flip ? luna::flipProjectionY(projection) : projection;
}

float halton(uint32_t index, uint32_t base)
{
    float result = 0.0f;
    float fraction = 1.0f / static_cast<float>(base);
    while (index > 0) {
        result += static_cast<float>(index % base) * fraction;
        index /= base;
        fraction /= static_cast<float>(base);
    }
    return result;
}

uint32_t haltonJitterSampleIndex(uint64_t temporal_frame_index)
{
    return static_cast<uint32_t>(temporal_frame_index % kHaltonJitterSampleCount) + 1;
}

glm::vec2 haltonJitterPixels(uint32_t sample_index)
{
    return glm::vec2(halton(sample_index, 2), halton(sample_index, 3)) - glm::vec2(0.5f);
}

glm::vec2 jitterPixelsToNdc(glm::vec2 jitter_pixels, uint32_t framebuffer_width, uint32_t framebuffer_height)
{
    if (framebuffer_width == 0 || framebuffer_height == 0) {
        return glm::vec2(0.0f);
    }

    return glm::vec2(2.0f * jitter_pixels.x / static_cast<float>(framebuffer_width),
                     -2.0f * jitter_pixels.y / static_cast<float>(framebuffer_height));
}

glm::mat4 applyProjectionJitter(glm::mat4 projection, glm::vec2 jitter_ndc)
{
    for (int column = 0; column < 4; ++column) {
        projection[column][0] += jitter_ndc.x * projection[column][3];
        projection[column][1] += jitter_ndc.y * projection[column][3];
    }
    return projection;
}

RenderViewMatrices buildViewMatrices(const Camera& camera,
                                     const luna::RHI::RHICapabilities& capabilities,
                                     uint32_t framebuffer_width,
                                     uint32_t framebuffer_height,
                                     glm::vec2 jitter_ndc = glm::vec2(0.0f))
{
    const float aspect_ratio = framebuffer_height > 0
                                   ? static_cast<float>(framebuffer_width) / static_cast<float>(framebuffer_height)
                                   : kFallbackAspectRatio;

    RenderViewMatrices matrices{};
    matrices.view = camera.getViewMatrix();
    matrices.projection = adjustProjectionForConventions(camera.getProjectionMatrix(aspect_ratio),
                                                         capabilities.conventions);
    matrices.projection = applyProjectionJitter(matrices.projection, jitter_ndc);
    matrices.view_projection = matrices.projection * matrices.view;
    matrices.inverse_view = glm::inverse(matrices.view);
    matrices.inverse_projection = glm::inverse(matrices.projection);
    matrices.inverse_view_projection = glm::inverse(matrices.view_projection);
    return matrices;
}

} // namespace

RenderViewFrameState RenderViewHistory::beginFrame(const Camera& camera,
                                                   const luna::RHI::RHICapabilities& capabilities,
                                                   uint64_t frame_index,
                                                   uint32_t framebuffer_width,
                                                   uint32_t framebuffer_height,
                                                   bool invalidate_history)
{
    const glm::uvec2 viewport_size{framebuffer_width, framebuffer_height};
    const uint64_t temporal_frame_index = m_next_temporal_frame_index;
    const uint32_t jitter_sample_index = haltonJitterSampleIndex(temporal_frame_index);
    const glm::vec2 jitter_pixels = haltonJitterPixels(jitter_sample_index);
    const glm::vec2 jitter_ndc = jitterPixelsToNdc(jitter_pixels, framebuffer_width, framebuffer_height);
    const RenderViewMatrices current_matrices =
        buildViewMatrices(camera, capabilities, framebuffer_width, framebuffer_height);
    const RenderViewMatrices current_jittered_matrices =
        buildViewMatrices(camera, capabilities, framebuffer_width, framebuffer_height, jitter_ndc);
    const bool previous_frame_compatible =
        m_has_previous_frame && !invalidate_history && m_previous_viewport_size == viewport_size;

    m_pending_frame = RenderViewFrameState{
        .current = current_matrices,
        .previous = previous_frame_compatible ? m_previous_matrices : current_matrices,
        .current_jittered = current_jittered_matrices,
        .previous_jittered = previous_frame_compatible ? m_previous_jittered_matrices : current_jittered_matrices,
        .viewport_size = viewport_size,
        .previous_viewport_size = previous_frame_compatible ? m_previous_viewport_size : viewport_size,
        .jitter_pixels = jitter_pixels,
        .previous_jitter_pixels = previous_frame_compatible ? m_previous_jitter_pixels : jitter_pixels,
        .jitter_ndc = jitter_ndc,
        .previous_jitter_ndc = previous_frame_compatible ? m_previous_jitter_ndc : jitter_ndc,
        .frame_index = frame_index,
        .previous_frame_index = previous_frame_compatible ? m_previous_frame_index : frame_index,
        .temporal_frame_index = temporal_frame_index,
        .previous_temporal_frame_index =
            previous_frame_compatible ? m_previous_temporal_frame_index : temporal_frame_index,
        .jitter_sample_index = jitter_sample_index,
        .previous_jitter_sample_index =
            previous_frame_compatible ? m_previous_jitter_sample_index : jitter_sample_index,
        .history_valid = previous_frame_compatible,
    };
    m_has_pending_frame = true;
    return m_pending_frame;
}

void RenderViewHistory::commitFrame() noexcept
{
    if (!m_has_pending_frame) {
        return;
    }

    m_previous_matrices = m_pending_frame.current;
    m_previous_jittered_matrices = m_pending_frame.current_jittered;
    m_previous_viewport_size = m_pending_frame.viewport_size;
    m_previous_jitter_pixels = m_pending_frame.jitter_pixels;
    m_previous_jitter_ndc = m_pending_frame.jitter_ndc;
    m_previous_frame_index = m_pending_frame.frame_index;
    m_previous_temporal_frame_index = m_pending_frame.temporal_frame_index;
    m_previous_jitter_sample_index = m_pending_frame.jitter_sample_index;
    m_next_temporal_frame_index = m_pending_frame.temporal_frame_index + 1;
    m_has_previous_frame = true;
    m_has_pending_frame = false;
}

void RenderViewHistory::reset() noexcept
{
    m_pending_frame = {};
    m_previous_matrices = {};
    m_previous_jittered_matrices = {};
    m_previous_viewport_size = {};
    m_previous_jitter_pixels = {};
    m_previous_jitter_ndc = {};
    m_previous_frame_index = 0;
    m_previous_temporal_frame_index = 0;
    m_next_temporal_frame_index = 0;
    m_previous_jitter_sample_index = 0;
    m_has_previous_frame = false;
    m_has_pending_frame = false;
}

} // namespace luna
