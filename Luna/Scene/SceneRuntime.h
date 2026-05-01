#pragma once

#include "Script/ScriptSystem.h"

#include <cstddef>
#include <memory>

namespace luna {

class IScriptRuntime;
class Scene;
struct ScriptProperty;

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
    void setScriptProperty(UUID entity_id, UUID script_id, const ScriptProperty& property, size_t property_index);
    [[nodiscard]] bool isRunning() const noexcept;

private:
    Scene* m_scene{nullptr};
    ScriptSystem m_script_system;
    bool m_running{false};
};

} // namespace luna
