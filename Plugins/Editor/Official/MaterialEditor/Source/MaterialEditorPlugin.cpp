#include "Luna/Editor/EditorBuiltinPluginRegistration.h"
#include "MaterialEditorPlugin.h"

#include <cstdint>

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <variant>

namespace {

constexpr const char* kPluginId = "luna.editor.material-editor";
constexpr const char* kDockspaceWindowId = "luna.editor.material-editor";
constexpr const char* kToolsWindowId = "luna.editor.material-editor.tools";
constexpr const char* kPreviewWindowId = "luna.editor.material-editor.preview";
constexpr const char* kNewMaterialPopupId = "MaterialEditorNewMaterialPopup";

constexpr std::array<std::string_view, 3> kMaterialTemplates{
    "Lit",
    "Unlit",
    "Transparent",
};

constexpr std::array<std::string_view, 2> kPreviewBackgroundModes{
    "Procedural Sky",
    "HDR Sky",
};

std::string assetLabel(luna::editor::Host& host, luna::AssetHandle handle, std::string_view empty_label)
{
    if (!handle.isValid()) {
        return std::string(empty_label);
    }

    const luna::editor::AssetInfo info = host.assets().describeAsset(handle);
    if (!info.label.empty()) {
        return info.label;
    }
    if (!info.project_path.empty()) {
        return info.project_path.filename().string();
    }
    return handle.toString();
}

std::string assetDetail(luna::editor::Host& host, luna::AssetHandle handle)
{
    if (!handle.isValid()) {
        return "Drop an asset here";
    }

    const luna::editor::AssetInfo info = host.assets().describeAsset(handle);
    if (!info.project_path.empty()) {
        return info.project_path.generic_string();
    }
    if (!info.detail.empty()) {
        return info.detail;
    }
    return handle.toString();
}

luna::editor::StatusVariant materialStatusVariant(const std::optional<luna::editor::MaterialDocument>& document)
{
    if (!document) {
        return luna::editor::StatusVariant::Neutral;
    }
    if (!document->editable) {
        return luna::editor::StatusVariant::Warning;
    }
    return document->dirty ? luna::editor::StatusVariant::Warning : luna::editor::StatusVariant::Success;
}

std::string materialStatusLabel(const std::optional<luna::editor::MaterialDocument>& document)
{
    if (!document) {
        return "No Material";
    }
    if (!document->editable) {
        return "Read Only";
    }
    return document->dirty ? "Dirty" : "Clean";
}

const char* blendModeLabel(luna::editor::MaterialBlendMode mode)
{
    switch (mode) {
        case luna::editor::MaterialBlendMode::Opaque:
            return "Opaque";
        case luna::editor::MaterialBlendMode::Masked:
            return "Masked";
        case luna::editor::MaterialBlendMode::Transparent:
            return "Transparent";
        case luna::editor::MaterialBlendMode::Additive:
            return "Additive";
    }
    return "Opaque";
}

bool drawBlendModeCombo(luna::editor::Ui& ui, luna::editor::MaterialBlendMode& mode)
{
    if (!ui.beginCombo("Blend Mode", blendModeLabel(mode))) {
        return false;
    }

    bool changed = false;
    constexpr std::array modes{
        luna::editor::MaterialBlendMode::Opaque,
        luna::editor::MaterialBlendMode::Masked,
        luna::editor::MaterialBlendMode::Transparent,
        luna::editor::MaterialBlendMode::Additive,
    };

    for (const luna::editor::MaterialBlendMode candidate : modes) {
        const bool selected = mode == candidate;
        if (ui.selectable(blendModeLabel(candidate), selected)) {
            mode = candidate;
            changed = true;
        }
        if (selected) {
            ui.setItemDefaultFocus();
        }
    }

    ui.endCombo();
    return changed;
}

bool drawTextureSlot(luna::editor::Host& host,
                     luna::editor::Ui& ui,
                     std::string_view id,
                     std::string_view label,
                     luna::AssetHandle& handle)
{
    ui.text(label);

    bool changed = false;
    const bool clicked =
        ui.assetField(id,
                      assetLabel(host, handle, "None"),
                      assetDetail(host, handle),
                      handle.isValid() ? luna::editor::StatusVariant::Info : luna::editor::StatusVariant::Neutral,
                      luna::editor::Vec2{.x = -1.0f, .y = 0.0f});
    (void) clicked;

    const bool hovered = ui.isItemHovered();
    if (ui.beginDragDropTarget()) {
        luna::editor::AssetDropPayload payload{};
        if (ui.acceptAssetDragDropPayload(payload, {luna::AssetType::Texture}) && payload.handle != handle) {
            handle = payload.handle;
            changed = true;
        }
        ui.endDragDropTarget();
    }

    if (ui.beginPopupContextItem(std::string(id) + ".Context")) {
        if (ui.menuItem("Clear", false, handle.isValid())) {
            handle = luna::AssetHandle(0);
            changed = true;
        }
        ui.endPopup();
    }

    if (hovered) {
        ui.setTooltip("Drop a texture asset here. Right-click to clear.");
    }

    return changed;
}

bool drawAssetSlot(luna::editor::Host& host,
                   luna::editor::Ui& ui,
                   std::string_view id,
                   std::string_view label,
                   luna::AssetHandle& handle,
                   std::initializer_list<luna::AssetType> accepted_types,
                   std::string_view tooltip)
{
    ui.text(label);

    bool changed = false;
    const bool clicked =
        ui.assetField(id,
                      assetLabel(host, handle, "None"),
                      assetDetail(host, handle),
                      handle.isValid() ? luna::editor::StatusVariant::Info : luna::editor::StatusVariant::Neutral,
                      luna::editor::Vec2{.x = -1.0f, .y = 0.0f});
    (void) clicked;

    const bool hovered = ui.isItemHovered();
    if (ui.beginDragDropTarget()) {
        luna::editor::AssetDropPayload payload{};
        if (ui.acceptAssetDragDropPayload(payload, accepted_types) && payload.handle != handle) {
            handle = payload.handle;
            changed = true;
        }
        ui.endDragDropTarget();
    }

    if (ui.beginPopupContextItem(std::string(id) + ".Context")) {
        if (ui.menuItem("Clear", false, handle.isValid())) {
            handle = luna::AssetHandle(0);
            changed = true;
        }
        ui.endPopup();
    }

    if (hovered && !tooltip.empty()) {
        ui.setTooltip(tooltip);
    }

    return changed;
}

void clampSurface(luna::editor::MaterialSurfaceProperties& surface)
{
    surface.base_color_factor.x = std::clamp(surface.base_color_factor.x, 0.0f, 1.0f);
    surface.base_color_factor.y = std::clamp(surface.base_color_factor.y, 0.0f, 1.0f);
    surface.base_color_factor.z = std::clamp(surface.base_color_factor.z, 0.0f, 1.0f);
    surface.base_color_factor.w = std::clamp(surface.base_color_factor.w, 0.0f, 1.0f);
    surface.emissive_factor.x = std::max(surface.emissive_factor.x, 0.0f);
    surface.emissive_factor.y = std::max(surface.emissive_factor.y, 0.0f);
    surface.emissive_factor.z = std::max(surface.emissive_factor.z, 0.0f);
    surface.metallic_factor = std::clamp(surface.metallic_factor, 0.0f, 1.0f);
    surface.roughness_factor = std::clamp(surface.roughness_factor, 0.0f, 1.0f);
    surface.normal_scale = std::clamp(surface.normal_scale, 0.0f, 4.0f);
    surface.occlusion_strength = std::clamp(surface.occlusion_strength, 0.0f, 1.0f);
    surface.alpha_cutoff = std::clamp(surface.alpha_cutoff, 0.0f, 1.0f);
}

luna::editor::MaterialSurfaceProperties materialSurfaceForTemplate(int template_index)
{
    luna::editor::MaterialSurfaceProperties surface{};
    switch (template_index) {
        case 1:
            surface.unlit = true;
            surface.roughness_factor = 1.0f;
            break;
        case 2:
            surface.blend_mode = luna::editor::MaterialBlendMode::Transparent;
            surface.base_color_factor.w = 0.6f;
            surface.double_sided = true;
            surface.roughness_factor = 0.35f;
            break;
        default:
            break;
    }
    return surface;
}

std::string materialPathForName(std::string_view name)
{
    std::string safe_name;
    safe_name.reserve(name.size());
    for (char c : name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            safe_name.push_back('_');
        } else {
            safe_name.push_back(c);
        }
    }
    if (safe_name.empty()) {
        safe_name = "New Material";
    }
    return "Assets/Materials/" + safe_name + ".lunamat";
}

} // namespace

namespace luna::editor {

PluginDescriptor MaterialEditorPlugin::descriptor() const
{
    return PluginDescriptor{
        .id = kPluginId,
        .display_name = "Material Editor",
        .version = "0.1.0",
    };
}

bool MaterialEditorPlugin::onLoad(Host& host)
{
    const bool command_registered = host.commands().registerCommand(CommandDescriptor{
        .id = commands::kOpenMaterialEditor,
        .label = "Material Editor",
        .description = "Open the material editor.",
        .execute =
            [this](Host& host) {
                open(host);
            },
    });

    const bool tools_window_registered = host.windows().registerWindow(WindowDescriptor{
        .id = kToolsWindowId,
        .title = "Material Tools",
        .default_open = true,
        .default_size = Vec2{.x = 520.0f, .y = 680.0f},
        .dockspace_id = kDockspaceWindowId,
        .show_in_window_menu = false,
        .draw =
            [this](WindowDrawContext& context) {
                drawToolsWindow(context);
            },
    });

    const bool preview_window_registered = host.windows().registerWindow(WindowDescriptor{
        .id = kPreviewWindowId,
        .title = "Material Preview",
        .default_open = true,
        .default_size = Vec2{.x = 720.0f, .y = 520.0f},
        .flags = static_cast<WindowFlags>(WindowFlag::NoPadding),
        .dockspace_id = kDockspaceWindowId,
        .show_in_window_menu = false,
        .draw =
            [this](WindowDrawContext& context) {
                drawPreviewWindow(context);
            },
    });

    const bool dockspace_window_registered = host.windows().registerDockspaceWindow(DockspaceWindowDescriptor{
        .id = kDockspaceWindowId,
        .title = "Material Editor",
        .default_open = false,
        .default_size = Vec2{.x = 1120.0f, .y = 720.0f},
        .docked_windows =
            {
                DockedWindowDescriptor{
                    .window_id = kToolsWindowId,
                    .direction = DockSplitDirection::Left,
                    .ratio = 0.34f,
                },
                DockedWindowDescriptor{
                    .window_id = kPreviewWindowId,
                    .direction = DockSplitDirection::Right,
                    .ratio = 0.66f,
                },
            },
    });

    if (!command_registered || !tools_window_registered || !preview_window_registered || !dockspace_window_registered) {
        onUnload(host);
        return false;
    }

    return true;
}

void MaterialEditorPlugin::onUnload(Host& host)
{
    if (m_preview_viewport != kInvalidViewportId) {
        host.viewport().destroySceneViewport(m_preview_viewport);
        m_preview_viewport = kInvalidViewportId;
    }
    host.windows().unregisterWindow(kDockspaceWindowId);
    host.windows().unregisterWindow(kPreviewWindowId);
    host.windows().unregisterWindow(kToolsWindowId);
    host.commands().unregisterCommand(commands::kOpenMaterialEditor);
}

void MaterialEditorPlugin::open(Host& host)
{
    const CommandSubject subject = host.commands().takeSubject(commands::kOpenMaterialEditor);
    if (subject) {
        if (const auto* material_handle = std::get_if<uint64_t>(&*subject)) {
            selectMaterial(host, AssetHandle{*material_handle});
        }
    }

    host.windows().setWindowOpen(kDockspaceWindowId, true);
}

void MaterialEditorPlugin::selectMaterial(Host& host, AssetHandle handle)
{
    if (!handle.isValid()) {
        m_selected_material = AssetHandle{0};
        return;
    }

    const AssetInfo info = host.assets().describeAsset(handle);
    if (info.type != AssetType::Material) {
        m_status = "Selected asset is not a material.";
        return;
    }

    m_selected_material = handle;
    m_status.clear();
}

std::optional<MaterialDocument> MaterialEditorPlugin::readSelectedMaterial(Host& host)
{
    if (!m_selected_material.isValid()) {
        return std::nullopt;
    }

    std::optional<MaterialDocument> document = host.materials().readMaterial(m_selected_material);
    if (!document) {
        m_status = "Failed to read selected material.";
    }
    return document;
}

void MaterialEditorPlugin::drawToolsWindow(WindowDrawContext& context)
{
    Host& host = context.host();
    Ui& ui = context.ui();

    std::optional<MaterialDocument> document = readSelectedMaterial(host);

    drawToolbar(host, ui, document);

    ui.spacing();
    if (!m_status.empty()) {
        ui.textWrapped(m_status);
        ui.spacing();
    }

    if (!document) {
        ui.emptyState("No material selected", "Drop a material asset or create a new material.");
        return;
    }

    drawPreviewControls(host, ui);
    ui.spacing();
    drawMaterialBody(host, ui, *document);
}

void MaterialEditorPlugin::drawPreviewWindow(WindowDrawContext& context)
{
    Host& host = context.host();
    Ui& ui = context.ui();

    const std::optional<MaterialDocument> document = readSelectedMaterial(host);
    if (!document) {
        ui.emptyState("No material selected", "Open Material Tools and select a material.");
        return;
    }

    drawPreviewViewport(host, ui, *document);
}

void MaterialEditorPlugin::drawToolbar(Host& host, Ui& ui, const std::optional<MaterialDocument>& document)
{
    ui.beginPanel("##MaterialEditorToolbar");
    ui.heading("Material Editor");

    ui.badge(materialStatusLabel(document), materialStatusVariant(document));
    ui.sameLine();

    if (ui.button("New", ButtonVariant::Subtle)) {
        if (!m_new_material_name.empty()) {
            m_new_material_path = materialPathForName(m_new_material_name);
        }
        ui.openPopup(kNewMaterialPopupId);
    }

    ui.sameLine();
    const bool can_save = document && document->editable && document->dirty;
    if (!can_save) {
        ui.beginDisabled();
    }
    if (ui.button("Save", ButtonVariant::Primary)) {
        const MaterialEditResult result = host.materials().saveMaterial(document->handle);
        m_status = result.message;
    }
    if (!can_save) {
        ui.endDisabled();
    }

    ui.sameLine();
    const bool can_revert = document && document->editable && document->dirty;
    if (!can_revert) {
        ui.beginDisabled();
    }
    if (ui.button("Revert", ButtonVariant::Subtle)) {
        const MaterialEditResult result = host.materials().revertMaterial(document->handle);
        m_status = result.message;
    }
    if (!can_revert) {
        ui.endDisabled();
    }

    const std::string selected_label = document ? assetLabel(host, document->handle, "No Material") : "No Material";
    const std::string selected_detail =
        document ? document->project_path.generic_string() : "Drop a material asset here";
    const bool clicked = ui.assetField("##MaterialEditorSelectedMaterial",
                                       selected_label,
                                       selected_detail,
                                       document ? StatusVariant::Info : StatusVariant::Neutral,
                                       Vec2{.x = -1.0f, .y = 0.0f});
    (void) clicked;

    if (ui.beginDragDropTarget()) {
        AssetDropPayload payload{};
        if (ui.acceptAssetDragDropPayload(payload, {AssetType::Material})) {
            selectMaterial(host, payload.handle);
        }
        ui.endDragDropTarget();
    }

    if (ui.isItemHovered()) {
        ui.setTooltip("Drop a material asset here.");
    }

    if (document) {
        ui.keyValue("Path", document->project_path.generic_string());
        ui.keyValue("Handle", document->handle.toString());
    }

    drawNewMaterialPopup(host, ui);
    ui.endPanel();
}

void MaterialEditorPlugin::drawNewMaterialPopup(Host& host, Ui& ui)
{
    if (!ui.beginPopup(kNewMaterialPopupId)) {
        return;
    }

    ui.heading("New Material");
    const bool name_changed = ui.inputText("Name", m_new_material_name, 128);
    if (name_changed) {
        m_new_material_path = materialPathForName(m_new_material_name);
    }
    ui.inputText("Path", m_new_material_path, 256);

    const std::string_view selected_template = kMaterialTemplates[static_cast<std::size_t>(
        std::clamp(m_new_material_template, 0, static_cast<int>(kMaterialTemplates.size()) - 1))];
    if (ui.beginCombo("Template", selected_template)) {
        for (std::size_t index = 0; index < kMaterialTemplates.size(); ++index) {
            const bool selected = m_new_material_template == static_cast<int>(index);
            if (ui.selectable(kMaterialTemplates[index], selected)) {
                m_new_material_template = static_cast<int>(index);
            }
            if (selected) {
                ui.setItemDefaultFocus();
            }
        }
        ui.endCombo();
    }

    ui.separator();
    if (ui.button("Create", Vec2{.x = 120.0f, .y = 0.0f}, ButtonVariant::Primary)) {
        MaterialCreateRequest request{};
        request.name = m_new_material_name.empty() ? "New Material" : m_new_material_name;
        request.project_path = std::filesystem::path(m_new_material_path);
        request.surface = materialSurfaceForTemplate(m_new_material_template);

        const MaterialCreateResult result = host.materials().createMaterial(request);
        m_status = result.message;
        if (result.success && result.handle.isValid()) {
            m_selected_material = result.handle;
            ui.closeCurrentPopup();
        }
    }
    ui.sameLine();
    if (ui.button("Cancel", Vec2{.x = 120.0f, .y = 0.0f}, ButtonVariant::Subtle)) {
        ui.closeCurrentPopup();
    }

    ui.endPopup();
}

void MaterialEditorPlugin::drawPreviewControls(Host& host, Ui& ui)
{
    if (!ui.beginSection("##MaterialEditorPreviewControls", "Preview")) {
        return;
    }

    ui.keyValue("Viewport", host.windows().isWindowOpen(kPreviewWindowId) ? "Open" : "Closed");
    if (ui.button("Open Preview", Vec2{.x = -1.0f, .y = 0.0f}, ButtonVariant::Subtle)) {
        host.windows().setWindowOpen(kPreviewWindowId, true);
    }

    const std::string_view selected_background = kPreviewBackgroundModes[static_cast<std::size_t>(
        std::clamp(m_preview_background, 0, static_cast<int>(kPreviewBackgroundModes.size()) - 1))];
    if (ui.beginCombo("Sky", selected_background)) {
        for (std::size_t index = 0; index < kPreviewBackgroundModes.size(); ++index) {
            const bool selected = m_preview_background == static_cast<int>(index);
            if (ui.selectable(kPreviewBackgroundModes[index], selected)) {
                m_preview_background = static_cast<int>(index);
            }
            if (selected) {
                ui.setItemDefaultFocus();
            }
        }
        ui.endCombo();
    }

    if (m_preview_background == 1) {
        (void) drawAssetSlot(host,
                             ui,
                             "##MaterialEditorPreviewHdrSky",
                             "HDR",
                             m_preview_environment_map,
                             {AssetType::Texture},
                             "Drop an HDR or environment texture here. Right-click to clear.");
    }

    (void) ui.sliderFloat("Sky Intensity", m_preview_sky_intensity, 0.0f, 8.0f, "%.2f");
    (void) ui.sliderFloat("Light Intensity", m_preview_light_intensity, 0.0f, 8.0f, "%.2f");
    if (ui.button("Reset Camera", Vec2{.x = -1.0f, .y = 0.0f}, ButtonVariant::Subtle)) {
        resetPreviewCamera();
    }
    ui.endSection();
}

void MaterialEditorPlugin::drawPreviewViewport(Host& host, Ui& ui, const MaterialDocument& document)
{
    if (m_preview_viewport == kInvalidViewportId || !host.viewport().isSceneViewportValid(m_preview_viewport)) {
        m_preview_viewport = host.viewport().createSceneViewport("MaterialEditorPreview");
    }

    SceneViewportPreviewState preview_state{};
    preview_state.material = document.handle;
    preview_state.mesh_kind = SceneViewportPreviewMesh::Sphere;
    preview_state.environment.background = m_preview_background == 1 && m_preview_environment_map.isValid()
                                               ? SceneViewportPreviewBackground::EnvironmentMap
                                               : SceneViewportPreviewBackground::ProceduralSky;
    preview_state.environment.environment_map = m_preview_environment_map;
    preview_state.environment.ibl_enabled = true;
    preview_state.environment.intensity = std::max(m_preview_sky_intensity, 0.0f);
    preview_state.environment.sky_intensity = std::max(m_preview_sky_intensity, 0.0f);
    preview_state.environment.diffuse_intensity = std::max(m_preview_light_intensity, 0.0f);
    preview_state.environment.specular_intensity = std::max(m_preview_light_intensity, 0.0f);
    preview_state.environment.procedural_sun_intensity = 18.0f * std::max(m_preview_light_intensity, 0.0f);
    preview_state = m_preview_camera.applyTo(preview_state);

    const bool preview_ready = m_preview_viewport != kInvalidViewportId &&
                               host.viewport().setSceneViewportPreview(m_preview_viewport, preview_state);
    if (preview_ready) {
        const SceneViewportDrawResult draw_result =
            host.viewport().drawSceneViewport(ui,
                                              m_preview_viewport,
                                              SceneViewportDrawOptions{
                                                  .preserve_aspect = true,
                                                  .fill_available = true,
                                                  .requested_size = Vec2{.x = 0.0f, .y = 0.0f},
                                              });
        (void) m_preview_camera.updateFromViewport(draw_result);
        if (!draw_result.drawn) {
            ui.emptyState("Preview warming up", "The preview render target is not ready yet.");
        }
    } else {
        ui.emptyState("Preview unavailable", "The preview viewport could not be created.");
    }
}

void MaterialEditorPlugin::resetPreviewCamera()
{
    m_preview_camera.reset();
}

void MaterialEditorPlugin::drawMaterialBody(Host& host, Ui& ui, MaterialDocument document)
{
    if (!document.editable) {
        ui.textDisabled("This material is read only.");
    }

    if (!document.editable) {
        ui.beginDisabled();
    }

    if (ui.beginSection("##MaterialEditorTextures", "Textures")) {
        MaterialTextureSet textures = document.textures;
        bool textures_changed = false;
        textures_changed |= drawTextureSlot(host, ui, "##MaterialEditorBaseColor", "Base Color", textures.base_color);
        textures_changed |= drawTextureSlot(host, ui, "##MaterialEditorNormal", "Normal", textures.normal);
        textures_changed |= drawTextureSlot(
            host, ui, "##MaterialEditorMetallicRoughness", "Packed Metallic Roughness", textures.metallic_roughness);
        textures_changed |= drawTextureSlot(host, ui, "##MaterialEditorMetallic", "Metallic", textures.metallic);
        textures_changed |= drawTextureSlot(host, ui, "##MaterialEditorRoughness", "Roughness", textures.roughness);
        textures_changed |= drawTextureSlot(host, ui, "##MaterialEditorEmissive", "Emissive", textures.emissive);
        textures_changed |= drawTextureSlot(host, ui, "##MaterialEditorOcclusion", "Occlusion", textures.occlusion);
        if (textures_changed) {
            applyTextureChanges(host, textures);
        }
        ui.endSection();
    }

    if (ui.beginSection("##MaterialEditorSurface", "Surface")) {
        MaterialSurfaceProperties surface = document.surface;
        bool surface_changed = false;

        surface_changed |= ui.colorEdit4("Base Color Factor", surface.base_color_factor);
        surface_changed |= ui.colorEdit3("Emissive Factor", surface.emissive_factor);

        const char* metallic_label =
            (document.textures.metallic.isValid() || document.textures.metallic_roughness.isValid())
                ? "Metallic Multiplier"
                : "Metallic";
        const char* roughness_label =
            (document.textures.roughness.isValid() || document.textures.metallic_roughness.isValid())
                ? "Roughness Multiplier"
                : "Roughness";
        surface_changed |= ui.sliderFloat(metallic_label, surface.metallic_factor, 0.0f, 1.0f, "%.3f");
        surface_changed |= ui.sliderFloat(roughness_label, surface.roughness_factor, 0.0f, 1.0f, "%.3f");
        surface_changed |= ui.sliderFloat("Normal Scale", surface.normal_scale, 0.0f, 4.0f, "%.3f");
        surface_changed |= ui.sliderFloat("Occlusion Strength", surface.occlusion_strength, 0.0f, 1.0f, "%.3f");
        surface_changed |= ui.sliderFloat("Alpha Cutoff", surface.alpha_cutoff, 0.0f, 1.0f, "%.3f");
        surface_changed |= drawBlendModeCombo(ui, surface.blend_mode);
        surface_changed |= ui.checkbox("Double Sided", surface.double_sided);
        surface_changed |= ui.checkbox("Unlit", surface.unlit);

        if (surface_changed) {
            clampSurface(surface);
            applySurfaceChanges(host, surface);
        }

        ui.endSection();
    }

    if (!document.editable) {
        ui.endDisabled();
    }
}

void MaterialEditorPlugin::applyTextureChanges(Host& host, const MaterialTextureSet& textures)
{
    if (!m_selected_material.isValid()) {
        return;
    }

    if (!host.materials().setMaterialTextures(m_selected_material, textures)) {
        m_status = "Failed to update material textures.";
        return;
    }

    m_status.clear();
}

void MaterialEditorPlugin::applySurfaceChanges(Host& host, const MaterialSurfaceProperties& surface)
{
    if (!m_selected_material.isValid()) {
        return;
    }

    if (!host.materials().setMaterialSurface(m_selected_material, surface)) {
        m_status = "Failed to update material surface.";
        return;
    }

    m_status.clear();
}

std::unique_ptr<Plugin> createMaterialEditorPlugin()
{
    return std::make_unique<MaterialEditorPlugin>();
}

namespace {

const EditorBuiltinPluginFactoryRegistration kMaterialEditorPluginRegistration{
    kPluginId,
    createMaterialEditorPlugin,
};

} // namespace

} // namespace luna::editor
