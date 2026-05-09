#include "Asset/BuiltinAssets.h"
#include "EditorAssetDragDrop.h"
#include "EditorContext.h"
#include "EditorUI.h"
#include "Scene/Components.h"
#include "SceneHierarchyPanel.h"

#include <cstdint>
#include <imgui.h>
#include <unordered_set>
#include <vector>

namespace {

constexpr const char* kEntityDragPayload = "LUNA_ENTITY";

struct HierarchyDrawContext {
    luna::EditorContext& editor_context;
    luna::EntityManager& entity_manager;
    std::unordered_set<luna::UUID>& rendered_entities;
};

bool canEditScene(const luna::EditorContext& editor_context)
{
    return !editor_context.isRuntimeViewportEnabled();
}

const char* entityLabel(luna::Entity entity)
{
    return entity.hasComponent<luna::TagComponent>() ? entity.getComponent<luna::TagComponent>().tag.c_str()
                                                     : "Unnamed Entity";
}

bool isDescendantOf(luna::Entity entity, luna::Entity potential_ancestor)
{
    if (!entity || !potential_ancestor) {
        return false;
    }

    std::unordered_set<luna::UUID> visited_entities;
    for (luna::Entity current = entity.getParent(); current; current = current.getParent()) {
        if (!visited_entities.insert(current.getUUID()).second) {
            return false;
        }
        if (current == potential_ancestor) {
            return true;
        }
    }

    return false;
}

bool readEntityDragPayload(const ImGuiPayload* payload, luna::UUID& out_entity_id)
{
    if (payload == nullptr || payload->Data == nullptr || payload->DataSize != sizeof(uint64_t)) {
        return false;
    }

    out_entity_id = luna::UUID(*static_cast<const uint64_t*>(payload->Data));
    return out_entity_id.isValid();
}

bool acceptEntityDragPayload(luna::UUID& out_entity_id)
{
    return readEntityDragPayload(ImGui::AcceptDragDropPayload(kEntityDragPayload), out_entity_id);
}

luna::Entity createEmptyEntity(luna::EditorContext& editor_context, luna::Entity parent = {})
{
    if (!canEditScene(editor_context)) {
        return {};
    }

    return editor_context.createEntity("Empty Entity", parent);
}

void drawCreateEntityMenu(luna::EditorContext& editor_context, luna::Entity parent = {})
{
    const bool disabled = editor_context.isRuntimeViewportEnabled();
    if (disabled) {
        ImGui::BeginDisabled();
    }

    if (ImGui::MenuItem(parent ? "Create Child" : "Create Empty Entity")) {
        createEmptyEntity(editor_context, parent);
    }

    if (ImGui::MenuItem(parent ? "Create Child Camera" : "Create Camera")) {
        editor_context.createCameraEntity(parent);
    }

    if (ImGui::BeginMenu(parent ? "Create Child Light" : "Create Light")) {
        if (ImGui::MenuItem("Directional Light")) {
            editor_context.createDirectionalLightEntity(parent);
        }
        if (ImGui::MenuItem("Point Light")) {
            editor_context.createPointLightEntity(parent);
        }
        if (ImGui::MenuItem("Spot Light")) {
            editor_context.createSpotLightEntity(parent);
        }
        ImGui::EndMenu();
    }

    if (disabled) {
        ImGui::EndDisabled();
    }
}

void drawCreatePrimitiveMenu(luna::EditorContext& editor_context, luna::Entity parent = {})
{
    if (!ImGui::BeginMenu("3D Object")) {
        return;
    }

    const bool disabled = editor_context.isRuntimeViewportEnabled();
    if (disabled) {
        ImGui::BeginDisabled();
    }

    for (const auto& mesh : luna::BuiltinAssets::getBuiltinMeshes()) {
        if (ImGui::MenuItem(mesh.Name)) {
            editor_context.createPrimitiveEntity(mesh.Handle, parent);
        }
    }

    if (disabled) {
        ImGui::EndDisabled();
    }

    ImGui::EndMenu();
}

void drawCreateHierarchyMenu(luna::EditorContext& editor_context, luna::Entity parent = {})
{
    drawCreateEntityMenu(editor_context, parent);
    drawCreatePrimitiveMenu(editor_context, parent);
}

bool drawEntityContextMenu(luna::EditorContext& editor_context, luna::Entity entity)
{
    if (!ImGui::BeginPopupContextItem()) {
        return false;
    }

    drawCreateHierarchyMenu(editor_context, entity);

    bool delete_entity = false;
    const bool edit_mode = canEditScene(editor_context);
    if (!edit_mode) {
        ImGui::BeginDisabled();
    }

    ImGui::Separator();
    if (entity.hasParent() && ImGui::MenuItem("Detach From Parent")) {
        editor_context.reparentEntity(entity, {}, true);
    }

    if (ImGui::MenuItem("Delete Entity")) {
        delete_entity = true;
    }

    if (!edit_mode) {
        ImGui::EndDisabled();
    }

    ImGui::EndPopup();
    return delete_entity;
}

void applyMeshAssetToEntity(luna::EditorContext& editor_context,
                            luna::Entity entity,
                            const luna::editor::AssetDragDropData& payload)
{
    if (!canEditScene(editor_context)) {
        return;
    }

    editor_context.applyMeshAssetToEntity(entity, luna::editor::getAssetHandle(payload));
    editor_context.setSelectedEntity(entity);
}

void collectOwnedEntities(luna::EntityManager& entity_manager,
                          luna::Entity entity,
                          std::unordered_set<luna::UUID>& owned_entities)
{
    if (!entity || !owned_entities.insert(entity.getUUID()).second) {
        return;
    }

    for (const luna::UUID child_uuid : entity.getChildren()) {
        if (luna::Entity child = entity_manager.findEntityByUUID(child_uuid); child) {
            collectOwnedEntities(entity_manager, child, owned_entities);
        }
    }
}

bool isRootOrOrphan(luna::EntityManager& entity_manager, luna::Entity entity)
{
    const luna::UUID parent_uuid = entity.getParentUUID();
    return !parent_uuid.isValid() || !entity_manager.containsEntity(parent_uuid);
}

bool reparentDroppedEntity(HierarchyDrawContext& context, luna::Entity dropped_entity, luna::Entity new_parent)
{
    if (!canEditScene(context.editor_context) || !dropped_entity) {
        return false;
    }

    bool changed = false;
    if (new_parent) {
        if (dropped_entity == new_parent || isDescendantOf(new_parent, dropped_entity)) {
            return false;
        }
        changed = context.editor_context.reparentEntity(dropped_entity, new_parent, true);
    } else if (dropped_entity.hasParent()) {
        changed = context.editor_context.reparentEntity(dropped_entity, {}, true);
    }
    return changed;
}

bool acceptEntityDrop(HierarchyDrawContext& context, luna::Entity new_parent)
{
    luna::UUID dropped_id;
    if (!acceptEntityDragPayload(dropped_id)) {
        return false;
    }

    luna::Entity dropped_entity = context.entity_manager.findEntityByUUID(dropped_id);
    return reparentDroppedEntity(context, dropped_entity, new_parent);
}

bool acceptAssetDrop(HierarchyDrawContext& context, luna::Entity target = {})
{
    luna::editor::AssetDragDropData asset_payload{};
    if (!luna::editor::acceptAssetDragDropPayload(asset_payload, {luna::AssetType::Mesh, luna::AssetType::Model})) {
        return false;
    }

    const luna::AssetHandle asset_handle = luna::editor::getAssetHandle(asset_payload);
    switch (luna::editor::getAssetType(asset_payload)) {
        case luna::AssetType::Mesh:
            if (target) {
                applyMeshAssetToEntity(context.editor_context, target, asset_payload);
            } else {
                context.editor_context.createEntityFromMeshAsset(asset_handle);
            }
            return true;
        case luna::AssetType::Model:
            if (target) {
                context.editor_context.createEntityFromModelAsset(asset_handle, target);
            } else {
                context.editor_context.createEntityFromModelAsset(asset_handle);
            }
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

void acceptHierarchyDrop(HierarchyDrawContext& context, luna::Entity target)
{
    acceptEntityDrop(context, target);
    acceptAssetDrop(context, target);
}

void destroyEntityAndFixSelection(HierarchyDrawContext& context, luna::Entity entity)
{
    const luna::Entity current_selection = context.editor_context.getSelectedEntity();
    const bool clear_selection =
        current_selection && (current_selection == entity || isDescendantOf(current_selection, entity));
    context.editor_context.destroyEntity(entity);
    if (clear_selection) {
        context.editor_context.setSelectedEntity({});
    }
}

void drawHierarchyContextMenu(luna::EditorContext& editor_context)
{
    if (!ImGui::BeginPopupContextItem("HierarchyContext", ImGuiPopupFlags_MouseButtonRight)) {
        return;
    }

    drawCreateHierarchyMenu(editor_context);
    ImGui::EndPopup();
}

void drawHierarchyDropTarget(HierarchyDrawContext& context, luna::Entity target = {})
{
    if (!ImGui::BeginDragDropTarget()) {
        return;
    }

    if (canEditScene(context.editor_context)) {
        acceptHierarchyDrop(context, target);
    }

    ImGui::EndDragDropTarget();
}

void drawEntityNode(HierarchyDrawContext& context, luna::Entity entity)
{
    if (!entity || !context.rendered_entities.insert(entity.getUUID()).second) {
        return;
    }

    const char* label = entityLabel(entity);
    const bool has_children = entity.hasChildren();
    const luna::Entity selected_entity = context.editor_context.getSelectedEntity();

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!has_children) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (entity == selected_entity) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    const bool opened = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint64_t>(entity.getUUID()))),
                                          flags,
                                          "%s",
                                          label);

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        context.editor_context.setSelectedEntity(entity);
    }

    if (canEditScene(context.editor_context) && ImGui::BeginDragDropSource()) {
        const uint64_t entity_id = static_cast<uint64_t>(entity.getUUID());
        ImGui::SetDragDropPayload(kEntityDragPayload, &entity_id, sizeof(entity_id));
        ImGui::TextUnformatted(label);
        ImGui::EndDragDropSource();
    }

    drawHierarchyDropTarget(context, entity);
    const bool delete_entity = drawEntityContextMenu(context.editor_context, entity);

    if (opened && has_children) {
        const std::vector<luna::UUID> children = entity.getChildren();
        for (const luna::UUID child_uuid : children) {
            if (luna::Entity child = context.entity_manager.findEntityByUUID(child_uuid); child) {
                drawEntityNode(context, child);
            }
        }
        ImGui::TreePop();
    }

    if (delete_entity) {
        destroyEntityAndFixSelection(context, entity);
    }
}

template <typename View>
void drawHierarchyTree(HierarchyDrawContext& context, View&& view)
{
    std::unordered_set<luna::UUID> owned_entities;

    for (const auto entity_handle : view) {
        luna::Entity entity(entity_handle, &context.entity_manager);
        if (isRootOrOrphan(context.entity_manager, entity)) {
            collectOwnedEntities(context.entity_manager, entity, owned_entities);
            drawEntityNode(context, entity);
        }
    }

    for (const auto entity_handle : view) {
        luna::Entity entity(entity_handle, &context.entity_manager);
        if (!owned_entities.contains(entity.getUUID())) {
            drawEntityNode(context, entity);
        }
    }
}

} // namespace

namespace luna {

SceneHierarchyPanel::SceneHierarchyPanel(EditorContext& editor_context)
    : m_editor_context(&editor_context)
{}

void SceneHierarchyPanel::onImGuiRender()
{
    if (m_editor_context == nullptr) {
        return;
    }

    ImGui::SetNextWindowSize(editor::ui::scaled(300.0f, 360.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Scene Hierarchy");

    auto& scene = m_editor_context->getInspectionScene();
    auto& entity_manager = scene.entityManager();

    if (Entity selected_entity = m_editor_context->getSelectedEntity(); selected_entity && !selected_entity.isValid()) {
        m_editor_context->setSelectedEntity({});
    }

    std::unordered_set<UUID> rendered_entities;
    HierarchyDrawContext draw_context{*m_editor_context, entity_manager, rendered_entities};

    auto view = entity_manager.registry().view<TagComponent, RelationshipComponent>();
    if (view.begin() == view.end()) {
        ImGui::TextUnformatted("No entities in scene.");
    } else {
        drawHierarchyTree(draw_context, view);
    }

    const float drop_zone_height = ImGui::GetContentRegionAvail().y;
    if (drop_zone_height > 0.0f) {
        ImGui::InvisibleButton("##HierarchyDropZone", ImVec2(-1.0f, drop_zone_height));

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            m_editor_context->setSelectedEntity({});
        }

        drawHierarchyContextMenu(*m_editor_context);
        drawHierarchyDropTarget(draw_context);
    }

    ImGui::End();
}

} // namespace luna
