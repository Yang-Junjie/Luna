#pragma once

#include "Asset/Asset.h"
#include "Core/UUID.h"
#include "Scene/Entity.h"

#include <glm/vec3.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace luna::authoring {

class AuthoringSession;

enum class AuthoringInspectionKind : uint8_t {
    Scene,
    Entity,
    Hierarchy,
};

struct AuthoringTransformInspection {
    glm::vec3 translation{0.0f, 0.0f, 0.0f};
    glm::vec3 rotation_degrees{0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};
};

struct AuthoringCameraInspection {
    bool primary{false};
    bool fixed_aspect_ratio{false};
    std::string projection;
    float perspective_fov_degrees{0.0f};
    float perspective_near{0.0f};
    float perspective_far{0.0f};
    float orthographic_size{0.0f};
    float orthographic_near{0.0f};
    float orthographic_far{0.0f};
};

struct AuthoringLightInspection {
    std::string type;
    bool enabled{false};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float intensity{0.0f};
    float range{0.0f};
    float inner_cone_angle_degrees{0.0f};
    float outer_cone_angle_degrees{0.0f};
};

struct AuthoringMeshInspection {
    AssetHandle mesh_handle{0};
    std::vector<AssetHandle> submesh_materials;
};

struct AuthoringEntityInspection {
    std::string ref;
    UUID entity_id{0};
    std::string name;
    UUID parent_id{0};
    std::vector<UUID> children;
    std::vector<std::string> components;

    bool has_transform{false};
    AuthoringTransformInspection transform;

    bool has_camera{false};
    AuthoringCameraInspection camera;

    bool has_light{false};
    AuthoringLightInspection light;

    bool has_mesh{false};
    AuthoringMeshInspection mesh;
};

struct AuthoringInspection {
    AuthoringInspectionKind kind{AuthoringInspectionKind::Scene};
    std::string ref;
    std::vector<AuthoringEntityInspection> entities;
};

[[nodiscard]] AuthoringEntityInspection inspectAuthoringEntity(Entity entity, std::string ref = {});
[[nodiscard]] AuthoringInspection inspectAuthoringScene(AuthoringSession& session);
[[nodiscard]] AuthoringInspection inspectAuthoringEntityResult(Entity entity, std::string ref = {});
[[nodiscard]] AuthoringInspection inspectAuthoringHierarchy(AuthoringSession& session);

} // namespace luna::authoring
