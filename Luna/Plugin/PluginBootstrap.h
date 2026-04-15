#pragma once

namespace luna {

class PluginRegistry;

void registerResolvedPlugins(PluginRegistry& registry);
const char* getResolvedRenderGraphProviderId();

} // namespace luna
