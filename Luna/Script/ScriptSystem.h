#pragma once

#include "Core/Timestep.h"
#include "Core/UUID.h"

#include <cstddef>
#include <memory>

namespace luna {

class IScriptRuntime;
class Scene;
struct ScriptProperty;

class ScriptSystem {
public:
    ScriptSystem();
    ~ScriptSystem();

    void setRuntime(std::unique_ptr<IScriptRuntime> runtime);
    [[nodiscard]] IScriptRuntime* runtime() noexcept;
    [[nodiscard]] const IScriptRuntime* runtime() const noexcept;

    bool initialize();
    void shutdown();
    void onRuntimeStart(Scene& scene);
    void onRuntimeStop(Scene& scene);
    void onUpdate(Scene& scene, Timestep timestep);
    void setScriptProperty(Scene& scene,
                           UUID entity_id,
                           UUID script_id,
                           const ScriptProperty& property,
                           size_t property_index);

private:
    std::unique_ptr<IScriptRuntime> m_runtime;
    bool m_initialized{false};
    bool m_runtime_started{false};
};

} // namespace luna
