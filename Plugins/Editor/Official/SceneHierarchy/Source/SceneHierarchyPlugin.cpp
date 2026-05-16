#include "SceneHierarchyPlugin.h"

#include "EditorApi/EditorApi.h"
#include "Luna/Editor/EditorBuiltinPluginRegistration.h"

#include "Asset/BuiltinAssets.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

constexpr const char* kPluginId = "luna.editor.scene-hierarchy";
constexpr const char* kWindowId = "luna.editor.scene-hierarchy.window";
constexpr const char* kEntityDragPayload = "LUNA_ENTITY";

uint64_t entityKey(luna::editor::EntityId entity_id)
{
    return static_cast<uint64_t>(entity_id);
}

std::string entityScopedId(luna::editor::EntityId entity_id)
{
    return std::to_string(entityKey(entity_id));
}

struct HierarchyDrawContext {
    luna::editor::Host& host;
    std::unordered_map<uint64_t, const luna::editor::SceneEntityInfo*> entity_by_id;
    std::unordered_set<uint64_t> rendered_entities;
};

bool canEditScene(luna::editor::Host& host)
{
    return host.scene().canEditScene();
}

std::string entityLabel(const luna::editor::SceneEntityInfo& entity)
{
    return entity.name.empty() ? "Unnamed Entity" : entity.name;
}

void drawCreateEntityMenu(luna::editor::Host& host, luna::editor::EntityId parent_id = {})
{
    luna::editor::Ui& ui = host.ui();
    const bool disabled = !canEditScene(host);
    if (disabled) {
        ui.beginDisabled();
    }

    if (ui.menuItem(parent_id.isValid() ? "Create Child" : "Create Empty Entity")) {
        host.scene().createEntity(luna::editor::SceneEntityCreateRequest{
            .kind = luna::editor::SceneEntityCreateKind::Empty,
            .name = "Empty Entity",
            .parent_id = parent_id,
        });
    }

    if (ui.menuItem(parent_id.isValid() ? "Create Child Camera" : "Create Camera")) {
        host.scene().createEntity(luna::editor::SceneEntityCreateRequest{
            .kind = luna::editor::SceneEntityCreateKind::Camera,
            .parent_id = parent_id,
        });
    }

    if (ui.beginMenu(parent_id.isValid() ? "Create Child Light" : "Create Light")) {
        if (ui.menuItem("Directional Light")) {
            host.scene().createEntity(luna::editor::SceneEntityCreateRequest{
                .kind = luna::editor::SceneEntityCreateKind::DirectionalLight,
                .parent_id = parent_id,
            });
        }
        if (ui.menuItem("Point Light")) {
            host.scene().createEntity(luna::editor::SceneEntityCreateRequest{
                .kind = luna::editor::SceneEntityCreateKind::PointLight,
                .parent_id = parent_id,
            });
        }
        if (ui.menuItem("Spot Light")) {
            host.scene().createEntity(luna::editor::SceneEntityCreateRequest{
                .kind = luna::editor::SceneEntityCreateKind::SpotLight,
                .parent_id = parent_id,
            });
        }
        ui.endMenu();
    }

    if (disabled) {
        ui.endDisabled();
    }
}

void drawCreatePrimitiveMenu(luna::editor::Host& host, luna::editor::EntityId parent_id = {})
{
    luna::editor::Ui& ui = host.ui();
    if (!ui.beginMenu("3D Object")) {
        return;
    }

    const bool disabled = !canEditScene(host);
    if (disabled) {
        ui.beginDisabled();
    }

    for (const auto& mesh : luna::BuiltinAssets::getBuiltinMeshes()) {
        if (ui.menuItem(mesh.Name)) {
            host.scene().createEntity(luna::editor::SceneEntityCreateRequest{
                .kind = luna::editor::SceneEntityCreateKind::PrimitiveMesh,
                .parent_id = parent_id,
                .asset_handle = mesh.Handle,
            });
        }
    }

    if (disabled) {
        ui.endDisabled();
    }

    ui.endMenu();
}

void drawCreateHierarchyMenu(luna::editor::Host& host, luna::editor::EntityId parent_id = {})
{
    drawCreateEntityMenu(host, parent_id);
    drawCreatePrimitiveMenu(host, parent_id);
}

bool readEntityDrop(luna::editor::Ui& ui, luna::editor::EntityId& out_entity_id)
{
    uint64_t entity_id = 0;
    if (!ui.acceptDragDropPayload(kEntityDragPayload, &entity_id, sizeof(entity_id))) {
        return false;
    }

    out_entity_id = luna::editor::EntityId(entity_id);
    return out_entity_id.isValid();
}

bool acceptEntityDrop(HierarchyDrawContext& context, luna::editor::EntityId new_parent_id)
{
    luna::editor::EntityId dropped_id;
    if (!readEntityDrop(context.host.ui(), dropped_id)) {
        return false;
    }

    if (new_parent_id.isValid() &&
        (dropped_id == new_parent_id || context.host.scene().isEntityDescendantOf(new_parent_id, dropped_id))) {
        return false;
    }

    return context.host.scene().reparentEntity(dropped_id, new_parent_id, true);
}

bool acceptAssetDrop(HierarchyDrawContext& context, luna::editor::EntityId target_id = {})
{
    luna::editor::AssetDropPayload asset_payload{};
    if (!context.host.ui().acceptAssetDragDropPayload(asset_payload, {luna::AssetType::Mesh, luna::AssetType::Model})) {
        return false;
    }

    switch (asset_payload.type) {
        case luna::AssetType::Mesh:
            if (target_id.isValid()) {
                if (context.host.scene().applyMeshAssetToEntity(target_id, asset_payload.handle)) {
                    context.host.selection().selectEntity(target_id);
                }
            } else {
                context.host.scene().createEntity(luna::editor::SceneEntityCreateRequest{
                    .kind = luna::editor::SceneEntityCreateKind::MeshAsset,
                    .asset_handle = asset_payload.handle,
                });
            }
            return true;
        case luna::AssetType::Model:
            context.host.scene().createEntity(luna::editor::SceneEntityCreateRequest{
                .kind = luna::editor::SceneEntityCreateKind::ModelAsset,
                .parent_id = target_id,
                .asset_handle = asset_payload.handle,
            });
            return true;
        case luna::AssetType::None:
        case luna::AssetType::Texture:
        case luna::AssetType::Material:
        case luna::AssetType::Scene:
        case luna::AssetType::Script:
            break;
    }

    return false;
}

void drawHierarchyDropTarget(HierarchyDrawContext& context, luna::editor::EntityId target_id = {})
{
    luna::editor::Ui& ui = context.host.ui();
    if (!ui.beginDragDropTarget()) {
        return;
    }

    if (canEditScene(context.host)) {
        acceptEntityDrop(context, target_id);
        acceptAssetDrop(context, target_id);
    }

    ui.endDragDropTarget();
}

bool drawEntityContextMenu(HierarchyDrawContext& context, const luna::editor::SceneEntityInfo& entity)
{
    luna::editor::Ui& ui = context.host.ui();
    if (!ui.beginPopupContextItem("entity-context-" + entityScopedId(entity.id))) {
        return false;
    }

    drawCreateHierarchyMenu(context.host, entity.id);

    bool delete_entity = false;
    const bool edit_mode = canEditScene(context.host);
    if (!edit_mode) {
        ui.beginDisabled();
    }

    ui.separator();
    if (entity.parent_id.isValid() && ui.menuItem("Detach From Parent")) {
        context.host.scene().reparentEntity(entity.id, {}, true);
    }

    if (ui.menuItem("Delete Entity")) {
        delete_entity = true;
    }

    if (!edit_mode) {
        ui.endDisabled();
    }

    ui.endPopup();
    return delete_entity;
}

void drawHierarchyContextMenu(luna::editor::Host& host)
{
    luna::editor::Ui& ui = host.ui();
    if (!ui.beginPopupContextItem("HierarchyContext")) {
        return;
    }

    drawCreateHierarchyMenu(host);
    ui.endPopup();
}

void drawEntityNode(HierarchyDrawContext& context, const luna::editor::SceneEntityInfo& entity)
{
    if (!context.rendered_entities.insert(entityKey(entity.id)).second) {
        return;
    }

    luna::editor::Ui& ui = context.host.ui();
    const luna::editor::EntityId selected_entity_id = context.host.selection().selectedEntityId();
    const bool has_children = !entity.child_ids.empty();

    luna::editor::TreeNodeFlags flags = luna::editor::TreeNodeFlag::OpenOnArrow |
                                        luna::editor::TreeNodeFlag::OpenOnDoubleClick |
                                        luna::editor::TreeNodeFlag::SpanAvailWidth;
    if (!has_children) {
        flags = flags | luna::editor::TreeNodeFlag::Leaf | luna::editor::TreeNodeFlag::NoTreePushOnOpen;
    }
    if (entity.id == selected_entity_id) {
        flags = flags | luna::editor::TreeNodeFlag::Selected;
    }

    const bool opened = ui.treeNodeEx(entityScopedId(entity.id), entityLabel(entity), flags);

    if (ui.isItemClicked(luna::editor::MouseButton::Left)) {
        context.host.selection().selectEntity(entity.id);
    }

    if (canEditScene(context.host) && ui.beginDragDropSource()) {
        const uint64_t entity_id = entityKey(entity.id);
        ui.setDragDropPayload(kEntityDragPayload, &entity_id, sizeof(entity_id));
        ui.text(entityLabel(entity));
        ui.endDragDropSource();
    }

    drawHierarchyDropTarget(context, entity.id);
    const bool delete_entity = drawEntityContextMenu(context, entity);

    if (opened && has_children) {
        for (const luna::editor::EntityId child_id : entity.child_ids) {
            const auto child_it = context.entity_by_id.find(entityKey(child_id));
            if (child_it != context.entity_by_id.end()) {
                drawEntityNode(context, *child_it->second);
            }
        }
        ui.treePop();
    }

    if (delete_entity) {
        context.host.scene().destroyEntity(entity.id);
    }
}

void drawHierarchyTree(HierarchyDrawContext& context, const std::vector<luna::editor::SceneEntityInfo>& entities)
{
    for (const luna::editor::SceneEntityInfo& entity : entities) {
        context.entity_by_id.emplace(entityKey(entity.id), &entity);
    }

    for (const luna::editor::SceneEntityInfo& entity : entities) {
        if (!entity.parent_id.isValid() || !context.entity_by_id.contains(entityKey(entity.parent_id))) {
            drawEntityNode(context, entity);
        }
    }

    for (const luna::editor::SceneEntityInfo& entity : entities) {
        if (!context.rendered_entities.contains(entityKey(entity.id))) {
            drawEntityNode(context, entity);
        }
    }
}

} // namespace

namespace luna::editor {

class SceneHierarchyPlugin final : public Plugin {
public:
    [[nodiscard]] PluginDescriptor descriptor() const override
    {
        return PluginDescriptor{
            .id = kPluginId,
            .display_name = "Scene Hierarchy",
            .version = "0.1.0",
        };
    }

    bool onLoad(Host& host) override
    {
        return host.windows().registerWindow(WindowDescriptor{
            .id = kWindowId,
            .title = "Scene Hierarchy",
            .default_open = true,
            .default_size = Vec2{.x = 300.0f, .y = 360.0f},
            .draw =
                [](WindowDrawContext& context) {
                    Host& host = context.host();
                    Ui& ui = context.ui();

                    const EntityId selected_entity_id = host.selection().selectedEntityId();
                    if (selected_entity_id.isValid() && !host.scene().entityExists(selected_entity_id)) {
                        host.selection().clearSelection();
                    }

                    const std::vector<SceneEntityInfo> entities = host.scene().entityHierarchy();
                    HierarchyDrawContext draw_context{.host = host};

                    if (entities.empty()) {
                        ui.text("No entities in scene.");
                    } else {
                        drawHierarchyTree(draw_context, entities);
                    }

                    const Vec2 available = ui.contentRegionAvail();
                    if (available.y > 0.0f) {
                        ui.invisibleButton("##HierarchyDropZone", Vec2{.x = -1.0f, .y = available.y});
                        if (ui.isItemClicked(MouseButton::Left)) {
                            host.selection().clearSelection();
                        }

                        drawHierarchyContextMenu(host);
                        drawHierarchyDropTarget(draw_context);
                    }
                },
        });
    }

    void onUnload(Host& host) override
    {
        host.windows().unregisterWindow(kWindowId);
    }
};

std::unique_ptr<Plugin> createSceneHierarchyPlugin()
{
    return std::make_unique<SceneHierarchyPlugin>();
}

namespace {

const EditorBuiltinPluginFactoryRegistration kSceneHierarchyPluginRegistration{
    kPluginId,
    createSceneHierarchyPlugin,
};

} // namespace

} // namespace luna::editor
