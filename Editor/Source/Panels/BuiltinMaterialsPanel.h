#pragma once

#include "Asset/Asset.h"

namespace luna {

class BuiltinMaterialsPanel {
public:
    void onImGuiRender(bool& open);
    void focusMaterial(AssetHandle material_handle);

private:
    AssetHandle m_selected_material{0};
};

} // namespace luna
