#pragma once

#include "EditorApi/EditorPlugin.h"

#include <memory>

namespace luna::editor {

std::unique_ptr<Plugin> createCoreCommandsPlugin();

} // namespace luna::editor
