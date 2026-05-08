#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Authoring/AuthoringExecutor.h"
#include "Authoring/AuthoringSession.h"
#include "Core/Log.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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

bool containsString(const std::vector<std::string>& values, std::string_view value)
{
    return std::any_of(values.begin(), values.end(), [value](const std::string& candidate) {
        return candidate == value;
    });
}

void writeTextFile(const std::filesystem::path& path, std::string_view contents)
{
    std::ofstream output_stream(path, std::ios::binary | std::ios::trunc);
    output_stream << contents;
}

std::string readTextFile(const std::filesystem::path& path)
{
    std::ifstream input_stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input_stream), std::istreambuf_iterator<char>()};
}

void testExecutorAppliesTypedPlan(TestContext& context)
{
    TempDirectory temp("AuthoringExecutor");
    const std::filesystem::path scene_path = temp.path() / "ExecutorScene";
    const std::filesystem::path normalized_scene_path = luna::SceneSerializer::normalizeScenePath(scene_path);

    luna::Scene scene;
    luna::authoring::AuthoringSession session(scene);
    luna::authoring::AuthoringExecutor executor(session);

    luna::authoring::AuthoringPlan plan;
    plan.commands.push_back({.kind = luna::authoring::AuthoringCommandKind::NewScene});
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::CreatePrimitive,
        .alias = "Box",
        .mesh = "Cube",
    });
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::Rename,
        .name = "PlayerBox",
        .entity = {.value = "Box"},
    });
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::SetTransform,
        .entity = {.value = "Box"},
        .translation = {1.0f, 2.0f, 3.0f},
        .rotation_degrees = {0.0f, 45.0f, 0.0f},
        .scale = {1.0f, 1.0f, 1.0f},
    });
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::CreatePointLight,
        .alias = "KeyLight",
    });
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::SetLightIntensity,
        .entity = {.value = "KeyLight"},
        .value = 25.0f,
    });
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::InspectEntity,
        .entity = {.value = "Box"},
    });
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::InspectHierarchy,
    });
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::VerifyEntityExists,
        .entity = {.value = "Box"},
    });
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::VerifyHasComponent,
        .component = "Mesh",
        .entity = {.value = "Box"},
    });
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::VerifyEntityCountAtLeast,
        .count = 4,
    });
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::SaveScene,
        .path = scene_path,
    });
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::VerifySceneSaved,
    });

    luna::authoring::AuthoringReport report;
    context.expect(executor.execute(plan, report), "typed authoring plan should execute");
    context.expect(report.errors.empty(), "typed authoring plan should not report errors");
    context.expect(report.diagnostics.empty(), "typed authoring plan should not report diagnostics");
    context.expect(report.entities.size() == 2, "report should include two explicit aliases");
    context.expect(report.inspections.size() == 2, "report should include requested inspections");
    context.expect(report.verifications.size() == 4, "report should include requested verifications");
    context.expect(std::all_of(report.verifications.begin(), report.verifications.end(), [](const auto& verification) {
                       return verification.ok;
                   }),
                   "all requested verifications should pass");
    context.expect(report.saved_scenes.size() == 1, "report should include saved scene path");
    context.expect(report.saved_scenes.front() == normalized_scene_path, "saved scene path should be normalized");
    context.expect(report.scene.path == normalized_scene_path, "scene snapshot should include saved path");
    context.expect(report.scene.entity_count == 4, "scene should include bootstrap entities plus two created entities");
    context.expect(!report.scene.dirty, "scene should be clean after save");

    luna::Entity box = scene.entityManager().findEntityByUUID(report.entities.front().entity_id);
    context.expect(box && box.getName() == "PlayerBox", "renamed alias should resolve to renamed entity");
    context.expect(box && sameVec3(box.transform().translation, {1.0f, 2.0f, 3.0f}),
                   "transform command should update translation");
    context.expect(report.inspections.front().kind == luna::authoring::AuthoringInspectionKind::Entity,
                   "first inspection should be entity inspection");
    context.expect(report.inspections.front().entities.size() == 1, "entity inspection should include one entity");
    if (!report.inspections.front().entities.empty()) {
        const auto& inspected_entity = report.inspections.front().entities.front();
        context.expect(inspected_entity.ref == "Box", "entity inspection should preserve requested ref");
        context.expect(inspected_entity.name == "PlayerBox", "entity inspection should include renamed entity name");
        context.expect(containsString(inspected_entity.components, "Transform"),
                       "entity inspection should include transform component");
        context.expect(containsString(inspected_entity.components, "Mesh"),
                       "entity inspection should include mesh component");
        context.expect(inspected_entity.has_transform &&
                           sameVec3(inspected_entity.transform.translation, {1.0f, 2.0f, 3.0f}),
                       "entity inspection should include transform data");
    }

    luna::Entity light = scene.entityManager().findEntityByUUID(report.entities.back().entity_id);
    context.expect(light && light.hasComponent<luna::LightComponent>(), "light alias should resolve to light entity");
    context.expect(light && light.getComponent<luna::LightComponent>().intensity == 25.0f,
                   "light command should update intensity");
    context.expect(report.inspections.back().kind == luna::authoring::AuthoringInspectionKind::Hierarchy,
                   "second inspection should be hierarchy inspection");
    context.expect(report.inspections.back().entities.size() == 4, "hierarchy inspection should include all entities");
}

void testExecutorReportsStructuredDiagnostics(TestContext& context)
{
    luna::Scene scene;
    luna::authoring::AuthoringSession session(scene);
    luna::authoring::AuthoringExecutor executor(session);

    luna::authoring::AuthoringPlan plan;
    plan.commands.push_back({.kind = luna::authoring::AuthoringCommandKind::NewScene});
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::Rename,
        .name = "NeverRenamed",
        .entity = {.value = "MissingEntity"},
    });

    luna::authoring::AuthoringReport report;
    context.expect(!executor.execute(plan, report), "unknown entity plan should fail");
    context.expect(!report.errors.empty(), "unknown entity plan should preserve legacy errors");
    context.expect(report.diagnostics.size() == 1, "unknown entity plan should report one diagnostic");
    if (!report.diagnostics.empty()) {
        const luna::authoring::AuthoringDiagnostic& diagnostic = report.diagnostics.front();
        context.expect(diagnostic.code == luna::authoring::AuthoringDiagnosticCode::UnknownEntity,
                       "unknown entity should use UnknownEntity code");
        context.expect(diagnostic.phase == luna::authoring::AuthoringDiagnosticPhase::Validate,
                       "unknown entity should be caught during validation");
        context.expect(diagnostic.has_command_index && diagnostic.command_index == 1,
                       "unknown entity should include failing command index");
        context.expect(diagnostic.command == "name", "unknown entity should include command name");
        context.expect(diagnostic.entity_ref == "MissingEntity",
                       "unknown entity should include unresolved entity ref");
        context.expect(diagnostic.recoverable, "unknown entity should be recoverable");
        context.expect(diagnostic.expected == "resolvable entity reference",
                       "unknown entity should include expected value");
        context.expect(diagnostic.actual == "MissingEntity",
                       "unknown entity should include actual value");
    }
}

void testExecutorRollsBackFailedPlan(TestContext& context)
{
    luna::Scene scene;
    luna::authoring::AuthoringSession session(scene);
    luna::authoring::AuthoringExecutor executor(session);

    (void) session.createScene();
    (void) session.consumeEvents();
    const size_t entity_count_before = scene.entityManager().entityCount();

    luna::authoring::AuthoringPlan plan;
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::CreateEntity,
        .alias = "Temporary",
        .name = "Temporary Entity",
    });
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::Unparent,
        .child = {.value = "Temporary"},
    });

    luna::authoring::AuthoringReport report;
    context.expect(!executor.execute(plan, report), "failed executor plan should report failure");
    context.expect(scene.entityManager().entityCount() == entity_count_before,
                   "failed executor plan should rollback scene mutations");
    context.expect(!scene.entityManager().findEntityByUUID(report.entities.empty()
                                                               ? luna::UUID(0)
                                                               : report.entities.front().entity_id),
                   "failed executor plan should remove created alias entity from the scene");
    context.expect(!session.hasOpenTransaction(), "failed executor plan should close its transaction");
}

void testExecutorRemovesSavedFileWhenFailedPlanRollsBack(TestContext& context)
{
    TempDirectory temp("AuthoringExecutorFileRollback");
    const std::filesystem::path scene_path = temp.path() / "GeneratedScene";
    const std::filesystem::path normalized_scene_path = luna::SceneSerializer::normalizeScenePath(scene_path);

    luna::Scene scene;
    luna::authoring::AuthoringSession session(scene);
    luna::authoring::AuthoringExecutor executor(session);

    luna::authoring::AuthoringPlan plan;
    plan.commands.push_back({.kind = luna::authoring::AuthoringCommandKind::NewScene});
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::CreateEntity,
        .alias = "Temporary",
        .name = "Temporary Entity",
    });
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::SaveScene,
        .path = scene_path,
    });
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::Unparent,
        .child = {.value = "Temporary"},
    });

    luna::authoring::AuthoringReport report;
    context.expect(!executor.execute(plan, report), "failed saved plan should report failure");
    context.expect(!std::filesystem::exists(normalized_scene_path),
                   "failed saved plan should remove newly-created scene file");
    context.expect(report.saved_scenes.empty(), "rolled back save should not remain in saved scene report");
    context.expect(!session.hasOpenTransaction(), "failed saved plan should close its transaction");
}

void testExecutorRestoresExistingSavedFileWhenFailedPlanRollsBack(TestContext& context)
{
    TempDirectory temp("AuthoringExecutorFileRestore");
    const std::filesystem::path scene_path = temp.path() / "ExistingScene";
    const std::filesystem::path normalized_scene_path = luna::SceneSerializer::normalizeScenePath(scene_path);
    const std::string original_contents = "Scene: Original\nEntities: []\n";
    writeTextFile(normalized_scene_path, original_contents);

    luna::Scene scene;
    luna::authoring::AuthoringSession session(scene);
    luna::authoring::AuthoringExecutor executor(session);

    luna::authoring::AuthoringPlan plan;
    plan.commands.push_back({.kind = luna::authoring::AuthoringCommandKind::NewScene});
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::CreateEntity,
        .alias = "Temporary",
        .name = "Temporary Entity",
    });
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::SaveScene,
        .path = scene_path,
    });
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::VerifyEntityExists,
        .entity = {.value = "MissingEntity"},
    });

    luna::authoring::AuthoringReport report;
    context.expect(!executor.execute(plan, report), "failed overwrite plan should report failure");
    context.expect(std::filesystem::exists(normalized_scene_path),
                   "failed overwrite plan should keep existing scene file");
    context.expect(readTextFile(normalized_scene_path) == original_contents,
                   "failed overwrite plan should restore original scene file contents");
    context.expect(report.saved_scenes.empty(), "rolled back overwrite should not remain in saved scene report");
    context.expect(!session.hasOpenTransaction(), "failed overwrite plan should close its transaction");
}

void testExecutorRefusesNonFileSaveTarget(TestContext& context)
{
    TempDirectory temp("AuthoringExecutorNonFileSave");
    const std::filesystem::path scene_path = temp.path() / "Blocked.lunascene";
    std::filesystem::create_directories(scene_path);

    luna::Scene scene;
    luna::authoring::AuthoringSession session(scene);
    luna::authoring::AuthoringExecutor executor(session);

    luna::authoring::AuthoringPlan plan;
    plan.commands.push_back({.kind = luna::authoring::AuthoringCommandKind::NewScene});
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::SaveScene,
        .path = scene_path,
    });

    luna::authoring::AuthoringReport report;
    context.expect(!executor.execute(plan, report), "save to a non-file target should fail");
    context.expect(std::filesystem::is_directory(scene_path), "non-file save target should remain a directory");
    context.expect(report.diagnostics.size() == 1, "non-file save target should report one diagnostic");
    if (!report.diagnostics.empty()) {
        context.expect(report.diagnostics.front().code == luna::authoring::AuthoringDiagnosticCode::SaveSceneFailed,
                       "non-file save target should use SaveSceneFailed diagnostic code");
        context.expect(report.diagnostics.front().recoverable, "non-file save target failure should be recoverable");
        context.expect(report.diagnostics.front().expected == "regular file path",
                       "non-file save target failure should include expected value");
        context.expect(report.diagnostics.front().actual == scene_path.string(),
                       "non-file save target failure should include actual value");
    }
    context.expect(!session.hasOpenTransaction(), "non-file save target failure should close its transaction");
}

void testExecutorValidateDoesNotMutateOrWriteFiles(TestContext& context)
{
    TempDirectory temp("AuthoringExecutorDryRun");
    const std::filesystem::path scene_path = temp.path() / "DryRunScene";
    const std::filesystem::path normalized_scene_path = luna::SceneSerializer::normalizeScenePath(scene_path);

    luna::Scene scene;
    luna::authoring::AuthoringSession session(scene);
    luna::authoring::AuthoringExecutor executor(session);
    const size_t entity_count_before = scene.entityManager().entityCount();

    luna::authoring::AuthoringPlan plan;
    plan.commands.push_back({.kind = luna::authoring::AuthoringCommandKind::NewScene});
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::CreatePrimitive,
        .alias = "DryRunBox",
        .mesh = "Cube",
    });
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::SaveScene,
        .path = scene_path,
    });

    luna::authoring::AuthoringReport report;
    context.expect(executor.validate(plan, report), "valid dry-run plan should validate");
    context.expect(report.errors.empty(), "valid dry-run plan should not report errors");
    context.expect(report.diagnostics.empty(), "valid dry-run plan should not report diagnostics");
    context.expect(scene.entityManager().entityCount() == entity_count_before,
                   "dry-run validation should not mutate the scene");
    context.expect(!std::filesystem::exists(normalized_scene_path),
                   "dry-run validation should not write scene files");
}

void testExecutorValidateReportsOverwriteWarning(TestContext& context)
{
    TempDirectory temp("AuthoringExecutorDryRunOverwrite");
    const std::filesystem::path scene_path = temp.path() / "ExistingScene";
    const std::filesystem::path normalized_scene_path = luna::SceneSerializer::normalizeScenePath(scene_path);
    const std::string original_contents = "Scene: Original\nEntities: []\n";
    writeTextFile(normalized_scene_path, original_contents);

    luna::Scene scene;
    luna::authoring::AuthoringSession session(scene);
    luna::authoring::AuthoringExecutor executor(session);

    luna::authoring::AuthoringPlan plan;
    plan.commands.push_back({.kind = luna::authoring::AuthoringCommandKind::NewScene});
    plan.commands.push_back({
        .kind = luna::authoring::AuthoringCommandKind::SaveScene,
        .path = scene_path,
    });

    luna::authoring::AuthoringReport report;
    context.expect(executor.validate(plan, report), "overwrite dry-run should validate with a warning");
    context.expect(report.errors.empty(), "overwrite dry-run warning should not be an error");
    context.expect(report.diagnostics.size() == 1, "overwrite dry-run should report one diagnostic");
    if (!report.diagnostics.empty()) {
        context.expect(report.diagnostics.front().severity == luna::authoring::AuthoringDiagnosticSeverity::Warning,
                       "overwrite diagnostic should be a warning");
        context.expect(report.diagnostics.front().code == luna::authoring::AuthoringDiagnosticCode::FileOverwrite,
                       "overwrite diagnostic should use FileOverwrite code");
        context.expect(report.diagnostics.front().recoverable, "overwrite warning should be recoverable");
        context.expect(report.diagnostics.front().expected == "non-existing scene file",
                       "overwrite warning should include expected value");
        context.expect(report.diagnostics.front().actual == normalized_scene_path.string(),
                       "overwrite warning should include actual value");
    }
    context.expect(readTextFile(normalized_scene_path) == original_contents,
                   "overwrite dry-run should not change existing file contents");
}

} // namespace

int main()
{
    luna::Logger::init("", luna::Logger::Level::Warn);
    luna::AssetManager::get().init();

    TestContext context;
    testExecutorAppliesTypedPlan(context);
    testExecutorReportsStructuredDiagnostics(context);
    testExecutorRollsBackFailedPlan(context);
    testExecutorRemovesSavedFileWhenFailedPlanRollsBack(context);
    testExecutorRestoresExistingSavedFileWhenFailedPlanRollsBack(context);
    testExecutorRefusesNonFileSaveTarget(context);
    testExecutorValidateDoesNotMutateOrWriteFiles(context);
    testExecutorValidateReportsOverwriteWarning(context);

    luna::AssetManager::get().clear();
    luna::AssetDatabase::clear();
    luna::Logger::shutdown();
    return context.result();
}
