#pragma once

#include "Core/Layer.h"
#include "Plugin/ServiceRegistry.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace luna {

namespace editor {
class EditorRegistry;
}

class PluginRegistry {
public:
    using LayerFactory = std::function<std::unique_ptr<Layer>()>;

    struct LayerContribution {
        std::string m_id;
        bool m_overlay = false;
        LayerFactory m_factory;
    };

    explicit PluginRegistry(ServiceRegistry& services, editor::EditorRegistry* editor_registry = nullptr);

    ServiceRegistry& services()
    {
        return m_services;
    }

    const ServiceRegistry& services() const
    {
        return m_services;
    }

    bool hasEditorRegistry() const
    {
        return m_editor_registry != nullptr;
    }

    editor::EditorRegistry& editor();
    const editor::EditorRegistry& editor() const;

    void addLayer(std::string id, LayerFactory factory);
    void addOverlay(std::string id, LayerFactory factory);

    const std::vector<LayerContribution>& layers() const
    {
        return m_layers;
    }

private:
    void addLayerContribution(std::string id, bool overlay, LayerFactory factory);

private:
    ServiceRegistry& m_services;
    editor::EditorRegistry* m_editor_registry = nullptr;
    std::vector<LayerContribution> m_layers;
};

} // namespace luna
