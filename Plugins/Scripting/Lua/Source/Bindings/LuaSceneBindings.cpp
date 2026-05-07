#include "../LuaPluginRuntime.h"
#include "LuaSceneBindings.h"

#include <glm/vec3.hpp>
#include <sol/sol.hpp>
#include <string>
#include <utility>

namespace {

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
    glm::vec3 getVec3(int (*getter)(void*, uint64_t, LunaScriptVec3*),
                      const glm::vec3& fallback = glm::vec3(0.0f)) const
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

LuaEntity makeLuaEntity(lua_plugin::LuaPluginRuntime& runtime, uint64_t entity_id)
{
    return LuaEntity{runtime.hostApi(), runtime.activeSceneContext(), entity_id};
}

int enumerateScriptProperty(void* user_data, const LunaScriptPropertyDesc* script_property)
{
    if (user_data == nullptr || script_property == nullptr) {
        return 1;
    }

    auto* context = static_cast<std::pair<lua_plugin::LuaPluginRuntime*, sol::table*>*>(user_data);
    sol::table& instance_table = *context->second;
    lua_plugin::LuaPluginRuntime& runtime = *context->first;

    if (script_property->name == nullptr || script_property->name[0] == '\0') {
        return 1;
    }

    lua_plugin::assignLuaScriptProperty(runtime, instance_table, *script_property);

    return 1;
}

} // namespace

namespace lua_plugin {

void bindLuaSceneApi(LuaPluginRuntime& runtime)
{
    sol::state& lua_state = runtime.luaState();

    lua_state.new_usertype<glm::vec3>("Vec3",
                                      sol::constructors<glm::vec3(), glm::vec3(float, float, float)>(),
                                      "x",
                                      &glm::vec3::x,
                                      "y",
                                      &glm::vec3::y,
                                      "z",
                                      &glm::vec3::z);
    lua_state.new_usertype<LuaEntity>("Entity",
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

void assignLuaScriptProperty(LuaPluginRuntime& runtime,
                             sol::table& instance_table,
                             const LunaScriptPropertyDesc& script_property)
{
    if (script_property.name == nullptr || script_property.name[0] == '\0') {
        return;
    }

    switch (script_property.type) {
        case LunaScriptPropertyType_Bool:
            instance_table[script_property.name] = script_property.bool_value != 0;
            break;
        case LunaScriptPropertyType_Int:
            instance_table[script_property.name] = script_property.int_value;
            break;
        case LunaScriptPropertyType_Float:
            instance_table[script_property.name] = script_property.float_value;
            break;
        case LunaScriptPropertyType_String:
            instance_table[script_property.name] =
                script_property.string_value != nullptr ? script_property.string_value : "";
            break;
        case LunaScriptPropertyType_Vec3:
            instance_table[script_property.name] =
                glm::vec3(script_property.vec3_value.x, script_property.vec3_value.y, script_property.vec3_value.z);
            break;
        case LunaScriptPropertyType_Entity:
            instance_table[script_property.name] = makeLuaEntity(runtime, script_property.entity_value);
            break;
        case LunaScriptPropertyType_Asset:
            instance_table[script_property.name] = script_property.asset_value;
            break;
        default:
            break;
    }
}

void initializeLuaScriptInstanceTable(LuaPluginRuntime& runtime,
                                      sol::table& instance_table,
                                      const LunaScriptInstanceDesc& script_instance)
{
    const LunaScriptHostApi* host_api = runtime.hostApi();

    instance_table["entity"] = makeLuaEntity(runtime, script_instance.entity_id);
    instance_table["script_id"] = std::to_string(script_instance.script_id);
    instance_table["script_asset"] = script_instance.script_asset;

    if (host_api != nullptr && host_api->scene_enumerate_script_properties != nullptr) {
        std::pair<LuaPluginRuntime*, sol::table*> property_context{&runtime, &instance_table};
        host_api->scene_enumerate_script_properties(runtime.activeSceneContext(),
                                                    script_instance.entity_id,
                                                    script_instance.script_id,
                                                    &property_context,
                                                    &enumerateScriptProperty);
    }
}

} // namespace lua_plugin
