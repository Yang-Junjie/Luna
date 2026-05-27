#include "Core/Log.h"
#include "Scene/Components/ScriptComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"

#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace {

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

void testSceneEnvironmentBackgroundRoundTrip(TestContext& context)
{
    luna::Scene scene;
    auto& environment = scene.environmentSettings();
    environment.backgroundMode = luna::SceneBackgroundMode::SolidColor;
    environment.backgroundColor = glm::vec3(0.25f, 0.50f, 0.75f);
    environment.enabled = false;
    environment.iblEnabled = true;
    environment.environmentMapHandle = luna::AssetHandle(42);
    environment.intensity = 2.0f;

    const std::string serialized = luna::SceneSerializer::serializeToString(scene);
    context.expect(serialized.find("BackgroundMode: SolidColor") != std::string::npos,
                   "scene environment should serialize background mode");
    context.expect(serialized.find("BackgroundColor:") != std::string::npos,
                   "scene environment should serialize background color");
    context.expect(serialized.find("Enabled: false") != std::string::npos,
                   "legacy Enabled field should mirror solid-color background mode");

    luna::Scene loaded_scene;
    context.expect(luna::SceneSerializer::deserializeFromString(loaded_scene, serialized, "round-trip scene"),
                   "scene with background settings should deserialize");

    const auto& loaded_environment = loaded_scene.environmentSettings();
    context.expect(loaded_environment.backgroundMode == luna::SceneBackgroundMode::SolidColor,
                   "background mode should round-trip");
    context.expect(sameVec3(loaded_environment.backgroundColor, environment.backgroundColor),
                   "background color should round-trip");
    context.expect(!loaded_environment.enabled,
                   "legacy enabled field should be synchronized after loading solid-color mode");
    context.expect(loaded_environment.environmentMapHandle == environment.environmentMapHandle,
                   "environment map handle should round-trip");
}

void testLegacyEnvironmentEnabledMigration(TestContext& context)
{
    constexpr std::string_view scene_data = R"(
Scene: LegacySolid
Environment:
  Enabled: false
  IblEnabled: true
  EnvironmentMapHandle: 0
Entities: []
)";

    luna::Scene scene;
    context.expect(luna::SceneSerializer::deserializeFromString(scene, scene_data, "legacy solid scene"),
                   "legacy scene with Enabled=false should deserialize");
    context.expect(scene.environmentSettings().backgroundMode == luna::SceneBackgroundMode::SolidColor,
                   "legacy Enabled=false should migrate to solid-color background");
    context.expect(!scene.environmentSettings().enabled,
                   "legacy enabled field should remain false after solid-color migration");
}

void testLegacyEnvironmentMapInference(TestContext& context)
{
    constexpr std::string_view scene_data = R"(
Scene: LegacyEnvironmentMap
Environment:
  Enabled: true
  IblEnabled: true
  EnvironmentMapHandle: 99
Entities: []
)";

    luna::Scene scene;
    context.expect(luna::SceneSerializer::deserializeFromString(scene, scene_data, "legacy environment map scene"),
                   "legacy scene with environment map should deserialize");
    context.expect(scene.environmentSettings().backgroundMode == luna::SceneBackgroundMode::EnvironmentMap,
                   "legacy scene with a valid environment map should infer environment-map background");
    context.expect(scene.environmentSettings().enabled,
                   "legacy enabled field should remain true after environment-map inference");
}

void testScriptPropertyMetadataRoundTrip(TestContext& context)
{
    luna::Scene scene;
    luna::Entity entity = scene.entityManager().createEntity("Scripted");
    auto& script_component = entity.addComponent<luna::ScriptComponent>();
    luna::ScriptEntry script{};
    script.id = luna::UUID(77);
    script.typeName = "MetadataSmoke";

    luna::ScriptProperty property{};
    property.name = "speed";
    property.type = luna::ScriptPropertyType::Float;
    property.floatValue = 3.5f;
    property.metadata.displayName = "Move Speed";
    property.metadata.description = "Base movement speed.";
    property.metadata.category = "Movement";
    property.metadata.hasMinValue = true;
    property.metadata.hasMaxValue = true;
    property.metadata.hasStepValue = true;
    property.metadata.minValue = 0.0f;
    property.metadata.maxValue = 20.0f;
    property.metadata.stepValue = 0.25f;
    property.metadata.assetType = "Texture";
    property.metadata.entityFilter = "Camera";
    property.metadata.options.push_back({"Walk", 0, "walk"});
    property.metadata.options.push_back({"Fly", 1, "fly"});
    script.properties.push_back(std::move(property));
    script_component.scripts.push_back(std::move(script));

    const std::string serialized = luna::SceneSerializer::serializeToString(scene);
    context.expect(serialized.find("Metadata:") != std::string::npos, "script property metadata should serialize");
    context.expect(serialized.find("DisplayName: Move Speed") != std::string::npos,
                   "script property display name should serialize");
    context.expect(serialized.find("Options:") != std::string::npos, "script property options should serialize");

    luna::Scene loaded_scene;
    context.expect(luna::SceneSerializer::deserializeFromString(loaded_scene, serialized, "script metadata scene"),
                   "scene with script property metadata should deserialize");
    luna::Entity loaded_entity = loaded_scene.entityManager().findEntityByUUID(entity.getUUID());
    if (!context.expect(static_cast<bool>(loaded_entity), "script metadata entity should round-trip")) {
        return;
    }
    if (!context.expect(loaded_entity.hasComponent<luna::ScriptComponent>(),
                        "script metadata component should round-trip")) {
        return;
    }

    const luna::ScriptComponent& loaded_script_component = loaded_entity.getComponent<luna::ScriptComponent>();
    if (!context.expect(!loaded_script_component.scripts.empty(), "script entry should round-trip")) {
        return;
    }
    if (!context.expect(!loaded_script_component.scripts[0].properties.empty(), "script property should round-trip")) {
        return;
    }

    const luna::ScriptProperty& loaded_property = loaded_script_component.scripts[0].properties[0];
    context.expect(loaded_property.metadata.displayName == "Move Speed",
                   "script property display name should round-trip");
    context.expect(loaded_property.metadata.category == "Movement", "script property category should round-trip");
    context.expect(loaded_property.metadata.hasMinValue && loaded_property.metadata.minValue == 0.0f,
                   "script property min should round-trip");
    context.expect(loaded_property.metadata.hasMaxValue && loaded_property.metadata.maxValue == 20.0f,
                   "script property max should round-trip");
    context.expect(loaded_property.metadata.hasStepValue && loaded_property.metadata.stepValue == 0.25f,
                   "script property step should round-trip");
    context.expect(loaded_property.metadata.assetType == "Texture", "script property asset type should round-trip");
    context.expect(loaded_property.metadata.entityFilter == "Camera",
                   "script property entity filter should round-trip");
    context.expect(loaded_property.metadata.options.size() == 2, "script property options should round-trip");
    if (loaded_property.metadata.options.size() == 2) {
        context.expect(loaded_property.metadata.options[1].label == "Fly",
                       "script property option label should round-trip");
        context.expect(loaded_property.metadata.options[1].stringValue == "fly",
                       "script property option string value should round-trip");
    }
}

void testSceneShadowSettingsRoundTrip(TestContext& context)
{
    luna::Scene scene;
    scene.shadowSettings().mode = luna::SceneShadowMode::PcfShadowMap;
    scene.shadowSettings().pcfShadowDistance = 72.0f;
    scene.shadowSettings().pcfMapSize = 8'192;
    scene.shadowSettings().csmCascadeSize = 4'096;

    const std::string serialized = luna::SceneSerializer::serializeToString(scene);
    context.expect(serialized.find("Shadows:") != std::string::npos, "scene shadow settings should serialize");
    context.expect(serialized.find("Mode: PcfShadowMap") != std::string::npos,
                   "scene shadow settings should serialize PCF shadow mode");
    context.expect(serialized.find("PcfShadowDistance: 72") != std::string::npos,
                   "scene shadow settings should serialize PCF shadow distance");
    context.expect(serialized.find("PcfMapSize: 8192") != std::string::npos,
                   "scene shadow settings should serialize PCF map size");
    context.expect(serialized.find("CsmCascadeSize: 4096") != std::string::npos,
                   "scene shadow settings should serialize CSM cascade size");

    luna::Scene loaded_scene;
    context.expect(luna::SceneSerializer::deserializeFromString(loaded_scene, serialized, "shadow settings scene"),
                   "scene with shadow settings should deserialize");
    context.expect(loaded_scene.shadowSettings().mode == luna::SceneShadowMode::PcfShadowMap,
                   "PCF shadow mode should round-trip");
    context.expect(loaded_scene.shadowSettings().pcfShadowDistance == 72.0f, "PCF shadow distance should round-trip");
    context.expect(loaded_scene.shadowSettings().pcfMapSize == 8'192, "PCF map size should round-trip");
    context.expect(loaded_scene.shadowSettings().csmCascadeSize == 4'096, "CSM cascade size should round-trip");
}

void testLegacyCsmEnabledMigration(TestContext& context)
{
    constexpr std::string_view scene_data = R"(
Scene: LegacyNoCsm
Environment:
  Enabled: true
  IblEnabled: true
  EnvironmentMapHandle: 0
Shadows:
  CascadedShadowsEnabled: false
Entities: []
)";

    luna::Scene scene;
    context.expect(luna::SceneSerializer::deserializeFromString(scene, scene_data, "legacy no-csm scene"),
                   "legacy scene with disabled CSM should deserialize");
    context.expect(scene.shadowSettings().mode == luna::SceneShadowMode::None,
                   "legacy disabled CSM should migrate to no shadows");
}

} // namespace

int main()
{
    luna::Logger::init("", luna::Logger::Level::Warn);

    TestContext context;
    testSceneEnvironmentBackgroundRoundTrip(context);
    testLegacyEnvironmentEnabledMigration(context);
    testLegacyEnvironmentMapInference(context);
    testScriptPropertyMetadataRoundTrip(context);
    testSceneShadowSettingsRoundTrip(context);
    testLegacyCsmEnabledMigration(context);

    luna::Logger::shutdown();
    return context.result();
}
