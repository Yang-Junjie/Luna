#include "SceneRuntime.h"

#include "Core/Log.h"
#include "Scene.h"
#include "Script/ScriptRuntime.h"

namespace luna {

SceneRuntime::SceneRuntime(Scene& scene)
    : m_scene(&scene)
{}

SceneRuntime::~SceneRuntime()
{
    stop();
}

void SceneRuntime::setScriptRuntime(std::unique_ptr<IScriptRuntime> runtime)
{
    if (m_running) {
        stop();
    }

    m_script_system.setRuntime(std::move(runtime));
}

ScriptSystem& SceneRuntime::scriptSystem() noexcept
{
    return m_script_system;
}

const ScriptSystem& SceneRuntime::scriptSystem() const noexcept
{
    return m_script_system;
}

Scene& SceneRuntime::scene() noexcept
{
    return *m_scene;
}

const Scene& SceneRuntime::scene() const noexcept
{
    return *m_scene;
}

bool SceneRuntime::start()
{
    if (m_running) {
        return true;
    }

    if (!m_script_system.initialize()) {
        LUNA_CORE_ERROR("Failed to initialize scene runtime for scene '{}'", m_scene->getName());
        return false;
    }

    m_script_system.onRuntimeStart(*m_scene);
    m_running = true;
    LUNA_CORE_INFO("Scene runtime started for scene '{}'", m_scene->getName());
    return true;
}

void SceneRuntime::stop()
{
    if (!m_running) {
        return;
    }

    m_script_system.onRuntimeStop(*m_scene);
    m_script_system.shutdown();
    m_running = false;
    LUNA_CORE_INFO("Scene runtime stopped for scene '{}'", m_scene->getName());
}

void SceneRuntime::update(Timestep timestep)
{
    if (!m_running) {
        return;
    }

    m_script_system.onUpdate(*m_scene, timestep);
    m_scene->renderFromRuntimeCamera();
}

void SceneRuntime::setScriptProperty(UUID entity_id,
                                     UUID script_id,
                                     const ScriptProperty& property,
                                     size_t property_index)
{
    if (!m_running) {
        return;
    }

    m_script_system.setScriptProperty(*m_scene, entity_id, script_id, property, property_index);
}

bool SceneRuntime::isRunning() const noexcept
{
    return m_running;
}

} // namespace luna
