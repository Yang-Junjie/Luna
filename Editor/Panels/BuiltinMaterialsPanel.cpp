#include "BuiltinMaterialsPanel.h"

#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Asset/BuiltinAssets.h"
#include "Project/BuiltinMaterialOverrides.h"
#include "Renderer/Material.h"

#include <algorithm>
#include <filesystem>

#include <imgui.h>

namespace luna {
namespace {

const BuiltinMaterialDescriptor& firstBuiltinMaterial()
{
    return BuiltinAssets::getBuiltinMaterials().front();
}

const char* blendModeToString(Material::BlendMode blend_mode)
{
    switch (blend_mode) {
        case Material::BlendMode::Opaque:
            return "Opaque";
        case Material::BlendMode::Masked:
            return "Masked";
        case Material::BlendMode::Transparent:
            return "Transparent";
        case Material::BlendMode::Additive:
            return "Additive";
        default:
            return "Unknown";
    }
}

} // namespace

void BuiltinMaterialsPanel::focusMaterial(AssetHandle material_handle)
{
    if (BuiltinAssets::isBuiltinMaterial(material_handle)) {
        m_selected_material = material_handle;
    }
}

void BuiltinMaterialsPanel::onImGuiRender(bool& open)
{
    if (!open) {
        return;
    }

    if (!m_selected_material.isValid() || !BuiltinAssets::isBuiltinMaterial(m_selected_material)) {
        m_selected_material = firstBuiltinMaterial().Handle;
    }

    ImGui::SetNextWindowSize(ImVec2(420.0f, 520.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Builtin Materials", &open)) {
        ImGui::End();
        return;
    }

    const char* selected_name = BuiltinAssets::getDisplayName(m_selected_material);
    if (ImGui::BeginCombo("Material", selected_name[0] != '\0' ? selected_name : "Unknown")) {
        for (const auto& material : BuiltinAssets::getBuiltinMaterials()) {
            const bool selected = m_selected_material == material.Handle;
            if (ImGui::Selectable(material.Name, selected)) {
                m_selected_material = material.Handle;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    auto material = AssetManager::get().loadAssetAs<Material>(m_selected_material);
    if (!material) {
        ImGui::TextDisabled("Selected built-in material is not loaded.");
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("Handle: %s", m_selected_material.toString().c_str());
    if (AssetDatabase::exists(m_selected_material)) {
        const auto& metadata = AssetDatabase::getAssetMetadata(m_selected_material);
        ImGui::TextDisabled("Path: %s", metadata.FilePath.generic_string().c_str());
    }
    ImGui::TextDisabled("Global built-in asset. Changes affect every user of this material.");
    ImGui::TextDisabled("Version: %llu", static_cast<unsigned long long>(material->getVersion()));
    const std::filesystem::path overrides_path = BuiltinMaterialOverrides::getOverridesPath();
    ImGui::TextDisabled("Overrides: %s", overrides_path.empty() ? "No project loaded" : overrides_path.generic_string().c_str());

    ImGui::SeparatorText("Surface");
    Material::SurfaceProperties surface = material->getSurface();
    bool changed = false;

    changed |= ImGui::ColorEdit4("Base Color", &surface.BaseColorFactor.x);
    changed |= ImGui::ColorEdit3("Emissive", &surface.EmissiveFactor.x);
    changed |= ImGui::SliderFloat("Metallic", &surface.MetallicFactor, 0.0f, 1.0f);
    changed |= ImGui::SliderFloat("Roughness", &surface.RoughnessFactor, 0.0f, 1.0f);
    changed |= ImGui::SliderFloat("Alpha Cutoff", &surface.AlphaCutoff, 0.0f, 1.0f);
    changed |= ImGui::Checkbox("Unlit", &surface.Unlit);

    ImGui::TextDisabled("Blend Mode: %s", blendModeToString(surface.BlendModeValue));
    ImGui::TextDisabled("Texture and blend-mode editing are intentionally disabled in this first pass.");

    if (changed) {
        surface.MetallicFactor = std::clamp(surface.MetallicFactor, 0.0f, 1.0f);
        surface.RoughnessFactor = std::clamp(surface.RoughnessFactor, 0.0f, 1.0f);
        surface.AlphaCutoff = std::clamp(surface.AlphaCutoff, 0.0f, 1.0f);
        material->setSurface(surface);
    }

    ImGui::Separator();
    if (ImGui::Button("Reset Selected", ImVec2(-1.0f, 0.0f))) {
        material->resetSurface();
    }

    if (ImGui::Button("Reset All Builtin Materials", ImVec2(-1.0f, 0.0f))) {
        for (const auto& descriptor : BuiltinAssets::getBuiltinMaterials()) {
            if (auto builtin_material = AssetManager::get().loadAssetAs<Material>(descriptor.Handle)) {
                builtin_material->resetSurface();
            }
        }
    }

    ImGui::SeparatorText("Project Overrides");
    if (ImGui::Button("Save Overrides", ImVec2(-1.0f, 0.0f))) {
        BuiltinMaterialOverrides::save();
    }

    if (ImGui::Button("Reload Overrides", ImVec2(-1.0f, 0.0f))) {
        for (const auto& descriptor : BuiltinAssets::getBuiltinMaterials()) {
            if (auto builtin_material = AssetManager::get().loadAssetAs<Material>(descriptor.Handle)) {
                builtin_material->resetSurface();
            }
        }
        BuiltinMaterialOverrides::load();
    }

    if (ImGui::Button("Clear Selected Override", ImVec2(-1.0f, 0.0f))) {
        BuiltinMaterialOverrides::clearSelected(m_selected_material);
    }

    if (ImGui::Button("Clear All Overrides", ImVec2(-1.0f, 0.0f))) {
        BuiltinMaterialOverrides::clearAll();
    }

    ImGui::End();
}

} // namespace luna
