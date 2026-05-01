#pragma once

namespace luna {

class Entity;
struct ScriptComponent;

bool drawScriptComponentInspector(Entity owner_entity, ScriptComponent& script_component);

} // namespace luna
