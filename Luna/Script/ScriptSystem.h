#pragma once

#include "Core/Timestep.h"

#include <memory>

namespace luna {

class IScriptRuntime;
class Scene;

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

private:
    std::unique_ptr<IScriptRuntime> m_runtime;
    bool m_initialized{false};
    bool m_runtime_started{false};
};

} // namespace luna
