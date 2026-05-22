#pragma once

#include "EditorDocumentContext.h"

#include "Core/Timestep.h"
#include "Core/UUID.h"

#include <cstddef>
#include <memory>
#include <string>

namespace luna {

class IScriptRuntime;
class Scene;
class SceneRuntime;

class EditorRuntimeViewportController final {
public:
    EditorRuntimeViewportController();
    ~EditorRuntimeViewportController();

    EditorRuntimeViewportController(const EditorRuntimeViewportController&) = delete;
    EditorRuntimeViewportController& operator=(const EditorRuntimeViewportController&) = delete;

    [[nodiscard]] bool isRuntimeViewportEnabled() const noexcept;
    [[nodiscard]] bool isRuntimeViewportRequested() const noexcept;
    void setRuntimeViewportRequested(bool enabled) noexcept;

    [[nodiscard]] EditorDocumentContext& runtimeDocumentContext() noexcept;
    [[nodiscard]] const EditorDocumentContext& runtimeDocumentContext() const noexcept;

    [[nodiscard]] Scene& activeRenderScene(Scene& authoring_scene) noexcept;
    [[nodiscard]] const Scene& activeRenderScene(const Scene& authoring_scene) const noexcept;
    [[nodiscard]] size_t runtimeEntityCount() const noexcept;

    [[nodiscard]] bool setRuntimeViewportEnabled(bool enabled,
                                                 const Scene& authoring_scene,
                                                 std::unique_ptr<IScriptRuntime> script_runtime = {});
    void resetRuntimeViewport();
    void update(Timestep dt);
    void patchRuntimeScriptProperty(UUID entity_id, size_t script_index, size_t property_index);

private:
    bool beginRuntimeViewport(const Scene& authoring_scene, std::unique_ptr<IScriptRuntime> script_runtime);
    void endRuntimeViewport();

private:
    EditorDocumentContext m_runtime_document_context{"luna.document.runtime.scene",
                                                     EditorDocumentKind::RuntimeSceneSnapshot,
                                                     false};
    std::unique_ptr<Scene> m_runtime_scene;
    std::unique_ptr<SceneRuntime> m_runtime_scene_runtime;
    bool m_runtime_viewport_enabled{false};
    bool m_runtime_viewport_requested{false};
};

} // namespace luna
