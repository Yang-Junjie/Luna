#pragma once

#include "Renderer/RenderPipeline.h"
#include "Vulkan/camera.h"

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace luna {

struct SceneBackgroundEffectData {
    glm::vec4 data1{0.0f};
    glm::vec4 data2{0.0f};
    glm::vec4 data3{0.0f};
    glm::vec4 data4{0.0f};
};

struct SceneBackgroundEffect {
    std::string name;
    SceneBackgroundEffectData data{};
};

class SceneNode : public std::enable_shared_from_this<SceneNode> {
public:
    virtual ~SceneNode() = default;

    std::string name;
    std::vector<std::shared_ptr<SceneNode>> children;

    glm::vec3 translation{0.0f};
    glm::vec3 rotationEulerDegrees{0.0f};
    glm::vec3 scale{1.0f};
    glm::vec3 initialTranslation{0.0f};
    glm::vec3 initialRotationEulerDegrees{0.0f};
    glm::vec3 initialScale{1.0f};
    glm::mat4 localTransform{1.0f};
    glm::mat4 worldTransform{1.0f};

    void updateLocalTransform();
    void refreshTransform(const glm::mat4& parentMatrix = glm::mat4{1.0f});
};

class SceneMeshNode final : public SceneNode {
public:
    std::string meshName;
};

class SceneDocument {
public:
    std::unordered_map<std::string, std::shared_ptr<SceneNode>> nodes;
    std::vector<std::shared_ptr<SceneNode>> topNodes;
};

class ISceneController {
public:
    virtual ~ISceneController() = default;

    virtual float& renderScale() = 0;
    virtual Camera& camera() = 0;
    virtual std::vector<SceneBackgroundEffect>& backgroundEffects() = 0;
    virtual int& currentBackgroundEffect() = 0;
    virtual std::shared_ptr<SceneDocument> findScene(std::string_view sceneName) const = 0;
};

std::shared_ptr<IRenderPipeline> CreateDefaultSceneRenderPipeline();

} // namespace luna
