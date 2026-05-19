#include "Luna/Editor/Native/NativePlugin.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace native = luna::editor::native;

constexpr const char* kPluginId = "luna.source.native-sample";
constexpr const char* kDisplayName = "Native Sample Tool";
constexpr const char* kVersion = "0.1.0";
constexpr const char* kWindowId = "luna.source.native-sample.window";
constexpr const char* kOpenCommandId = "luna.source.native-sample.open";
constexpr const char* kCreateEntityCommandId = "luna.source.native-sample.create-entity";

std::string assetTypeLabel(LunaEditorAssetType type)
{
    switch (type) {
        case LunaEditorAssetType_Texture:
            return "Texture";
        case LunaEditorAssetType_Mesh:
            return "Mesh";
        case LunaEditorAssetType_Material:
            return "Material";
        case LunaEditorAssetType_Model:
            return "Model";
        case LunaEditorAssetType_Scene:
            return "Scene";
        case LunaEditorAssetType_Script:
            return "Script";
        case LunaEditorAssetType_None:
        default:
            return "None";
    }
}

struct NativeSampleState {
    int enabled{1};
    int open_count{0};
    int create_entity_count{0};
    float intensity{0.5f};
    native::Vec3 accent{.x = 0.2f, .y = 0.7f, .z = 0.9f};
    char label[96]{"Native sample state"};
    char create_entity_name[96]{"Native Sample Entity"};
    char selected_entity_name[96]{};
    std::string asset_note;
    std::string project_note;
    std::string refresh_note;
    native::EntityId selected_entity_id{0u};
    native::RegisteredCommand open_command;
    native::RegisteredCommand create_entity_command;
    native::RegisteredWindow window;
    native::RegisteredMenuItemsForCommand open_menu_items;
    native::RegisteredMenuItemsForCommand create_menu_items;
    native::SceneViewportHandle preview_viewport;
    int show_preview_viewport{1};

    void cleanup() noexcept
    {
        preview_viewport.reset();
        create_menu_items.reset();
        open_menu_items.reset();
        window.reset();
        create_entity_command.reset();
        open_command.reset();
    }
};

NativeSampleState g_state{};

int canOpenSampleWindow(void*, const LunaEditorHostApi* host_api)
{
    const native::Host host(host_api);
    return host.windows().canSetOpen() ? 1 : 0;
}

int isSampleWindowOpen(void*, const LunaEditorHostApi* host_api)
{
    const native::Host host(host_api);
    return host.windows().isOpen(kWindowId) ? 1 : 0;
}

void executeOpenSampleWindow(void* command_user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<NativeSampleState*>(command_user_data);
    if (state != nullptr) {
        ++state->open_count;
    }

    const native::Host host(host_api);
    host.windows().setOpen(kWindowId, true);
}

int canCreateSampleEntity(void*, const LunaEditorHostApi* host_api)
{
    const native::Host host(host_api);
    return host.scene().canEdit() ? 1 : 0;
}

void rememberCreatedEntity(NativeSampleState* state, native::EntityId entity_id, const std::string& entity_name)
{
    if (state == nullptr) {
        return;
    }

    ++state->create_entity_count;
    state->selected_entity_id = entity_id;
    std::snprintf(state->selected_entity_name, sizeof(state->selected_entity_name), "%s", entity_name.c_str());
}

void executeCreateSampleEntity(void* command_user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<NativeSampleState*>(command_user_data);
    const native::Host host(host_api);
    if (!host.valid() || !host.scene().canEdit()) {
        return;
    }

    const std::string entity_name = (state != nullptr && state->create_entity_name[0] != '\0')
                                        ? state->create_entity_name
                                        : std::string("Native Sample Entity");
    const native::EntityId entity_id = host.scene().createEntity(entity_name.c_str());
    if (entity_id == 0u) {
        host.log().warn("Native sample could not create an entity.");
        return;
    }

    host.selection().selectEntity(entity_id);
    rememberCreatedEntity(state, entity_id, entity_name);
    host.log().info("Native sample created and selected an entity.");
}

void createCameraEntity(NativeSampleState& state, const native::Host& host)
{
    if (!host.scene().canEdit()) {
        return;
    }

    const std::string entity_name =
        state.create_entity_name[0] != '\0' ? state.create_entity_name : std::string("Native Sample Camera");
    const native::SceneEntityCreateRequest request{
        .kind = LunaEditorSceneEntityCreateKind_Camera,
        .name = entity_name.c_str(),
        .parent_id = 0u,
        .asset_handle = 0u,
    };

    const native::EntityId entity_id = host.scene().createEntity(request);
    if (entity_id == 0u) {
        host.log().warn("Native sample could not create a camera entity.");
        return;
    }

    host.selection().selectEntity(entity_id);
    rememberCreatedEntity(&state, entity_id, entity_name);
    host.log().info("Native sample created a camera entity.");
}

void drawPluginAssetSection(const native::Host& host, const native::Ui& ui, NativeSampleState& state)
{
    ui.separatorText("Plugin Asset");

    if (state.asset_note.empty()) {
        state.asset_note = host.pluginAssets().readText("welcome.txt");
    }

    if (!state.asset_note.empty()) {
        ui.textWrapped(state.asset_note.c_str());
    } else {
        ui.textDisabled("Plugin asset assets/welcome.txt was not found.");
    }

    const std::string plugin_root = host.pluginAssets().rootPath();
    const std::string asset_root = host.pluginAssets().assetRootPath();
    ui.textWrapped(("Plugin Root: " + (plugin_root.empty() ? std::string("-") : plugin_root)).c_str());
    ui.textWrapped(("Asset Root: " + (asset_root.empty() ? std::string("-") : asset_root)).c_str());
}

void drawProjectSection(const native::Host& host, const native::Ui& ui, NativeSampleState& state)
{
    ui.separatorText("Project");

    if (!host.project().hasProjectLoaded()) {
        ui.textDisabled("No project is loaded.");
        return;
    }

    const native::ProjectInfo project = host.project().info();
    ui.textWrapped(("Project: " + (project.name.empty() ? std::string("Untitled") : project.name)).c_str());
    ui.textWrapped(("Version: " + (project.version.empty() ? std::string("-") : project.version)).c_str());
    ui.textWrapped(("Author: " + (project.author.empty() ? std::string("-") : project.author)).c_str());
    ui.textWrapped(("Description: " + (project.description.empty() ? std::string("-") : project.description)).c_str());
    ui.textWrapped(("Start Scene: " + (project.start_scene.empty() ? std::string("-") : project.start_scene)).c_str());
    ui.textWrapped(("Root: " + host.project().rootPath()).c_str());
    ui.textWrapped(("Assets Path: " + (project.assets_path.empty() ? std::string("-") : project.assets_path)).c_str());
    ui.textWrapped(("Selected Script Plugin: " +
                    (project.selected_script_plugin_id.empty() ? std::string("-") : project.selected_script_plugin_id))
                       .c_str());
    ui.textWrapped(("Selected Script Backend: " +
                    (project.selected_script_backend_name.empty() ? std::string("-")
                                                                  : project.selected_script_backend_name))
                       .c_str());

    if (ui.button("Save Project", native::fillWidth(), LunaEditorButtonVariant_Subtle)) {
        state.project_note = host.project().save() ? "Project save requested." : "Project save failed.";
    }
    if (!state.project_note.empty()) {
        ui.textDisabled(state.project_note.c_str());
    }
}

void drawAssetSection(const native::Host& host, const native::Ui& ui, NativeSampleState& state)
{
    ui.separatorText("Assets");

    ui.text(("Revision: " + std::to_string(host.assets().revision())).c_str());
    if (ui.button("Refresh Assets", native::fillWidth(), LunaEditorButtonVariant_Subtle)) {
        const native::AssetRefreshResult result = host.assets().refreshDetailed();
        state.refresh_note = result.message;
        if (state.refresh_note.empty()) {
            state.refresh_note = result.success ? "Asset refresh finished." : "Asset refresh failed.";
        }
    }
    if (!state.refresh_note.empty()) {
        ui.textWrapped(state.refresh_note.c_str());
    }

    const std::vector<native::AssetInfo> assets = host.assets().list(LunaEditorAssetType_None, false);
    ui.metric("Project Assets",
              std::to_string(assets.size()).c_str(),
              "Enumerated through host.assets()",
              assets.empty() ? LunaEditorStatusVariant_Warning : LunaEditorStatusVariant_Info,
              native::vec2(-1.0f, 0.0f));
    if (assets.empty()) {
        ui.emptyState("No project assets", "No project assets are visible to the native plugin API.");
        return;
    }

    const native::AssetInfo& first = assets.front();
    const std::string first_asset_name =
        first.label.empty() ? (first.project_path.empty() ? std::string("-") : first.project_path) : first.label;
    const std::string first_asset_detail =
        first.detail.empty() ? assetTypeLabel(first.type) : first.detail + " / " + assetTypeLabel(first.type);
    (void) ui.assetField("NativeSampleFirstAsset",
                         first_asset_name.c_str(),
                         first_asset_detail.c_str(),
                         first.loading ? LunaEditorStatusVariant_Warning : LunaEditorStatusVariant_Info,
                         native::vec2(-1.0f, 0.0f));
    if (ui.isItemHovered()) {
        ui.setTooltip("Styled asset field from the native UI ABI.");
    }
    if (ui.beginDragDropTarget()) {
        native::AssetDropPayload dropped_asset{};
        if (ui.acceptAssetDragDropPayload(&dropped_asset)) {
            state.refresh_note = "Dropped asset handle: " + std::to_string(dropped_asset.handle);
        }
        ui.endDragDropTarget();
    }
    ui.keyValue("Project Path", first.project_path.empty() ? "-" : first.project_path.c_str());
    ui.keyValue("Absolute Path", first.absolute_path.empty() ? "-" : first.absolute_path.c_str());
    if (!first.project_path.empty()) {
        const std::string handle_text = std::to_string(host.assets().findHandleByPath(first.project_path.c_str()));
        ui.keyValue("Handle by Path", handle_text.c_str());
        const std::string resolved_path = host.assets().resolveProjectPath(first.project_path.c_str());
        if (!resolved_path.empty()) {
            ui.keyValue("Resolved Path", resolved_path.c_str());
        }
    }
    if (!first.absolute_path.empty()) {
        const std::string relative_path = host.assets().makeProjectRelativePath(first.absolute_path.c_str());
        if (!relative_path.empty()) {
            ui.keyValue("Relative From Absolute", relative_path.c_str());
        }
    }

    const uint32_t table_flags = LunaEditorTableFlag_RowBg | LunaEditorTableFlag_BordersInnerH |
                                 LunaEditorTableFlag_SizingStretchProp;
    if (!ui.beginTable("NativeSampleAssets", 4, table_flags)) {
        return;
    }

    ui.tableSetupColumn("Name", LunaEditorTableColumnFlag_WidthStretch, 1.5f);
    ui.tableSetupColumn("Type", LunaEditorTableColumnFlag_WidthFixed, 92.0f);
    ui.tableSetupColumn("State", LunaEditorTableColumnFlag_WidthFixed, 72.0f);
    ui.tableSetupColumn("Path", LunaEditorTableColumnFlag_WidthStretch, 2.0f);
    ui.tableHeadersRow();

    const size_t row_count = (std::min)(assets.size(), static_cast<size_t>(6u));
    for (size_t index = 0u; index < row_count; ++index) {
        const native::AssetInfo& asset = assets[index];
        const std::string name =
            asset.label.empty() ? (asset.project_path.empty() ? std::string("-") : asset.project_path) : asset.label;
        const std::string path = asset.project_path.empty() ? std::string("-") : asset.project_path;

        ui.tableNextRow();
        ui.tableNextColumn();
        ui.text(name.c_str());
        ui.tableNextColumn();
        ui.text(assetTypeLabel(asset.type).c_str());
        ui.tableNextColumn();
        ui.text(asset.loading ? "Loading" : "Ready");
        ui.tableNextColumn();
        ui.textWrapped(path.c_str());
    }

    ui.endTable();
}

void drawSelectedEntityComponents(const native::Host& host,
                                  const native::Ui& ui,
                                  NativeSampleState& state,
                                  native::EntityId selected_entity)
{
    ui.separatorText("Components");

    LunaEditorSceneCameraComponent camera{};
    camera.struct_size = sizeof(LunaEditorSceneCameraComponent);
    camera.api_version = LUNA_EDITOR_SCENE_CAMERA_COMPONENT_API_VERSION;
    const bool has_camera = host.scene().getCameraComponent(selected_entity, &camera);
    ui.text(has_camera ? "Camera component: present" : "Camera component: missing");
    if (!has_camera && host.scene().canEdit() &&
        ui.button("Add Camera Component", native::fillWidth(), LunaEditorButtonVariant_Subtle)) {
        camera = native::makeDefaultPerspectiveCamera();
        (void) host.scene().addComponent(selected_entity, LunaEditorSceneComponentKind_Camera);
        (void) host.scene().setCameraComponent(selected_entity, camera);
    }

    LunaEditorSceneLightComponent light{};
    light.struct_size = sizeof(LunaEditorSceneLightComponent);
    light.api_version = LUNA_EDITOR_SCENE_LIGHT_COMPONENT_API_VERSION;
    const bool has_light = host.scene().getLightComponent(selected_entity, &light);
    ui.text(has_light ? "Light component: present" : "Light component: missing");
    if (!has_light && host.scene().canEdit() &&
        ui.button("Add Directional Light", native::fillWidth(), LunaEditorButtonVariant_Subtle)) {
        light = native::makeDefaultDirectionalLight();
        (void) host.scene().addComponent(selected_entity, LunaEditorSceneComponentKind_Light);
        (void) host.scene().setLightComponent(selected_entity, light);
    }

    uint64_t material_handles[8]{};
    LunaEditorSceneMeshComponent mesh{};
    mesh.struct_size = sizeof(LunaEditorSceneMeshComponent);
    mesh.api_version = LUNA_EDITOR_SCENE_MESH_COMPONENT_API_VERSION;
    mesh.submesh_material_handles = material_handles;
    mesh.submesh_material_capacity = 8u;
    const bool has_mesh = host.scene().getMeshComponent(selected_entity, &mesh);
    ui.text(has_mesh ? "Mesh component: present" : "Mesh component: missing");
    if (has_mesh) {
        ui.text(("Mesh Handle: " + std::to_string(mesh.mesh_handle)).c_str());
        ui.text(("Submeshes: " + std::to_string(mesh.submesh_count)).c_str());
        ui.text(("Materials: " + std::to_string(mesh.submesh_material_count)).c_str());
    }

    if (host.scene().canEdit() &&
        ui.button("Delete Selected Entity", native::fillWidth(), LunaEditorButtonVariant_Danger)) {
        if (host.scene().destroyEntity(selected_entity)) {
            host.selection().clear();
            state.selected_entity_id = 0u;
            state.selected_entity_name[0] = '\0';
        }
    }
}

void drawSceneSection(const native::Host& host, const native::Ui& ui, NativeSampleState& state)
{
    ui.separatorText("Scene");

    const std::string scene_label = host.scene().label();
    const bool can_edit_scene = host.scene().canEdit();
    ui.textWrapped(("Scene: " + (scene_label.empty() ? std::string("Untitled") : scene_label)).c_str());
    ui.text(("Entities: " + std::to_string(host.scene().entityCount())).c_str());

    if (!can_edit_scene) {
        ui.textDisabled("Scene editing is disabled while runtime owns the scene.");
    }

    if (!can_edit_scene) {
        ui.beginDisabled();
    }
    ui.inputTextWithHint(
        "Create Entity Name", "Native Sample Entity", state.create_entity_name, sizeof(state.create_entity_name));

    if (ui.button("Create Empty Entity", native::fillWidth(), LunaEditorButtonVariant_Primary)) {
        (void) host.commands().execute(kCreateEntityCommandId);
    }
    if (ui.button("Create Camera Entity", native::fillWidth(), LunaEditorButtonVariant_Subtle)) {
        createCameraEntity(state, host);
    }
    if (!can_edit_scene) {
        ui.endDisabled();
    }

    const native::EntityId selected_entity = host.selection().selectedEntityId();
    ui.text(("Selected Entity: " + std::to_string(selected_entity)).c_str());
    if (selected_entity == 0u || !host.scene().entityExists(selected_entity)) {
        state.selected_entity_id = 0u;
        state.selected_entity_name[0] = '\0';
        ui.textDisabled("No entity selected.");
        return;
    }

    const native::SceneEntityInfo entity = host.scene().entityInfo(selected_entity);
    if (state.selected_entity_id != selected_entity) {
        state.selected_entity_id = selected_entity;
        std::snprintf(state.selected_entity_name, sizeof(state.selected_entity_name), "%s", entity.name.c_str());
    }

    ui.separatorText("Selected Entity");
    ui.text(("ID: " + std::to_string(selected_entity)).c_str());
    ui.textWrapped(("Parent: " + (entity.parent_name.empty() ? std::string("-") : entity.parent_name)).c_str());
    ui.text(("Children: " + std::to_string(entity.child_count)).c_str());

    if (!can_edit_scene) {
        ui.beginDisabled();
    }
    const bool entity_name_changed =
        ui.inputText("Entity Name", state.selected_entity_name, sizeof(state.selected_entity_name));
    if (can_edit_scene && entity_name_changed && ui.isItemDeactivatedAfterEdit()) {
        (void) host.scene().setEntityName(selected_entity, state.selected_entity_name);
    }

    LunaEditorSceneTransform transform = host.scene().transform(selected_entity);
    bool transform_changed = false;
    transform_changed |= ui.dragFloat3("Translation", &transform.translation, 0.05f, -1000.0f, 1000.0f);
    transform_changed |= ui.dragFloat3("Rotation", &transform.rotation_degrees, 0.25f, -360.0f, 360.0f);
    transform_changed |= ui.dragFloat3("Scale", &transform.scale, 0.01f, 0.01f, 1000.0f);
    if (!can_edit_scene) {
        ui.endDisabled();
    }
    if (can_edit_scene && transform_changed) {
        (void) host.scene().setTransform(selected_entity, transform);
    }

    drawSelectedEntityComponents(host, ui, state, selected_entity);
}

void drawViewportSection(const native::Host& host, const native::Ui& ui, NativeSampleState& state)
{
    ui.separatorText("Viewport");

    const native::Vec3 camera_position = host.viewport().editorCameraPosition();
    char camera_text[128]{};
    std::snprintf(camera_text,
                  sizeof(camera_text),
                  "Editor Camera: %.2f, %.2f, %.2f",
                  camera_position.x,
                  camera_position.y,
                  camera_position.z);
    ui.text(camera_text);

    const std::string gizmo_operation = host.viewport().gizmoOperationName();
    const std::string gizmo_mode = host.viewport().gizmoModeName();
    ui.textWrapped(("Gizmo: " + (gizmo_operation.empty() ? std::string("Unknown") : gizmo_operation) + " / " +
                    (gizmo_mode.empty() ? std::string("Unknown") : gizmo_mode))
                       .c_str());
    ui.text(("Default Scene Viewport: " + std::to_string(host.viewport().defaultSceneViewport())).c_str());

    int pick_debug = host.viewport().pickDebugVisualizationEnabled() ? 1 : 0;
    if (ui.checkbox("Pick Debug Visualization", &pick_debug)) {
        host.viewport().setPickDebugVisualizationEnabled(pick_debug != 0);
    }

    int editor_grid = host.viewport().editorGridEnabled() ? 1 : 0;
    if (ui.checkbox("Editor Grid", &editor_grid)) {
        host.viewport().setEditorGridEnabled(editor_grid != 0);
    }

    int runtime_requested = host.runtimeViewport().requested() ? 1 : 0;
    if (ui.checkbox("Request Runtime Viewport", &runtime_requested)) {
        host.runtimeViewport().setRequested(runtime_requested != 0);
    }
    ui.text(("Runtime Entities: " + std::to_string(host.runtimeViewport().entityCount())).c_str());

    if (ui.checkbox("Show Independent Preview", &state.show_preview_viewport) && state.show_preview_viewport == 0) {
        state.preview_viewport.reset();
    }
    if (state.show_preview_viewport == 0) {
        ui.textDisabled("Independent preview viewport is disabled.");
        return;
    }

    if (!state.preview_viewport || !host.viewport().isSceneViewportValid(state.preview_viewport.id())) {
        state.preview_viewport = host.viewport().createScopedSceneViewport("NativeSampleViewport");
    }
    if (!state.preview_viewport) {
        ui.textDisabled("Independent preview viewport could not be created.");
        return;
    }

    const native::Vec2 available = ui.contentRegionAvail();
    const float width = (std::max)(available.x, 320.0f);
    const float height = (std::max)(width * 0.5625f, 180.0f);

    LunaEditorViewportPresentation presentation = native::makeViewportPresentation();
    if (host.viewport().syncSceneViewport(state.preview_viewport.id(),
                                          static_cast<uint32_t>(width),
                                          static_cast<uint32_t>(height),
                                          &presentation) &&
        presentation.presentable != 0) {
        (void) ui.image(presentation.scene_texture, native::vec2(width, height));
        if (ui.isItemHovered()) {
            ui.setTooltip("This image is drawn from a plugin-owned scene viewport instance.");
        }
    } else {
        ui.textDisabled("Independent viewport texture is not available.");
    }
}

void drawNativeSampleWindow(void* window_user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<NativeSampleState*>(window_user_data);
    const native::Host host(host_api);
    if (state == nullptr || !host.valid()) {
        return;
    }

    const native::Ui ui = host.ui();
    if (!ui.canDrawText()) {
        return;
    }

    ui.heading("Native Sample", "Native editor plugin mainline sample.");
    ui.beginPanel("NativeSampleSummary");
    ui.keyValue("Boundary", "Luna native editor SDK and public host APIs only");
    ui.badge("Styled UI ABI", LunaEditorStatusVariant_Success);
    ui.endPanel();
    ui.separator();

    ui.separatorText("State");
    ui.checkbox("Enabled", &state->enabled);
    ui.sliderFloat("Intensity", &state->intensity, 0.0f, 1.0f, "%.2f");
    ui.colorEdit3("Accent", &state->accent);
    ui.inputTextWithHint("Label", "Plugin-local state", state->label, sizeof(state->label));
    ui.text(("Open command executions: " + std::to_string(state->open_count)).c_str());
    ui.text(("Created entities: " + std::to_string(state->create_entity_count)).c_str());

    if (ui.button("Trigger Open Command", native::fillWidth(), LunaEditorButtonVariant_Subtle)) {
        (void) host.commands().execute(kOpenCommandId);
    }

    drawPluginAssetSection(host, ui, *state);
    drawProjectSection(host, ui, *state);
    drawAssetSection(host, ui, *state);
    drawSceneSection(host, ui, *state);
    drawViewportSection(host, ui, *state);
}

void resetNativeSampleState(NativeSampleState& state)
{
    state.cleanup();
    state.enabled = 1;
    state.open_count = 0;
    state.create_entity_count = 0;
    state.intensity = 0.5f;
    state.accent = native::Vec3{.x = 0.2f, .y = 0.7f, .z = 0.9f};
    state.selected_entity_id = 0u;
    state.selected_entity_name[0] = '\0';
    state.show_preview_viewport = 1;
    state.asset_note.clear();
    state.project_note.clear();
    state.refresh_note.clear();
    std::snprintf(state.label, sizeof(state.label), "%s", "Native sample state");
    std::snprintf(state.create_entity_name, sizeof(state.create_entity_name), "%s", "Native Sample Entity");
}

int loadNativeSample(void* plugin_user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<NativeSampleState*>(plugin_user_data);
    const native::Host host(host_api);
    if (state == nullptr || !host.valid()) {
        return 0;
    }

    resetNativeSampleState(*state);

    if (!host.commands().canRegister() || !host.windows().canRegister() || !host.menus().canAdd() ||
        !host.pluginAssets().available() || !host.project().available() || !host.assets().available() ||
        !host.scene().available() || !host.selection().available() || !host.viewport().available() ||
        !host.runtimeViewport().available()) {
        host.log().error("Native sample requires command, window, menu, asset, project, scene, selection, viewport, and runtime viewport APIs.");
        return 0;
    }

    native::CommandDescriptor open_command{};
    open_command.id = kOpenCommandId;
    open_command.label = "Open Native Sample Tool";
    open_command.description = "Opens the native editor plugin mainline sample window.";
    open_command.shortcut = "";
    open_command.user_data = state;
    open_command.can_execute = &canOpenSampleWindow;
    open_command.is_checked = &isSampleWindowOpen;
    open_command.execute = &executeOpenSampleWindow;

    native::RegisteredCommand open_registration = host.commands().registerScoped(open_command);
    if (!open_registration) {
        host.log().error("Failed to register native sample open command.");
        return 0;
    }

    native::CommandDescriptor create_command{};
    create_command.id = kCreateEntityCommandId;
    create_command.label = "Create Native Sample Entity";
    create_command.description = "Creates and selects an entity through public editor APIs.";
    create_command.shortcut = "";
    create_command.user_data = state;
    create_command.can_execute = &canCreateSampleEntity;
    create_command.execute = &executeCreateSampleEntity;

    native::RegisteredCommand create_registration = host.commands().registerScoped(create_command);
    if (!create_registration) {
        host.log().error("Failed to register native sample create entity command.");
        return 0;
    }

    native::WindowDescriptor window{};
    window.id = kWindowId;
    window.title = "Native Sample";
    window.default_open = false;
    window.default_size = native::vec2(560.0f, 720.0f);
    window.flags = LunaEditorWindowFlag_None;
    window.user_data = state;
    window.draw = &drawNativeSampleWindow;

    native::RegisteredWindow window_registration = host.windows().registerScoped(window);
    if (!window_registration) {
        host.log().error("Failed to register native sample window.");
        return 0;
    }

    native::MenuItemDescriptor open_menu_item{};
    open_menu_item.menu_path = "Tools/Native Sample";
    open_menu_item.command_id = kOpenCommandId;
    open_menu_item.label = "Open Native Sample Tool";
    open_menu_item.shortcut = "";

    native::RegisteredMenuItemsForCommand open_menu_registration =
        host.menus().addScopedItemsForCommand(open_menu_item);
    if (!open_menu_registration) {
        host.log().error("Failed to register native sample open menu item.");
        return 0;
    }

    native::MenuItemDescriptor create_menu_item{};
    create_menu_item.menu_path = "Tools/Native Sample";
    create_menu_item.command_id = kCreateEntityCommandId;
    create_menu_item.label = "Create Native Sample Entity";
    create_menu_item.shortcut = "";

    native::RegisteredMenuItemsForCommand create_menu_registration =
        host.menus().addScopedItemsForCommand(create_menu_item);
    if (!create_menu_registration) {
        host.log().error("Failed to register native sample create entity menu item.");
        return 0;
    }

    state->open_command = std::move(open_registration);
    state->create_entity_command = std::move(create_registration);
    state->window = std::move(window_registration);
    state->open_menu_items = std::move(open_menu_registration);
    state->create_menu_items = std::move(create_menu_registration);

    host.log().info("Native sample loaded.");
    return 1;
}

void unloadNativeSample(void* plugin_user_data, const LunaEditorHostApi* host_api)
{
    auto* state = static_cast<NativeSampleState*>(plugin_user_data);
    const native::Host host(host_api);
    if (state != nullptr) {
        state->cleanup();
    }
    host.log().info("Native sample unloaded.");
}

} // namespace

extern "C" LUNA_EDITOR_PLUGIN_EXPORT int LunaCreateEditorPlugin(uint32_t host_api_version,
                                                                const LunaEditorHostApi* host_api,
                                                                LunaEditorPluginApi* out_plugin_api)
{
    if (!native::isCompatibleHost(host_api_version, host_api) || out_plugin_api == nullptr) {
        return 0;
    }

    native::PluginDescriptor plugin{};
    plugin.plugin_id = kPluginId;
    plugin.display_name = kDisplayName;
    plugin.version = kVersion;
    plugin.user_data = &g_state;
    plugin.on_load = &loadNativeSample;
    plugin.on_unload = &unloadNativeSample;

    return native::fillPluginApi(plugin, out_plugin_api) ? 1 : 0;
}
