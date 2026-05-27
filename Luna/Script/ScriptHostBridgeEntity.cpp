#include "Scene/Components/TransformComponent.h"
#include "ScriptHostBridgeInternal.h"

namespace {

int entityIsValid(void* scene_context, uint64_t entity_id)
{
    luna::Scene* scene = luna::getScriptSceneContext(scene_context);
    const luna::Entity entity = luna::findScriptEntityById(scene, entity_id);
    return entity.isValid() ? 1 : 0;
}

const char* entityGetName(void* scene_context, uint64_t entity_id)
{
    luna::Scene* scene = luna::getScriptSceneContext(scene_context);
    const luna::Entity entity = luna::findScriptEntityById(scene, entity_id);
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

    luna::Scene* scene = luna::getScriptSceneContext(scene_context);
    const luna::Entity entity = luna::findScriptEntityById(scene, entity_id);
    if (!entity || !entity.hasComponent<luna::TransformComponent>()) {
        return 0;
    }

    *out_value = luna::toScriptVec3(entity.transform().translation);
    return 1;
}

int entitySetTranslation(void* scene_context, uint64_t entity_id, const LunaScriptVec3* value)
{
    if (value == nullptr) {
        return 0;
    }

    luna::Scene* scene = luna::getScriptSceneContext(scene_context);
    luna::Entity entity = luna::findScriptEntityById(scene, entity_id);
    if (!entity || !entity.hasComponent<luna::TransformComponent>()) {
        return 0;
    }

    entity.transform().translation = luna::toGlmVec3(*value);
    return 1;
}

int entityGetRotation(void* scene_context, uint64_t entity_id, LunaScriptVec3* out_value)
{
    if (out_value == nullptr) {
        return 0;
    }

    luna::Scene* scene = luna::getScriptSceneContext(scene_context);
    const luna::Entity entity = luna::findScriptEntityById(scene, entity_id);
    if (!entity || !entity.hasComponent<luna::TransformComponent>()) {
        return 0;
    }

    *out_value = luna::toScriptVec3(entity.transform().getRotationEuler());
    return 1;
}

int entitySetRotation(void* scene_context, uint64_t entity_id, const LunaScriptVec3* value)
{
    if (value == nullptr) {
        return 0;
    }

    luna::Scene* scene = luna::getScriptSceneContext(scene_context);
    luna::Entity entity = luna::findScriptEntityById(scene, entity_id);
    if (!entity || !entity.hasComponent<luna::TransformComponent>()) {
        return 0;
    }

    entity.transform().setRotationEuler(luna::toGlmVec3(*value));
    return 1;
}

int entityGetScale(void* scene_context, uint64_t entity_id, LunaScriptVec3* out_value)
{
    if (out_value == nullptr) {
        return 0;
    }

    luna::Scene* scene = luna::getScriptSceneContext(scene_context);
    const luna::Entity entity = luna::findScriptEntityById(scene, entity_id);
    if (!entity || !entity.hasComponent<luna::TransformComponent>()) {
        return 0;
    }

    *out_value = luna::toScriptVec3(entity.transform().scale);
    return 1;
}

int entitySetScale(void* scene_context, uint64_t entity_id, const LunaScriptVec3* value)
{
    if (value == nullptr) {
        return 0;
    }

    luna::Scene* scene = luna::getScriptSceneContext(scene_context);
    luna::Entity entity = luna::findScriptEntityById(scene, entity_id);
    if (!entity || !entity.hasComponent<luna::TransformComponent>()) {
        return 0;
    }

    entity.transform().scale = luna::toGlmVec3(*value);
    return 1;
}

} // namespace

namespace luna {

void registerScriptEntityHostApi(LunaScriptHostApi& host_api)
{
    host_api.entity_is_valid = &entityIsValid;
    host_api.entity_get_name = &entityGetName;
    host_api.entity_get_translation = &entityGetTranslation;
    host_api.entity_set_translation = &entitySetTranslation;
    host_api.entity_get_rotation = &entityGetRotation;
    host_api.entity_set_rotation = &entitySetRotation;
    host_api.entity_get_scale = &entityGetScale;
    host_api.entity_set_scale = &entitySetScale;
}

} // namespace luna
