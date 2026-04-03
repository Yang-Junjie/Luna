#pragma once
#include "vk_descriptors.h"
#include "vk_types.h"

#include <filesystem>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <optional>
#include <unordered_map>

struct GeoSurface {
    uint32_t startIndex{0};
    uint32_t count{0};
    MaterialInstance* material{nullptr};
};

struct MeshAsset {
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

struct RenderObject {
    uint32_t indexCount{0};
    uint32_t firstIndex{0};
    vk::Buffer indexBuffer{};

    MaterialInstance* material{nullptr};

    glm::mat4 transform{1.0f};
    vk::DeviceAddress vertexBufferAddress{};
};

struct DrawContext {
    std::vector<RenderObject> opaqueSurfaces;
    std::vector<RenderObject> transparentSurfaces;

    void clear();
};

class IRenderable {
public:
    virtual ~IRenderable() = default;
    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};

class Node : public IRenderable, public std::enable_shared_from_this<Node> {
public:
    std::string name;
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

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
    void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

class MeshNode : public Node {
public:
    std::shared_ptr<MeshAsset> mesh;

    void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

class VulkanEngine;

class LoadedGLTF : public IRenderable {
public:
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    std::vector<std::shared_ptr<Node>> topNodes;
    std::vector<AllocatedImage> images;
    std::vector<vk::Sampler> samplers;
    std::vector<AllocatedBuffer> materialDataBuffers;
    std::vector<std::shared_ptr<MaterialInstance>> materials;
    DescriptorAllocator materialDescriptorPool;
    VulkanEngine* creator{nullptr};

    ~LoadedGLTF() override;

    void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
    void clearAll();
};

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine* engine, const std::filesystem::path& filePath);
