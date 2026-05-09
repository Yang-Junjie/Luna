#pragma once

#include "Core/KeyCodes.h"
#include "Core/MouseCodes.h"

#include <glm/glm.hpp>

namespace luna {

class Input {
public:
    static bool isKeyPressed(KeyCode key);

    static bool isMouseButtonPressed(MouseCode button);

    static glm::vec2 getMousePosition();

    static glm::vec2 getMouseDelta();

    static glm::vec2 getMouseScrollOffset();

    static void setCursorMode(CursorMode mode);

    static CursorMode getCursorMode();

    static void setMousePosition(float x, float y);
    static void setRawMouseMotion(bool enabled);

    static float getMouseX();

    static float getMouseY();

    static float getMouseDeltaX();

    static float getMouseDeltaY();

    static float getMouseScrollX();

    static float getMouseScrollY();

    static void resetFrameState();
    static void recordMouseMoved(float x, float y);
    static void recordMouseScrolled(float x_offset, float y_offset);
};
} // namespace luna
