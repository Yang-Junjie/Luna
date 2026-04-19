#include "LunaEditorApp.h"
#include "Scene/Components.h"
#include "SceneHierarchyPanel.h"

#include <imgui.h>

namespace luna {

SceneHierarchyPanel::SceneHierarchyPanel(LunaEditorApplication& application)
    : m_application(&application)
{}

void SceneHierarchyPanel::onImGuiRender()
{
    if (m_application == nullptr) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(280.0f, 320.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Scene Hierarchy");

    auto& scene = m_application->getScene();
    auto view = scene.registry().view<TagComponent>();
    const Entity selected_entity = m_application->getSelectedEntity();

    if (view.begin() == view.end()) {
        ImGui::TextUnformatted("No entities in scene.");
    } else {
        for (const auto entity_handle : view) {
            Entity entity(entity_handle, &scene);
            const auto& tag = view.get<TagComponent>(entity_handle);

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                                       ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (entity == selected_entity) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }

            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(entity))),
                              flags,
                              "%s",
                              tag.tag.c_str());
            if (ImGui::IsItemClicked()) {
                m_application->setSelectedEntity(entity);
            }
        }
    }

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()) {
        m_application->setSelectedEntity({});
    }

    ImGui::End();
}

} // namespace luna
