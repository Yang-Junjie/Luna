#include "Authoring/AuthoringSession.h"
#include "Core/Log.h"
#include "Scene/Components.h"
#include "Scene/SceneSerializer.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string_view>
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

} // namespace

int main()
{
    luna::Logger::init("", luna::Logger::Level::Warn);

    TestContext context;
    testAuthoringSessionSceneLifecycle(context);

    luna::Logger::shutdown();
    return context.result();
}
