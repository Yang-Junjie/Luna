#pragma once

#include "EditorApi/EditorViewportService.h"
#include "Viewport/SceneViewportInstanceManager.h"
#include "Viewport/TextureViewportInstanceManager.h"
#include "Viewport/ViewportInteraction.h"

#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace luna {

class Renderer;

class EditorViewportCoordinator final {
public:
    [[nodiscard]] editor::ViewportId defaultSceneViewportId() const noexcept;
    [[nodiscard]] static constexpr editor::ViewportId runtimeSceneViewportId() noexcept
    {
        return (std::numeric_limits<editor::ViewportId>::max)();
    }

    [[nodiscard]] editor::ViewportId activeSceneViewportId(bool runtime_viewport_enabled) const noexcept;
    [[nodiscard]] SceneViewportInstance& activeSceneViewport(bool runtime_viewport_enabled) noexcept;
    [[nodiscard]] const SceneViewportInstance& activeSceneViewport(bool runtime_viewport_enabled) const noexcept;

    [[nodiscard]] editor::ViewportId createSceneViewport(std::string_view owner_id = {});
    bool destroySceneViewport(editor::ViewportId viewport_id, Renderer& renderer);
    [[nodiscard]] bool isSceneViewportValid(editor::ViewportId viewport_id) const noexcept;
    [[nodiscard]] SceneViewportInstance* findSceneViewport(editor::ViewportId viewport_id) noexcept;
    [[nodiscard]] const SceneViewportInstance* findSceneViewport(editor::ViewportId viewport_id) const noexcept;
    void clearPluginSceneViewports(Renderer& renderer);

    [[nodiscard]] editor::ViewportId createTextureViewport(std::string_view owner_id = {});
    bool destroyTextureViewport(editor::ViewportId viewport_id);
    [[nodiscard]] bool isTextureViewportValid(editor::ViewportId viewport_id) const noexcept;
    [[nodiscard]] editor::TextureViewportPresentation syncTextureViewport(editor::ViewportId viewport_id,
                                                                          editor::TextureView texture,
                                                                          editor::UVec2 framebuffer_size);
    [[nodiscard]] editor::TextureViewportPresentation textureViewportPresentation(editor::ViewportId viewport_id) const;
    void clearTextureViewports();

    [[nodiscard]] std::vector<editor::ViewportId> destroyViewportsForOwner(std::string_view owner_id,
                                                                           Renderer& renderer);

    void beginFrameInteractions() noexcept;
    void clearInteractions() noexcept;
    void clearViewportInteraction(editor::ViewportId viewport_id);
    [[nodiscard]] const ViewportInteractionState&
        recordViewportSurfaceInteraction(editor::ViewportId viewport_id,
                                         std::string_view owner_id,
                                         const ViewportInteractionInput& input);
    [[nodiscard]] bool isViewportInputAllowed(editor::ViewportId viewport_id) const noexcept;
    void setDefaultViewportMouseCaptured(bool captured);

private:
    [[nodiscard]] editor::ViewportId allocateViewportId() noexcept;
    void rememberOwner(editor::ViewportId viewport_id, std::string_view owner_id);

    SceneViewportInstanceManager m_scene_viewports;
    TextureViewportInstanceManager m_texture_viewports;
    ViewportInteractionTracker m_interactions;
    editor::ViewportId m_next_viewport_id{editor::kDefaultViewportId + 1u};
    std::unordered_map<editor::ViewportId, std::string> m_viewport_owner_by_id;
};

} // namespace luna
