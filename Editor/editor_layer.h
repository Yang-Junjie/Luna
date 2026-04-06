#pragma once

#include "Core/layer.h"

#include <array>

namespace luna::editor {

class EditorApp;

class EditorLayer final : public Layer {
public:
    explicit EditorLayer(EditorApp& app);

    void onUpdate(Timestep timestep) override;

    bool selfTestPassed() const
    {
        return m_selfTestPassed;
    }

private:
    void updateSelfTest();
    void buildUi(float frameTimeMs);
    std::array<float, 4> computeClearColor() const;

    EditorApp& m_app;
    bool m_showDemoWindow = true;
    bool m_warmBackground = true;
    float m_backgroundMix = 0.35f;
    bool m_selfTestPassed = false;
    float m_elapsedSeconds = 0.0f;
    int m_selfTestPhase = 0;
};

} // namespace luna::editor
