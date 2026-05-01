#include "ScriptSystem.h"

#include "Core/Log.h"
#include "ScriptRuntime.h"

namespace luna {

ScriptSystem::ScriptSystem() = default;

ScriptSystem::~ScriptSystem()
{
    shutdown();
}

void ScriptSystem::setRuntime(std::unique_ptr<IScriptRuntime> runtime)
{
    if (m_initialized) {
        shutdown();
    }
    m_runtime = std::move(runtime);
}

IScriptRuntime* ScriptSystem::runtime() noexcept
{
    return m_runtime.get();
}

const IScriptRuntime* ScriptSystem::runtime() const noexcept
{
    return m_runtime.get();
}

bool ScriptSystem::initialize()
{
    if (m_initialized) {
        return true;
    }

    if (!m_runtime) {
        LUNA_CORE_WARN("Script system initialized without an active script runtime");
        m_initialized = true;
        return true;
    }

    if (!m_runtime->initialize()) {
        LUNA_CORE_ERROR("Failed to initialize script runtime '{}'", m_runtime->name());
        return false;
    }

    LUNA_CORE_INFO("Initialized script runtime '{}'", m_runtime->name());
    m_initialized = true;
    return true;
}

void ScriptSystem::shutdown()
{
    if (!m_initialized) {
        return;
    }

    if (m_runtime_started) {
        LUNA_CORE_WARN("Script system shutdown requested while runtime scene is still active");
        m_runtime_started = false;
    }

    if (m_runtime) {
        m_runtime->shutdown();
    }
    m_initialized = false;
}

void ScriptSystem::onRuntimeStart(Scene& scene)
{
    if (!m_initialized && !initialize()) {
        return;
    }

    if (!m_runtime || m_runtime_started) {
        return;
    }

    m_runtime->onRuntimeStart(scene);
    m_runtime_started = true;
}

void ScriptSystem::onRuntimeStop(Scene& scene)
{
    if (!m_runtime || !m_runtime_started) {
        return;
    }

    m_runtime->onRuntimeStop(scene);
    m_runtime_started = false;
}

void ScriptSystem::onUpdate(Scene& scene, Timestep timestep)
{
    if (!m_runtime || !m_runtime_started) {
        return;
    }

    m_runtime->onUpdate(scene, timestep);
}

void ScriptSystem::setScriptProperty(Scene& scene,
                                     UUID entity_id,
                                     UUID script_id,
                                     const ScriptProperty& property,
                                     size_t property_index)
{
    if (!m_runtime || !m_runtime_started) {
        return;
    }

    m_runtime->setScriptProperty(scene, entity_id, script_id, property, property_index);
}

} // namespace luna
