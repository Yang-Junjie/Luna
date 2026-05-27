#include "../LuaPluginRuntime.h"
#include "LuaSceneBindings.h"
#include "LuaSharedBindings.h"

#include <cstdint>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <limits>
#include <sol/sol.hpp>
#include <string>
#include <utility>

namespace {

constexpr float kMinLookDirectionLengthSquared = 1.0e-6f;

glm::quat makeOrientation(const glm::vec3& euler_radians)
{
    const glm::quat pitch_rotation = glm::angleAxis(euler_radians.x, glm::vec3{1.0f, 0.0f, 0.0f});
    const glm::quat yaw_rotation = glm::angleAxis(euler_radians.y, glm::vec3{0.0f, 1.0f, 0.0f});
    const glm::quat roll_rotation = glm::angleAxis(euler_radians.z, glm::vec3{0.0f, 0.0f, 1.0f});
    return glm::normalize(yaw_rotation * pitch_rotation * roll_rotation);
}

struct LuaCamera {
    bool primary{true};
    bool fixed_aspect_ratio{false};
    int projection_type{LunaScriptCameraProjectionType_Perspective};
    float perspective_vertical_fov{0.872664626f};
    float perspective_near{0.05f};
    float perspective_far{500.0f};
    float orthographic_size{10.0f};
    float orthographic_near{-100.0f};
    float orthographic_far{100.0f};
};

LuaCamera toLuaCamera(const LunaScriptCameraDesc& desc)
{
    LuaCamera camera{};
    camera.primary = desc.primary != 0;
    camera.fixed_aspect_ratio = desc.fixed_aspect_ratio != 0;
    camera.projection_type = static_cast<int>(desc.projection_type);
    camera.perspective_vertical_fov = desc.perspective_vertical_fov_radians;
    camera.perspective_near = desc.perspective_near;
    camera.perspective_far = desc.perspective_far;
    camera.orthographic_size = desc.orthographic_size;
    camera.orthographic_near = desc.orthographic_near;
    camera.orthographic_far = desc.orthographic_far;
    return camera;
}

LunaScriptCameraDesc toScriptCameraDesc(const LuaCamera& camera)
{
    LunaScriptCameraDesc desc{};
    desc.primary = camera.primary ? 1 : 0;
    desc.fixed_aspect_ratio = camera.fixed_aspect_ratio ? 1 : 0;
    desc.projection_type = static_cast<LunaScriptCameraProjectionType>(camera.projection_type);
    desc.perspective_vertical_fov_radians = camera.perspective_vertical_fov;
    desc.perspective_near = camera.perspective_near;
    desc.perspective_far = camera.perspective_far;
    desc.orthographic_size = camera.orthographic_size;
    desc.orthographic_near = camera.orthographic_near;
    desc.orthographic_far = camera.orthographic_far;
    return desc;
}

sol::object makeLuaUint64(sol::state& lua_state, uint64_t value)
{
    constexpr uint64_t kMaxLuaInteger = static_cast<uint64_t>((std::numeric_limits<lua_Integer>::max)());
    if (value <= kMaxLuaInteger) {
        return sol::make_object(lua_state, static_cast<lua_Integer>(value));
    }

    return sol::make_object(lua_state, std::to_string(value));
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

    glm::vec3 getForwardDirection() const
    {
        return glm::normalize(makeOrientation(getRotation()) * glm::vec3(0.0f, 0.0f, -1.0f));
    }

    glm::vec3 getRightDirection() const
    {
        return glm::normalize(makeOrientation(getRotation()) * glm::vec3(1.0f, 0.0f, 0.0f));
    }

    glm::vec3 getUpDirection() const
    {
        return glm::normalize(makeOrientation(getRotation()) * glm::vec3(0.0f, 1.0f, 0.0f));
    }

    void translateWorld(const glm::vec3& delta)
    {
        setTranslation(getTranslation() + delta);
    }

    void translateLocal(const glm::vec3& delta)
    {
        glm::vec3 translation = getTranslation();
        translation += getRightDirection() * delta.x;
        translation += getUpDirection() * delta.y;
        translation += getForwardDirection() * delta.z;
        setTranslation(translation);
    }

    bool lookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f))
    {
        const glm::vec3 direction = target - getTranslation();
        if (glm::dot(direction, direction) <= kMinLookDirectionLengthSquared) {
            return false;
        }

        const glm::vec3 normalized_direction = glm::normalize(direction);
        glm::vec3 normalized_up =
            glm::dot(up, up) > kMinLookDirectionLengthSquared ? glm::normalize(up) : glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::vec3 cross_with_requested_up = glm::cross(normalized_direction, normalized_up);
        if (glm::dot(cross_with_requested_up, cross_with_requested_up) <= kMinLookDirectionLengthSquared) {
            normalized_up = glm::vec3(0.0f, 1.0f, 0.0f);
            const glm::vec3 cross_with_world_up = glm::cross(normalized_direction, normalized_up);
            if (glm::dot(cross_with_world_up, cross_with_world_up) <= kMinLookDirectionLengthSquared) {
                normalized_up = glm::vec3(0.0f, 0.0f, 1.0f);
            }
        }

        setRotation(glm::eulerAngles(glm::quatLookAtRH(normalized_direction, normalized_up)));
        return true;
    }

    bool hasCamera() const
    {
        return host_api != nullptr && host_api->entity_has_camera != nullptr &&
               host_api->entity_has_camera(scene_context, entity_id) != 0;
    }

    sol::object getCamera(sol::this_state state) const
    {
        sol::state_view lua(state);
        if (host_api == nullptr || host_api->entity_get_camera == nullptr) {
            return sol::make_object(lua, sol::nil);
        }

        LunaScriptCameraDesc camera{};
        if (host_api->entity_get_camera(scene_context, entity_id, &camera) == 0) {
            return sol::make_object(lua, sol::nil);
        }

        return sol::make_object(lua, toLuaCamera(camera));
    }

    bool setCamera(const LuaCamera& camera)
    {
        if (host_api == nullptr || host_api->entity_set_camera == nullptr) {
            return false;
        }

        const LunaScriptCameraDesc desc = toScriptCameraDesc(camera);
        return host_api->entity_set_camera(scene_context, entity_id, &desc) != 0;
    }

    bool setPrimaryCamera(bool primary)
    {
        return host_api != nullptr && host_api->entity_set_camera_primary != nullptr &&
               host_api->entity_set_camera_primary(scene_context, entity_id, primary ? 1 : 0) != 0;
    }

    bool setPerspectiveCamera(float vertical_fov_radians, float near_clip, float far_clip)
    {
        return host_api != nullptr && host_api->entity_set_perspective_camera != nullptr &&
               host_api->entity_set_perspective_camera(
                   scene_context, entity_id, vertical_fov_radians, near_clip, far_clip) != 0;
    }

    bool setOrthographicCamera(float vertical_size, float near_clip, float far_clip)
    {
        return host_api != nullptr && host_api->entity_set_orthographic_camera != nullptr &&
               host_api->entity_set_orthographic_camera(scene_context, entity_id, vertical_size, near_clip, far_clip) !=
                   0;
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

    bindLuaCameraProjectionConstants(lua_state);
    bindLuaVec3Type(lua_state);
    lua_state.new_usertype<LuaCamera>("Camera",
                                      sol::call_constructor,
                                      sol::constructors<LuaCamera()>(),
                                      "primary",
                                      &LuaCamera::primary,
                                      "fixed_aspect_ratio",
                                      &LuaCamera::fixed_aspect_ratio,
                                      "projection_type",
                                      &LuaCamera::projection_type,
                                      "perspective_vertical_fov",
                                      &LuaCamera::perspective_vertical_fov,
                                      "perspective_near",
                                      &LuaCamera::perspective_near,
                                      "perspective_far",
                                      &LuaCamera::perspective_far,
                                      "orthographic_size",
                                      &LuaCamera::orthographic_size,
                                      "orthographic_near",
                                      &LuaCamera::orthographic_near,
                                      "orthographic_far",
                                      &LuaCamera::orthographic_far);
    lua_state.new_usertype<LuaEntity>(
        "Entity",
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
        sol::property(&LuaEntity::getScale, &LuaEntity::setScale),
        "forward",
        sol::property(&LuaEntity::getForwardDirection),
        "right",
        sol::property(&LuaEntity::getRightDirection),
        "up",
        sol::property(&LuaEntity::getUpDirection),
        "translate_world",
        &LuaEntity::translateWorld,
        "translate_local",
        &LuaEntity::translateLocal,
        "look_at",
        sol::overload(
            [](LuaEntity& entity, const glm::vec3& target) {
                return entity.lookAt(target);
            },
            [](LuaEntity& entity, const glm::vec3& target, const glm::vec3& up) {
                return entity.lookAt(target, up);
            }),
        "has_camera",
        &LuaEntity::hasCamera,
        "get_camera",
        [](const LuaEntity& entity, sol::this_state state) {
            return entity.getCamera(state);
        },
        "set_camera",
        &LuaEntity::setCamera,
        "set_primary_camera",
        &LuaEntity::setPrimaryCamera,
        "set_perspective_camera",
        &LuaEntity::setPerspectiveCamera,
        "set_orthographic_camera",
        &LuaEntity::setOrthographicCamera);
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
            instance_table[script_property.name] = makeLuaUint64(runtime.luaState(), script_property.asset_value);
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
    instance_table["script_asset"] = makeLuaUint64(runtime.luaState(), script_instance.script_asset);

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
