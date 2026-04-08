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

    static void setCursorMode(CursorMode mode);

    static void setMousePosition(float x, float y);
    static void setRawMouseMotion(bool enabled);

    static float getMouseX();

    static float getMouseY();
};
} // namespace luna

