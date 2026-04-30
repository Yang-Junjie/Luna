#pragma once

#include "ScriptRuntime.h"

#include <memory>

namespace sol {
class state;
}

namespace luna {

class LuaScriptRuntime final : public IScriptRuntime {
public:
    LuaScriptRuntime();
    ~LuaScriptRuntime() override;

    const char* name() const noexcept override;
    bool initialize() override;
    void shutdown() override;
    void onRuntimeStart(Scene& scene) override;
    void onRuntimeStop(Scene& scene) override;
    void onUpdate(Scene& scene, Timestep timestep) override;

private:
    std::unique_ptr<sol::state> m_lua_state;
};

} // namespace luna
