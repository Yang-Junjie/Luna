#pragma once

#include "Core/layer.h"

namespace luna::editor {

class EditorLayer final : public Layer {
public:
    EditorLayer()
        : Layer("EditorLayer")
    {}
};

} // namespace luna::editor
