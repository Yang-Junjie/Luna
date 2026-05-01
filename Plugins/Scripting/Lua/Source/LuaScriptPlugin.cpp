#include "Script/ScriptHostApi.h"
#include "Script/ScriptPluginApi.h"

#include <glm/vec3.hpp>
#include <sol/sol.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

void hostLog(const LunaScriptHostApi* host_api, LunaScriptHostLogLevel level, const std::string& message)
{
    if (host_api != nullptr && host_api->log != nullptr) {
        host_api->log(host_api->user_data, level, message.c_str());
    }
}

struct LuaEntity {
    const LunaScriptHostApi* host_api{nullptr};
    void* scene_context{nullptr};
    uint64_t entity_id{0};

    bool isValid() const
    {
        return host_api != nullptr && host_api->entity_is_valid != nullptr &&
               host_api->entity_is_valid(scene_context, entity_id) != 0;
    }

    std::string getName() const
    {
        if (host_api == nullptr || host_api->entity_get_name == nullptr) {
            return {};
        }

        const char* name = host_api->entity_get_name(scene_context, entity_id);
        return name != nullptr ? name : "";
    }

    std::string getUuidString() const
    {
        return std::to_string(entity_id);
    }

    glm::vec3 getTranslation() const
    {
        return getVec3(host_api != nullptr ? host_api->entity_get_translation : nullptr);
    }

    void setTranslation(const glm::vec3& value)
    {
        setVec3(host_api != nullptr ? host_api->entity_set_translation : nullptr, value);
    }

    glm::vec3 getRotation() const
    {
        return getVec3(host_api != nullptr ? host_api->entity_get_rotation : nullptr);
    }

    void setRotation(const glm::vec3& value)
    {
        setVec3(host_api != nullptr ? host_api->entity_set_rotation : nullptr, value);
    }

    glm::vec3 getScale() const
    {
        return getVec3(host_api != nullptr ? host_api->entity_get_scale : nullptr, glm::vec3(1.0f));
    }

    void setScale(const glm::vec3& value)
    {
        setVec3(host_api != nullptr ? host_api->entity_set_scale : nullptr, value);
    }

private:
    glm::vec3 getVec3(int (*getter)(void*, uint64_t, LunaScriptVec3*), const glm::vec3& fallback = glm::vec3(0.0f)) const
    {
        if (getter == nullptr) {
            return fallback;
        }

        LunaScriptVec3 value{};
        if (getter(scene_context, entity_id, &value) == 0) {
            return fallback;
        }

        return glm::vec3(value.x, value.y, value.z);
    }

    void setVec3(int (*setter)(void*, uint64_t, const LunaScriptVec3*), const glm::vec3& value)
    {
        if (setter == nullptr) {
            return;
        }

        const LunaScriptVec3 converted{value.x, value.y, value.z};
        setter(scene_context, entity_id, &converted);
    }
};

struct LuaScriptInstance {
    int execution_order{0};
    size_t creation_index{0};
    std::string debug_name;
    sol::environment environment;
    sol::table self;
    sol::protected_function on_create;
    sol::protected_function on_update;
    sol::protected_function on_destroy;
};

class LuaPluginRuntime {
public:
    explicit LuaPluginRuntime(const LunaScriptHostApi* host_api)
        : m_host_api(host_api)
    {}

    bool initialize()
    {
        if (m_lua_state) {
            return true;
        }

        m_lua_state = std::make_unique<sol::state>();
        m_lua_state->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
        bindTypes();
        bindLogApi();
        bindInputApi();
        bindKeyCodes();
        bindMouseCodes();
        updateTimeBindings(0.0f, 0.0f);
        hostLog(m_host_api, LunaScriptHostLogLevel_Info, "Lua script plugin created Lua state");
        return true;
    }

    void shutdown()
    {
        m_script_instances.clear();
        m_elapsed_time_seconds = 0.0f;
        m_active_scene_context = nullptr;

        if (!m_lua_state) {
            return;
        }

        m_lua_state.reset();
        hostLog(m_host_api, LunaScriptHostLogLevel_Info, "Lua script plugin destroyed Lua state");
    }

    void onRuntimeStart(void* scene_context)
    {
        m_script_instances.clear();
        m_elapsed_time_seconds = 0.0f;
        m_active_scene_context = scene_context;

        if (!m_lua_state && !initialize()) {
            return;
        }

        updateTimeBindings(0.0f, 0.0f);

        m_scripted_entity_count = 0;
        m_creation_index = 0;
        if (m_host_api == nullptr || m_host_api->scene_enumerate_script_instances == nullptr ||
            m_host_api->scene_enumerate_script_instances(scene_context, this, &enumerateScriptInstance) == 0) {
            hostLog(m_host_api,
                    LunaScriptHostLogLevel_Warn,
                    "Lua script plugin could not enumerate script instances for runtime start");
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

        std::string scene_name = getSceneName(scene_context);
        hostLog(m_host_api,
                LunaScriptHostLogLevel_Info,
                "Lua script plugin starting scene '" + scene_name + "' with " +
                    std::to_string(m_scripted_entity_count) + " scripted entity(s) and " +
                    std::to_string(m_script_instances.size()) + " active script instance(s)");
    }

    void onRuntimeStop(void* scene_context)
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
            updateTimeBindings(0.0f, 0.0f);
        }
        m_active_scene_context = nullptr;

        hostLog(m_host_api,
                LunaScriptHostLogLevel_Info,
                "Lua script plugin stopping scene '" + getSceneName(scene_context) + "'");
    }

    void onUpdate(void* scene_context, float delta_time_seconds)
    {
        if (!m_lua_state) {
            return;
        }

        m_active_scene_context = scene_context;
        m_elapsed_time_seconds += delta_time_seconds;
        updateTimeBindings(delta_time_seconds, m_elapsed_time_seconds);

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

private:
    static int enumerateScriptInstance(void* user_data, const LunaScriptInstanceDesc* script_instance)
    {
        if (user_data == nullptr || script_instance == nullptr) {
            return 1;
        }

        static_cast<LuaPluginRuntime*>(user_data)->handleScriptInstance(*script_instance);
        return 1;
    }

    static int enumerateScriptProperty(void* user_data, const LunaScriptPropertyDesc* script_property)
    {
        if (user_data == nullptr || script_property == nullptr) {
            return 1;
        }

        auto* context = static_cast<PropertyEnumerationContext*>(user_data);
        context->runtime->applyScriptProperty(*context->instance_table, *script_property);
        return 1;
    }

    void handleScriptInstance(const LunaScriptInstanceDesc& script_instance)
    {
        ++m_scripted_entity_count;

        const std::string language = script_instance.language != nullptr ? script_instance.language : "";
        if (toLower(language) != "lua") {
            return;
        }

        const char* source = script_instance.source != nullptr ? script_instance.source : "";
        if (source[0] == '\0') {
            hostLog(m_host_api,
                    LunaScriptHostLogLevel_Warn,
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
            const std::string prototype_name =
                script_instance.type_name != nullptr ? script_instance.type_name : std::string{};
            if (!prototype_name.empty()) {
                sol::object named_prototype = environment[prototype_name];
                if (named_prototype.is<sol::table>()) {
                    prototype = named_prototype.as<sol::table>();
                }
            }
        }

        if (!prototype.valid()) {
            hostLog(m_host_api,
                    LunaScriptHostLogLevel_Warn,
                    "Lua script '" + chunk_name + "' did not expose a prototype table");
            return;
        }

        sol::table instance_table = m_lua_state->create_table();
        instance_table["entity"] = LuaEntity{m_host_api, m_active_scene_context, script_instance.entity_id};
        instance_table["script_id"] = std::to_string(script_instance.script_id);
        instance_table["script_asset"] = script_instance.script_asset;

        if (m_host_api != nullptr && m_host_api->scene_enumerate_script_properties != nullptr) {
            PropertyEnumerationContext property_context{this, &instance_table};
            m_host_api->scene_enumerate_script_properties(
                m_active_scene_context,
                script_instance.entity_id,
                script_instance.script_id,
                &property_context,
                &enumerateScriptProperty);
        }

        sol::table metatable = m_lua_state->create_table();
        metatable["__index"] = prototype;
        instance_table[sol::metatable_key] = metatable;

        auto instance = std::make_unique<LuaScriptInstance>();
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

    void bindTypes()
    {
        m_lua_state->new_usertype<glm::vec3>("Vec3",
                                             sol::constructors<glm::vec3(), glm::vec3(float, float, float)>(),
                                             "x",
                                             &glm::vec3::x,
                                             "y",
                                             &glm::vec3::y,
                                             "z",
                                             &glm::vec3::z);
        m_lua_state->new_usertype<LuaEntity>("Entity",
                                             "is_valid",
                                             &LuaEntity::isValid,
                                             "name",
                                             sol::property(&LuaEntity::getName),
                                             "uuid",
                                             sol::property(&LuaEntity::getUuidString),
                                             "translation",
                                             sol::property(&LuaEntity::getTranslation, &LuaEntity::setTranslation),
                                             "rotation",
                                             sol::property(&LuaEntity::getRotation, &LuaEntity::setRotation),
                                             "scale",
                                             sol::property(&LuaEntity::getScale, &LuaEntity::setScale));
    }

    void bindLogApi()
    {
        sol::table log = m_lua_state->create_named_table("Log");
        log.set_function("info", [this](const std::string& message) {
            hostLog(m_host_api, LunaScriptHostLogLevel_Info, "[Lua] " + message);
        });
        log.set_function("warn", [this](const std::string& message) {
            hostLog(m_host_api, LunaScriptHostLogLevel_Warn, "[Lua] " + message);
        });
        log.set_function("error", [this](const std::string& message) {
            hostLog(m_host_api, LunaScriptHostLogLevel_Error, "[Lua] " + message);
        });
    }

    void bindInputApi()
    {
        sol::table input = m_lua_state->create_named_table("Input");
        input.set_function("is_key_pressed", [this](int key_code) {
            return m_host_api != nullptr && m_host_api->input_is_key_pressed != nullptr &&
                   m_host_api->input_is_key_pressed(key_code) != 0;
        });
        input.set_function("is_mouse_button_pressed", [this](int button_code) {
            return m_host_api != nullptr && m_host_api->input_is_mouse_button_pressed != nullptr &&
                   m_host_api->input_is_mouse_button_pressed(button_code) != 0;
        });
        input.set_function("get_mouse_x", [this]() {
            return m_host_api != nullptr && m_host_api->input_get_mouse_x != nullptr ? m_host_api->input_get_mouse_x()
                                                                                     : 0.0f;
        });
        input.set_function("get_mouse_y", [this]() {
            return m_host_api != nullptr && m_host_api->input_get_mouse_y != nullptr ? m_host_api->input_get_mouse_y()
                                                                                     : 0.0f;
        });
    }

    void bindKeyCodes()
    {
        sol::table key_code = m_lua_state->create_named_table("KeyCode");
        key_code["None"] = 0;
        key_code["LeftShift"] = 1;
        key_code["RightShift"] = 2;
        key_code["LeftControl"] = 3;
        key_code["RightControl"] = 4;
        key_code["LeftAlt"] = 5;
        key_code["RightAlt"] = 6;
        key_code["Space"] = 7;
        key_code["Enter"] = 8;
        key_code["Delete"] = 9;
        key_code["Escape"] = 10;
        key_code["Up"] = 11;
        key_code["Down"] = 12;
        key_code["Left"] = 13;
        key_code["Right"] = 14;
        key_code["A"] = 65;
        key_code["B"] = 66;
        key_code["C"] = 67;
        key_code["D"] = 68;
        key_code["E"] = 69;
        key_code["F"] = 70;
        key_code["G"] = 71;
        key_code["H"] = 72;
        key_code["I"] = 73;
        key_code["J"] = 74;
        key_code["K"] = 75;
        key_code["L"] = 76;
        key_code["M"] = 77;
        key_code["N"] = 78;
        key_code["O"] = 79;
        key_code["P"] = 80;
        key_code["Q"] = 81;
        key_code["R"] = 82;
        key_code["S"] = 83;
        key_code["T"] = 84;
        key_code["U"] = 85;
        key_code["V"] = 86;
        key_code["W"] = 87;
        key_code["X"] = 88;
        key_code["Y"] = 89;
        key_code["Z"] = 90;
    }

    void bindMouseCodes()
    {
        sol::table mouse_code = m_lua_state->create_named_table("MouseCode");
        mouse_code["None"] = 0;
        mouse_code["Left"] = 1;
        mouse_code["Right"] = 2;
        mouse_code["Middle"] = 3;
        mouse_code["XButton1"] = 4;
        mouse_code["XButton2"] = 5;
    }

    void updateTimeBindings(float delta_time, float elapsed_time)
    {
        sol::object time_object = (*m_lua_state)["Time"];
        sol::table time =
            time_object.is<sol::table>() ? time_object.as<sol::table>() : m_lua_state->create_named_table("Time");

        time["delta_time"] = delta_time;
        time["elapsed_time"] = elapsed_time;
    }

    void applyScriptProperty(sol::table& instance_table, const LunaScriptPropertyDesc& property)
    {
        if (property.name == nullptr || property.name[0] == '\0') {
            return;
        }

        switch (property.type) {
            case LunaScriptPropertyType_Bool:
                instance_table[property.name] = property.bool_value != 0;
                break;
            case LunaScriptPropertyType_Int:
                instance_table[property.name] = property.int_value;
                break;
            case LunaScriptPropertyType_Float:
                instance_table[property.name] = property.float_value;
                break;
            case LunaScriptPropertyType_String:
                instance_table[property.name] = property.string_value != nullptr ? property.string_value : "";
                break;
            case LunaScriptPropertyType_Vec3:
                instance_table[property.name] =
                    glm::vec3(property.vec3_value.x, property.vec3_value.y, property.vec3_value.z);
                break;
            case LunaScriptPropertyType_Entity:
                instance_table[property.name] = LuaEntity{m_host_api, m_active_scene_context, property.entity_value};
                break;
            case LunaScriptPropertyType_Asset:
                instance_table[property.name] = property.asset_value;
                break;
            default:
                break;
        }
    }

    void logLuaError(const std::string& callback_name, const std::string& script_name, const std::string& error) const
    {
        hostLog(m_host_api,
                LunaScriptHostLogLevel_Error,
                "Lua script '" + script_name + "' failed during " + callback_name + ": " + error);
    }

    std::string getScriptChunkName(const LunaScriptInstanceDesc& script_instance) const
    {
        if (script_instance.asset_name != nullptr && script_instance.asset_name[0] != '\0') {
            return script_instance.asset_name;
        }
        if (script_instance.type_name != nullptr && script_instance.type_name[0] != '\0') {
            return script_instance.type_name;
        }
        return "LuaScript";
    }

    std::string getScriptDebugName(const LunaScriptInstanceDesc& script_instance) const
    {
        const std::string entity_name = script_instance.entity_name != nullptr ? script_instance.entity_name : "Entity";
        return entity_name + "::" + getScriptChunkName(script_instance);
    }

    std::string getSceneName(void* scene_context) const
    {
        if (m_host_api == nullptr || m_host_api->scene_get_name == nullptr) {
            return {};
        }

        const char* scene_name = m_host_api->scene_get_name(scene_context);
        return scene_name != nullptr ? scene_name : "";
    }

private:
    struct PropertyEnumerationContext {
        LuaPluginRuntime* runtime{nullptr};
        sol::table* instance_table{nullptr};
    };

private:
    const LunaScriptHostApi* m_host_api{nullptr};
    std::unique_ptr<sol::state> m_lua_state;
    std::vector<std::unique_ptr<LuaScriptInstance>> m_script_instances;
    float m_elapsed_time_seconds{0.0f};
    void* m_active_scene_context{nullptr};
    size_t m_creation_index{0};
    size_t m_scripted_entity_count{0};
};

struct LuaPluginBackendState {
    const LunaScriptHostApi* host_api{nullptr};
};

int destroyLuaRuntime(void* runtime_user_data)
{
    delete static_cast<LuaPluginRuntime*>(runtime_user_data);
    return 1;
}

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

int createLuaRuntime(void* backend_user_data, LunaScriptRuntimeApi* out_runtime_api)
{
    if (backend_user_data == nullptr || out_runtime_api == nullptr) {
        return 0;
    }

    const auto* backend_state = static_cast<const LuaPluginBackendState*>(backend_user_data);
    auto* runtime = new LuaPluginRuntime(backend_state->host_api);

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
    return 1;
}

} // namespace

extern "C" {

#if defined(_WIN32)
__declspec(dllexport)
#endif
int LunaCreateScriptPlugin(uint32_t host_api_version,
                           const LunaScriptHostApi* host_api,
                           LunaScriptPluginApi* out_plugin_api)
{
    if (host_api == nullptr || out_plugin_api == nullptr || host_api_version != LUNA_SCRIPT_HOST_API_VERSION) {
        return 0;
    }

    static LuaPluginBackendState backend_state{};
    static const char* supported_extensions[] = {".lua"};
    static LunaScriptBackendApi backend_api{};

    backend_state.host_api = host_api;

    backend_api.struct_size = sizeof(LunaScriptBackendApi);
    backend_api.api_version = LUNA_SCRIPT_BACKEND_API_VERSION;
    backend_api.backend_name = "Lua";
    backend_api.display_name = "Luna Lua";
    backend_api.language_name = "Lua";
    backend_api.supported_extensions = supported_extensions;
    backend_api.supported_extension_count = 1;
    backend_api.backend_user_data = &backend_state;
    backend_api.create_runtime = &createLuaRuntime;

    out_plugin_api->struct_size = sizeof(LunaScriptPluginApi);
    out_plugin_api->api_version = LUNA_SCRIPT_PLUGIN_API_VERSION;
    out_plugin_api->plugin_name = "Luna Lua";
    out_plugin_api->backends = &backend_api;
    out_plugin_api->backend_count = 1;
    return 1;
}

} // extern "C"
