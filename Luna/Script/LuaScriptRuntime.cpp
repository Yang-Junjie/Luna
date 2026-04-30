#include "LuaScriptRuntime.h"

#include "Core/Log.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <sol/sol.hpp>

namespace luna {

LuaScriptRuntime::LuaScriptRuntime() = default;

LuaScriptRuntime::~LuaScriptRuntime()
{
    shutdown();
}

const char* LuaScriptRuntime::name() const noexcept
{
    return "Lua";
}

bool LuaScriptRuntime::initialize()
{
    if (m_lua_state) {
        return true;
    }

    m_lua_state = std::make_unique<sol::state>();
    m_lua_state->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
    LUNA_CORE_INFO("Lua script runtime created Lua state");
    return true;
}

void LuaScriptRuntime::shutdown()
{
    if (!m_lua_state) {
        return;
    }

    m_lua_state.reset();
    LUNA_CORE_INFO("Lua script runtime destroyed Lua state");
}

void LuaScriptRuntime::onRuntimeStart(Scene& scene)
{
    const auto view = scene.entityManager().registry().view<ScriptComponent>();
    size_t scripted_entity_count = 0;
    for ([[maybe_unused]] const auto entity : view) {
        ++scripted_entity_count;
    }

    LUNA_CORE_INFO("Lua script runtime starting scene '{}' with {} scripted entity(s)",
                   scene.getName(),
                   scripted_entity_count);
}

void LuaScriptRuntime::onRuntimeStop(Scene& scene)
{
    LUNA_CORE_INFO("Lua script runtime stopping scene '{}'", scene.getName());
}

void LuaScriptRuntime::onUpdate(Scene&, Timestep)
{}

} // namespace luna
