#include "LuaPluginRuntime.h"

#include "Bindings/LuaCoreBindings.h"
#include "Bindings/LuaInputBindings.h"
#include "Bindings/LuaSceneBindings.h"

#include <sol/sol.hpp>

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <utility>

namespace {

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

} // namespace

namespace lua_plugin {

struct LuaPluginRuntime::LuaScriptInstance {
    uint64_t entity_id{0};
    uint64_t script_id{0};
    int execution_order{0};
    size_t creation_index{0};
    std::string debug_name;
    sol::environment environment;
    sol::table self;
    sol::protected_function on_create;
    sol::protected_function on_update;
    sol::protected_function on_destroy;
};

LuaPluginRuntime::LuaPluginRuntime(const LunaScriptHostApi* host_api)
    : m_host_api(host_api)
{}

LuaPluginRuntime::~LuaPluginRuntime()
{
    shutdown();
}

bool LuaPluginRuntime::initialize()
{
    if (m_lua_state) {
        return true;
    }

    m_lua_state = std::make_unique<sol::state>();
    m_lua_state->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

    bindLuaSceneApi(*this);
    bindLuaCoreApi(*this);
    bindLuaInputApi(*this);
    updateLuaTimeApi(*this, 0.0f, 0.0f);

    log(LunaScriptHostLogLevel_Info, "Lua script plugin created Lua state");
    return true;
}

void LuaPluginRuntime::shutdown()
{
    m_script_instances.clear();
    m_elapsed_time_seconds = 0.0f;
    m_active_scene_context = nullptr;

    if (!m_lua_state) {
        return;
    }

    m_lua_state.reset();
    log(LunaScriptHostLogLevel_Info, "Lua script plugin destroyed Lua state");
}

void LuaPluginRuntime::onRuntimeStart(void* scene_context)
{
    m_script_instances.clear();
    m_elapsed_time_seconds = 0.0f;
    m_active_scene_context = scene_context;

    if (!m_lua_state && !initialize()) {
        return;
    }

    updateLuaTimeApi(*this, 0.0f, 0.0f);

    m_scripted_entity_count = 0;
    m_creation_index = 0;
    if (m_host_api == nullptr || m_host_api->scene_enumerate_script_instances == nullptr ||
        m_host_api->scene_enumerate_script_instances(scene_context, this, &enumerateScriptInstance) == 0) {
        log(LunaScriptHostLogLevel_Warn, "Lua script plugin could not enumerate script instances for runtime start");
    }

    std::sort(m_script_instances.begin(), m_script_instances.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs->execution_order != rhs->execution_order) {
            return lhs->execution_order < rhs->execution_order;
        }

        return lhs->creation_index < rhs->creation_index;
    });

    for (const auto& instance : m_script_instances) {
        if (!instance->on_create.valid()) {
            continue;
        }

        sol::protected_function_result result = instance->on_create(instance->self);
        if (!result.valid()) {
            sol::error error = result;
            logLuaError("OnCreate", instance->debug_name, error.what());
        }
    }

    const std::string scene_name = getSceneName(scene_context);
    log(LunaScriptHostLogLevel_Info,
        "Lua script plugin starting scene '" + scene_name + "' with " +
            std::to_string(m_scripted_entity_count) + " scripted entity(s) and " +
            std::to_string(m_script_instances.size()) + " active script instance(s)");
}

void LuaPluginRuntime::onRuntimeStop(void* scene_context)
{
    for (auto it = m_script_instances.rbegin(); it != m_script_instances.rend(); ++it) {
        if (!(*it)->on_destroy.valid()) {
            continue;
        }

        sol::protected_function_result result = (*it)->on_destroy((*it)->self);
        if (!result.valid()) {
            sol::error error = result;
            logLuaError("OnDestroy", (*it)->debug_name, error.what());
        }
    }

    m_script_instances.clear();
    m_elapsed_time_seconds = 0.0f;
    if (m_lua_state) {
        updateLuaTimeApi(*this, 0.0f, 0.0f);
    }
    m_active_scene_context = nullptr;

    log(LunaScriptHostLogLevel_Info, "Lua script plugin stopping scene '" + getSceneName(scene_context) + "'");
}

void LuaPluginRuntime::onUpdate(void* scene_context, float delta_time_seconds)
{
    if (!m_lua_state) {
        return;
    }

    m_active_scene_context = scene_context;
    m_elapsed_time_seconds += delta_time_seconds;
    updateLuaTimeApi(*this, delta_time_seconds, m_elapsed_time_seconds);

    for (const auto& instance : m_script_instances) {
        if (!instance->on_update.valid()) {
            continue;
        }

        sol::protected_function_result result = instance->on_update(instance->self, delta_time_seconds);
        if (!result.valid()) {
            sol::error error = result;
            logLuaError("OnUpdate", instance->debug_name, error.what());
        }
    }
}

void LuaPluginRuntime::setScriptProperty(void* scene_context,
                                         uint64_t entity_id,
                                         uint64_t script_id,
                                         const LunaScriptPropertyValueDesc& property)
{
    if (!m_lua_state || property.name == nullptr || property.name[0] == '\0') {
        return;
    }

    m_active_scene_context = scene_context;

    auto instance_it = std::find_if(m_script_instances.begin(), m_script_instances.end(), [&](const auto& instance) {
        return instance && instance->entity_id == entity_id && instance->script_id == script_id;
    });
    if (instance_it == m_script_instances.end()) {
        return;
    }

    LunaScriptPropertyDesc desc{};
    desc.name = property.name;
    desc.type = property.type;
    desc.bool_value = property.bool_value;
    desc.int_value = property.int_value;
    desc.float_value = property.float_value;
    desc.string_value = property.string_value;
    desc.vec3_value = property.vec3_value;
    desc.entity_value = property.entity_value;
    desc.asset_value = property.asset_value;
    assignLuaScriptProperty(*this, (*instance_it)->self, desc);
}

const LunaScriptHostApi* LuaPluginRuntime::hostApi() const noexcept
{
    return m_host_api;
}

sol::state& LuaPluginRuntime::luaState() noexcept
{
    return *m_lua_state;
}

const sol::state& LuaPluginRuntime::luaState() const noexcept
{
    return *m_lua_state;
}

void* LuaPluginRuntime::activeSceneContext() const noexcept
{
    return m_active_scene_context;
}

void LuaPluginRuntime::log(LunaScriptHostLogLevel level, const std::string& message) const
{
    if (m_host_api != nullptr && m_host_api->log != nullptr) {
        m_host_api->log(m_host_api->user_data, level, message.c_str());
    }
}

int LuaPluginRuntime::enumerateScriptInstance(void* user_data, const LunaScriptInstanceDesc* script_instance)
{
    if (user_data == nullptr || script_instance == nullptr) {
        return 1;
    }

    static_cast<LuaPluginRuntime*>(user_data)->handleScriptInstance(*script_instance);
    return 1;
}

void LuaPluginRuntime::handleScriptInstance(const LunaScriptInstanceDesc& script_instance)
{
    ++m_scripted_entity_count;

    const std::string language = script_instance.language != nullptr ? script_instance.language : "";
    if (toLower(language) != "lua") {
        return;
    }

    const char* source = script_instance.source != nullptr ? script_instance.source : "";
    if (source[0] == '\0') {
        log(LunaScriptHostLogLevel_Warn,
            "Lua script plugin skipped an empty script source on entity '" +
                std::string(script_instance.entity_name != nullptr ? script_instance.entity_name : "") + "'");
        return;
    }

    sol::environment environment(*m_lua_state, sol::create, m_lua_state->globals());
    const std::string chunk_name = getScriptChunkName(script_instance);
    sol::load_result load_result = m_lua_state->load(source, chunk_name);
    if (!load_result.valid()) {
        sol::error error = load_result;
        logLuaError("load", chunk_name, error.what());
        return;
    }

    sol::protected_function chunk = load_result;
    sol::set_environment(environment, chunk);

    sol::protected_function_result execute_result = chunk();
    if (!execute_result.valid()) {
        sol::error error = execute_result;
        logLuaError("initialization", chunk_name, error.what());
        return;
    }

    sol::object returned_object = execute_result.return_count() > 0
                                      ? execute_result.get<sol::object>()
                                      : sol::make_object(*m_lua_state, sol::nil);
    sol::table prototype;
    if (returned_object.is<sol::table>()) {
        prototype = returned_object.as<sol::table>();
    } else {
        const std::string prototype_name = script_instance.type_name != nullptr ? script_instance.type_name : "";
        if (!prototype_name.empty()) {
            sol::object named_prototype = environment[prototype_name];
            if (named_prototype.is<sol::table>()) {
                prototype = named_prototype.as<sol::table>();
            }
        }
    }

    if (!prototype.valid()) {
        log(LunaScriptHostLogLevel_Warn, "Lua script '" + chunk_name + "' did not expose a prototype table");
        return;
    }

    sol::table instance_table = m_lua_state->create_table();
    initializeLuaScriptInstanceTable(*this, instance_table, script_instance);

    sol::table metatable = m_lua_state->create_table();
    metatable["__index"] = prototype;
    instance_table[sol::metatable_key] = metatable;

    auto instance = std::make_unique<LuaScriptInstance>();
    instance->entity_id = script_instance.entity_id;
    instance->script_id = script_instance.script_id;
    instance->execution_order = script_instance.execution_order;
    instance->creation_index = m_creation_index++;
    instance->debug_name = getScriptDebugName(script_instance);
    instance->environment = std::move(environment);
    instance->self = std::move(instance_table);

    if (sol::object on_create_object = instance->self["OnCreate"];
        on_create_object.is<sol::protected_function>()) {
        instance->on_create = on_create_object.as<sol::protected_function>();
    }
    if (sol::object on_update_object = instance->self["OnUpdate"];
        on_update_object.is<sol::protected_function>()) {
        instance->on_update = on_update_object.as<sol::protected_function>();
    }
    if (sol::object on_destroy_object = instance->self["OnDestroy"];
        on_destroy_object.is<sol::protected_function>()) {
        instance->on_destroy = on_destroy_object.as<sol::protected_function>();
    }

    m_script_instances.push_back(std::move(instance));
}

void LuaPluginRuntime::logLuaError(const std::string& callback_name,
                                   const std::string& script_name,
                                   const std::string& error) const
{
    log(LunaScriptHostLogLevel_Error,
        "Lua script '" + script_name + "' failed during " + callback_name + ": " + error);
}

std::string LuaPluginRuntime::getScriptChunkName(const LunaScriptInstanceDesc& script_instance) const
{
    if (script_instance.asset_name != nullptr && script_instance.asset_name[0] != '\0') {
        return script_instance.asset_name;
    }
    if (script_instance.type_name != nullptr && script_instance.type_name[0] != '\0') {
        return script_instance.type_name;
    }
    return "LuaScript";
}

std::string LuaPluginRuntime::getScriptDebugName(const LunaScriptInstanceDesc& script_instance) const
{
    const std::string entity_name = script_instance.entity_name != nullptr ? script_instance.entity_name : "Entity";
    return entity_name + "::" + getScriptChunkName(script_instance);
}

std::string LuaPluginRuntime::getSceneName(void* scene_context) const
{
    if (m_host_api == nullptr || m_host_api->scene_get_name == nullptr) {
        return {};
    }

    const char* scene_name = m_host_api->scene_get_name(scene_context);
    return scene_name != nullptr ? scene_name : "";
}

namespace {

int initializeLuaRuntime(void* runtime_user_data)
{
    return runtime_user_data != nullptr && static_cast<LuaPluginRuntime*>(runtime_user_data)->initialize() ? 1 : 0;
}

void shutdownLuaRuntime(void* runtime_user_data)
{
    if (runtime_user_data != nullptr) {
        static_cast<LuaPluginRuntime*>(runtime_user_data)->shutdown();
    }
}

void onLuaRuntimeStart(void* runtime_user_data, void* scene_context)
{
    if (runtime_user_data != nullptr) {
        static_cast<LuaPluginRuntime*>(runtime_user_data)->onRuntimeStart(scene_context);
    }
}

void onLuaRuntimeStop(void* runtime_user_data, void* scene_context)
{
    if (runtime_user_data != nullptr) {
        static_cast<LuaPluginRuntime*>(runtime_user_data)->onRuntimeStop(scene_context);
    }
}

void onLuaRuntimeUpdate(void* runtime_user_data, void* scene_context, float delta_time_seconds)
{
    if (runtime_user_data != nullptr) {
        static_cast<LuaPluginRuntime*>(runtime_user_data)->onUpdate(scene_context, delta_time_seconds);
    }
}

void setLuaRuntimeScriptProperty(void* runtime_user_data,
                                 void* scene_context,
                                 uint64_t entity_id,
                                 uint64_t script_id,
                                 const LunaScriptPropertyValueDesc* property)
{
    if (runtime_user_data != nullptr && property != nullptr) {
        static_cast<LuaPluginRuntime*>(runtime_user_data)->setScriptProperty(scene_context, entity_id, script_id, *property);
    }
}

} // namespace

int createLuaRuntimeApi(const LunaScriptHostApi* host_api, LunaScriptRuntimeApi* out_runtime_api)
{
    if (host_api == nullptr || out_runtime_api == nullptr) {
        return 0;
    }

    auto* runtime = new LuaPluginRuntime(host_api);
    out_runtime_api->struct_size = sizeof(LunaScriptRuntimeApi);
    out_runtime_api->api_version = LUNA_SCRIPT_RUNTIME_API_VERSION;
    out_runtime_api->runtime_user_data = runtime;
    out_runtime_api->destroy_runtime = [](void* runtime_user_data) {
        delete static_cast<LuaPluginRuntime*>(runtime_user_data);
    };
    out_runtime_api->initialize = &initializeLuaRuntime;
    out_runtime_api->shutdown = &shutdownLuaRuntime;
    out_runtime_api->on_runtime_start = &onLuaRuntimeStart;
    out_runtime_api->on_runtime_stop = &onLuaRuntimeStop;
    out_runtime_api->on_update = &onLuaRuntimeUpdate;
    out_runtime_api->set_script_property = &setLuaRuntimeScriptProperty;
    return 1;
}

} // namespace lua_plugin
