#include "LunaEditorLayer.h"
#include "Scene/Components.h"
#include "SceneHierarchyPanel.h"

#include <imgui.h>
#include <unordered_set>

namespace {

void drawEntityNode(luna::LunaEditorLayer& editor_layer,
                    luna::EntityManager& entity_manager,
                    luna::Entity entity,
                    luna::Entity selected_entity,
                    std::unordered_set<luna::UUID>& visited_entities)
{
    if (!entity || !visited_entities.insert(entity.getUUID()).second) {
        return;
    }

    const auto& tag = entity.getComponent<luna::TagComponent>();
    const bool has_children = entity.hasChildren();
    luna::Entity clicked_entity{};

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!has_children) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (entity == selected_entity) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    const bool opened = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(entity))),
                                          flags,
                                          "%s",
                                          tag.tag.c_str());
    if (ImGui::IsItemClicked()) {
        clicked_entity = entity;
    }

    if (opened) {
        for (const luna::UUID child_uuid : entity.getChildren()) {
            if (luna::Entity child = entity_manager.findEntityByUUID(child_uuid); child) {
                drawEntityNode(editor_layer, entity_manager, child, selected_entity, visited_entities);
            }
        }
        ImGui::TreePop();
    }

    if (clicked_entity) {
        editor_layer.setSelectedEntity(clicked_entity);
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

    ImGui::SetNextWindowSize(ImVec2(280.0f, 320.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Scene Hierarchy");

    auto& scene = m_editor_layer->getScene();
    auto& entity_manager = scene.entityManager();
    const Entity selected_entity = m_editor_layer->getSelectedEntity();
    auto view = entity_manager.registry().view<TagComponent, RelationshipComponent>();

    if (view.begin() == view.end()) {
        ImGui::TextUnformatted("No entities in scene.");
    } else {
        std::unordered_set<UUID> visited_entities;
        for (const auto entity_handle : view) {
            Entity entity(entity_handle, &entity_manager);
            const UUID parent_uuid = entity.getParentUUID();
            if (!parent_uuid.isValid() || !entity_manager.containsEntity(parent_uuid)) {
                drawEntityNode(*m_editor_layer, entity_manager, entity, selected_entity, visited_entities);
            }
        }

        for (const auto entity_handle : view) {
            Entity entity(entity_handle, &entity_manager);
            if (!visited_entities.contains(entity.getUUID())) {
                drawEntityNode(*m_editor_layer, entity_manager, entity, selected_entity, visited_entities);
            }
        }
    }

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()) {
        m_editor_layer->setSelectedEntity({});
    }

    ImGui::End();
}

} // namespace luna
