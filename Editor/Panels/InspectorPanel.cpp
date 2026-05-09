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
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <string>
#include <utility>

namespace {

const luna::editor::ui::PropertyLayout kInspectorHeaderLayout{96.0f};

void syncMeshMaterialSlots(luna::MeshComponent& mesh_component)
{
    if (!mesh_component.meshHandle.isValid()) {
        return;
    }

    const auto mesh = luna::AssetManager::get().requestAssetAs<luna::Mesh>(mesh_component.meshHandle);
    if (!mesh || !mesh->isValid()) {
        return;
    }

    mesh_component.resizeSubmeshMaterials(mesh_component.resolveSubmeshCount(mesh->getSubMeshes().size()));
}

void assignDefaultMaterialSlots(luna::MeshComponent& mesh_component)
{
    for (uint32_t submesh_index = 0; submesh_index < mesh_component.getSubmeshMaterialCount(); ++submesh_index) {
        if (!mesh_component.getSubmeshMaterial(submesh_index).isValid()) {
            mesh_component.setSubmeshMaterial(submesh_index, luna::BuiltinMaterials::DefaultLit);
        }
    }
}

bool addComponent(luna::EditorContext& editor_context,
                  luna::Entity selected_entity,
                  luna::authoring::AuthoringComponentKind component_kind,
                  bool use_authoring)
{
    if (use_authoring) {
        return editor_context.addComponent(selected_entity, component_kind);
    }

    switch (component_kind) {
        case luna::authoring::AuthoringComponentKind::Mesh:
            if (!selected_entity.hasComponent<luna::MeshComponent>()) {
                selected_entity.addComponent<luna::MeshComponent>();
                return true;
            }
            break;
        case luna::authoring::AuthoringComponentKind::Camera:
            if (!selected_entity.hasComponent<luna::CameraComponent>()) {
                selected_entity.addComponent<luna::CameraComponent>();
                return true;
            }
            break;
        case luna::authoring::AuthoringComponentKind::Light:
            if (!selected_entity.hasComponent<luna::LightComponent>()) {
                selected_entity.addComponent<luna::LightComponent>();
                return true;
            }
            break;
        case luna::authoring::AuthoringComponentKind::Script:
            if (!selected_entity.hasComponent<luna::ScriptComponent>()) {
                selected_entity.addComponent<luna::ScriptComponent>();
                return true;
            }
            break;
    }

    return false;
}

bool drawAddComponentPopup(luna::EditorContext& editor_context, luna::Entity selected_entity, bool use_authoring)
{
    bool changed = false;
    if (ImGui::BeginPopup("AddComponentPopup")) {
        if (!selected_entity.hasComponent<luna::MeshComponent>() && ImGui::MenuItem("Mesh")) {
            changed |= addComponent(editor_context,
                                    selected_entity,
                                    luna::authoring::AuthoringComponentKind::Mesh,
                                    use_authoring);
            ImGui::CloseCurrentPopup();
        }

        if (!selected_entity.hasComponent<luna::CameraComponent>() && ImGui::MenuItem("Camera")) {
            changed |= addComponent(editor_context,
                                    selected_entity,
                                    luna::authoring::AuthoringComponentKind::Camera,
                                    use_authoring);
            ImGui::CloseCurrentPopup();
        }

        if (!selected_entity.hasComponent<luna::LightComponent>() && ImGui::MenuItem("Light")) {
            changed |= addComponent(editor_context,
                                    selected_entity,
                                    luna::authoring::AuthoringComponentKind::Light,
                                    use_authoring);
            ImGui::CloseCurrentPopup();
        }

        if (!selected_entity.hasComponent<luna::ScriptComponent>() && ImGui::MenuItem("Script")) {
            changed |= addComponent(editor_context,
                                    selected_entity,
                                    luna::authoring::AuthoringComponentKind::Script,
                                    use_authoring);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return changed;
}

bool drawEntityHeader(luna::EditorContext& editor_context, luna::Entity selected_entity, bool use_authoring)
{
    bool changed = false;

    ImGui::TextDisabled("Entity");
    auto& tag_component = selected_entity.getComponent<luna::TagComponent>();
    if (use_authoring) {
        std::string name = tag_component.tag;
        if (luna::editor::ui::drawTextInput("Name", name, 256, kInspectorHeaderLayout)) {
            changed |= editor_context.setEntityName(selected_entity, std::move(name));
        }
    } else {
        changed |= luna::editor::ui::drawTextInput("Name", tag_component.tag, 256, kInspectorHeaderLayout);
    }
    luna::editor::ui::drawTextValue("UUID", selected_entity.getUUID().toString(), kInspectorHeaderLayout);

    if (luna::editor::ui::drawButton("Add Component", luna::editor::ui::ButtonVariant::Subtle, ImVec2(-1.0f, 0.0f))) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    changed |= drawAddComponentPopup(editor_context, selected_entity, use_authoring);
    ImGui::Spacing();
    ImGui::Separator();
    return changed;
}

template <typename T, typename UIFunction, typename CommitFunction, typename RemoveFunction>
bool drawInspectorComponentSection(const char* label,
                                   luna::Entity entity,
                                   UIFunction&& ui_function,
                                   bool allow_remove,
                                   bool use_authoring,
                                   CommitFunction&& commit_component,
                                   RemoveFunction&& remove_component)
{
    if (!entity.hasComponent<T>()) {
        return false;
    }

    if (!use_authoring) {
        return luna::editor::ui::drawComponentSection<T>(
            label, entity, std::forward<UIFunction>(ui_function), allow_remove);
    }

    T component_draft = entity.getComponent<T>();
    bool changed = false;
    ImGui::PushID(label);

    const bool open = luna::editor::ui::beginSection(label);

    bool remove_requested = false;
    if (allow_remove && ImGui::BeginPopupContextItem("ComponentSettings")) {
        if (ImGui::MenuItem("Remove Component")) {
            remove_requested = true;
        }
        ImGui::EndPopup();
    }

    if (open) {
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, luna::editor::ui::scale(10.0f));
        changed |= ui_function(component_draft);
        ImGui::PopStyleVar();
        luna::editor::ui::endSection();
    }

    bool committed = false;
    if (remove_requested) {
        committed = remove_component();
    } else if (changed) {
        committed = commit_component(component_draft);
    }

    ImGui::PopID();
    return committed || changed || remove_requested;
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

    ImGui::SetNextWindowSize(editor::ui::scaled(380.0f, 520.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Inspector");
    editor::ui::pushCompactInspectorStyle();

    const bool editing_runtime_scene = m_editor_context->isRuntimeViewportEnabled();
    const bool use_authoring = !editing_runtime_scene;
    bool scene_changed = false;

    Entity selected_entity = m_editor_context->getSelectedEntity();
    if (selected_entity && !selected_entity.isValid()) {
        m_editor_context->setSelectedEntity({});
        selected_entity = {};
    }

    if (!selected_entity) {
        ImGui::TextUnformatted("Select an entity to inspect.");
        editor::ui::popCompactInspectorStyle();
        ImGui::End();
        return;
    }

    scene_changed |= drawEntityHeader(*m_editor_context, selected_entity, use_authoring);

    scene_changed |= drawInspectorComponentSection<TransformComponent>(
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
        false,
        use_authoring,
        [&](const TransformComponent& transform) {
            return m_editor_context->setEntityTransform(selected_entity, transform);
        },
        []() {
            return false;
        });

    scene_changed |= editor::ui::drawComponentSection<RelationshipComponent>(
        "Relationship", selected_entity, [&](RelationshipComponent&) {
            bool changed = false;
            Entity parent = selected_entity.getParent();
            if (parent) {
                if (editor::ui::drawButton(parent.getName().c_str(),
                                           editor::ui::ButtonVariant::Subtle,
                                           ImVec2(-1.0f, 0.0f))) {
                    m_editor_context->setSelectedEntity(parent);
                }
                if (ImGui::BeginPopupContextItem("ParentContext")) {
                    if (ImGui::MenuItem("Detach From Parent")) {
                        if (use_authoring) {
                            changed |= m_editor_context->reparentEntity(selected_entity, {}, true);
                        } else {
                            changed |= selected_entity.clearParent(true);
                        }
                    }
                    ImGui::EndPopup();
                }
                editor::ui::drawTextValue("Parent UUID", parent.getUUID().toString(), kInspectorHeaderLayout);
            } else {
                editor::ui::drawTextValue("Parent", "None", kInspectorHeaderLayout);
            }

            ImGui::SeparatorText("Children");
            if (!selected_entity.hasChildren()) {
                ImGui::TextUnformatted("No child entities.");
            } else {
                for (const UUID child_uuid : selected_entity.getChildren()) {
                    luna::Entity child = selected_entity.getEntityManager()->findEntityByUUID(child_uuid);
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

    scene_changed |= drawInspectorComponentSection<CameraComponent>(
        "Camera", selected_entity, [&](CameraComponent& camera_component) {
            bool changed = false;
            changed |= editor::ui::drawBool("Primary", camera_component.primary);
            changed |= editor::ui::drawBool("Fixed Aspect", camera_component.fixedAspectRatio);

            const char* projection_label = camera_component.projectionType == Camera::ProjectionType::Orthographic
                                               ? "Orthographic"
                                               : "Perspective";
            changed |= editor::ui::drawCombo("Projection", projection_label, [&]() {
                bool projection_changed = false;
                if (ImGui::Selectable("Perspective",
                                      camera_component.projectionType == Camera::ProjectionType::Perspective)) {
                    if (camera_component.projectionType != Camera::ProjectionType::Perspective) {
                        camera_component.projectionType = Camera::ProjectionType::Perspective;
                        projection_changed = true;
                    }
                }
                if (ImGui::Selectable("Orthographic",
                                      camera_component.projectionType == Camera::ProjectionType::Orthographic)) {
                    if (camera_component.projectionType != Camera::ProjectionType::Orthographic) {
                        camera_component.projectionType = Camera::ProjectionType::Orthographic;
                        projection_changed = true;
                    }
                }
                return projection_changed;
            });

            if (camera_component.projectionType == Camera::ProjectionType::Perspective) {
                float fov_degrees = glm::degrees(camera_component.perspectiveVerticalFovRadians);
                if (editor::ui::drawFloat("Vertical FOV", fov_degrees, 0.25f, 1.0f, 179.0f, "%.2f deg")) {
                    camera_component.perspectiveVerticalFovRadians = glm::radians(fov_degrees);
                    changed = true;
                }
                changed |= editor::ui::drawFloat("Near", camera_component.perspectiveNear, 0.01f, 0.001f, 1000.0f);
                changed |= editor::ui::drawFloat("Far", camera_component.perspectiveFar, 1.0f, 0.001f, 10000.0f);
            } else {
                changed |= editor::ui::drawFloat("Size", camera_component.orthographicSize, 0.1f, 0.001f, 10000.0f);
                changed |=
                    editor::ui::drawFloat("Near", camera_component.orthographicNear, 0.1f, -10000.0f, 10000.0f);
                changed |=
                    editor::ui::drawFloat("Far", camera_component.orthographicFar, 0.1f, -10000.0f, 10000.0f);
            }
            return changed;
        },
        true,
        use_authoring,
        [&](const CameraComponent& camera_component) {
            return m_editor_context->setCameraComponent(selected_entity, camera_component);
        },
        [&]() {
            return m_editor_context->removeComponent(selected_entity, authoring::AuthoringComponentKind::Camera);
        });

    scene_changed |= drawInspectorComponentSection<LightComponent>(
        "Light", selected_entity, [](LightComponent& light_component) {
            bool changed = false;
            changed |= editor::ui::drawBool("Enabled", light_component.enabled);

            const char* type_label = "Directional";
            if (light_component.type == LightComponent::Type::Point) {
                type_label = "Point";
            } else if (light_component.type == LightComponent::Type::Spot) {
                type_label = "Spot";
            }
            changed |= editor::ui::drawCombo("Type", type_label, [&]() {
                bool type_changed = false;
                if (ImGui::Selectable("Directional", light_component.type == LightComponent::Type::Directional)) {
                    if (light_component.type != LightComponent::Type::Directional) {
                        light_component.type = LightComponent::Type::Directional;
                        type_changed = true;
                    }
                }
                if (ImGui::Selectable("Point", light_component.type == LightComponent::Type::Point)) {
                    if (light_component.type != LightComponent::Type::Point) {
                        light_component.type = LightComponent::Type::Point;
                        type_changed = true;
                    }
                }
                if (ImGui::Selectable("Spot", light_component.type == LightComponent::Type::Spot)) {
                    if (light_component.type != LightComponent::Type::Spot) {
                        light_component.type = LightComponent::Type::Spot;
                        type_changed = true;
                    }
                }
                return type_changed;
            });

            changed |= editor::ui::drawColor3("Color", light_component.color);
            changed |= editor::ui::drawFloat("Intensity", light_component.intensity, 0.05f, 0.0f, 100.0f, "%.2f");
            if (light_component.type == LightComponent::Type::Point || light_component.type == LightComponent::Type::Spot) {
                changed |= editor::ui::drawFloat("Range", light_component.range, 0.1f, 0.001f, 1000.0f, "%.2f");
            }
            if (light_component.type == LightComponent::Type::Spot) {
                glm::vec2 cone_angles = glm::degrees(glm::vec2(light_component.innerConeAngleRadians,
                                                               light_component.outerConeAngleRadians));
                if (editor::ui::drawVec2Control("Cone Angles", cone_angles, 0.5f, 0.1f, 89.0f, "%.1f")) {
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
        true,
        use_authoring,
        [&](const LightComponent& light_component) {
            return m_editor_context->setLightComponent(selected_entity, light_component);
        },
        [&]() {
            return m_editor_context->removeComponent(selected_entity, authoring::AuthoringComponentKind::Light);
        });

    scene_changed |= drawInspectorComponentSection<MeshComponent>(
        "Mesh", selected_entity, [&](MeshComponent& mesh_component) {
            bool changed = false;
            const AssetHandle previous_mesh_handle = mesh_component.meshHandle;
            const std::string current_builtin_label = BuiltinAssets::isBuiltinMesh(mesh_component.meshHandle)
                                                        ? BuiltinAssets::getDisplayName(mesh_component.meshHandle)
                                                        : "None";
            changed |= editor::ui::drawCombo("Primitive", current_builtin_label.c_str(), [&]() {
                bool primitive_changed = false;
                if (ImGui::Selectable("None", !BuiltinAssets::isBuiltinMesh(mesh_component.meshHandle))) {
                    if (mesh_component.meshHandle != AssetHandle(0)) {
                        mesh_component.meshHandle = AssetHandle(0);
                        primitive_changed = true;
                    }
                }

                for (const auto& mesh : BuiltinAssets::getBuiltinMeshes()) {
                    const bool selected = mesh_component.meshHandle == mesh.Handle;
                    if (ImGui::Selectable(mesh.Name, selected)) {
                        if (mesh_component.meshHandle != mesh.Handle) {
                            mesh_component.meshHandle = mesh.Handle;
                            primitive_changed = true;
                        }
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                return primitive_changed;
            });

            changed |= editor::ui::drawAssetHandleSelector("Mesh Asset", mesh_component.meshHandle, {AssetType::Mesh});

            if (mesh_component.meshHandle != previous_mesh_handle) {
                mesh_component.resetSubmeshRange();
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
                const size_t active_submesh_count = mesh_component.resolveSubmeshCount(mesh->getSubMeshes().size());
                editor::ui::drawTextValue("Submeshes",
                                          std::to_string(active_submesh_count) + " / " +
                                              std::to_string(mesh->getSubMeshes().size()));
            } else {
                if (mesh_component.meshHandle.isValid() && AssetManager::get().isAssetLoading(mesh_component.meshHandle)) {
                    ImGui::TextDisabled("Mesh asset is loading...");
                }
                int slot_count = static_cast<int>(mesh_component.getSubmeshMaterialCount());
                if (editor::ui::drawInt("Material Slots", slot_count)) {
                    slot_count = (std::max)(slot_count, 0);
                    const size_t requested_slot_count = static_cast<size_t>(slot_count);
                    if (mesh_component.getSubmeshMaterialCount() != requested_slot_count) {
                        mesh_component.resizeSubmeshMaterials(requested_slot_count);
                        changed = true;
                    }
                }
            }

            ImGui::SeparatorText("Submesh Materials");
            bool sync_material_slots = false;
            if (ImGui::BeginPopupContextItem("SubmeshMaterialsContext")) {
                if (ImGui::MenuItem("Sync Material Slots To Mesh", nullptr, false, mesh_loaded)) {
                    sync_material_slots = true;
                }
                if (ImGui::MenuItem("Clear All Materials", nullptr, false, mesh_component.getSubmeshMaterialCount() > 0)) {
                    mesh_component.clearAllSubmeshMaterials();
                    changed = true;
                }
                ImGui::EndPopup();
            }
            if (sync_material_slots && mesh_loaded) {
                const size_t previous_slot_count = mesh_component.getSubmeshMaterialCount();
                mesh_component.resizeSubmeshMaterials(mesh_component.resolveSubmeshCount(mesh->getSubMeshes().size()));
                changed |= mesh_component.getSubmeshMaterialCount() != previous_slot_count;
            }
            if (mesh_component.getSubmeshMaterialCount() == 0) {
                ImGui::TextUnformatted("No material slots.");
            } else {
                for (uint32_t submesh_index = 0;
                     submesh_index < static_cast<uint32_t>(mesh_component.getSubmeshMaterialCount());
                     ++submesh_index) {
                    ImGui::PushID(static_cast<int>(submesh_index));
                    ImGui::Text("Submesh %u", submesh_index);

                    AssetHandle material_handle = mesh_component.getSubmeshMaterial(submesh_index);
                    bool clear_material = false;
                    bool edit_builtin_material = false;
                    if (ImGui::BeginPopupContextItem("SubmeshMaterialContext")) {
                        if (ImGui::MenuItem("Edit Builtin Material",
                                            nullptr,
                                            false,
                                            BuiltinAssets::isBuiltinMaterial(material_handle))) {
                            edit_builtin_material = true;
                        }
                        if (ImGui::MenuItem("Clear Material", nullptr, false, material_handle.isValid())) {
                            clear_material = true;
                        }
                        if (ImGui::MenuItem("Clear All Materials",
                                            nullptr,
                                            false,
                                            mesh_component.getSubmeshMaterialCount() > 0)) {
                            mesh_component.clearAllSubmeshMaterials();
                            material_handle = AssetHandle(0);
                            changed = true;
                        }
                        ImGui::EndPopup();
                    }
                    if (edit_builtin_material && BuiltinAssets::isBuiltinMaterial(material_handle)) {
                        m_editor_context->openBuiltinMaterialsPanel(material_handle);
                    }
                    const std::string current_builtin_material_label = BuiltinAssets::isBuiltinMaterial(material_handle)
                                                                     ? BuiltinAssets::getDisplayName(material_handle)
                                                                     : "None";
                    changed |= editor::ui::drawCombo("Builtin Material", current_builtin_material_label.c_str(), [&]() {
                        bool material_changed = false;
                        if (ImGui::Selectable("None", !BuiltinAssets::isBuiltinMaterial(material_handle))) {
                            if (material_handle != AssetHandle(0)) {
                                material_handle = AssetHandle(0);
                                material_changed = true;
                            }
                        }

                        for (const auto& material : BuiltinAssets::getBuiltinMaterials()) {
                            const bool selected = material_handle == material.Handle;
                            if (ImGui::Selectable(material.Name, selected)) {
                                if (material_handle != material.Handle) {
                                    material_handle = material.Handle;
                                    material_changed = true;
                                }
                            }
                            if (selected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        return material_changed;
                    });

                    changed |= editor::ui::drawAssetHandleSelector(
                        "Material Asset", material_handle, {AssetType::Material});
                    if (mesh_component.getSubmeshMaterial(submesh_index) != material_handle) {
                        mesh_component.setSubmeshMaterial(submesh_index, material_handle);
                        changed = true;
                    }
                    if (BuiltinAssets::isBuiltinMaterial(material_handle)) {
                        ImGui::TextDisabled("Global builtin material; edits affect all users.");
                    }

                    if (clear_material) {
                        if (mesh_component.getSubmeshMaterial(submesh_index).isValid()) {
                            mesh_component.clearSubmeshMaterial(submesh_index);
                            changed = true;
                        }
                    }

                    ImGui::Separator();
                    ImGui::PopID();
                }
            }
            return changed;
        },
        true,
        use_authoring,
        [&](const MeshComponent& mesh_component) {
            return m_editor_context->setMeshComponent(selected_entity, mesh_component);
        },
        [&]() {
            return m_editor_context->removeComponent(selected_entity, authoring::AuthoringComponentKind::Mesh);
        });

    scene_changed |= drawInspectorComponentSection<ScriptComponent>(
        "Script", selected_entity, [this, selected_entity, editing_runtime_scene](ScriptComponent& script_component) {
            const ScriptComponentInspectorChange change = drawScriptComponentInspector(selected_entity, script_component);
            if (editing_runtime_scene && change.property_value_changed) {
                m_editor_context->patchRuntimeScriptProperty(selected_entity.getUUID(),
                                                           change.script_index,
                                                           change.property_index);
            }
            return change.changed;
        },
        true,
        use_authoring,
        [&](const ScriptComponent& script_component) {
            return m_editor_context->setScriptComponent(selected_entity, script_component);
        },
        [&]() {
            return m_editor_context->removeComponent(selected_entity, authoring::AuthoringComponentKind::Script);
        });

    editor::ui::popCompactInspectorStyle();
    ImGui::End();
}

} // namespace luna
