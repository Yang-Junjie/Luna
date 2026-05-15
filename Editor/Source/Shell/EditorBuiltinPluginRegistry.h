#pragma once

#include "Luna/Editor/EditorBuiltinPluginRegistration.h"

#include <string_view>

namespace luna::editor {

class EditorBuiltinPluginRegistry final {
public:
    [[nodiscard]] static EditorBuiltinPluginFactory findFactory(std::string_view plugin_id);
};

} // namespace luna::editor
