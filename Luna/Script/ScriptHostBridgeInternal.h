#pragma once

#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "ScriptHostBridge.h"

#include <glm/vec3.hpp>

namespace luna {

void registerScriptSceneHostApi(LunaScriptHostApi& host_api);
void registerScriptEntityHostApi(LunaScriptHostApi& host_api);
void registerScriptInputHostApi(LunaScriptHostApi& host_api);

inline Scene* getScriptSceneContext(void* scene_context)
{
    return static_cast<Scene*>(scene_context);
}

inline Entity findScriptEntityById(Scene* scene, uint64_t entity_id)
{
    if (scene == nullptr || entity_id == 0) {
        return {};
    }

    return scene->entityManager().findEntityByUUID(UUID(entity_id));
}

inline LunaScriptVec3 toScriptVec3(const glm::vec3& value)
{
    return LunaScriptVec3{value.x, value.y, value.z};
}

inline glm::vec3 toGlmVec3(const LunaScriptVec3& value)
{
    return glm::vec3(value.x, value.y, value.z);
}

} // namespace luna
