#pragma once

#include "Core/Timestep.h"
#include "Core/UUID.h"

#include <cstddef>

namespace luna {

class Scene;
struct ScriptProperty;

class IScriptRuntime {
public:
    virtual ~IScriptRuntime() = default;

    virtual const char* name() const noexcept = 0;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual void onRuntimeStart(Scene& scene) = 0;
    virtual void onRuntimeStop(Scene& scene) = 0;
    virtual void onUpdate(Scene& scene, Timestep timestep) = 0;
    virtual void setScriptProperty(
        Scene& scene, UUID entity_id, UUID script_id, const ScriptProperty& property, size_t property_index) = 0;
};

} // namespace luna
