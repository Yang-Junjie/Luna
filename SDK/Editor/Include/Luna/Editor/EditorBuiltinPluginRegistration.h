#pragma once

#include "EditorApi/EditorPlugin.h"

#include <functional>
#include <memory>
#include <string>

namespace luna::editor {

using EditorBuiltinPluginFactory = std::function<std::unique_ptr<Plugin>()>;

bool registerBuiltinEditorPluginFactory(std::string plugin_id, EditorBuiltinPluginFactory factory);

class EditorBuiltinPluginFactoryRegistration final {
public:
    EditorBuiltinPluginFactoryRegistration(std::string plugin_id, EditorBuiltinPluginFactory factory);
};

} // namespace luna::editor
