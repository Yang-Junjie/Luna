#pragma once

#include "Luna/Editor/Native/NativeHost.h"

namespace luna::editor::native {

[[nodiscard]] inline bool fillPluginApi(const PluginDescriptor& descriptor,
                                        LunaEditorPluginApi* out_plugin_api) noexcept
{
    return writePluginApi(descriptor, out_plugin_api);
}

} // namespace luna::editor::native
