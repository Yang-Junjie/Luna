#include "Core/Log.h"
#include "EditorRuntimeViewportController.h"
#include "Scene/Components/ScriptComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneRuntime.h"
#include "Scene/SceneSerializer.h"
#include "Script/ScriptRuntime.h"

#include <utility>

namespace luna {

EditorRuntimeViewportController::EditorRuntimeViewportController() = default;

EditorRuntimeViewportController::~EditorRuntimeViewportController() = default;

bool EditorRuntimeViewportController::isRuntimeViewportEnabled() const noexcept
{
    return m_runtime_viewport_enabled;
}

bool EditorRuntimeViewportController::isRuntimeViewportRequested() const noexcept
{
    return m_runtime_viewport_requested;
}

void EditorRuntimeViewportController::setRuntimeViewportRequested(bool enabled) noexcept
{
    m_runtime_viewport_requested = enabled;
}

EditorDocumentContext& EditorRuntimeViewportController::runtimeDocumentContext() noexcept
{
    return m_runtime_document_context;
}

const EditorDocumentContext& EditorRuntimeViewportController::runtimeDocumentContext() const noexcept
{
    return m_runtime_document_context;
}

Scene& EditorRuntimeViewportController::activeRenderScene(Scene& authoring_scene) noexcept
{
    return m_runtime_viewport_enabled && m_runtime_scene ? *m_runtime_scene : authoring_scene;
}

const Scene& EditorRuntimeViewportController::activeRenderScene(const Scene& authoring_scene) const noexcept
{
    return m_runtime_viewport_enabled && m_runtime_scene ? *m_runtime_scene : authoring_scene;
}

size_t EditorRuntimeViewportController::runtimeEntityCount() const noexcept
{
    return m_runtime_scene ? m_runtime_scene->entityManager().entityCount() : 0u;
}

bool EditorRuntimeViewportController::setRuntimeViewportEnabled(bool enabled,
                                                                const Scene& authoring_scene,
                                                                std::unique_ptr<IScriptRuntime> script_runtime)
{
    if (enabled == m_runtime_viewport_enabled) {
        if (!enabled) {
            m_runtime_viewport_requested = false;
        }
        return true;
    }

    if (enabled) {
        return beginRuntimeViewport(authoring_scene, std::move(script_runtime));
    }

    m_runtime_viewport_requested = false;
    endRuntimeViewport();
    return true;
}

void EditorRuntimeViewportController::resetRuntimeViewport()
{
    m_runtime_viewport_requested = false;
    endRuntimeViewport();
}

void EditorRuntimeViewportController::update(Timestep dt)
{
    if (m_runtime_scene_runtime && m_runtime_scene_runtime->isRunning()) {
        m_runtime_scene_runtime->update(dt);
    }
}

void EditorRuntimeViewportController::patchRuntimeScriptProperty(UUID entity_id,
                                                                 size_t script_index,
                                                                 size_t property_index)
{
    if (!m_runtime_viewport_enabled || !m_runtime_scene || !m_runtime_scene_runtime ||
        !m_runtime_scene_runtime->isRunning()) {
        return;
    }

    Entity runtime_entity = m_runtime_scene->entityManager().findEntityByUUID(entity_id);
    if (!runtime_entity || !runtime_entity.hasComponent<ScriptComponent>()) {
        return;
    }

    const ScriptComponent& runtime_script_component = runtime_entity.getComponent<ScriptComponent>();
    if (script_index >= runtime_script_component.scripts.size()) {
        return;
    }

    const ScriptEntry& runtime_script = runtime_script_component.scripts[script_index];
    if (property_index >= runtime_script.properties.size()) {
        return;
    }

    const ScriptProperty& runtime_property = runtime_script.properties[property_index];
    m_runtime_scene_runtime->setScriptProperty(entity_id, runtime_script.id, runtime_property, property_index);
}

bool EditorRuntimeViewportController::beginRuntimeViewport(const Scene& authoring_scene,
                                                           std::unique_ptr<IScriptRuntime> script_runtime)
{
    const std::string runtime_scene_snapshot = SceneSerializer::serializeToString(authoring_scene);
    if (runtime_scene_snapshot.empty()) {
        LUNA_EDITOR_WARN("Failed to serialize current editor scene for runtime viewport");
        m_runtime_viewport_enabled = false;
        m_runtime_viewport_requested = false;
        return false;
    }

    m_runtime_scene = std::make_unique<Scene>();
    if (!SceneSerializer::deserializeFromString(
            *m_runtime_scene, runtime_scene_snapshot, "runtime viewport snapshot")) {
        LUNA_EDITOR_WARN("Failed to create runtime scene snapshot for runtime viewport");
        m_runtime_scene.reset();
        m_runtime_viewport_enabled = false;
        m_runtime_viewport_requested = false;
        return false;
    }

    m_runtime_scene->setAssetLoadBehavior(authoring_scene.getAssetLoadBehavior());
    m_runtime_document_context.bindScene(m_runtime_scene.get());
    m_runtime_document_context.setRunning(false);
    m_runtime_scene_runtime = std::make_unique<SceneRuntime>(*m_runtime_scene);
    m_runtime_scene_runtime->setScriptRuntime(std::move(script_runtime));
    if (!m_runtime_scene_runtime->start()) {
        LUNA_EDITOR_WARN("Failed to start runtime viewport scene");
        m_runtime_scene_runtime.reset();
        m_runtime_scene.reset();
        m_runtime_document_context.bindScene(nullptr);
        m_runtime_document_context.setRunning(false);
        m_runtime_viewport_enabled = false;
        m_runtime_viewport_requested = false;
        return false;
    }

    m_runtime_viewport_enabled = true;
    m_runtime_document_context.setRunning(true);
    LUNA_EDITOR_INFO("Runtime viewport started with {} entities", m_runtime_scene->entityManager().entityCount());
    return true;
}

void EditorRuntimeViewportController::endRuntimeViewport()
{
    if (!m_runtime_viewport_enabled && !m_runtime_scene && !m_runtime_scene_runtime) {
        return;
    }

    if (m_runtime_scene_runtime) {
        m_runtime_scene_runtime->stop();
        m_runtime_scene_runtime.reset();
    }

    m_runtime_document_context.bindScene(nullptr);
    m_runtime_document_context.setRunning(false);
    m_runtime_scene.reset();
    m_runtime_viewport_enabled = false;
    LUNA_EDITOR_INFO("Runtime viewport stopped");
}

} // namespace luna
