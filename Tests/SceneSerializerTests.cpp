#include "Core/Log.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"

#include <iostream>
#include <string_view>

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
    testSceneShadowSettingsRoundTrip(context);
    testLegacyCsmEnabledMigration(context);

    luna::Logger::shutdown();
    return context.result();
}
