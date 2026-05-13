#pragma once

#include "EditorApi/EditorPlugin.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace luna::editor {

using EditorBuiltinPluginFactory = std::function<std::unique_ptr<Plugin>()>;

class EditorBuiltinPluginRegistry final {
public:
    static bool registerFactory(std::string plugin_id, EditorBuiltinPluginFactory factory);
    [[nodiscard]] static EditorBuiltinPluginFactory findFactory(std::string_view plugin_id);
};

class EditorBuiltinPluginFactoryRegistration final {
public:
    EditorBuiltinPluginFactoryRegistration(std::string plugin_id, EditorBuiltinPluginFactory factory);
};

} // namespace luna::editor
