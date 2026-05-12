#pragma once

#include "EditorApi/EditorTypes.h"

#include <cstddef>
#include <string>

namespace luna::editor {

class Ui;

struct ViewportPresentation {
    TextureView scene_texture;
    UVec2 framebuffer_size{};
    bool presentable{false};
};

class ViewportService {
public:
    virtual ~ViewportService() = default;

    virtual ViewportPresentation syncSceneViewport(UVec2 framebuffer_size) = 0;
    virtual TextureView sceneTextureView() const = 0;
    virtual void drawDefaultSceneViewport(Ui& ui) = 0;

    virtual Vec3 editorCameraPosition() const noexcept = 0;
    virtual std::string gizmoOperationName() const = 0;
    virtual std::string gizmoModeName() const = 0;

    virtual bool pickDebugVisualizationEnabled() const noexcept = 0;
    virtual void setPickDebugVisualizationEnabled(bool enabled) = 0;
    virtual bool editorGridEnabled() const noexcept = 0;
    virtual void setEditorGridEnabled(bool enabled) = 0;
};

} // namespace luna::editor
