#pragma once

#include "Core/Application.h"

namespace luna::editor {

class EditorApp final : public Application {
public:
    EditorApp();

protected:
    void onInit() override;
};

} // namespace luna::editor

