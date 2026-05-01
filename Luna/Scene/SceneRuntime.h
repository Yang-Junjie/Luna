#pragma once

#include "Script/ScriptSystem.h"

#include <memory>

namespace luna {

class IScriptRuntime;
class Scene;

class SceneRuntime {
public:
    explicit SceneRuntime(Scene& scene);
    ~SceneRuntime();

    SceneRuntime(const SceneRuntime&) = delete;
    SceneRuntime& operator=(const SceneRuntime&) = delete;

    void setScriptRuntime(std::unique_ptr<IScriptRuntime> runtime);
    [[nodiscard]] ScriptSystem& scriptSystem() noexcept;
    [[nodiscard]] const ScriptSystem& scriptSystem() const noexcept;
    [[nodiscard]] Scene& scene() noexcept;
    [[nodiscard]] const Scene& scene() const noexcept;

    bool start();
    void stop();
    void update(Timestep timestep);
    [[nodiscard]] bool isRunning() const noexcept;

private:
    Scene* m_scene{nullptr};
    ScriptSystem m_script_system;
    bool m_running{false};
};

} // namespace luna
