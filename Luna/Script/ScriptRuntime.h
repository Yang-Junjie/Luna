#pragma once

#include "Core/Timestep.h"

namespace luna {

class Scene;

class IScriptRuntime {
public:
    virtual ~IScriptRuntime() = default;

    virtual const char* name() const noexcept = 0;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual void onRuntimeStart(Scene& scene) = 0;
    virtual void onRuntimeStop(Scene& scene) = 0;
    virtual void onUpdate(Scene& scene, Timestep timestep) = 0;
};

} // namespace luna
