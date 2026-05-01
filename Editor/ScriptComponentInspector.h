#pragma once

namespace luna {

class Entity;
struct ScriptComponent;

struct ScriptComponentInspectorChange {
    bool changed{false};
    bool script_structure_changed{false};
    bool property_value_changed{false};
    size_t script_index{0};
    size_t property_index{0};
};

ScriptComponentInspectorChange drawScriptComponentInspector(Entity owner_entity, ScriptComponent& script_component);

} // namespace luna
