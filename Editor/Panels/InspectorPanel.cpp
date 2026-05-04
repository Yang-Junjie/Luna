#include "InspectorPanel.h"

#include "Asset/AssetManager.h"
#include "Asset/BuiltinAssets.h"
#include "EditorContext.h"
#include "EditorUI.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Scene/Components.h"
#include "ScriptComponentInspector.h"

#include <algorithm>
#include <cstring>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <string>

namespace {

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

InspectorPanel::InspectorPanel(EditorContext& editor_context)
    : m_editor_context(&editor_context)
{}

void InspectorPanel::onImGuiRender()
{
    if (m_editor_context == nullptr) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(380.0f, 520.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Inspector");

    const bool editing_runtime_scene = m_editor_context->isRuntimeViewportEnabled();
    bool scene_changed = false;

    Entity selected_entity = m_editor_context->getSelectedEntity();
    if (selected_entity && !selected_entity.isValid()) {
        m_editor_context->setSelectedEntity({});
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
        scene_changed = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Add Component")) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        if (!selected_entity.hasComponent<MeshComponent>() && ImGui::MenuItem("Mesh")) {
            selected_entity.addComponent<MeshComponent>();
            scene_changed = true;
            ImGui::CloseCurrentPopup();
        }

        if (!selected_entity.hasComponent<CameraComponent>() && ImGui::MenuItem("Camera")) {
            selected_entity.addComponent<CameraComponent>();
            scene_changed = true;
            ImGui::CloseCurrentPopup();
        }

        if (!selected_entity.hasComponent<LightComponent>() && ImGui::MenuItem("Light")) {
            selected_entity.addComponent<LightComponent>();
            scene_changed = true;
            ImGui::CloseCurrentPopup();
        }

        if (!selected_entity.hasComponent<ScriptComponent>() && ImGui::MenuItem("Script")) {
            selected_entity.addComponent<ScriptComponent>();
            scene_changed = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::TextDisabled("UUID: %s", selected_entity.getUUID().toString().c_str());
    ImGui::Separator();

    scene_changed |= editor::ui::drawComponentSection<TransformComponent>(
        "Transform", selected_entity, [&](TransformComponent& transform) {
            bool changed = false;
            changed |= editor::ui::drawVec3Control("Translation", transform.translation, 0.0f);

            glm::vec3 rotation_degrees = glm::degrees(transform.rotation);
            if (editor::ui::drawVec3Control("Rotation", rotation_degrees, 0.0f)) {
                transform.setRotationEuler(glm::radians(rotation_degrees));
                changed = true;
            }

            changed |= editor::ui::drawVec3Control("Scale", transform.scale, 1.0f);
            return changed;
        },
        false);

    scene_changed |= editor::ui::drawComponentSection<RelationshipComponent>(
        "Relationship", selected_entity, [&](RelationshipComponent&) {
            bool changed = false;
            Entity parent = selected_entity.getParent();
            if (parent) {
                if (ImGui::Button(parent.getName().c_str(), ImVec2(-1.0f, 0.0f))) {
                    m_editor_context->setSelectedEntity(parent);
                }
                ImGui::TextDisabled("Parent UUID: %s", parent.getUUID().toString().c_str());

                if (ImGui::Button("Detach From Parent", ImVec2(-1.0f, 0.0f))) {
                    changed |= selected_entity.clearParent(true);
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
                        m_editor_context->setSelectedEntity(child);
                    }
                    ImGui::TextDisabled("UUID: %s", child.getUUID().toString().c_str());
                }
            }
            return changed;
        },
        false);

    scene_changed |= editor::ui::drawComponentSection<CameraComponent>(
        "Camera", selected_entity, [&](CameraComponent& camera_component) {
            bool changed = false;
            changed |= ImGui::Checkbox("Primary", &camera_component.primary);
            changed |= ImGui::Checkbox("Fixed Aspect Ratio", &camera_component.fixedAspectRatio);

            const char* projection_label = camera_component.projectionType == Camera::ProjectionType::Orthographic
                                               ? "Orthographic"
                                               : "Perspective";
            if (ImGui::BeginCombo("Projection", projection_label)) {
                if (ImGui::Selectable("Perspective",
                                      camera_component.projectionType == Camera::ProjectionType::Perspective)) {
                    if (camera_component.projectionType != Camera::ProjectionType::Perspective) {
                        camera_component.projectionType = Camera::ProjectionType::Perspective;
                        changed = true;
                    }
                }
                if (ImGui::Selectable("Orthographic",
                                      camera_component.projectionType == Camera::ProjectionType::Orthographic)) {
                    if (camera_component.projectionType != Camera::ProjectionType::Orthographic) {
                        camera_component.projectionType = Camera::ProjectionType::Orthographic;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }

            if (camera_component.projectionType == Camera::ProjectionType::Perspective) {
                float fov_degrees = glm::degrees(camera_component.perspectiveVerticalFovRadians);
                if (ImGui::DragFloat("Vertical FOV", &fov_degrees, 0.25f, 1.0f, 179.0f, "%.2f deg")) {
                    camera_component.perspectiveVerticalFovRadians = glm::radians(fov_degrees);
                    changed = true;
                }
                changed |= ImGui::DragFloat("Near", &camera_component.perspectiveNear, 0.01f, 0.001f, 1000.0f);
                changed |= ImGui::DragFloat("Far", &camera_component.perspectiveFar, 1.0f, 0.001f, 10000.0f);
            } else {
                changed |= ImGui::DragFloat("Size", &camera_component.orthographicSize, 0.1f, 0.001f, 10000.0f);
                changed |= ImGui::DragFloat("Near", &camera_component.orthographicNear, 0.1f, -10000.0f, 10000.0f);
                changed |= ImGui::DragFloat("Far", &camera_component.orthographicFar, 0.1f, -10000.0f, 10000.0f);
            }
            return changed;
        },
        true);

    scene_changed |= editor::ui::drawComponentSection<LightComponent>(
        "Light", selected_entity, [](LightComponent& light_component) {
            bool changed = false;
            changed |= ImGui::Checkbox("Enabled", &light_component.enabled);

            const char* type_label = "Directional";
            if (light_component.type == LightComponent::Type::Point) {
                type_label = "Point";
            } else if (light_component.type == LightComponent::Type::Spot) {
                type_label = "Spot";
            }
            if (ImGui::BeginCombo("Type", type_label)) {
                if (ImGui::Selectable("Directional", light_component.type == LightComponent::Type::Directional)) {
                    if (light_component.type != LightComponent::Type::Directional) {
                        light_component.type = LightComponent::Type::Directional;
                        changed = true;
                    }
                }
                if (ImGui::Selectable("Point", light_component.type == LightComponent::Type::Point)) {
                    if (light_component.type != LightComponent::Type::Point) {
                        light_component.type = LightComponent::Type::Point;
                        changed = true;
                    }
                }
                if (ImGui::Selectable("Spot", light_component.type == LightComponent::Type::Spot)) {
                    if (light_component.type != LightComponent::Type::Spot) {
                        light_component.type = LightComponent::Type::Spot;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }

            changed |= ImGui::ColorEdit3("Color", &light_component.color.x);
            changed |= ImGui::DragFloat("Intensity", &light_component.intensity, 0.05f, 0.0f, 100.0f, "%.2f");
            if (light_component.type == LightComponent::Type::Point || light_component.type == LightComponent::Type::Spot) {
                changed |= ImGui::DragFloat("Range", &light_component.range, 0.1f, 0.001f, 1000.0f, "%.2f");
            }
            if (light_component.type == LightComponent::Type::Spot) {
                glm::vec2 cone_angles = glm::degrees(glm::vec2(light_component.innerConeAngleRadians,
                                                               light_component.outerConeAngleRadians));
                if (ImGui::DragFloat2("Cone Angles", &cone_angles.x, 0.5f, 0.1f, 89.0f, "%.1f")) {
                    cone_angles.x = (std::clamp)(cone_angles.x, 0.1f, 89.0f);
                    cone_angles.y = (std::clamp)(cone_angles.y, cone_angles.x, 89.0f);
                    light_component.innerConeAngleRadians = glm::radians(cone_angles.x);
                    light_component.outerConeAngleRadians = glm::radians(cone_angles.y);
                    changed = true;
                }
            }
            if (light_component.type == LightComponent::Type::Directional) {
                ImGui::TextDisabled("Directional light uses -entity forward as light direction.");
            } else if (light_component.type == LightComponent::Type::Spot) {
                ImGui::TextDisabled("Spot light emits along entity forward direction.");
            } else {
                ImGui::TextDisabled("Point light uses entity world position.");
            }
            return changed;
        },
        true);

    scene_changed |= editor::ui::drawComponentSection<MeshComponent>(
        "Mesh", selected_entity, [&](MeshComponent& mesh_component) {
            bool changed = false;
            const AssetHandle previous_mesh_handle = mesh_component.meshHandle;
            const std::string current_builtin_label = BuiltinAssets::isBuiltinMesh(mesh_component.meshHandle)
                                                        ? BuiltinAssets::getDisplayName(mesh_component.meshHandle)
                                                        : "None";
            if (ImGui::BeginCombo("Primitive", current_builtin_label.c_str())) {
                if (ImGui::Selectable("None", !BuiltinAssets::isBuiltinMesh(mesh_component.meshHandle))) {
                    if (mesh_component.meshHandle != AssetHandle(0)) {
                        mesh_component.meshHandle = AssetHandle(0);
                        changed = true;
                    }
                }

                for (const auto& mesh : BuiltinAssets::getBuiltinMeshes()) {
                    const bool selected = mesh_component.meshHandle == mesh.Handle;
                    if (ImGui::Selectable(mesh.Name, selected)) {
                        if (mesh_component.meshHandle != mesh.Handle) {
                            mesh_component.meshHandle = mesh.Handle;
                            changed = true;
                        }
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            changed |= editor::ui::drawAssetHandleEditor("Mesh Handle", mesh_component.meshHandle, {AssetType::Mesh});

            if (mesh_component.meshHandle != previous_mesh_handle) {
                mesh_component.clearAllSubmeshMaterials();
                syncMeshMaterialSlots(mesh_component);
                assignDefaultMaterialSlots(mesh_component);
                changed = true;
            }

            const auto mesh = mesh_component.meshHandle.isValid()
                                  ? AssetManager::get().requestAssetAs<Mesh>(mesh_component.meshHandle)
                                  : std::shared_ptr<Mesh>{};
            const bool mesh_loaded = mesh && mesh->isValid();

            if (mesh_loaded) {
                ImGui::Text("Submeshes: %zu", mesh->getSubMeshes().size());
                if (ImGui::Button("Sync Material Slots To Mesh", ImVec2(-1.0f, 0.0f))) {
                    const size_t previous_slot_count = mesh_component.getSubmeshMaterialCount();
                    mesh_component.resizeSubmeshMaterials(mesh->getSubMeshes().size());
                    changed |= mesh_component.getSubmeshMaterialCount() != previous_slot_count;
                }
            } else {
                if (mesh_component.meshHandle.isValid() && AssetManager::get().isAssetLoading(mesh_component.meshHandle)) {
                    ImGui::TextDisabled("Mesh asset is loading...");
                }
                int slot_count = static_cast<int>(mesh_component.getSubmeshMaterialCount());
                if (ImGui::InputInt("Material Slot Count", &slot_count)) {
                    slot_count = (std::max)(slot_count, 0);
                    const size_t requested_slot_count = static_cast<size_t>(slot_count);
                    if (mesh_component.getSubmeshMaterialCount() != requested_slot_count) {
                        mesh_component.resizeSubmeshMaterials(requested_slot_count);
                        changed = true;
                    }
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

                    changed |= editor::ui::drawAssetHandleEditor(
                        "Material Handle", material_handle, {AssetType::Material});
                    if (mesh_component.getSubmeshMaterial(submesh_index) != material_handle) {
                        mesh_component.setSubmeshMaterial(submesh_index, material_handle);
                        changed = true;
                    }
                    if (BuiltinAssets::isBuiltinMaterial(material_handle)) {
                        ImGui::TextDisabled("Global builtin material; edits affect all users.");
                        if (ImGui::Button("Edit Builtin Material", ImVec2(-1.0f, 0.0f))) {
                            m_editor_context->openBuiltinMaterialsPanel(material_handle);
                        }
                    }

                    if (ImGui::Button("Clear Material", ImVec2(-1.0f, 0.0f))) {
                        if (mesh_component.getSubmeshMaterial(submesh_index).isValid()) {
                            mesh_component.clearSubmeshMaterial(submesh_index);
                            changed = true;
                        }
                    }

                    ImGui::Separator();
                    ImGui::PopID();
                }

                if (ImGui::Button("Clear All Materials", ImVec2(-1.0f, 0.0f))) {
                    if (mesh_component.getSubmeshMaterialCount() > 0) {
                        mesh_component.clearAllSubmeshMaterials();
                        changed = true;
                    }
                }
            }
            return changed;
        },
        true);

    scene_changed |= editor::ui::drawComponentSection<ScriptComponent>(
        "Script", selected_entity, [this, selected_entity, editing_runtime_scene](ScriptComponent& script_component) {
            const ScriptComponentInspectorChange change = drawScriptComponentInspector(selected_entity, script_component);
            if (editing_runtime_scene && change.property_value_changed) {
                m_editor_context->patchRuntimeScriptProperty(selected_entity.getUUID(),
                                                           change.script_index,
                                                           change.property_index);
            }
            return change.changed;
        },
        true);

    if (scene_changed && !editing_runtime_scene) {
        m_editor_context->markSceneDirty();
    }

    ImGui::End();
}

} // namespace luna
