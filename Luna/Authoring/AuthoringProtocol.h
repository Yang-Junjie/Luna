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

struct AuthoringReport {
    AuthoringProtocolInfo protocol;
    AuthoringSceneSnapshot scene;
    std::vector<AuthoringEntityBinding> entities;
    std::vector<std::filesystem::path> saved_scenes;
    std::vector<AuthoringInspection> inspections;
    std::vector<AuthoringVerification> verifications;
    std::vector<std::string> errors;
};

[[nodiscard]] AuthoringSceneSnapshot captureAuthoringSceneSnapshot(const AuthoringSession& session);

[[nodiscard]] bool parseAuthoringCommandTokens(const std::vector<std::string>& tokens,
                                               AuthoringPlan& plan,
                                               std::vector<std::string>& errors,
                                               size_t start_index = 0);

} // namespace luna::authoring
