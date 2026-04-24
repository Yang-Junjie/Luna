#include "EditorAssetDragDrop.h"
#include "Asset/BuiltinAssets.h"
#include "LunaEditorLayer.h"
#include "Scene/Components.h"
#include "SceneHierarchyPanel.h"

#include <imgui.h>
#include <unordered_set>
#include <vector>

namespace {

constexpr const char* kEntityDragPayload = "LUNA_ENTITY";

bool isDescendantOf(luna::Entity entity, luna::Entity potential_ancestor)
{
    if (!entity || !potential_ancestor) {
        return false;
    }

    for (luna::Entity current = entity.getParent(); current; current = current.getParent()) {
        if (current == potential_ancestor) {
            return true;
        }
    }

    return false;
}

luna::Entity createEmptyEntity(luna::LunaEditorLayer& editor_layer, luna::Entity parent = {})
{
    auto& entity_manager = editor_layer.getScene().entityManager();
    luna::Entity entity = parent ? entity_manager.createChildEntity(parent, "Empty Entity")
                                 : entity_manager.createEntity("Empty Entity");
    if (entity) {
        editor_layer.setSelectedEntity(entity);
    }

    return entity;
}

void drawCreatePrimitiveMenu(luna::LunaEditorLayer& editor_layer, luna::Entity parent = {})
{
    if (!ImGui::BeginMenu("3D Object")) {
        return;
    }

    for (const auto& mesh : luna::BuiltinAssets::getBuiltinMeshes()) {
        if (ImGui::MenuItem(mesh.Name)) {
            editor_layer.createPrimitiveEntity(mesh.Handle, parent);
        }
    }

    ImGui::EndMenu();
}

void applyMeshAssetToEntity(luna::LunaEditorLayer& editor_layer,
                            luna::Entity entity,
                            const luna::editor::AssetDragDropData& payload)
{
    editor_layer.applyMeshAssetToEntity(entity, luna::editor::getAssetHandle(payload));
    editor_layer.setSelectedEntity(entity);
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

void drawEntityNode(luna::LunaEditorLayer& editor_layer,
                    luna::EntityManager& entity_manager,
                    luna::Entity entity,
                    luna::Entity selected_entity,
                    std::unordered_set<luna::UUID>& rendered_entities)
{
    if (!entity || !rendered_entities.insert(entity.getUUID()).second) {
        return;
    }

    const char* label = entity.hasComponent<luna::TagComponent>() ? entity.getComponent<luna::TagComponent>().tag.c_str()
                                                                  : "Unnamed Entity";
    const bool has_children = entity.hasChildren();

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
        editor_layer.setSelectedEntity(entity);
    }

    if (ImGui::BeginDragDropSource()) {
        const uint64_t entity_id = static_cast<uint64_t>(entity.getUUID());
        ImGui::SetDragDropPayload(kEntityDragPayload, &entity_id, sizeof(entity_id));
        ImGui::TextUnformatted(label);
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kEntityDragPayload)) {
            const uint64_t dropped_id = *static_cast<const uint64_t*>(payload->Data);
            luna::Entity dropped_entity = entity_manager.findEntityByUUID(luna::UUID(dropped_id));

            if (dropped_entity && dropped_entity != entity && !isDescendantOf(entity, dropped_entity)) {
                entity_manager.setParent(dropped_entity, entity, true);
            }
        }

        luna::editor::AssetDragDropData asset_payload{};
        if (luna::editor::acceptAssetDragDropPayload(asset_payload, {luna::AssetType::Mesh})) {
            applyMeshAssetToEntity(editor_layer, entity, asset_payload);
        }
        ImGui::EndDragDropTarget();
    }

    bool delete_entity = false;
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Create Child")) {
            createEmptyEntity(editor_layer, entity);
        }

        drawCreatePrimitiveMenu(editor_layer, entity);

        if (entity.hasParent() && ImGui::MenuItem("Detach From Parent")) {
            entity.clearParent(true);
        }

        if (ImGui::MenuItem("Delete Entity")) {
            delete_entity = true;
        }

        ImGui::EndPopup();
    }

    if (opened && has_children) {
        const std::vector<luna::UUID> children = entity.getChildren();
        for (const luna::UUID child_uuid : children) {
            if (luna::Entity child = entity_manager.findEntityByUUID(child_uuid); child) {
                drawEntityNode(editor_layer, entity_manager, child, editor_layer.getSelectedEntity(), rendered_entities);
            }
        }
        ImGui::TreePop();
    }

    if (delete_entity) {
        const luna::Entity current_selection = editor_layer.getSelectedEntity();
        const bool clear_selection = current_selection && (current_selection == entity || isDescendantOf(current_selection, entity));
        entity_manager.destroyEntity(entity);
        if (clear_selection) {
            editor_layer.setSelectedEntity({});
        }
    }
}

} // namespace

namespace luna {

SceneHierarchyPanel::SceneHierarchyPanel(LunaEditorLayer& editor_layer)
    : m_editor_layer(&editor_layer)
{}

void SceneHierarchyPanel::onImGuiRender()
{
    if (m_editor_layer == nullptr) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(300.0f, 360.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Scene Hierarchy");

    auto& scene = m_editor_layer->getScene();
    auto& entity_manager = scene.entityManager();

    Entity selected_entity = m_editor_layer->getSelectedEntity();
    if (selected_entity && !selected_entity.isValid()) {
        m_editor_layer->setSelectedEntity({});
        selected_entity = {};
    }

    auto view = entity_manager.registry().view<TagComponent, RelationshipComponent>();
    if (view.begin() == view.end()) {
        ImGui::TextUnformatted("No entities in scene.");
    } else {
        std::unordered_set<UUID> owned_entities;
        std::unordered_set<UUID> rendered_entities;

        for (const auto entity_handle : view) {
            Entity entity(entity_handle, &entity_manager);
            const UUID parent_uuid = entity.getParentUUID();
            if (!parent_uuid.isValid() || !entity_manager.containsEntity(parent_uuid)) {
                collectOwnedEntities(entity_manager, entity, owned_entities);
                drawEntityNode(*m_editor_layer, entity_manager, entity, selected_entity, rendered_entities);
            }
        }

        for (const auto entity_handle : view) {
            Entity entity(entity_handle, &entity_manager);
            if (!owned_entities.contains(entity.getUUID())) {
                drawEntityNode(*m_editor_layer,
                               entity_manager,
                               entity,
                               m_editor_layer->getSelectedEntity(),
                               rendered_entities);
            }
        }
    }

    const float drop_zone_height = ImGui::GetContentRegionAvail().y;
    if (drop_zone_height > 0.0f) {
        ImGui::InvisibleButton("##HierarchyDropZone", ImVec2(-1.0f, drop_zone_height));

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            m_editor_layer->setSelectedEntity({});
        }

        if (ImGui::BeginPopupContextItem("HierarchyContext", ImGuiPopupFlags_MouseButtonRight)) {
            if (ImGui::MenuItem("Create Empty Entity")) {
                createEmptyEntity(*m_editor_layer);
            }
            drawCreatePrimitiveMenu(*m_editor_layer);
            ImGui::EndPopup();
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kEntityDragPayload)) {
                const uint64_t dropped_id = *static_cast<const uint64_t*>(payload->Data);
                if (Entity dropped_entity = entity_manager.findEntityByUUID(UUID(dropped_id)); dropped_entity) {
                    dropped_entity.clearParent(true);
                }
            }

            luna::editor::AssetDragDropData asset_payload{};
            if (luna::editor::acceptAssetDragDropPayload(asset_payload, {luna::AssetType::Mesh})) {
                m_editor_layer->createEntityFromMeshAsset(luna::editor::getAssetHandle(asset_payload));
            }
            ImGui::EndDragDropTarget();
        }
    }

    ImGui::End();
}

} // namespace luna
