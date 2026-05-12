#pragma once

#include "EditorApi/EditorTypes.h"

#include <cstddef>
#include <string>

namespace luna::editor {

class ViewportService {
public:
    virtual ~ViewportService() = default;

    virtual bool isRuntimeViewportEnabled() const noexcept = 0;
    virtual bool isRuntimeViewportRequested() const noexcept = 0;
    virtual void setRuntimeViewportRequested(bool enabled) = 0;
    virtual size_t runtimeEntityCount() const noexcept = 0;

    virtual Vec3 editorCameraPosition() const noexcept = 0;
    virtual std::string gizmoOperationName() const = 0;
    virtual std::string gizmoModeName() const = 0;

    virtual bool pickDebugVisualizationEnabled() const noexcept = 0;
    virtual void setPickDebugVisualizationEnabled(bool enabled) = 0;
    virtual bool editorGridEnabled() const noexcept = 0;
    virtual void setEditorGridEnabled(bool enabled) = 0;
};

} // namespace luna::editor
