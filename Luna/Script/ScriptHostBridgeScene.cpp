#include "ScriptHostBridgeInternal.h"

#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Scene/Components/ScriptComponent.h"
#include "Script/ScriptAsset.h"

namespace {

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
    if (luna::Scene* scene = luna::getScriptSceneContext(scene_context)) {
        return scene->getName().c_str();
    }

    return "";
}

int sceneEnumerateScriptInstances(void* scene_context, void* user_data, LunaScriptEnumerateInstancesFn enumerate_fn)
{
    if (enumerate_fn == nullptr) {
        return 0;
    }

    luna::Scene* scene = luna::getScriptSceneContext(scene_context);
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

    luna::Scene* scene = luna::getScriptSceneContext(scene_context);
    if (scene == nullptr) {
        return 0;
    }

    luna::Entity entity = luna::findScriptEntityById(scene, entity_id);
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
            descriptor.vec3_value = luna::toScriptVec3(property.vec3Value);
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

} // namespace

namespace luna {

void registerScriptSceneHostApi(LunaScriptHostApi& host_api)
{
    host_api.scene_get_name = &sceneGetName;
    host_api.scene_enumerate_script_instances = &sceneEnumerateScriptInstances;
    host_api.scene_enumerate_script_properties = &sceneEnumerateScriptProperties;
}

} // namespace luna
