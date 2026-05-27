#pragma once

#include "Script/ScriptHostApi.h"
#include "Script/ScriptPluginApi.h"

#include <cstddef>

#include <memory>
#include <sol/sol.hpp>
#include <string>
#include <vector>

namespace lua_plugin {

class LuaPluginRuntime {
public:
    explicit LuaPluginRuntime(const LunaScriptHostApi* host_api);
    ~LuaPluginRuntime();

    LuaPluginRuntime(const LuaPluginRuntime&) = delete;
    LuaPluginRuntime& operator=(const LuaPluginRuntime&) = delete;

    bool initialize();
    void shutdown();
    void onRuntimeStart(void* scene_context);
    void onRuntimeStop(void* scene_context);
    void onUpdate(void* scene_context, float delta_time_seconds);
    void setScriptProperty(void* scene_context,
                           uint64_t entity_id,
                           uint64_t script_id,
                           const LunaScriptPropertyValueDesc& property);

    [[nodiscard]] const LunaScriptHostApi* hostApi() const noexcept;
    [[nodiscard]] sol::state& luaState() noexcept;
    [[nodiscard]] const sol::state& luaState() const noexcept;
    [[nodiscard]] void* activeSceneContext() const noexcept;
    void log(LunaScriptHostLogLevel level, const std::string& message) const;

private:
    struct LuaScriptInstance;

    static int enumerateScriptInstance(void* user_data, const LunaScriptInstanceDesc* script_instance);
    void handleScriptInstance(const LunaScriptInstanceDesc& script_instance);
    void logLuaError(const std::string& callback_name, const std::string& script_name, const std::string& error) const;
    std::string getScriptChunkName(const LunaScriptInstanceDesc& script_instance) const;
    std::string getScriptDebugName(const LunaScriptInstanceDesc& script_instance) const;
    std::string getSceneName(void* scene_context) const;

private:
    const LunaScriptHostApi* m_host_api{nullptr};
    std::unique_ptr<sol::state> m_lua_state;
    std::vector<std::unique_ptr<LuaScriptInstance>> m_script_instances;
    float m_elapsed_time_seconds{0.0f};
    void* m_active_scene_context{nullptr};
    size_t m_creation_index{0};
    size_t m_scripted_entity_count{0};
};

int createLuaRuntimeApi(const LunaScriptHostApi* host_api, LunaScriptRuntimeApi* out_runtime_api);

} // namespace lua_plugin
