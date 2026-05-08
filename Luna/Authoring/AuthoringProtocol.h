#pragma once

#include "Authoring/AuthoringInspection.h"
#include "Authoring/AuthoringVerification.h"
#include "Core/UUID.h"

#include <glm/vec3.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace luna::authoring {

class AuthoringSession;

inline constexpr std::string_view kAuthoringProtocolName = "luna.authoring";
inline constexpr uint32_t kAuthoringProtocolVersion = 1;

enum class AuthoringCommandKind : uint8_t {
    NewScene,
    OpenScene,
    SaveScene,
    CreateEntity,
    CreateCamera,
    CreateDirectionalLight,
    CreatePointLight,
    CreateSpotLight,
    CreatePrimitive,
    Parent,
    Unparent,
    Rename,
    SetTransform,
    SetLightIntensity,
    SetLightColor,
    SetCameraPerspective,
    SetCameraOrthographic,
    InspectScene,
    InspectEntity,
    InspectHierarchy,
    VerifySceneSaved,
    VerifyEntityExists,
    VerifyHasComponent,
    VerifyEntityCountAtLeast,
    Summary,
};

enum class AuthoringCommandEffect : uint8_t {
    None = 0,
    ReadsScene = 1 << 0,
    MutatesScene = 1 << 1,
    ReadsFileSystem = 1 << 2,
    WritesFileSystem = 1 << 3,
};

[[nodiscard]] constexpr AuthoringCommandEffect operator|(AuthoringCommandEffect lhs,
                                                         AuthoringCommandEffect rhs) noexcept
{
    return static_cast<AuthoringCommandEffect>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

[[nodiscard]] constexpr bool hasAuthoringCommandEffect(AuthoringCommandEffect effects,
                                                       AuthoringCommandEffect effect) noexcept
{
    return (static_cast<uint8_t>(effects) & static_cast<uint8_t>(effect)) != 0;
}

enum class AuthoringDiagnosticSeverity : uint8_t {
    Info,
    Warning,
    Error,
};

enum class AuthoringDiagnosticPhase : uint8_t {
    Parse,
    Validate,
    Execute,
    Verify,
};

enum class AuthoringDiagnosticCode : uint8_t {
    InvalidPlan,
    ProtocolMismatch,
    UnsupportedCommand,
    UnsupportedVerifyCheck,
    MissingArgument,
    InvalidArgument,
    InvalidNumber,
    NoBoundScene,
    UnknownEntity,
    UnknownBuiltinAsset,
    MissingComponent,
    OpenSceneFailed,
    SaveSceneFailed,
    ProjectLoadFailed,
    ExecutionFailed,
    VerificationFailed,
};

struct AuthoringEntityRef {
    std::string value;
};

struct AuthoringProtocolInfo {
    std::string name{std::string(kAuthoringProtocolName)};
    uint32_t version{kAuthoringProtocolVersion};
};

struct AuthoringCommand {
    AuthoringCommandKind kind{AuthoringCommandKind::Summary};

    std::filesystem::path path;
    std::string alias;
    std::string name;
    std::string mesh;
    std::string component;

    AuthoringEntityRef entity;
    AuthoringEntityRef child;
    AuthoringEntityRef parent;

    glm::vec3 translation{0.0f, 0.0f, 0.0f};
    glm::vec3 rotation_degrees{0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};
    glm::vec3 color{1.0f, 1.0f, 1.0f};

    float value{0.0f};
    float fov_degrees{0.0f};
    float near_plane{0.0f};
    float far_plane{0.0f};
    float size{0.0f};
    size_t count{0};
};

struct AuthoringPlan {
    AuthoringProtocolInfo protocol;
    std::filesystem::path project_file_path;
    std::vector<AuthoringCommand> commands;
};

struct AuthoringEntityBinding {
    std::string alias;
    UUID entity_id{0};
    std::string name;
};

struct AuthoringSceneSnapshot {
    std::string name;
    std::filesystem::path path;
    size_t entity_count{0};
    bool dirty{false};
};

struct AuthoringDiagnostic {
    AuthoringDiagnosticSeverity severity{AuthoringDiagnosticSeverity::Error};
    AuthoringDiagnosticPhase phase{AuthoringDiagnosticPhase::Execute};
    AuthoringDiagnosticCode code{AuthoringDiagnosticCode::ExecutionFailed};
    bool has_command_index{false};
    size_t command_index{0};
    std::string command;
    std::string field;
    std::string entity_ref;
    std::string component;
    std::filesystem::path path;
    std::string message;
};

struct AuthoringReport {
    AuthoringProtocolInfo protocol;
    AuthoringSceneSnapshot scene;
    std::vector<AuthoringEntityBinding> entities;
    std::vector<std::filesystem::path> saved_scenes;
    std::vector<AuthoringInspection> inspections;
    std::vector<AuthoringVerification> verifications;
    std::vector<AuthoringDiagnostic> diagnostics;
    std::vector<std::string> errors;
};

[[nodiscard]] AuthoringSceneSnapshot captureAuthoringSceneSnapshot(const AuthoringSession& session);

[[nodiscard]] AuthoringCommandEffect authoringCommandEffects(AuthoringCommandKind kind);
[[nodiscard]] bool authoringCommandReadsScene(AuthoringCommandKind kind);
[[nodiscard]] bool authoringCommandMutatesScene(AuthoringCommandKind kind);
[[nodiscard]] bool authoringCommandReadsFileSystem(AuthoringCommandKind kind);
[[nodiscard]] bool authoringCommandWritesFileSystem(AuthoringCommandKind kind);
[[nodiscard]] bool authoringCommandIsReadOnly(AuthoringCommandKind kind);

void appendAuthoringDiagnostic(AuthoringReport& report, AuthoringDiagnostic diagnostic);

[[nodiscard]] bool parseAuthoringCommandTokens(const std::vector<std::string>& tokens,
                                               AuthoringPlan& plan,
                                               std::vector<std::string>& errors,
                                               size_t start_index = 0,
                                               std::vector<AuthoringDiagnostic>* diagnostics = nullptr);

} // namespace luna::authoring
