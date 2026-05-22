#include "Viewport/EditorViewportCoordinator.h"

#include "Renderer/Renderer.h"

namespace {

std::string toOwnedString(std::string_view value)
{
    return std::string(value.data(), value.size());
}

luna::editor::TextureViewportPresentation toEditorTextureViewportPresentation(
    const luna::TextureViewportPresentation& presentation)
{
    return luna::editor::TextureViewportPresentation{
        .texture = presentation.texture,
        .framebuffer_size = presentation.framebuffer_size,
        .presentable = presentation.presentable,
    };
}

} // namespace

namespace luna {

editor::ViewportId EditorViewportCoordinator::defaultSceneViewportId() const noexcept
{
    return m_scene_viewports.defaultViewportId();
}

editor::ViewportId EditorViewportCoordinator::activeSceneViewportId(bool runtime_viewport_enabled) const noexcept
{
    return runtime_viewport_enabled ? runtimeSceneViewportId() : defaultSceneViewportId();
}

SceneViewportInstance& EditorViewportCoordinator::activeSceneViewport(bool runtime_viewport_enabled) noexcept
{
    return runtime_viewport_enabled ? m_scene_viewports.runtimeViewport() : m_scene_viewports.defaultViewport();
}

const SceneViewportInstance& EditorViewportCoordinator::activeSceneViewport(bool runtime_viewport_enabled) const noexcept
{
    return runtime_viewport_enabled ? m_scene_viewports.runtimeViewport() : m_scene_viewports.defaultViewport();
}

editor::ViewportId EditorViewportCoordinator::createSceneViewport(std::string_view owner_id)
{
    const editor::ViewportId viewport_id = allocateViewportId();
    if (!m_scene_viewports.createViewport(viewport_id)) {
        return editor::kInvalidViewportId;
    }

    rememberOwner(viewport_id, owner_id);
    return viewport_id;
}

bool EditorViewportCoordinator::destroySceneViewport(editor::ViewportId viewport_id, Renderer& renderer)
{
    if (!m_scene_viewports.destroyViewport(viewport_id, renderer)) {
        return false;
    }

    m_viewport_owner_by_id.erase(viewport_id);
    m_interactions.clearViewport(viewport_id);
    return true;
}

bool EditorViewportCoordinator::isSceneViewportValid(editor::ViewportId viewport_id) const noexcept
{
    return m_scene_viewports.isViewportValid(viewport_id);
}

SceneViewportInstance* EditorViewportCoordinator::findSceneViewport(editor::ViewportId viewport_id) noexcept
{
    return m_scene_viewports.findViewport(viewport_id);
}

const SceneViewportInstance* EditorViewportCoordinator::findSceneViewport(editor::ViewportId viewport_id) const noexcept
{
    return m_scene_viewports.findViewport(viewport_id);
}

void EditorViewportCoordinator::clearPluginSceneViewports(Renderer& renderer)
{
    for (auto owner_it = m_viewport_owner_by_id.begin(); owner_it != m_viewport_owner_by_id.end();) {
        const editor::ViewportId viewport_id = owner_it->first;
        if (viewport_id != defaultSceneViewportId() && m_scene_viewports.isViewportValid(viewport_id)) {
            m_interactions.clearViewport(viewport_id);
            owner_it = m_viewport_owner_by_id.erase(owner_it);
        } else {
            ++owner_it;
        }
    }
    m_scene_viewports.clearPluginViewports(renderer);
}

editor::ViewportId EditorViewportCoordinator::createTextureViewport(std::string_view owner_id)
{
    const editor::ViewportId viewport_id = allocateViewportId();
    if (!m_texture_viewports.createViewport(viewport_id)) {
        return editor::kInvalidViewportId;
    }

    rememberOwner(viewport_id, owner_id);
    return viewport_id;
}

bool EditorViewportCoordinator::destroyTextureViewport(editor::ViewportId viewport_id)
{
    if (!m_texture_viewports.destroyViewport(viewport_id)) {
        return false;
    }

    m_viewport_owner_by_id.erase(viewport_id);
    m_interactions.clearViewport(viewport_id);
    return true;
}

bool EditorViewportCoordinator::isTextureViewportValid(editor::ViewportId viewport_id) const noexcept
{
    return m_texture_viewports.isViewportValid(viewport_id);
}

editor::TextureViewportPresentation EditorViewportCoordinator::syncTextureViewport(editor::ViewportId viewport_id,
                                                                                   editor::TextureView texture,
                                                                                   editor::UVec2 framebuffer_size)
{
    TextureViewportInstance* viewport = m_texture_viewports.findViewport(viewport_id);
    if (viewport == nullptr) {
        return {};
    }

    return toEditorTextureViewportPresentation(viewport->sync(texture, framebuffer_size));
}

editor::TextureViewportPresentation
    EditorViewportCoordinator::textureViewportPresentation(editor::ViewportId viewport_id) const
{
    const TextureViewportInstance* viewport = m_texture_viewports.findViewport(viewport_id);
    return viewport != nullptr ? toEditorTextureViewportPresentation(viewport->presentation())
                               : editor::TextureViewportPresentation{};
}

void EditorViewportCoordinator::clearTextureViewports()
{
    for (auto owner_it = m_viewport_owner_by_id.begin(); owner_it != m_viewport_owner_by_id.end();) {
        if (m_texture_viewports.isViewportValid(owner_it->first)) {
            m_interactions.clearViewport(owner_it->first);
            owner_it = m_viewport_owner_by_id.erase(owner_it);
        } else {
            ++owner_it;
        }
    }
    m_texture_viewports.clearViewports();
}

std::vector<editor::ViewportId> EditorViewportCoordinator::destroyViewportsForOwner(std::string_view owner_id,
                                                                                    Renderer& renderer)
{
    std::vector<editor::ViewportId> destroyed_scene_viewports;
    if (owner_id.empty()) {
        return destroyed_scene_viewports;
    }

    std::vector<editor::ViewportId> viewports_to_destroy;
    const std::string owner_key = toOwnedString(owner_id);
    for (const auto& [viewport_id, viewport_owner] : m_viewport_owner_by_id) {
        if (viewport_owner == owner_key) {
            viewports_to_destroy.push_back(viewport_id);
        }
    }

    for (const editor::ViewportId viewport_id : viewports_to_destroy) {
        if (destroyTextureViewport(viewport_id)) {
            continue;
        }
        if (destroySceneViewport(viewport_id, renderer)) {
            destroyed_scene_viewports.push_back(viewport_id);
        }
    }

    m_interactions.clearOwner(owner_id);
    return destroyed_scene_viewports;
}

void EditorViewportCoordinator::beginFrameInteractions() noexcept
{
    m_interactions.beginFrame();
}

void EditorViewportCoordinator::clearInteractions() noexcept
{
    m_interactions.clear();
}

void EditorViewportCoordinator::clearViewportInteraction(editor::ViewportId viewport_id)
{
    m_interactions.clearViewport(viewport_id);
}

const ViewportInteractionState& EditorViewportCoordinator::recordViewportSurfaceInteraction(
    editor::ViewportId viewport_id,
    std::string_view owner_id,
    const ViewportInteractionInput& input)
{
    return m_interactions.recordSurface(viewport_id, owner_id, input);
}

bool EditorViewportCoordinator::isViewportInputAllowed(editor::ViewportId viewport_id) const noexcept
{
    return m_interactions.allowsInput(viewport_id);
}

void EditorViewportCoordinator::setDefaultViewportMouseCaptured(bool captured)
{
    m_interactions.setMouseCapture(defaultSceneViewportId(), captured);
}

editor::ViewportId EditorViewportCoordinator::allocateViewportId() noexcept
{
    editor::ViewportId viewport_id = m_next_viewport_id++;
    while (viewport_id == editor::kInvalidViewportId || viewport_id == defaultSceneViewportId() ||
           viewport_id == runtimeSceneViewportId() ||
           m_scene_viewports.isViewportValid(viewport_id) ||
           m_texture_viewports.isViewportValid(viewport_id)) {
        viewport_id = m_next_viewport_id++;
    }
    return viewport_id;
}

void EditorViewportCoordinator::rememberOwner(editor::ViewportId viewport_id, std::string_view owner_id)
{
    if (!owner_id.empty()) {
        m_viewport_owner_by_id[viewport_id] = toOwnedString(owner_id);
    }
}

} // namespace luna
