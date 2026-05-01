#include "ScriptHostBridge.h"

#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Core/Input.h"
#include "Core/KeyCodes.h"
#include "Core/MouseCodes.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/Components/ScriptComponent.h"
#include "Script/ScriptAsset.h"

#include <glm/vec3.hpp>
#include <string>

namespace {

luna::Scene* getScene(void* scene_context)
{
    return static_cast<luna::Scene*>(scene_context);
}

luna::Entity findEntity(luna::Scene* scene, uint64_t entity_id)
{
    if (scene == nullptr || entity_id == 0) {
        return {};
    }

    return scene->entityManager().findEntityByUUID(luna::UUID(entity_id));
}

LunaScriptVec3 toScriptVec3(const glm::vec3& value)
{
    return LunaScriptVec3{value.x, value.y, value.z};
}

glm::vec3 toGlmVec3(const LunaScriptVec3& value)
{
    return glm::vec3(value.x, value.y, value.z);
}

LunaScriptPropertyType toScriptPropertyType(luna::ScriptPropertyType type)
{
    switch (type) {
        case luna::ScriptPropertyType::Bool:
            return LunaScriptPropertyType_Bool;
        case luna::ScriptPropertyType::Int:
            return LunaScriptPropertyType_Int;
        case luna::ScriptPropertyType::Float:
            return LunaScriptPropertyType_Float;
        case luna::ScriptPropertyType::String:
            return LunaScriptPropertyType_String;
        case luna::ScriptPropertyType::Vec3:
            return LunaScriptPropertyType_Vec3;
        case luna::ScriptPropertyType::Entity:
            return LunaScriptPropertyType_Entity;
        case luna::ScriptPropertyType::Asset:
            return LunaScriptPropertyType_Asset;
        default:
            return LunaScriptPropertyType_Float;
    }
}

const char* sceneGetName(void* scene_context)
{
    if (luna::Scene* scene = getScene(scene_context)) {
        return scene->getName().c_str();
    }

    return "";
}

int sceneEnumerateScriptInstances(void* scene_context, void* user_data, LunaScriptEnumerateInstancesFn enumerate_fn)
{
    if (enumerate_fn == nullptr) {
        return 0;
    }

    luna::Scene* scene = getScene(scene_context);
    if (scene == nullptr) {
        return 0;
    }

    const auto view = scene->entityManager().registry().view<luna::ScriptComponent>();
    for (const auto entity_handle : view) {
        luna::Entity entity(entity_handle, &scene->entityManager());
        const luna::ScriptComponent& script_component = view.get<luna::ScriptComponent>(entity_handle);
        if (!script_component.enabled) {
            continue;
        }

        for (const luna::ScriptEntry& script_entry : script_component.scripts) {
            if (!script_entry.enabled || !script_entry.scriptAsset.isValid()) {
                continue;
            }

            if (!luna::AssetDatabase::exists(script_entry.scriptAsset)) {
                continue;
            }

            const luna::AssetMetadata& metadata = luna::AssetDatabase::getAssetMetadata(script_entry.scriptAsset);
            if (metadata.Type != luna::AssetType::Script) {
                continue;
            }

            const auto script_asset = luna::AssetManager::get().loadAssetAs<luna::ScriptAsset>(script_entry.scriptAsset);
            if (!script_asset) {
                continue;
            }

            LunaScriptInstanceDesc descriptor{};
            descriptor.entity_id = static_cast<uint64_t>(entity.getUUID());
            descriptor.entity_name = entity.getName().c_str();
            descriptor.script_id = static_cast<uint64_t>(script_entry.id);
            descriptor.script_asset = static_cast<uint64_t>(script_entry.scriptAsset);
            descriptor.type_name = script_entry.typeName.c_str();
            descriptor.asset_name = metadata.Name.c_str();
            descriptor.language = script_asset->language.c_str();
            descriptor.source = script_asset->source.c_str();
            descriptor.execution_order = script_entry.executionOrder;

            if (enumerate_fn(user_data, &descriptor) == 0) {
                return 0;
            }
        }
    }

    return 1;
}

int sceneEnumerateScriptProperties(void* scene_context,
                                   uint64_t entity_id,
                                   uint64_t script_id,
                                   void* user_data,
                                   LunaScriptEnumeratePropertiesFn enumerate_fn)
{
    if (enumerate_fn == nullptr) {
        return 0;
    }

    luna::Scene* scene = getScene(scene_context);
    if (scene == nullptr) {
        return 0;
    }

    luna::Entity entity = findEntity(scene, entity_id);
    if (!entity || !entity.hasComponent<luna::ScriptComponent>()) {
        return 0;
    }

    const luna::ScriptComponent& script_component = entity.getComponent<luna::ScriptComponent>();
    for (const luna::ScriptEntry& script_entry : script_component.scripts) {
        if (static_cast<uint64_t>(script_entry.id) != script_id) {
            continue;
        }

        for (const luna::ScriptProperty& property : script_entry.properties) {
            LunaScriptPropertyDesc descriptor{};
            descriptor.name = property.name.c_str();
            descriptor.type = toScriptPropertyType(property.type);
            descriptor.bool_value = property.boolValue ? 1 : 0;
            descriptor.int_value = property.intValue;
            descriptor.float_value = property.floatValue;
            descriptor.string_value = property.stringValue.c_str();
            descriptor.vec3_value = toScriptVec3(property.vec3Value);
            descriptor.entity_value = static_cast<uint64_t>(property.entityValue);
            descriptor.asset_value = static_cast<uint64_t>(property.assetValue);

            if (enumerate_fn(user_data, &descriptor) == 0) {
                return 0;
            }
        }

        return 1;
    }

    return 0;
}

int entityIsValid(void* scene_context, uint64_t entity_id)
{
    luna::Scene* scene = getScene(scene_context);
    const luna::Entity entity = findEntity(scene, entity_id);
    return entity.isValid() ? 1 : 0;
}

const char* entityGetName(void* scene_context, uint64_t entity_id)
{
    luna::Scene* scene = getScene(scene_context);
    const luna::Entity entity = findEntity(scene, entity_id);
    if (!entity) {
        return "";
    }

    return entity.getName().c_str();
}

int entityGetTranslation(void* scene_context, uint64_t entity_id, LunaScriptVec3* out_value)
{
    if (out_value == nullptr) {
        return 0;
    }

    luna::Scene* scene = getScene(scene_context);
    const luna::Entity entity = findEntity(scene, entity_id);
    if (!entity || !entity.hasComponent<luna::TransformComponent>()) {
        return 0;
    }

    *out_value = toScriptVec3(entity.transform().translation);
    return 1;
}

int entitySetTranslation(void* scene_context, uint64_t entity_id, const LunaScriptVec3* value)
{
    if (value == nullptr) {
        return 0;
    }

    luna::Scene* scene = getScene(scene_context);
    luna::Entity entity = findEntity(scene, entity_id);
    if (!entity || !entity.hasComponent<luna::TransformComponent>()) {
        return 0;
    }

    entity.transform().translation = toGlmVec3(*value);
    return 1;
}

int entityGetRotation(void* scene_context, uint64_t entity_id, LunaScriptVec3* out_value)
{
    if (out_value == nullptr) {
        return 0;
    }

    luna::Scene* scene = getScene(scene_context);
    const luna::Entity entity = findEntity(scene, entity_id);
    if (!entity || !entity.hasComponent<luna::TransformComponent>()) {
        return 0;
    }

    *out_value = toScriptVec3(entity.transform().getRotationEuler());
    return 1;
}

int entitySetRotation(void* scene_context, uint64_t entity_id, const LunaScriptVec3* value)
{
    if (value == nullptr) {
        return 0;
    }

    luna::Scene* scene = getScene(scene_context);
    luna::Entity entity = findEntity(scene, entity_id);
    if (!entity || !entity.hasComponent<luna::TransformComponent>()) {
        return 0;
    }

    entity.transform().setRotationEuler(toGlmVec3(*value));
    return 1;
}

int entityGetScale(void* scene_context, uint64_t entity_id, LunaScriptVec3* out_value)
{
    if (out_value == nullptr) {
        return 0;
    }

    luna::Scene* scene = getScene(scene_context);
    const luna::Entity entity = findEntity(scene, entity_id);
    if (!entity || !entity.hasComponent<luna::TransformComponent>()) {
        return 0;
    }

    *out_value = toScriptVec3(entity.transform().scale);
    return 1;
}

int entitySetScale(void* scene_context, uint64_t entity_id, const LunaScriptVec3* value)
{
    if (value == nullptr) {
        return 0;
    }

    luna::Scene* scene = getScene(scene_context);
    luna::Entity entity = findEntity(scene, entity_id);
    if (!entity || !entity.hasComponent<luna::TransformComponent>()) {
        return 0;
    }

    entity.transform().scale = toGlmVec3(*value);
    return 1;
}

int inputIsKeyPressed(int32_t key_code)
{
    return luna::Input::isKeyPressed(static_cast<luna::KeyCode>(key_code)) ? 1 : 0;
}

int inputIsMouseButtonPressed(int32_t button_code)
{
    return luna::Input::isMouseButtonPressed(static_cast<luna::MouseCode>(button_code)) ? 1 : 0;
}

float inputGetMouseX()
{
    return luna::Input::getMouseX();
}

float inputGetMouseY()
{
    return luna::Input::getMouseY();
}

} // namespace

namespace luna {

void initializeScriptHostApiBridge(LunaScriptHostApi& host_api)
{
    host_api.scene_get_name = &sceneGetName;
    host_api.scene_enumerate_script_instances = &sceneEnumerateScriptInstances;
    host_api.scene_enumerate_script_properties = &sceneEnumerateScriptProperties;
    host_api.entity_is_valid = &entityIsValid;
    host_api.entity_get_name = &entityGetName;
    host_api.entity_get_translation = &entityGetTranslation;
    host_api.entity_set_translation = &entitySetTranslation;
    host_api.entity_get_rotation = &entityGetRotation;
    host_api.entity_set_rotation = &entitySetRotation;
    host_api.entity_get_scale = &entityGetScale;
    host_api.entity_set_scale = &entitySetScale;
    host_api.input_is_key_pressed = &inputIsKeyPressed;
    host_api.input_is_mouse_button_pressed = &inputIsMouseButtonPressed;
    host_api.input_get_mouse_x = &inputGetMouseX;
    host_api.input_get_mouse_y = &inputGetMouseY;
}

} // namespace luna
