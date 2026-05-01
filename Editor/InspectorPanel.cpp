#include "InspectorPanel.h"

#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Asset/BuiltinAssets.h"
#include "EditorAssetDragDrop.h"
#include "LunaEditorLayer.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Scene/Components.h"
#include "ScriptComponentInspector.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <string>

namespace {

bool drawVec3Control(const std::string& label,
                     glm::vec3& values,
                     float reset_value = 0.0f,
                     float column_width = 100.0f,
                     float drag_speed = 0.1f)
{
    bool changed = false;
    ImGui::PushID(label.c_str());
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2{0.0f, 1.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{4.0f, 1.0f});

    if (ImGui::BeginTable("##Vec3Table", 2, ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, column_width);
        ImGui::TableSetupColumn("##controls", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFontSize() + 2.0f);

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label.c_str());

        ImGui::TableNextColumn();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{4.0f, 0.0f});

        const float line_height = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        const ImVec2 button_size{line_height + 3.0f, line_height};
        const float item_spacing = ImGui::GetStyle().ItemSpacing.x;
        const float item_width =
            (std::max)((ImGui::GetContentRegionAvail().x - button_size.x * 3.0f - item_spacing * 2.0f) / 3.0f, 1.0f);

        auto draw_axis_control =
            [&](const char* axis_label, float& value, const ImVec4& color, const ImVec4& hovered_color, bool last) {
                bool axis_changed = false;

                ImGui::PushStyleColor(ImGuiCol_Button, color);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered_color);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
                if (ImGui::Button(axis_label, button_size)) {
                    value = reset_value;
                    axis_changed = true;
                }
                ImGui::PopStyleColor(3);

                ImGui::SameLine();
                ImGui::SetNextItemWidth(item_width);
                if (ImGui::DragFloat(
                        (std::string("##") + axis_label).c_str(), &value, drag_speed, 0.0f, 0.0f, "%.2f")) {
                    axis_changed = true;
                }

                if (!last) {
                    ImGui::SameLine();
                }

                return axis_changed;
            };

        changed |= draw_axis_control(
            "X", values.x, ImVec4{0.80f, 0.10f, 0.15f, 1.0f}, ImVec4{0.90f, 0.20f, 0.20f, 1.0f}, false);
        changed |= draw_axis_control(
            "Y", values.y, ImVec4{0.20f, 0.70f, 0.20f, 1.0f}, ImVec4{0.30f, 0.80f, 0.30f, 1.0f}, false);
        changed |= draw_axis_control(
            "Z", values.z, ImVec4{0.10f, 0.25f, 0.80f, 1.0f}, ImVec4{0.20f, 0.35f, 0.90f, 1.0f}, true);

        ImGui::PopStyleVar();
        ImGui::EndTable();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopID();
    return changed;
}

std::string getAssetDisplayLabel(luna::AssetHandle handle)
{
    if (!handle.isValid()) {
        return "None";
    }

    if (!luna::AssetDatabase::exists(handle)) {
        return "Unknown Asset";
    }

    const auto& metadata = luna::AssetDatabase::getAssetMetadata(handle);
    if (!metadata.Name.empty()) {
        return metadata.Name;
    }

    if (!metadata.FilePath.empty()) {
        return metadata.FilePath.generic_string();
    }

    return "Unnamed Asset";
}

bool drawAssetHandleEditor(const char* label,
                          luna::AssetHandle& handle,
                          std::initializer_list<luna::AssetType> accepted_types = {})
{
    bool changed = false;
    unsigned long long raw_handle = static_cast<unsigned long long>(static_cast<uint64_t>(handle));
    if (ImGui::InputScalar(label, ImGuiDataType_U64, &raw_handle)) {
        handle = luna::AssetHandle(static_cast<uint64_t>(raw_handle));
        changed = true;
    }

    if (ImGui::BeginDragDropTarget()) {
        luna::editor::AssetDragDropData payload{};
        if (luna::editor::acceptAssetDragDropPayload(payload, accepted_types)) {
            handle = luna::editor::getAssetHandle(payload);
            changed = true;
        }
        ImGui::EndDragDropTarget();
    }

    return changed;
}

template <typename T, typename UIFunction>
void drawComponentSection(const char* label, luna::Entity entity, UIFunction&& ui_function, bool allow_remove)
{
    if (!entity.hasComponent<T>()) {
        return;
    }

    auto& component = entity.getComponent<T>();
    ImGui::PushID(label);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{6.0f, 4.0f});

    const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
                                     ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding |
                                     ImGuiTreeNodeFlags_AllowOverlap;
    const bool open = ImGui::TreeNodeEx("##Section", flags, "%s", label);
    ImGui::PopStyleVar();

    bool remove_component = false;
    if (allow_remove) {
        if (ImGui::BeginPopupContextItem("ComponentSettings")) {
            if (ImGui::MenuItem("Remove Component")) {
                remove_component = true;
            }
            ImGui::EndPopup();
        }
    }

    if (open) {
        ui_function(component);
        ImGui::TreePop();
    }

    if (remove_component) {
        entity.removeComponent<T>();
    }

    ImGui::PopID();
}

void syncMeshMaterialSlots(luna::MeshComponent& mesh_component)
{
    if (!mesh_component.meshHandle.isValid()) {
        return;
    }

    const auto mesh = luna::AssetManager::get().requestAssetAs<luna::Mesh>(mesh_component.meshHandle);
    if (!mesh || !mesh->isValid()) {
        return;
    }

    mesh_component.resizeSubmeshMaterials(mesh->getSubMeshes().size());
}

void assignDefaultMaterialSlots(luna::MeshComponent& mesh_component)
{
    for (uint32_t submesh_index = 0; submesh_index < mesh_component.getSubmeshMaterialCount(); ++submesh_index) {
        if (!mesh_component.getSubmeshMaterial(submesh_index).isValid()) {
            mesh_component.setSubmeshMaterial(submesh_index, luna::BuiltinMaterials::DefaultLit);
        }
    }
}

} // namespace

namespace luna {

InspectorPanel::InspectorPanel(LunaEditorLayer& editor_layer)
    : m_editor_layer(&editor_layer)
{}

void InspectorPanel::onImGuiRender()
{
    if (m_editor_layer == nullptr) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(380.0f, 520.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Inspector");

    const bool editing_runtime_scene = m_editor_layer->isRuntimeViewportEnabled();

    Entity selected_entity = m_editor_layer->getSelectedEntity();
    if (selected_entity && !selected_entity.isValid()) {
        m_editor_layer->setSelectedEntity({});
        selected_entity = {};
    }

    if (!selected_entity) {
        ImGui::TextUnformatted("Select an entity to inspect.");
        ImGui::End();
        return;
    }

    auto& tag_component = selected_entity.getComponent<TagComponent>();
    char tag_buffer[256] = {};
    strncpy_s(tag_buffer, tag_component.tag.c_str(), _TRUNCATE);

    if (ImGui::InputText("##Tag", tag_buffer, sizeof(tag_buffer))) {
        tag_component.tag = tag_buffer;
        if (!editing_runtime_scene) {
            m_editor_layer->markSceneDirty();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        if (!selected_entity.hasComponent<MeshComponent>() && ImGui::MenuItem("Mesh")) {
            selected_entity.addComponent<MeshComponent>();
            if (!editing_runtime_scene) {
                m_editor_layer->markSceneDirty();
            }
            ImGui::CloseCurrentPopup();
        }

        if (!selected_entity.hasComponent<CameraComponent>() && ImGui::MenuItem("Camera")) {
            selected_entity.addComponent<CameraComponent>();
            if (!editing_runtime_scene) {
                m_editor_layer->markSceneDirty();
            }
            ImGui::CloseCurrentPopup();
        }

        if (!selected_entity.hasComponent<LightComponent>() && ImGui::MenuItem("Light")) {
            selected_entity.addComponent<LightComponent>();
            if (!editing_runtime_scene) {
                m_editor_layer->markSceneDirty();
            }
            ImGui::CloseCurrentPopup();
        }

        if (!selected_entity.hasComponent<ScriptComponent>() && ImGui::MenuItem("Script")) {
            selected_entity.addComponent<ScriptComponent>();
            if (!editing_runtime_scene) {
                m_editor_layer->markSceneDirty();
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::TextDisabled("UUID: %s", selected_entity.getUUID().toString().c_str());
    ImGui::Separator();

    drawComponentSection<TransformComponent>(
        "Transform", selected_entity, [&](TransformComponent& transform) {
            drawVec3Control("Translation", transform.translation, 0.0f);

            glm::vec3 rotation_degrees = glm::degrees(transform.rotation);
            if (drawVec3Control("Rotation", rotation_degrees, 0.0f)) {
                transform.setRotationEuler(glm::radians(rotation_degrees));
            }

            drawVec3Control("Scale", transform.scale, 1.0f);
        },
        false);

    drawComponentSection<RelationshipComponent>(
        "Relationship", selected_entity, [&](RelationshipComponent&) {
            Entity parent = selected_entity.getParent();
            if (parent) {
                if (ImGui::Button(parent.getName().c_str(), ImVec2(-1.0f, 0.0f))) {
                    m_editor_layer->setSelectedEntity(parent);
                }
                ImGui::TextDisabled("Parent UUID: %s", parent.getUUID().toString().c_str());

                if (ImGui::Button("Detach From Parent", ImVec2(-1.0f, 0.0f))) {
                    selected_entity.clearParent(true);
                    if (!editing_runtime_scene) {
                        m_editor_layer->markSceneDirty();
                    }
                }
            } else {
                ImGui::TextUnformatted("Parent: None");
            }

            ImGui::SeparatorText("Children");
            if (!selected_entity.hasChildren()) {
                ImGui::TextUnformatted("No child entities.");
            } else {
                for (const UUID child_uuid : selected_entity.getChildren()) {
                    Entity child = selected_entity.getEntityManager()->findEntityByUUID(child_uuid);
                    if (!child) {
                        continue;
                    }

                    if (ImGui::Selectable(child.getName().c_str())) {
                        m_editor_layer->setSelectedEntity(child);
                    }
                    ImGui::TextDisabled("UUID: %s", child.getUUID().toString().c_str());
                }
            }
        },
        false);

    drawComponentSection<CameraComponent>(
        "Camera", selected_entity, [&](CameraComponent& camera_component) {
            ImGui::Checkbox("Primary", &camera_component.primary);
            ImGui::Checkbox("Fixed Aspect Ratio", &camera_component.fixedAspectRatio);

            const char* projection_label = camera_component.projectionType == Camera::ProjectionType::Orthographic
                                               ? "Orthographic"
                                               : "Perspective";
            if (ImGui::BeginCombo("Projection", projection_label)) {
                if (ImGui::Selectable("Perspective",
                                      camera_component.projectionType == Camera::ProjectionType::Perspective)) {
                    camera_component.projectionType = Camera::ProjectionType::Perspective;
                }
                if (ImGui::Selectable("Orthographic",
                                      camera_component.projectionType == Camera::ProjectionType::Orthographic)) {
                    camera_component.projectionType = Camera::ProjectionType::Orthographic;
                }
                ImGui::EndCombo();
            }

            if (camera_component.projectionType == Camera::ProjectionType::Perspective) {
                float fov_degrees = glm::degrees(camera_component.perspectiveVerticalFovRadians);
                if (ImGui::DragFloat("Vertical FOV", &fov_degrees, 0.25f, 1.0f, 179.0f, "%.2f deg")) {
                    camera_component.perspectiveVerticalFovRadians = glm::radians(fov_degrees);
                }
                ImGui::DragFloat("Near", &camera_component.perspectiveNear, 0.01f, 0.001f, 1000.0f);
                ImGui::DragFloat("Far", &camera_component.perspectiveFar, 1.0f, 0.001f, 10000.0f);
            } else {
                ImGui::DragFloat("Size", &camera_component.orthographicSize, 0.1f, 0.001f, 10000.0f);
                ImGui::DragFloat("Near", &camera_component.orthographicNear, 0.1f, -10000.0f, 10000.0f);
                ImGui::DragFloat("Far", &camera_component.orthographicFar, 0.1f, -10000.0f, 10000.0f);
            }
        },
        true);

    drawComponentSection<LightComponent>(
        "Light", selected_entity, [](LightComponent& light_component) {
            ImGui::Checkbox("Enabled", &light_component.enabled);

            const char* type_label = "Directional";
            if (light_component.type == LightComponent::Type::Point) {
                type_label = "Point";
            } else if (light_component.type == LightComponent::Type::Spot) {
                type_label = "Spot";
            }
            if (ImGui::BeginCombo("Type", type_label)) {
                if (ImGui::Selectable("Directional", light_component.type == LightComponent::Type::Directional)) {
                    light_component.type = LightComponent::Type::Directional;
                }
                if (ImGui::Selectable("Point", light_component.type == LightComponent::Type::Point)) {
                    light_component.type = LightComponent::Type::Point;
                }
                if (ImGui::Selectable("Spot", light_component.type == LightComponent::Type::Spot)) {
                    light_component.type = LightComponent::Type::Spot;
                }
                ImGui::EndCombo();
            }

            ImGui::ColorEdit3("Color", &light_component.color.x);
            ImGui::DragFloat("Intensity", &light_component.intensity, 0.05f, 0.0f, 100.0f, "%.2f");
            if (light_component.type == LightComponent::Type::Point || light_component.type == LightComponent::Type::Spot) {
                ImGui::DragFloat("Range", &light_component.range, 0.1f, 0.001f, 1000.0f, "%.2f");
            }
            if (light_component.type == LightComponent::Type::Spot) {
                glm::vec2 cone_angles = glm::degrees(glm::vec2(light_component.innerConeAngleRadians,
                                                               light_component.outerConeAngleRadians));
                if (ImGui::DragFloat2("Cone Angles", &cone_angles.x, 0.5f, 0.1f, 89.0f, "%.1f")) {
                    cone_angles.x = (std::clamp)(cone_angles.x, 0.1f, 89.0f);
                    cone_angles.y = (std::clamp)(cone_angles.y, cone_angles.x, 89.0f);
                    light_component.innerConeAngleRadians = glm::radians(cone_angles.x);
                    light_component.outerConeAngleRadians = glm::radians(cone_angles.y);
                }
            }
            if (light_component.type == LightComponent::Type::Directional) {
                ImGui::TextDisabled("Directional light uses -entity forward as light direction.");
            } else if (light_component.type == LightComponent::Type::Spot) {
                ImGui::TextDisabled("Spot light emits along entity forward direction.");
            } else {
                ImGui::TextDisabled("Point light uses entity world position.");
            }
        },
        true);

    drawComponentSection<MeshComponent>(
        "Mesh", selected_entity, [&](MeshComponent& mesh_component) {
            const AssetHandle previous_mesh_handle = mesh_component.meshHandle;
            const std::string current_builtin_label = BuiltinAssets::isBuiltinMesh(mesh_component.meshHandle)
                                                        ? BuiltinAssets::getDisplayName(mesh_component.meshHandle)
                                                        : "None";
            if (ImGui::BeginCombo("Primitive", current_builtin_label.c_str())) {
                if (ImGui::Selectable("None", !BuiltinAssets::isBuiltinMesh(mesh_component.meshHandle))) {
                    mesh_component.meshHandle = AssetHandle(0);
                }

                for (const auto& mesh : BuiltinAssets::getBuiltinMeshes()) {
                    const bool selected = mesh_component.meshHandle == mesh.Handle;
                    if (ImGui::Selectable(mesh.Name, selected)) {
                        mesh_component.meshHandle = mesh.Handle;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            drawAssetHandleEditor("Mesh Handle", mesh_component.meshHandle, {AssetType::Mesh});
            ImGui::TextDisabled("Mesh Asset: %s", getAssetDisplayLabel(mesh_component.meshHandle).c_str());

            if (mesh_component.meshHandle != previous_mesh_handle) {
                mesh_component.clearAllSubmeshMaterials();
                syncMeshMaterialSlots(mesh_component);
                assignDefaultMaterialSlots(mesh_component);
            }

            const auto mesh = mesh_component.meshHandle.isValid()
                                  ? AssetManager::get().requestAssetAs<Mesh>(mesh_component.meshHandle)
                                  : std::shared_ptr<Mesh>{};
            const bool mesh_loaded = mesh && mesh->isValid();

            if (mesh_loaded) {
                ImGui::Text("Submeshes: %zu", mesh->getSubMeshes().size());
                if (ImGui::Button("Sync Material Slots To Mesh", ImVec2(-1.0f, 0.0f))) {
                    mesh_component.resizeSubmeshMaterials(mesh->getSubMeshes().size());
                }
            } else {
                if (mesh_component.meshHandle.isValid() && AssetManager::get().isAssetLoading(mesh_component.meshHandle)) {
                    ImGui::TextDisabled("Mesh asset is loading...");
                }
                int slot_count = static_cast<int>(mesh_component.getSubmeshMaterialCount());
                if (ImGui::InputInt("Material Slot Count", &slot_count)) {
                    slot_count = (std::max)(slot_count, 0);
                    mesh_component.resizeSubmeshMaterials(static_cast<size_t>(slot_count));
                }
            }

            ImGui::SeparatorText("Submesh Materials");
            if (mesh_component.getSubmeshMaterialCount() == 0) {
                ImGui::TextUnformatted("No material slots.");
            } else {
                for (uint32_t submesh_index = 0;
                     submesh_index < static_cast<uint32_t>(mesh_component.getSubmeshMaterialCount());
                     ++submesh_index) {
                    ImGui::PushID(static_cast<int>(submesh_index));
                    ImGui::Text("Submesh %u", submesh_index);

                    AssetHandle material_handle = mesh_component.getSubmeshMaterial(submesh_index);
                    const std::string current_builtin_material_label = BuiltinAssets::isBuiltinMaterial(material_handle)
                                                                     ? BuiltinAssets::getDisplayName(material_handle)
                                                                     : "None";
                    if (ImGui::BeginCombo("Builtin Material", current_builtin_material_label.c_str())) {
                        if (ImGui::Selectable("None", !BuiltinAssets::isBuiltinMaterial(material_handle))) {
                            material_handle = AssetHandle(0);
                        }

                        for (const auto& material : BuiltinAssets::getBuiltinMaterials()) {
                            const bool selected = material_handle == material.Handle;
                            if (ImGui::Selectable(material.Name, selected)) {
                                material_handle = material.Handle;
                            }
                            if (selected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }

                    drawAssetHandleEditor("Material Handle", material_handle, {AssetType::Material});
                    mesh_component.setSubmeshMaterial(submesh_index, material_handle);
                    ImGui::TextDisabled("Material Asset: %s", getAssetDisplayLabel(material_handle).c_str());
                    if (BuiltinAssets::isBuiltinMaterial(material_handle)) {
                        ImGui::TextDisabled("Global builtin material; edits affect all users.");
                        if (ImGui::Button("Edit Builtin Material", ImVec2(-1.0f, 0.0f))) {
                            m_editor_layer->openBuiltinMaterialsPanel(material_handle);
                        }
                    }

                    if (ImGui::Button("Clear Material", ImVec2(-1.0f, 0.0f))) {
                        mesh_component.clearSubmeshMaterial(submesh_index);
                    }

                    ImGui::Separator();
                    ImGui::PopID();
                }

                if (ImGui::Button("Clear All Materials", ImVec2(-1.0f, 0.0f))) {
                    mesh_component.clearAllSubmeshMaterials();
                }
            }
        },
        true);

    drawComponentSection<ScriptComponent>(
        "Script", selected_entity, [this, selected_entity, editing_runtime_scene](ScriptComponent& script_component) {
            const ScriptComponentInspectorChange change = drawScriptComponentInspector(selected_entity, script_component);
            if (change.changed) {
                if (!editing_runtime_scene) {
                    m_editor_layer->markSceneDirty();
                } else if (change.property_value_changed) {
                    m_editor_layer->patchRuntimeScriptProperty(selected_entity.getUUID(),
                                                               change.script_index,
                                                               change.property_index);
                }
            }
        },
        true);

    ImGui::End();
}

} // namespace luna
