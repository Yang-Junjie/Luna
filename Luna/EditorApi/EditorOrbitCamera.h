#pragma once

#include "EditorApi/EditorTypes.h"
#include "EditorApi/EditorViewportService.h"

#include <cmath>

#include <algorithm>

namespace luna::editor {

struct OrbitCameraConfig {
    float orbit_sensitivity{0.008f};
    float zoom_sensitivity{0.16f};
    float min_pitch{-1.35f};
    float max_pitch{1.35f};
    float min_distance{1.2f};
    float max_distance{12.0f};
};

struct OrbitCameraState {
    float yaw{0.0f};
    float pitch{0.18f};
    float distance{3.1f};
    Vec3 target{0.0f, 0.0f, 0.0f};
    float vertical_fov_degrees{45.0f};
    float near_clip{0.05f};
    float far_clip{100.0f};
};

class OrbitCameraController {
public:
    explicit OrbitCameraController(OrbitCameraConfig config = {}) noexcept
        : m_config(config)
    {
        normalize();
    }

    [[nodiscard]] constexpr const OrbitCameraState& state() const noexcept
    {
        return m_state;
    }

    void setState(const OrbitCameraState& state) noexcept
    {
        m_state = state;
        normalize();
    }

    void reset(const OrbitCameraState& state = {}) noexcept
    {
        setState(state);
    }

    [[nodiscard]] Vec3 position() const noexcept
    {
        const float cos_pitch = std::cos(m_state.pitch);
        return Vec3{
            .x = m_state.target.x + m_state.distance * std::sin(m_state.yaw) * cos_pitch,
            .y = m_state.target.y + m_state.distance * std::sin(m_state.pitch),
            .z = m_state.target.z + m_state.distance * std::cos(m_state.yaw) * cos_pitch,
        };
    }

    [[nodiscard]] SceneViewportPreviewState applyTo(SceneViewportPreviewState preview_state) const noexcept
    {
        preview_state.override_camera = true;
        preview_state.camera_position = position();
        preview_state.camera_target = m_state.target;
        preview_state.camera_vertical_fov_degrees = m_state.vertical_fov_degrees;
        preview_state.camera_near_clip = m_state.near_clip;
        preview_state.camera_far_clip = m_state.far_clip;
        return preview_state;
    }

    bool updateFromViewport(const SceneViewportDrawResult& draw_result) noexcept
    {
        bool changed = false;
        if (draw_result.dragging) {
            m_state.yaw -= draw_result.mouse_drag_delta.x * m_config.orbit_sensitivity;
            m_state.pitch = std::clamp(m_state.pitch - draw_result.mouse_drag_delta.y * m_config.orbit_sensitivity,
                                       m_config.min_pitch,
                                       m_config.max_pitch);
            wrapYaw();
            changed = true;
        }

        if (draw_result.hovered && draw_result.mouse_wheel_delta.y != 0.0f) {
            const float zoom_base = std::max(0.01f, 1.0f - std::clamp(m_config.zoom_sensitivity, 0.0f, 0.99f));
            const float zoom_factor = std::pow(zoom_base, draw_result.mouse_wheel_delta.y);
            m_state.distance = std::clamp(m_state.distance * zoom_factor, m_config.min_distance, m_config.max_distance);
            changed = true;
        }

        return changed;
    }

private:
    static constexpr float kPi{3.14159265358979323846f};

    void normalize() noexcept
    {
        if (!std::isfinite(m_config.orbit_sensitivity) || m_config.orbit_sensitivity < 0.0f) {
            m_config.orbit_sensitivity = 0.008f;
        }
        if (!std::isfinite(m_config.zoom_sensitivity)) {
            m_config.zoom_sensitivity = 0.16f;
        }
        if (!std::isfinite(m_config.min_pitch)) {
            m_config.min_pitch = -1.35f;
        }
        if (!std::isfinite(m_config.max_pitch)) {
            m_config.max_pitch = 1.35f;
        }
        if (!std::isfinite(m_config.min_distance) || m_config.min_distance <= 0.0f) {
            m_config.min_distance = 1.2f;
        }
        if (!std::isfinite(m_config.max_distance) || m_config.max_distance <= 0.0f) {
            m_config.max_distance = 12.0f;
        }
        if (m_config.min_pitch > m_config.max_pitch) {
            std::swap(m_config.min_pitch, m_config.max_pitch);
        }
        if (m_config.min_distance > m_config.max_distance) {
            std::swap(m_config.min_distance, m_config.max_distance);
        }
        if (!std::isfinite(m_state.yaw)) {
            m_state.yaw = 0.0f;
        }
        if (!std::isfinite(m_state.pitch)) {
            m_state.pitch = 0.18f;
        }
        if (!std::isfinite(m_state.distance)) {
            m_state.distance = 3.1f;
        }
        if (!std::isfinite(m_state.target.x)) {
            m_state.target.x = 0.0f;
        }
        if (!std::isfinite(m_state.target.y)) {
            m_state.target.y = 0.0f;
        }
        if (!std::isfinite(m_state.target.z)) {
            m_state.target.z = 0.0f;
        }
        if (!std::isfinite(m_state.vertical_fov_degrees)) {
            m_state.vertical_fov_degrees = 45.0f;
        }
        if (!std::isfinite(m_state.near_clip) || m_state.near_clip <= 0.0f) {
            m_state.near_clip = 0.05f;
        }
        if (!std::isfinite(m_state.far_clip) || m_state.far_clip <= m_state.near_clip) {
            m_state.far_clip = 100.0f;
        }
        m_state.pitch = std::clamp(m_state.pitch, m_config.min_pitch, m_config.max_pitch);
        m_state.distance = std::clamp(m_state.distance, m_config.min_distance, m_config.max_distance);
        m_state.vertical_fov_degrees = std::clamp(m_state.vertical_fov_degrees, 1.0f, 120.0f);
        wrapYaw();
    }

    void wrapYaw() noexcept
    {
        while (m_state.yaw > kPi) {
            m_state.yaw -= kPi * 2.0f;
        }
        while (m_state.yaw < -kPi) {
            m_state.yaw += kPi * 2.0f;
        }
    }

    OrbitCameraConfig m_config{};
    OrbitCameraState m_state{};
};

} // namespace luna::editor
