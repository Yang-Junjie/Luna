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
    bool drawn{false};
    bool hovered{false};
    bool clicked{false};
    bool double_clicked{false};
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
    bool drawn{false};
    bool hovered{false};
    bool clicked{false};
    bool double_clicked{false};
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
