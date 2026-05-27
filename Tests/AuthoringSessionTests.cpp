#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Asset/Editor/ModelLoader.h"
#include "Asset/Model.h"
#include "Authoring/AuthoringSession.h"
#include "Core/Log.h"
#include "Renderer/Mesh.h"
#include "Scene/Components.h"
#include "Scene/SceneSerializer.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

class TempDirectory {
public:
    explicit TempDirectory(std::string_view name)
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path() / ("Luna-" + std::string(name) + "-" + std::to_string(now));
        std::filesystem::create_directories(m_path);
    }

    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;

    ~TempDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(m_path, ec);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept
    {
        return m_path;
    }

private:
    std::filesystem::path m_path;
};

class TestContext {
public:
    bool expect(bool condition, std::string_view message)
    {
        if (condition) {
            return true;
        }

        ++m_failures;
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }

    [[nodiscard]] int result() const noexcept
    {
        return m_failures == 0 ? 0 : 1;
    }

private:
    int m_failures{0};
};

bool sameVec3(const glm::vec3& lhs, const glm::vec3& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool hasEventType(const std::vector<luna::authoring::AuthoringEvent>& events, luna::authoring::AuthoringEventType type)
{
    return std::any_of(events.begin(), events.end(), [type](const auto& event) {
        return event.type == type;
    });
}

size_t countEventType(const std::vector<luna::authoring::AuthoringEvent>& events,
                      luna::authoring::AuthoringEventType type)
{
    return static_cast<size_t>(std::count_if(events.begin(), events.end(), [type](const auto& event) {
        return event.type == type;
    }));
}

void testAuthoringSessionSceneLifecycle(TestContext& context)
{
    TempDirectory temp("AuthoringSession");
    const std::filesystem::path scene_path = temp.path() / "Scenes" / "AuthoringSessionSample";
    const std::filesystem::path normalized_scene_path = luna::SceneSerializer::normalizeScenePath(scene_path);

    luna::Scene scene;
    luna::authoring::AuthoringSession session(scene);

    const auto bootstrap = session.createScene();
    context.expect(bootstrap.camera && bootstrap.directional_light,
                   "scene bootstrap should create camera and directional light");
    context.expect(scene.entityManager().entityCount() == 2, "bootstrap scene should contain two entities");
    context.expect(!session.isSceneDirty(), "bootstrap scene should end clean");
    context.expect(scene.getName() == "Untitled", "bootstrap scene should keep Untitled name before save");

    const auto bootstrap_events = session.consumeEvents();
    context.expect(hasEventType(bootstrap_events, luna::authoring::AuthoringEventType::SceneReset),
                   "bootstrap should emit scene reset");
    context.expect(countEventType(bootstrap_events, luna::authoring::AuthoringEventType::EntityCreated) == 2,
                   "bootstrap should emit two entity created events");
    context.expect(hasEventType(bootstrap_events, luna::authoring::AuthoringEventType::SceneCreated),
                   "bootstrap should emit scene created");

    const auto point_light = session.createPointLightEntity();
    const auto spot_light = session.createSpotLightEntity();
    context.expect(point_light && spot_light, "point and spot light helpers should create entities");
    context.expect(scene.entityManager().entityCount() == 4, "scene should contain four entities after helpers");
    context.expect(session.isSceneDirty(), "creating additional entities should dirty the scene");

    const auto helper_events = session.consumeEvents();
    context.expect(countEventType(helper_events, luna::authoring::AuthoringEventType::EntityCreated) == 2,
                   "light helpers should emit entity created events");

    context.expect(session.reparentEntity(spot_light, point_light, false),
                   "reparent helper should move an entity under a new parent");
    context.expect(spot_light.getParentUUID() == point_light.getUUID(), "spot light should remember the new parent");

    luna::SceneEnvironmentSettings environment_settings = scene.environmentSettings();
    environment_settings.backgroundMode = luna::SceneBackgroundMode::SolidColor;
    environment_settings.enabled = false;
    environment_settings.backgroundColor = {0.25f, 0.30f, 0.35f};
    context.expect(session.setSceneEnvironmentSettings(environment_settings),
                   "setting scene environment should report a change");

    luna::SceneShadowSettings shadow_settings = scene.shadowSettings();
    shadow_settings.mode = luna::SceneShadowMode::None;
    context.expect(session.setSceneShadowSettings(shadow_settings), "setting scene shadows should report a change");

    context.expect(session.setEntityName(point_light, "Key Point Light"), "entity rename should report a change");
    luna::TransformComponent point_transform = point_light.transform();
    point_transform.translation = {1.0f, 2.0f, 3.0f};
    context.expect(session.setEntityTransform(point_light, point_transform), "transform update should report a change");

    luna::CameraComponent camera_component = bootstrap.camera.getComponent<luna::CameraComponent>();
    camera_component.primary = false;
    context.expect(session.setCameraComponent(bootstrap.camera, camera_component),
                   "camera component update should report a change");

    luna::LightComponent light_component = point_light.getComponent<luna::LightComponent>();
    light_component.intensity = 25.0f;
    context.expect(session.setLightComponent(point_light, light_component),
                   "light component update should report a change");

    context.expect(session.addComponent(point_light, luna::authoring::AuthoringComponentKind::Mesh),
                   "component add helper should add a mesh component");
    luna::MeshComponent mesh_component = point_light.getComponent<luna::MeshComponent>();
    mesh_component.resizeSubmeshMaterials(1);
    mesh_component.setSubmeshMaterial(0, luna::AssetHandle(123));
    context.expect(session.setMeshComponent(point_light, mesh_component),
                   "mesh component update should report a change");
    context.expect(session.removeComponent(point_light, luna::authoring::AuthoringComponentKind::Mesh),
                   "component remove helper should remove a mesh component");

    context.expect(session.addComponent(point_light, luna::authoring::AuthoringComponentKind::Script),
                   "component add helper should add a script component");
    luna::ScriptComponent script_component = point_light.getComponent<luna::ScriptComponent>();
    script_component.enabled = false;
    context.expect(session.setScriptComponent(point_light, script_component),
                   "script component update should report a change");
    context.expect(session.removeComponent(point_light, luna::authoring::AuthoringComponentKind::Script),
                   "component remove helper should remove a script component");

    const auto temporary_entity = session.createEntity("Temporary");
    luna::UUID temporary_entity_id(0);
    context.expect(temporary_entity, "generic entity helper should create an entity");
    if (temporary_entity) {
        temporary_entity_id = temporary_entity.getUUID();
    }
    context.expect(session.destroyEntity(temporary_entity), "destroy helper should remove an entity");
    context.expect(!scene.entityManager().containsEntity(temporary_entity_id),
                   "destroy helper should remove the entity UUID from the scene");

    const auto mutation_events = session.consumeEvents();
    context.expect(hasEventType(mutation_events, luna::authoring::AuthoringEventType::EntityReparented),
                   "reparenting should emit entity reparented");
    context.expect(countEventType(mutation_events, luna::authoring::AuthoringEventType::SceneSettingsChanged) == 2,
                   "environment and shadow edits should emit scene settings events");
    context.expect(hasEventType(mutation_events, luna::authoring::AuthoringEventType::ComponentAdded),
                   "component additions should emit component added");
    context.expect(hasEventType(mutation_events, luna::authoring::AuthoringEventType::ComponentRemoved),
                   "component removals should emit component removed");
    context.expect(hasEventType(mutation_events, luna::authoring::AuthoringEventType::EntityModified),
                   "component and property edits should emit entity modified");
    context.expect(hasEventType(mutation_events, luna::authoring::AuthoringEventType::EntityDestroyed),
                   "destroying should emit entity destroyed");

    context.expect(session.saveSceneAs(scene_path), "saving the scene should succeed");
    context.expect(session.sceneFilePath() == normalized_scene_path, "scene file path should normalize on save");
    context.expect(!session.isSceneDirty(), "saved scene should end clean");
    context.expect(scene.getName() == normalized_scene_path.stem().string(),
                   "saving should rename Untitled scene to the file stem");

    const auto save_events = session.consumeEvents();
    context.expect(hasEventType(save_events, luna::authoring::AuthoringEventType::SceneSaved),
                   "saving should emit scene saved");

    luna::Scene reloaded_scene;
    luna::authoring::AuthoringSession reloaded_session(reloaded_scene);
    context.expect(reloaded_session.openScene(normalized_scene_path), "reloading the saved scene should succeed");
    context.expect(reloaded_session.sceneFilePath() == normalized_scene_path,
                   "reloaded scene should remember its file path");
    context.expect(!reloaded_session.isSceneDirty(), "reloaded scene should be clean");
    context.expect(reloaded_scene.entityManager().entityCount() == 4, "reloaded scene should keep all entities");
    context.expect(reloaded_scene.environmentSettings().backgroundMode == luna::SceneBackgroundMode::SolidColor,
                   "reloaded scene should keep authored environment settings");
    context.expect(reloaded_scene.shadowSettings().mode == luna::SceneShadowMode::None,
                   "reloaded scene should keep authored shadow settings");

    const auto reloaded_camera = reloaded_scene.entityManager().findEntityByUUID(bootstrap.camera.getUUID());
    const auto reloaded_directional_light =
        reloaded_scene.entityManager().findEntityByUUID(bootstrap.directional_light.getUUID());
    const auto reloaded_point_light = reloaded_scene.entityManager().findEntityByUUID(point_light.getUUID());
    const auto reloaded_spot_light = reloaded_scene.entityManager().findEntityByUUID(spot_light.getUUID());

    context.expect(reloaded_camera && reloaded_directional_light && reloaded_point_light && reloaded_spot_light,
                   "all authored entities should survive a save/load round-trip");
    if (reloaded_camera) {
        context.expect(sameVec3(reloaded_camera.transform().translation, glm::vec3{0.0f, 1.0f, 6.0f}),
                       "camera transform should survive round-trip");
    }
    if (reloaded_directional_light) {
        context.expect(reloaded_directional_light.getComponent<luna::LightComponent>().type ==
                           luna::LightComponent::Type::Directional,
                       "directional light should keep its type");
    }
    if (reloaded_point_light) {
        context.expect(reloaded_point_light.getComponent<luna::LightComponent>().type ==
                           luna::LightComponent::Type::Point,
                       "point light should keep its type");
        context.expect(reloaded_point_light.getName() == "Key Point Light",
                       "point light should keep its authored name");
        context.expect(sameVec3(reloaded_point_light.transform().translation, glm::vec3{1.0f, 2.0f, 3.0f}),
                       "point light should keep its authored transform");
        context.expect(reloaded_point_light.getComponent<luna::LightComponent>().intensity == 25.0f,
                       "point light should keep its authored intensity");
    }
    if (reloaded_spot_light) {
        context.expect(reloaded_spot_light.getComponent<luna::LightComponent>().type ==
                           luna::LightComponent::Type::Spot,
                       "spot light should keep its type");
        context.expect(reloaded_spot_light.getParentUUID() == point_light.getUUID(),
                       "spot light should keep its authored parent");
    }

    const auto load_events = reloaded_session.consumeEvents();
    context.expect(hasEventType(load_events, luna::authoring::AuthoringEventType::SceneLoaded),
                   "opening should emit scene loaded");
}

void testAuthoringSessionHistory(TestContext& context)
{
    luna::Scene scene;
    luna::authoring::AuthoringSession session(scene);
    (void) session.createScene();
    (void) session.consumeEvents();

    const size_t bootstrap_count = scene.entityManager().entityCount();
    luna::Entity entity = session.createEntity("UndoBox");
    const luna::UUID entity_id = entity ? entity.getUUID() : luna::UUID(0);
    context.expect(entity, "history test should create an entity");
    context.expect(session.canUndo(), "creating an entity should add an undo step");
    context.expect(scene.entityManager().entityCount() == bootstrap_count + 1,
                   "created entity should increase entity count");

    context.expect(session.undo(), "undo should restore the scene before entity creation");
    context.expect(scene.entityManager().entityCount() == bootstrap_count, "undo should remove created entity");
    context.expect(!scene.entityManager().containsEntity(entity_id), "undo should remove created entity UUID");
    context.expect(session.canRedo(), "undo should enable redo");

    context.expect(session.redo(), "redo should restore entity creation");
    context.expect(scene.entityManager().entityCount() == bootstrap_count + 1,
                   "redo should restore created entity count");
    entity = scene.entityManager().findEntityByUUID(entity_id);
    context.expect(entity && entity.getName() == "UndoBox", "redo should restore created entity identity");

    luna::TransformComponent transform = entity.transform();
    transform.translation = {4.0f, 5.0f, 6.0f};
    context.expect(session.setEntityTransform(entity, transform), "transform edit should create a history step");
    context.expect(session.undo(), "undo should restore previous transform");
    entity = scene.entityManager().findEntityByUUID(entity_id);
    context.expect(entity && sameVec3(entity.transform().translation, glm::vec3{0.0f, 0.0f, 0.0f}),
                   "undo should restore transform translation");
    context.expect(session.redo(), "redo should reapply transform");
    entity = scene.entityManager().findEntityByUUID(entity_id);
    context.expect(entity && sameVec3(entity.transform().translation, glm::vec3{4.0f, 5.0f, 6.0f}),
                   "redo should restore transform translation");

    context.expect(session.beginTransaction("Batch Create"), "explicit transaction should begin");
    luna::Entity first = session.createEntity("BatchA");
    luna::Entity second = session.createEntity("BatchB");
    const luna::UUID first_id = first ? first.getUUID() : luna::UUID(0);
    const luna::UUID second_id = second ? second.getUUID() : luna::UUID(0);
    context.expect(first && second, "explicit transaction should allow multiple creates");
    context.expect(session.commitTransaction(), "explicit transaction should commit");
    context.expect(scene.entityManager().entityCount() == bootstrap_count + 3,
                   "batch transaction should add two entities");
    context.expect(session.undo(), "undo should revert the whole explicit transaction");
    context.expect(scene.entityManager().entityCount() == bootstrap_count + 1,
                   "batch undo should remove both batch entities");
    context.expect(!scene.entityManager().containsEntity(first_id) && !scene.entityManager().containsEntity(second_id),
                   "batch undo should remove both entity UUIDs");

    const bool can_undo_before_rollback = session.canUndo();
    context.expect(session.beginTransaction("Rollback Create"), "rollback transaction should begin");
    luna::Entity rollback_entity = session.createEntity("RollbackOnly");
    const luna::UUID rollback_entity_id = rollback_entity ? rollback_entity.getUUID() : luna::UUID(0);
    context.expect(rollback_entity, "rollback transaction should create temporary entity");
    context.expect(session.rollbackTransaction(), "rollback transaction should restore previous state");
    context.expect(!scene.entityManager().containsEntity(rollback_entity_id),
                   "rollback should remove temporary entity");
    context.expect(scene.entityManager().entityCount() == bootstrap_count + 1, "rollback should preserve entity count");
    context.expect(session.canUndo() == can_undo_before_rollback, "rollback should not add a new undo step");
}

void testModelHierarchyRoundTrip(TestContext& context)
{
    TempDirectory temp("ModelHierarchy");
    const std::filesystem::path model_path = temp.path() / "Models" / "Hierarchy.lmodel";
    std::filesystem::create_directories(model_path.parent_path());

    std::ofstream stream(model_path, std::ios::out | std::ios::trunc);
    context.expect(stream.is_open(), "hierarchy fixture should open for writing");
    if (!stream.is_open()) {
        return;
    }

    stream << R"(Model:
  Name: HierarchyModel
  Source: Models/Hierarchy.gltf
  SourceMesh: 9001
  Materials:
    - SourceMaterialIndex: 0
      Handle: 9101
      FilePath: Materials/0_HierarchyModel_Mat0.lunamat
    - SourceMaterialIndex: 1
      Handle: 9102
      FilePath: Materials/1_HierarchyModel_Mat1.lunamat
  Nodes:
    - Name: RootNode
      Parent: -1
      Translation: [0.0, 0.0, 0.0]
      Rotation: [0.0, 0.0, 0.0]
      Scale: [1.0, 1.0, 1.0]
      Mesh: 9001
      FirstSubmesh: 1
      SubmeshCount: 1
      SubmeshMaterials: [9102]
    - Name: ChildNode
      Parent: 0
      Translation: [1.0, 0.0, 0.0]
      Rotation: [0.0, 0.0, 0.0]
      Scale: [1.0, 1.0, 1.0]
    - Name: GrandChildNode
      Parent: 1
      Translation: [0.0, 1.0, 0.0]
      Rotation: [0.0, 0.0, 0.0]
      Scale: [1.0, 1.0, 1.0]
)";
    stream.close();

    luna::AssetDatabase::clear();
    luna::AssetManager::get().clear();

    const luna::AssetHandle mesh_handle(9'001);
    const luna::AssetHandle model_handle(9'002);

    std::vector<luna::SubMesh> sub_meshes;
    for (int submesh_index = 0; submesh_index < 2; ++submesh_index) {
        luna::SubMesh sub_mesh;
        sub_mesh.Name = "Submesh_" + std::to_string(submesh_index);
        sub_mesh.Vertices = {
            {{0.0f, 0.0f, 0.0f}},
            {{1.0f, 0.0f, 0.0f}},
            {{0.0f, 1.0f, 0.0f}},
        };
        sub_mesh.Indices = {0, 1, 2};
        sub_meshes.push_back(std::move(sub_mesh));
    }

    auto mesh = luna::Mesh::create("HierarchyMesh", std::move(sub_meshes));
    context.expect(mesh && mesh->isValid(), "test mesh should be valid");
    if (!mesh || !mesh->isValid()) {
        luna::AssetDatabase::clear();
        return;
    }

    auto model = luna::ModelLoader::loadFromFile(model_path, "HierarchyModel");
    context.expect(model && model->isValid(), "hierarchy model should load");
    if (!model || !model->isValid()) {
        luna::AssetDatabase::clear();
        return;
    }

    luna::AssetManager::get().registerMemoryAsset(mesh_handle, mesh);
    luna::AssetManager::get().registerMemoryAsset(model_handle, model);

    context.expect(luna::AssetDatabase::exists(mesh_handle), "test mesh should be registered in asset database");
    context.expect(luna::AssetDatabase::exists(model_handle), "test model should be registered in asset database");
    context.expect(luna::AssetDatabase::getAssetMetadata(mesh_handle).Type == luna::AssetType::Mesh,
                   "test mesh metadata should be typed as mesh");
    context.expect(luna::AssetDatabase::getAssetMetadata(model_handle).Type == luna::AssetType::Model,
                   "test model metadata should be typed as model");
    context.expect(static_cast<bool>(luna::AssetManager::get().loadAssetAs<luna::Mesh>(mesh_handle)),
                   "test mesh should be registered in asset manager");
    context.expect(static_cast<bool>(luna::AssetManager::get().loadAssetAs<luna::Model>(model_handle)),
                   "test model should be registered in asset manager");

    const auto& nodes = model->getNodes();
    context.expect(nodes.size() == 3, "model loader should preserve all nodes");
    if (nodes.size() == 3) {
        context.expect(nodes[0].Children.size() == 1 && nodes[0].Children[0] == 1,
                       "root node should keep its child relation");
        context.expect(nodes[1].Children.size() == 1 && nodes[1].Children[0] == 2,
                       "child node should keep its grandchild relation");
        context.expect(nodes[0].FirstSubmesh == 1 && nodes[0].SubmeshCount == 1,
                       "root node should preserve submesh range");
        context.expect(nodes[0].SubmeshMaterials.size() == 1 &&
                           nodes[0].SubmeshMaterials[0] == luna::AssetHandle(9'102),
                       "root node should preserve submesh materials");
    }

    luna::Scene probe_scene;
    luna::authoring::AuthoringSession probe_session(probe_scene);
    (void) probe_session.createScene();
    const luna::Entity session_probe = probe_session.createEntity("Probe");
    context.expect(session_probe, "plain entity creation should work in a bootstrapped session");

    luna::Scene scene;
    luna::authoring::AuthoringSession session(scene);
    (void) session.createScene();
    const luna::Entity root = session.createEntityFromModelAsset(model_handle);
    context.expect(root, "model asset import should create a root entity");
    context.expect(scene.entityManager().entityCount() >= 5,
                   "model import should add the model hierarchy to the bootstrapped scene");
    context.expect(root.getChildCount() == 1, "model root should keep one top-level child");

    if (root && root.getChildCount() == 1) {
        const luna::Entity root_node = scene.entityManager().findEntityByUUID(root.getChildren().front());
        context.expect(root_node && root_node.getName() == "RootNode",
                       "first imported node should keep its source name");
        if (root_node) {
            context.expect(root_node.getParentUUID() == root.getUUID(), "first imported node should stay under root");
            context.expect(root_node.getChildCount() == 1, "root node should keep its child");
            if (root_node.hasComponent<luna::MeshComponent>()) {
                const auto& mesh_component = root_node.getComponent<luna::MeshComponent>();
                context.expect(mesh_component.firstSubmesh == 1,
                               "imported mesh component should preserve first submesh");
                context.expect(mesh_component.submeshCount == 1,
                               "imported mesh component should preserve submesh count");
                context.expect(mesh_component.getSubmeshMaterialCount() == 1,
                               "imported mesh component should keep the active material slot count");
                context.expect(mesh_component.getSubmeshMaterial(0) == luna::AssetHandle(9'102),
                               "imported mesh component should keep the source material binding");
            }

            const luna::Entity child_node = scene.entityManager().findEntityByUUID(root_node.getChildren().front());
            context.expect(child_node && child_node.getName() == "ChildNode",
                           "second imported node should keep its source name");
            if (child_node) {
                context.expect(child_node.getParentUUID() == root_node.getUUID(),
                               "second imported node should stay under the first node");
                context.expect(child_node.getChildCount() == 1, "child node should keep its child");

                const luna::Entity grand_child_node =
                    scene.entityManager().findEntityByUUID(child_node.getChildren().front());
                context.expect(grand_child_node && grand_child_node.getName() == "GrandChildNode",
                               "third imported node should keep its source name");
                if (grand_child_node) {
                    context.expect(grand_child_node.getParentUUID() == child_node.getUUID(),
                                   "third imported node should stay under the second node");
                }
            }
        }
    }

    luna::AssetManager::get().clear();
    luna::AssetDatabase::clear();
}

} // namespace

int main()
{
    luna::Logger::init("", luna::Logger::Level::Warn);

    TestContext context;
    testAuthoringSessionSceneLifecycle(context);
    testAuthoringSessionHistory(context);
    testModelHierarchyRoundTrip(context);

    luna::Logger::shutdown();
    return context.result();
}
