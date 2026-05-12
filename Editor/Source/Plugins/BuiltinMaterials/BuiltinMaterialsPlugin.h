#pragma once

#include "Asset/Asset.h"
#include "EditorApi/EditorPlugin.h"

#include <memory>

namespace luna::editor {

class BuiltinMaterialsPlugin final : public Plugin {
public:
    [[nodiscard]] PluginDescriptor descriptor() const override;
    bool onLoad(Host& host) override;
    void onUnload(Host& host) override;

    void focusMaterial(AssetHandle material_handle);

private:
    AssetHandle m_selected_material{0};
};

std::unique_ptr<BuiltinMaterialsPlugin> createBuiltinMaterialsPlugin();

} // namespace luna::editor
