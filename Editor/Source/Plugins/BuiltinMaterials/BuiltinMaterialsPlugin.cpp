#include "Plugins/BuiltinMaterials/BuiltinMaterialsPlugin.h"

#include "EditorApi/EditorApi.h"

#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Asset/BuiltinAssets.h"
#include "Project/BuiltinMaterialOverrides.h"
#include "Renderer/Material.h"

#include <algorithm>
#include <filesystem>
#include <string>

namespace {

constexpr const char* kPluginId = "luna.editor.builtin-materials";
constexpr const char* kWindowId = "luna.editor.builtin-materials.window";

const luna::BuiltinMaterialDescriptor& firstBuiltinMaterial()
{
    return luna::BuiltinAssets::getBuiltinMaterials().front();
}

const char* blendModeToString(luna::Material::BlendMode blend_mode)
{
    switch (blend_mode) {
        case luna::Material::BlendMode::Opaque:
            return "Opaque";
        case luna::Material::BlendMode::Masked:
            return "Masked";
        case luna::Material::BlendMode::Transparent:
            return "Transparent";
        case luna::Material::BlendMode::Additive:
            return "Additive";
        default:
            return "Unknown";
    }
}

luna::editor::Vec3 toEditorVec3(const glm::vec3& value)
{
    return luna::editor::Vec3{.x = value.x, .y = value.y, .z = value.z};
}

void fromEditorVec3(const luna::editor::Vec3& value, glm::vec3& out_value)
{
    out_value = glm::vec3{value.x, value.y, value.z};
}

luna::editor::Vec4 toEditorVec4(const glm::vec4& value)
{
    return luna::editor::Vec4{.x = value.x, .y = value.y, .z = value.z, .w = value.w};
}

void fromEditorVec4(const luna::editor::Vec4& value, glm::vec4& out_value)
{
    out_value = glm::vec4{value.x, value.y, value.z, value.w};
}

} // namespace

namespace luna::editor {

PluginDescriptor BuiltinMaterialsPlugin::descriptor() const
{
    return PluginDescriptor{
        .id = kPluginId,
        .display_name = "Builtin Materials",
        .version = "0.1.0",
    };
}

bool BuiltinMaterialsPlugin::onLoad(Host& host)
{
    return host.windows().registerWindow(WindowDescriptor{
        .id = kWindowId,
        .title = "Builtin Materials",
        .default_open = false,
        .default_size = Vec2{.x = 420.0f, .y = 520.0f},
        .draw =
            [this](WindowDrawContext& context) {
                Ui& ui = context.ui();

                if (!m_selected_material.isValid() || !BuiltinAssets::isBuiltinMaterial(m_selected_material)) {
                    m_selected_material = firstBuiltinMaterial().Handle;
                }

                const char* selected_name = BuiltinAssets::getDisplayName(m_selected_material);
                ui.textDisabled("Global built-in assets. Changes affect every user of the material.");

                if (ui.beginCombo("Material", selected_name[0] != '\0' ? selected_name : "Unknown")) {
                    for (const auto& material : BuiltinAssets::getBuiltinMaterials()) {
                        const bool selected = m_selected_material == material.Handle;
                        if (ui.selectable(material.Name, selected)) {
                            m_selected_material = material.Handle;
                        }
                        if (selected) {
                            ui.setItemDefaultFocus();
                        }
                    }
                    ui.endCombo();
                }

                auto material = AssetManager::get().loadAssetAs<Material>(m_selected_material);
                if (!material) {
                    ui.textDisabled("Selected built-in material is not loaded.");
                    return;
                }

                ui.textDisabled(std::string("Handle: ") + m_selected_material.toString());
                if (AssetDatabase::exists(m_selected_material)) {
                    const auto& metadata = AssetDatabase::getAssetMetadata(m_selected_material);
                    ui.textDisabled(std::string("Path: ") + metadata.FilePath.generic_string());
                }
                ui.textDisabled(std::string("Version: ") + std::to_string(material->getVersion()));

                const std::filesystem::path overrides_path = BuiltinMaterialOverrides::getOverridesPath();
                ui.textDisabled(std::string("Overrides: ") +
                                (overrides_path.empty() ? std::string("No project loaded")
                                                        : overrides_path.generic_string()));

                ui.separator();
                ui.text("Surface");

                auto surface = material->getSurface();
                bool changed = false;

                luna::editor::Vec4 base_color = toEditorVec4(surface.BaseColorFactor);
                if (ui.colorEdit4("Base Color", base_color)) {
                    fromEditorVec4(base_color, surface.BaseColorFactor);
                    changed = true;
                }

                luna::editor::Vec3 emissive = toEditorVec3(surface.EmissiveFactor);
                if (ui.colorEdit3("Emissive", emissive)) {
                    fromEditorVec3(emissive, surface.EmissiveFactor);
                    changed = true;
                }

                changed |= ui.sliderFloat("Metallic", surface.MetallicFactor, 0.0f, 1.0f);
                changed |= ui.sliderFloat("Roughness", surface.RoughnessFactor, 0.0f, 1.0f);
                changed |= ui.sliderFloat("Alpha Cutoff", surface.AlphaCutoff, 0.0f, 1.0f);
                changed |= ui.checkbox("Unlit", surface.Unlit);

                ui.textDisabled(std::string("Blend Mode: ") + blendModeToString(surface.BlendModeValue));
                ui.textDisabled("Texture and blend-mode editing are intentionally disabled in this first pass.");

                if (changed) {
                    surface.MetallicFactor = std::clamp(surface.MetallicFactor, 0.0f, 1.0f);
                    surface.RoughnessFactor = std::clamp(surface.RoughnessFactor, 0.0f, 1.0f);
                    surface.AlphaCutoff = std::clamp(surface.AlphaCutoff, 0.0f, 1.0f);
                    material->setSurface(surface);
                }

                ui.separator();
                if (ui.button("Reset Selected", Vec2{.x = -1.0f, .y = 0.0f})) {
                    material->resetSurface();
                }

                if (ui.button("Reset All Builtin Materials", Vec2{.x = -1.0f, .y = 0.0f})) {
                    for (const auto& descriptor : BuiltinAssets::getBuiltinMaterials()) {
                        if (auto builtin_material = AssetManager::get().loadAssetAs<Material>(descriptor.Handle)) {
                            builtin_material->resetSurface();
                        }
                    }
                }

                ui.separator();
                if (ui.button("Save Overrides", Vec2{.x = -1.0f, .y = 0.0f})) {
                    BuiltinMaterialOverrides::save();
                }

                if (ui.button("Reload Overrides", Vec2{.x = -1.0f, .y = 0.0f})) {
                    for (const auto& descriptor : BuiltinAssets::getBuiltinMaterials()) {
                        if (auto builtin_material = AssetManager::get().loadAssetAs<Material>(descriptor.Handle)) {
                            builtin_material->resetSurface();
                        }
                    }
                    BuiltinMaterialOverrides::load();
                }

                if (ui.button("Clear Selected Override", Vec2{.x = -1.0f, .y = 0.0f})) {
                    BuiltinMaterialOverrides::clearSelected(m_selected_material);
                }

                if (ui.button("Clear All Overrides", Vec2{.x = -1.0f, .y = 0.0f})) {
                    BuiltinMaterialOverrides::clearAll();
                }
            },
    });
}

void BuiltinMaterialsPlugin::onUnload(Host& host)
{
    host.windows().unregisterWindow(kWindowId);
}

void BuiltinMaterialsPlugin::focusMaterial(AssetHandle material_handle)
{
    if (BuiltinAssets::isBuiltinMaterial(material_handle)) {
        m_selected_material = material_handle;
    }
}

std::unique_ptr<BuiltinMaterialsPlugin> createBuiltinMaterialsPlugin()
{
    return std::make_unique<BuiltinMaterialsPlugin>();
}

} // namespace luna::editor
