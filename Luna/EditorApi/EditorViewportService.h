#pragma once

#include "EditorApi/EditorTypes.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace luna::editor {

class Ui;

struct ViewportPresentation {
    TextureView scene_texture;
    UVec2 framebuffer_size{};
    bool presentable{false};
};

struct SceneViewportDrawOptions {
    bool preserve_aspect{true};
    bool fill_available{true};
    Vec2 requested_size{};
};

struct SceneViewportDrawResult {
    ViewportPresentation presentation;
    Vec2 drawn_size{};
    Vec2 mouse_drag_delta{};
    Vec2 mouse_wheel_delta{};
    bool drawn{false};
    bool hovered{false};
    bool clicked{false};
    bool double_clicked{false};
    bool dragging{false};
};

enum class SceneViewportPreviewMesh : uint32_t {
    Sphere,
    Cube,
    Plane,
};

enum class SceneViewportPreviewBackground : uint32_t {
    SolidColor,
    ProceduralSky,
    EnvironmentMap,
};

struct SceneViewportPreviewEnvironment {
    SceneViewportPreviewBackground background{SceneViewportPreviewBackground::ProceduralSky};
    Vec3 background_color{0.10f, 0.10f, 0.12f};
    bool ibl_enabled{true};
    AssetHandle environment_map{0};
    float intensity{1.0f};
    float sky_intensity{1.0f};
    float diffuse_intensity{1.0f};
    float specular_intensity{1.0f};
    Vec3 procedural_sun_direction{0.4f, 0.6f, 0.3f};
    float procedural_sun_intensity{20.0f};
    float procedural_sun_angular_radius{0.02f};
    Vec3 procedural_sky_color_zenith{0.16f, 0.32f, 0.65f};
    Vec3 procedural_sky_color_horizon{0.55f, 0.70f, 0.95f};
    Vec3 procedural_ground_color{0.18f, 0.17f, 0.15f};
    float procedural_sky_exposure{1.5f};
};

struct SceneViewportPreviewState {
    AssetHandle material{0};
    AssetHandle mesh{0};
    SceneViewportPreviewMesh mesh_kind{SceneViewportPreviewMesh::Sphere};
    SceneViewportPreviewEnvironment environment{};
    bool override_camera{false};
    Vec3 camera_position{0.0f, 0.35f, 3.1f};
    Vec3 camera_target{0.0f, 0.0f, 0.0f};
    float camera_vertical_fov_degrees{45.0f};
    float camera_near_clip{0.05f};
    float camera_far_clip{100.0f};
};

struct TextureViewportPresentation {
    TextureView texture;
    UVec2 framebuffer_size{};
    bool presentable{false};
};

struct TextureViewportDrawOptions {
    bool preserve_aspect{true};
    bool fill_available{true};
};

struct TextureViewportDrawResult {
    TextureViewportPresentation presentation;
    Vec2 drawn_size{};
    Vec2 mouse_drag_delta{};
    Vec2 mouse_wheel_delta{};
    bool drawn{false};
    bool hovered{false};
    bool clicked{false};
    bool double_clicked{false};
    bool dragging{false};
};

class ViewportService {
public:
    virtual ~ViewportService() = default;

    virtual ViewportId defaultSceneViewport() const noexcept = 0;
    virtual ViewportId createSceneViewport(std::string_view debug_name = {}) = 0;
    virtual void destroySceneViewport(ViewportId viewport_id) = 0;
    virtual bool isSceneViewportValid(ViewportId viewport_id) const noexcept = 0;
    virtual ViewportPresentation syncSceneViewport(ViewportId viewport_id, UVec2 framebuffer_size) = 0;
    virtual TextureView sceneTextureView(ViewportId viewport_id) const = 0;
    virtual bool setSceneViewportPreview(ViewportId viewport_id, const SceneViewportPreviewState& state) = 0;
    virtual void clearSceneViewportPreview(ViewportId viewport_id) = 0;

    virtual ViewportPresentation syncSceneViewport(UVec2 framebuffer_size) = 0;
    virtual TextureView sceneTextureView() const = 0;
    virtual void drawDefaultSceneViewport(Ui& ui) = 0;
    virtual SceneViewportDrawResult drawSceneViewport(Ui& ui,
                                                      ViewportId viewport_id,
                                                      SceneViewportDrawOptions options = {}) = 0;

    virtual ViewportId createTextureViewport(std::string_view debug_name = {}) = 0;
    virtual void destroyTextureViewport(ViewportId viewport_id) = 0;
    virtual bool isTextureViewportValid(ViewportId viewport_id) const noexcept = 0;
    virtual TextureViewportPresentation
        syncTextureViewport(ViewportId viewport_id, TextureView texture, UVec2 framebuffer_size) = 0;
    virtual TextureViewportPresentation textureViewportPresentation(ViewportId viewport_id) const = 0;
    virtual TextureViewportDrawResult drawTextureViewport(Ui& ui,
                                                          ViewportId viewport_id,
                                                          TextureView texture,
                                                          TextureViewportDrawOptions options = {}) = 0;

    virtual Vec3 editorCameraPosition() const noexcept = 0;
    virtual std::string gizmoOperationName() const = 0;
    virtual std::string gizmoModeName() const = 0;

    virtual bool pickDebugVisualizationEnabled() const noexcept = 0;
    virtual void setPickDebugVisualizationEnabled(bool enabled) = 0;
    virtual bool editorGridEnabled() const noexcept = 0;
    virtual void setEditorGridEnabled(bool enabled) = 0;
};

} // namespace luna::editor
