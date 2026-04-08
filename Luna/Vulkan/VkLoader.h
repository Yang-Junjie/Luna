#pragma once
#include "VkDescriptors.h"
#include "VkTypes.h"

#include <filesystem>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <optional>
#include <unordered_map>

struct GeoSurface {
    uint32_t m_start_index{0};
    uint32_t m_count{0};
    MaterialInstance* m_material{nullptr};
};

struct MeshAsset {
    std::string m_name;

    std::vector<GeoSurface> m_surfaces;
    GPUMeshBuffers m_mesh_buffers;
};

struct RenderObject {
    uint32_t m_index_count{0};
    uint32_t m_first_index{0};
    vk::Buffer m_index_buffer{};

    MaterialInstance* m_material{nullptr};

    glm::mat4 m_transform{1.0f};
    vk::DeviceAddress m_vertex_buffer_address{};
};

struct DrawContext {
    std::vector<RenderObject> m_opaque_surfaces;
    std::vector<RenderObject> m_transparent_surfaces;

    void clear();
};

class IRenderable {
public:
    virtual ~IRenderable() = default;
    virtual void draw(const glm::mat4& top_matrix, DrawContext& ctx) = 0;
};

class Node : public IRenderable, public std::enable_shared_from_this<Node> {
public:
    std::string m_name;
    std::weak_ptr<Node> m_parent;
    std::vector<std::shared_ptr<Node>> m_children;

    glm::vec3 m_translation{0.0f};
    glm::vec3 m_rotation_euler_degrees{0.0f};
    glm::vec3 m_scale{1.0f};
    glm::vec3 m_initial_translation{0.0f};
    glm::vec3 m_initial_rotation_euler_degrees{0.0f};
    glm::vec3 m_initial_scale{1.0f};
    glm::mat4 m_local_transform{1.0f};
    glm::mat4 m_world_transform{1.0f};

    void updateLocalTransform();
    void refreshTransform(const glm::mat4& parent_matrix = glm::mat4{1.0f});
    void draw(const glm::mat4& top_matrix, DrawContext& ctx) override;
};

class MeshNode : public Node {
public:
    std::shared_ptr<MeshAsset> m_mesh;

    void draw(const glm::mat4& top_matrix, DrawContext& ctx) override;
};

class VulkanEngine;

class LoadedGLTF : public IRenderable {
public:
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> m_meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> m_nodes;
    std::vector<std::shared_ptr<Node>> m_top_nodes;
    std::vector<AllocatedImage> m_images;
    std::vector<vk::Sampler> m_samplers;
    std::vector<AllocatedBuffer> m_material_data_buffers;
    std::vector<std::shared_ptr<MaterialInstance>> m_materials;
    DescriptorAllocator m_material_descriptor_pool;
    VulkanEngine* m_creator{nullptr};

    ~LoadedGLTF() override;

    void draw(const glm::mat4& top_matrix, DrawContext& ctx) override;
    void clearAll();
};

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine* engine, const std::filesystem::path& file_path);

