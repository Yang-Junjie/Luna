#include "Renderer/SceneRenderPipeline.h"

#include "Core/Paths.h"
#include "Core/log.h"
#include "RHI/CommandContext.h"
#include "RHI/ResourceLayout.h"

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/tools.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace luna {
namespace {

struct SceneVertex {
    float position[3];
    float normal[3];
    float uv[2];
    float color[4];
};

struct alignas(16) SceneUniformData {
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    glm::mat4 viewproj{1.0f};
    glm::vec4 ambientColor{0.1f};
    glm::vec4 sunlightDirection{0.0f, 1.0f, 0.5f, 1.0f};
    glm::vec4 sunlightColor{1.0f};
};

struct alignas(16) SceneMaterialUniformData {
    glm::vec4 colorFactors{1.0f};
    glm::vec4 metalRoughFactors{1.0f};
};

struct MeshSurface {
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    size_t materialIndex = 0;
};

struct MeshGpu {
    BufferHandle vertexBuffer{};
    BufferHandle indexBuffer{};
    std::vector<MeshSurface> surfaces;
};

struct MaterialGpu {
    BufferHandle uniformBuffer{};
    ResourceSetHandle resourceSet{};
};

struct DrawItem {
    BufferHandle vertexBuffer{};
    BufferHandle indexBuffer{};
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    size_t materialIndex = 0;
    glm::mat4 worldMatrix{1.0f};
};

struct SceneRecord {
    std::shared_ptr<SceneDocument> document;
    std::unordered_map<std::string, MeshGpu> meshes;
    std::vector<MaterialGpu> materials;
};

glm::vec3 to_glm_vec3(const fastgltf::math::fvec3& value)
{
    return {value.x(), value.y(), value.z()};
}

glm::vec4 to_glm_vec4(const fastgltf::math::nvec4& value)
{
    return {value.x(), value.y(), value.z(), value.w()};
}

glm::mat4 to_glm_matrix(const fastgltf::math::fmat4x4& matrix)
{
    glm::mat4 result{1.0f};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            result[column][row] = matrix[column][row];
        }
    }
    return result;
}

glm::vec3 quaternion_to_euler_degrees(const glm::quat& q)
{
    return glm::degrees(glm::eulerAngles(q));
}

std::string scene_shader_path(std::string_view relativePath)
{
    return std::filesystem::path{LUNA_RHI_SCENE_SHADER_ROOT}.append(relativePath).lexically_normal().generic_string();
}

struct SceneLoadResult {
    std::shared_ptr<SceneDocument> document;
    std::unordered_map<std::string, MeshGpu> meshes;
    std::vector<SceneMaterialUniformData> materials;
};

std::optional<SceneLoadResult> load_scene(IRHIDevice& device, const std::filesystem::path& assetPath)
{
    auto gltfData = fastgltf::GltfDataBuffer::FromPath(assetPath);
    if (gltfData.error() != fastgltf::Error::None) {
        LUNA_CORE_ERROR("Failed to read glTF file '{}': {}", assetPath.string(), fastgltf::getErrorMessage(gltfData.error()));
        return std::nullopt;
    }

    constexpr auto kOptions = fastgltf::Options::LoadExternalBuffers | fastgltf::Options::DecomposeNodeMatrices;
    fastgltf::Parser parser{};
    auto asset = parser.loadGltf(gltfData.get(), assetPath.parent_path(), kOptions, fastgltf::Category::All);
    if (asset.error() != fastgltf::Error::None) {
        LUNA_CORE_ERROR("Failed to parse glTF file '{}': {}", assetPath.string(), fastgltf::getErrorMessage(asset.error()));
        return std::nullopt;
    }

    auto& gltf = asset.get();
    SceneLoadResult result{};
    result.document = std::make_shared<SceneDocument>();

    if (gltf.materials.empty()) {
        result.materials.push_back({});
    } else {
        result.materials.reserve(gltf.materials.size());
        for (const auto& material : gltf.materials) {
            SceneMaterialUniformData uniformData{};
            uniformData.colorFactors = to_glm_vec4(material.pbrData.baseColorFactor);
            uniformData.metalRoughFactors = {
                static_cast<float>(material.pbrData.metallicFactor),
                static_cast<float>(material.pbrData.roughnessFactor),
                0.0f,
                0.0f,
            };
            result.materials.push_back(uniformData);
        }
    }

    std::vector<std::shared_ptr<SceneMeshNode>> meshNodesByIndex(gltf.meshes.size());
    for (size_t meshIndex = 0; meshIndex < gltf.meshes.size(); ++meshIndex) {
        const auto& gltfMesh = gltf.meshes[meshIndex];
        std::string meshName = gltfMesh.name.empty() ? "mesh_" + std::to_string(meshIndex) : std::string(gltfMesh.name);
        if (result.meshes.contains(meshName)) {
            meshName += "_" + std::to_string(meshIndex);
        }

        std::vector<SceneVertex> vertices;
        std::vector<uint32_t> indices;
        MeshGpu meshGpu{};

        for (const auto& primitive : gltfMesh.primitives) {
            if (primitive.type != fastgltf::PrimitiveType::Triangles) {
                continue;
            }

            const auto positionIt = primitive.findAttribute("POSITION");
            if (positionIt == primitive.attributes.end()) {
                continue;
            }

            const auto& positionAccessor = gltf.accessors[positionIt->accessorIndex];
            const uint32_t firstVertex = static_cast<uint32_t>(vertices.size());
            vertices.resize(vertices.size() + positionAccessor.count);
            for (size_t vertexIndex = firstVertex; vertexIndex < vertices.size(); ++vertexIndex) {
                vertices[vertexIndex].color[0] = 1.0f;
                vertices[vertexIndex].color[1] = 1.0f;
                vertices[vertexIndex].color[2] = 1.0f;
                vertices[vertexIndex].color[3] = 1.0f;
            }

            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, positionAccessor, [&](auto position, size_t index) {
                auto& vertex = vertices[firstVertex + index];
                vertex.position[0] = position.x();
                vertex.position[1] = position.y();
                vertex.position[2] = position.z();
            });

            if (const auto normalIt = primitive.findAttribute("NORMAL"); normalIt != primitive.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, gltf.accessors[normalIt->accessorIndex], [&](auto normal, size_t index) {
                    auto& vertex = vertices[firstVertex + index];
                    vertex.normal[0] = normal.x();
                    vertex.normal[1] = normal.y();
                    vertex.normal[2] = normal.z();
                });
            }

            if (const auto uvIt = primitive.findAttribute("TEXCOORD_0"); uvIt != primitive.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(gltf, gltf.accessors[uvIt->accessorIndex], [&](auto uv, size_t index) {
                    auto& vertex = vertices[firstVertex + index];
                    vertex.uv[0] = uv.x();
                    vertex.uv[1] = uv.y();
                });
            }

            MeshSurface surface{};
            surface.firstIndex = static_cast<uint32_t>(indices.size());
            surface.materialIndex = primitive.materialIndex.has_value() && primitive.materialIndex.value() < result.materials.size()
                                        ? primitive.materialIndex.value()
                                        : 0;

            if (primitive.indicesAccessor.has_value()) {
                const auto& indexAccessor = gltf.accessors[primitive.indicesAccessor.value()];
                std::vector<uint32_t> primitiveIndices(indexAccessor.count);
                fastgltf::copyFromAccessor<uint32_t>(gltf, indexAccessor, primitiveIndices.data());
                for (uint32_t index : primitiveIndices) {
                    indices.push_back(firstVertex + index);
                }
                surface.indexCount = static_cast<uint32_t>(primitiveIndices.size());
            } else {
                surface.indexCount = static_cast<uint32_t>(positionAccessor.count);
                for (uint32_t index = 0; index < surface.indexCount; ++index) {
                    indices.push_back(firstVertex + index);
                }
            }

            meshGpu.surfaces.push_back(surface);
        }

        if (meshGpu.surfaces.empty()) {
            continue;
        }

        BufferDesc vertexBufferDesc{};
        vertexBufferDesc.size = static_cast<uint64_t>(vertices.size() * sizeof(SceneVertex));
        vertexBufferDesc.usage = BufferUsage::Vertex | BufferUsage::TransferDst;
        vertexBufferDesc.memoryUsage = MemoryUsage::Default;
        vertexBufferDesc.debugName = "SceneMeshVertices";
        if (device.createBuffer(vertexBufferDesc, &meshGpu.vertexBuffer, vertices.data()) != RHIResult::Success) {
            return std::nullopt;
        }

        BufferDesc indexBufferDesc{};
        indexBufferDesc.size = static_cast<uint64_t>(indices.size() * sizeof(uint32_t));
        indexBufferDesc.usage = BufferUsage::Index | BufferUsage::TransferDst;
        indexBufferDesc.memoryUsage = MemoryUsage::Default;
        indexBufferDesc.debugName = "SceneMeshIndices";
        if (device.createBuffer(indexBufferDesc, &meshGpu.indexBuffer, indices.data()) != RHIResult::Success) {
            device.destroyBuffer(meshGpu.vertexBuffer);
            return std::nullopt;
        }

        result.meshes.emplace(meshName, std::move(meshGpu));
    }

    std::vector<std::shared_ptr<SceneNode>> nodesByIndex(gltf.nodes.size());
    for (size_t nodeIndex = 0; nodeIndex < gltf.nodes.size(); ++nodeIndex) {
        const auto& gltfNode = gltf.nodes[nodeIndex];
        std::shared_ptr<SceneNode> node;
        if (gltfNode.meshIndex.has_value() && gltfNode.meshIndex.value() < gltf.meshes.size()) {
            auto meshNode = std::make_shared<SceneMeshNode>();
            meshNode->meshName = gltf.meshes[gltfNode.meshIndex.value()].name.empty()
                                     ? "mesh_" + std::to_string(gltfNode.meshIndex.value())
                                     : std::string(gltf.meshes[gltfNode.meshIndex.value()].name);
            node = meshNode;
        } else {
            node = std::make_shared<SceneNode>();
        }

        node->name = gltfNode.name.empty() ? "node_" + std::to_string(nodeIndex) : std::string(gltfNode.name);
        if (const auto* trs = std::get_if<fastgltf::TRS>(&gltfNode.transform)) {
            node->translation = to_glm_vec3(trs->translation);
            node->scale = to_glm_vec3(trs->scale);
            node->rotationEulerDegrees = quaternion_to_euler_degrees(glm::quat(
                trs->rotation.w(), trs->rotation.x(), trs->rotation.y(), trs->rotation.z()));
            node->initialTranslation = node->translation;
            node->initialRotationEulerDegrees = node->rotationEulerDegrees;
            node->initialScale = node->scale;
            node->updateLocalTransform();
        } else {
            node->localTransform = to_glm_matrix(std::get<fastgltf::math::fmat4x4>(gltfNode.transform));
        }
        node->worldTransform = node->localTransform;

        result.document->nodes[node->name] = node;
        nodesByIndex[nodeIndex] = node;
    }

    for (size_t nodeIndex = 0; nodeIndex < gltf.nodes.size(); ++nodeIndex) {
        const auto& gltfNode = gltf.nodes[nodeIndex];
        if (!nodesByIndex[nodeIndex]) {
            continue;
        }

        for (size_t childIndex : gltfNode.children) {
            if (childIndex < nodesByIndex.size() && nodesByIndex[childIndex]) {
                nodesByIndex[nodeIndex]->children.push_back(nodesByIndex[childIndex]);
            }
        }
    }

    const size_t sceneIndex = gltf.defaultScene.has_value() ? gltf.defaultScene.value() : 0;
    if (sceneIndex < gltf.scenes.size()) {
        for (size_t nodeIndex : gltf.scenes[sceneIndex].nodeIndices) {
            if (nodeIndex < nodesByIndex.size() && nodesByIndex[nodeIndex]) {
                result.document->topNodes.push_back(nodesByIndex[nodeIndex]);
            }
        }
    }

    return result;
}

} // namespace

void SceneNode::updateLocalTransform()
{
    const glm::vec3 rotationRadians = glm::radians(rotationEulerDegrees);
    const glm::quat rotation = glm::quat(rotationRadians);
    localTransform = glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) *
                     glm::scale(glm::mat4(1.0f), scale);
}

void SceneNode::refreshTransform(const glm::mat4& parentMatrix)
{
    worldTransform = parentMatrix * localTransform;
    for (const auto& child : children) {
        if (child) {
            child->refreshTransform(worldTransform);
        }
    }
}

class SceneRenderPipeline final : public IRenderPipeline, public ISceneController {
public:
    bool init(IRHIDevice& device) override;
    void shutdown(IRHIDevice& device) override;
    bool render(IRHIDevice& device, const FrameContext& frameContext) override;

    float& renderScale() override { return m_renderScale; }
    Camera& camera() override { return m_camera; }
    std::vector<SceneBackgroundEffect>& backgroundEffects() override { return m_backgroundEffects; }
    int& currentBackgroundEffect() override { return m_currentBackgroundEffect; }
    std::shared_ptr<SceneDocument> findScene(std::string_view sceneName) const override;

private:
    bool ensure_static_resources(IRHIDevice& device);
    bool ensure_size_dependent_resources(IRHIDevice& device, PixelFormat colorFormat, uint32_t width, uint32_t height);
    void collect_draw_items(const std::shared_ptr<SceneNode>& node, const SceneRecord& scene, std::vector<DrawItem>& drawItems) const;
    PipelineHandle current_effect_pipeline() const;

private:
    std::unordered_map<std::string, SceneRecord> m_scenes;
    std::vector<SceneBackgroundEffect> m_backgroundEffects;
    std::vector<PipelineHandle> m_backgroundPipelines;
    float m_renderScale = 1.0f;
    Camera m_camera{};

    ResourceLayoutHandle m_computeLayout{};
    ResourceLayoutHandle m_textureLayout{};
    ResourceLayoutHandle m_sceneLayout{};
    ResourceSetHandle m_computeSet{};
    ResourceSetHandle m_backgroundSampleSet{};
    ResourceSetHandle m_sceneColorSampleSet{};
    SamplerHandle m_linearSampler{};
    ImageHandle m_whiteTexture{};
    ImageHandle m_sceneDepthImage{};
    BufferHandle m_sceneBuffer{};
    PipelineHandle m_fullscreenPipeline{};
    PipelineHandle m_sceneFullscreenPipeline{};
    PipelineHandle m_scenePipeline{};
    ImageHandle m_backgroundImage{};
    ImageHandle m_sceneColorImage{};
    PixelFormat m_pipelineFormat = PixelFormat::Undefined;
    uint32_t m_scaledWidth = 0;
    uint32_t m_scaledHeight = 0;
    int m_currentBackgroundEffect = 0;
};

bool SceneRenderPipeline::init(IRHIDevice& device)
{
    m_camera.position = {0.0f, 0.0f, 5.0f};
    if (!ensure_static_resources(device)) {
        return false;
    }

    auto scene = load_scene(device, paths::asset("basicmesh.glb"));
    if (!scene.has_value()) {
        return false;
    }

    SceneRecord record{};
    record.document = scene->document;
    record.meshes = std::move(scene->meshes);
    record.materials.reserve(scene->materials.size());

    for (const auto& materialData : scene->materials) {
        MaterialGpu material{};
        const BufferDesc materialBufferDesc{
            .size = sizeof(SceneMaterialUniformData),
            .usage = BufferUsage::Uniform,
            .memoryUsage = MemoryUsage::Upload,
            .debugName = "SceneMaterialBuffer",
        };
        if (device.createBuffer(materialBufferDesc, &material.uniformBuffer, &materialData) != RHIResult::Success ||
            device.createResourceSet(m_sceneLayout, &material.resourceSet) != RHIResult::Success) {
            return false;
        }

        ResourceSetWriteDesc writeDesc{};
        writeDesc.buffers.push_back({0, m_sceneBuffer, 0, sizeof(SceneUniformData), ResourceType::UniformBuffer});
        writeDesc.buffers.push_back({1, material.uniformBuffer, 0, sizeof(SceneMaterialUniformData), ResourceType::UniformBuffer});
        writeDesc.images.push_back(
            {.binding = 2, .image = m_whiteTexture, .sampler = m_linearSampler, .type = ResourceType::CombinedImageSampler});
        writeDesc.images.push_back(
            {.binding = 3, .image = m_whiteTexture, .sampler = m_linearSampler, .type = ResourceType::CombinedImageSampler});
        if (device.updateResourceSet(material.resourceSet, writeDesc) != RHIResult::Success) {
            return false;
        }
        record.materials.push_back(material);
    }

    m_scenes.emplace("basicmesh", std::move(record));
    return true;
}

void SceneRenderPipeline::shutdown(IRHIDevice& device)
{
    for (auto& [_, scene] : m_scenes) {
        for (auto& [__, mesh] : scene.meshes) {
            if (mesh.indexBuffer.isValid()) device.destroyBuffer(mesh.indexBuffer);
            if (mesh.vertexBuffer.isValid()) device.destroyBuffer(mesh.vertexBuffer);
        }
        for (auto& material : scene.materials) {
            if (material.resourceSet.isValid()) device.destroyResourceSet(material.resourceSet);
            if (material.uniformBuffer.isValid()) device.destroyBuffer(material.uniformBuffer);
        }
    }
    m_scenes.clear();

    if (m_sceneColorSampleSet.isValid()) device.destroyResourceSet(m_sceneColorSampleSet);
    if (m_backgroundSampleSet.isValid()) device.destroyResourceSet(m_backgroundSampleSet);
    if (m_computeSet.isValid()) device.destroyResourceSet(m_computeSet);
    if (m_sceneColorImage.isValid()) device.destroyImage(m_sceneColorImage);
    if (m_sceneDepthImage.isValid()) device.destroyImage(m_sceneDepthImage);
    if (m_backgroundImage.isValid()) device.destroyImage(m_backgroundImage);
    if (m_scenePipeline.isValid()) device.destroyPipeline(m_scenePipeline);
    if (m_sceneFullscreenPipeline.isValid()) device.destroyPipeline(m_sceneFullscreenPipeline);
    if (m_fullscreenPipeline.isValid()) device.destroyPipeline(m_fullscreenPipeline);
    for (PipelineHandle pipeline : m_backgroundPipelines) {
        if (pipeline.isValid()) {
            device.destroyPipeline(pipeline);
        }
    }
    if (m_sceneLayout.isValid()) device.destroyResourceLayout(m_sceneLayout);
    if (m_textureLayout.isValid()) device.destroyResourceLayout(m_textureLayout);
    if (m_computeLayout.isValid()) device.destroyResourceLayout(m_computeLayout);
    if (m_sceneBuffer.isValid()) device.destroyBuffer(m_sceneBuffer);
    if (m_linearSampler.isValid()) device.destroySampler(m_linearSampler);
    if (m_whiteTexture.isValid()) device.destroyImage(m_whiteTexture);

    m_backgroundEffects.clear();
    m_backgroundPipelines.clear();
    m_sceneColorSampleSet = {};
    m_backgroundSampleSet = {};
    m_computeSet = {};
    m_sceneColorImage = {};
    m_sceneDepthImage = {};
    m_backgroundImage = {};
    m_scenePipeline = {};
    m_sceneFullscreenPipeline = {};
    m_fullscreenPipeline = {};
    m_sceneLayout = {};
    m_textureLayout = {};
    m_computeLayout = {};
    m_sceneBuffer = {};
    m_linearSampler = {};
    m_whiteTexture = {};
    m_pipelineFormat = PixelFormat::Undefined;
    m_scaledWidth = 0;
    m_scaledHeight = 0;
}

bool SceneRenderPipeline::render(IRHIDevice& device, const FrameContext& frameContext)
{
    const auto sceneIt = m_scenes.find("basicmesh");
    if (frameContext.commandContext == nullptr || !frameContext.backbuffer.isValid() || sceneIt == m_scenes.end()) {
        return false;
    }

    const uint32_t scaledWidth =
        std::max(1u, static_cast<uint32_t>(std::round(static_cast<float>(frameContext.renderWidth) * m_renderScale)));
    const uint32_t scaledHeight =
        std::max(1u, static_cast<uint32_t>(std::round(static_cast<float>(frameContext.renderHeight) * m_renderScale)));

    if (!ensure_size_dependent_resources(device, frameContext.backbufferFormat, scaledWidth, scaledHeight)) {
        return false;
    }

    SceneUniformData sceneData{};
    const float aspect = scaledHeight > 0 ? static_cast<float>(scaledWidth) / static_cast<float>(scaledHeight) : 1.0f;
    sceneData.view = m_camera.get_view_matrix();
    sceneData.proj = glm::perspectiveRH_ZO(glm::radians(70.0f), aspect, 0.1f, 10000.0f);
    sceneData.proj[1][1] *= -1.0f;
    sceneData.viewproj = sceneData.proj * sceneData.view;
    if (device.writeBuffer(m_sceneBuffer, &sceneData, sizeof(sceneData)) != RHIResult::Success) {
        return false;
    }

    std::vector<DrawItem> drawItems;
    for (const auto& topNode : sceneIt->second.document->topNodes) {
        if (!topNode) {
            continue;
        }
        topNode->refreshTransform(glm::mat4{1.0f});
        collect_draw_items(topNode, sceneIt->second, drawItems);
    }

    if (m_currentBackgroundEffect < 0 || m_currentBackgroundEffect >= static_cast<int>(m_backgroundEffects.size())) {
        m_currentBackgroundEffect = 0;
    }

    const auto& effect = m_backgroundEffects[static_cast<size_t>(m_currentBackgroundEffect)];
    const bool ok =
        frameContext.commandContext->transitionImage(m_backgroundImage, ImageLayout::General) == RHIResult::Success &&
        frameContext.commandContext->bindComputePipeline(current_effect_pipeline()) == RHIResult::Success &&
        frameContext.commandContext->bindResourceSet(m_computeSet) == RHIResult::Success &&
        frameContext.commandContext->pushConstants(&effect.data, sizeof(effect.data), 0, ShaderType::Compute) ==
            RHIResult::Success &&
        frameContext.commandContext->dispatch((scaledWidth + 15) / 16, (scaledHeight + 15) / 16, 1) ==
            RHIResult::Success &&
        frameContext.commandContext->transitionImage(m_backgroundImage, ImageLayout::ShaderReadOnly) == RHIResult::Success &&
        frameContext.commandContext->beginRendering(
            {.width = scaledWidth,
             .height = scaledHeight,
             .colorAttachments = {{m_sceneColorImage, frameContext.backbufferFormat, {0.04f, 0.05f, 0.06f, 1.0f}}},
             .depthAttachment = {m_sceneDepthImage, PixelFormat::D32Float, 1.0f}}) ==
            RHIResult::Success &&
        frameContext.commandContext->bindGraphicsPipeline(m_sceneFullscreenPipeline) == RHIResult::Success &&
        frameContext.commandContext->bindResourceSet(m_backgroundSampleSet) == RHIResult::Success &&
        frameContext.commandContext->draw({3, 1, 0, 0}) == RHIResult::Success &&
        frameContext.commandContext->bindGraphicsPipeline(m_scenePipeline) == RHIResult::Success;

    if (!ok) {
        return false;
    }

    for (const DrawItem& drawItem : drawItems) {
        if (drawItem.materialIndex >= sceneIt->second.materials.size()) {
            continue;
        }
        const MaterialGpu& material = sceneIt->second.materials[drawItem.materialIndex];
        if (frameContext.commandContext->bindResourceSet(material.resourceSet) != RHIResult::Success ||
            frameContext.commandContext->bindVertexBuffer(drawItem.vertexBuffer) != RHIResult::Success ||
            frameContext.commandContext->bindIndexBuffer(drawItem.indexBuffer, IndexFormat::UInt32) != RHIResult::Success ||
            frameContext.commandContext->pushConstants(
                &drawItem.worldMatrix, sizeof(drawItem.worldMatrix), 0, ShaderType::Vertex) != RHIResult::Success ||
            frameContext.commandContext->drawIndexed({drawItem.indexCount, 1, drawItem.firstIndex, 0, 0}) !=
                RHIResult::Success) {
            return false;
        }
    }

    return frameContext.commandContext->endRendering() == RHIResult::Success &&
           frameContext.commandContext->transitionImage(m_sceneColorImage, ImageLayout::ShaderReadOnly) ==
               RHIResult::Success &&
           frameContext.commandContext->beginRendering(
               {.width = frameContext.renderWidth,
                .height = frameContext.renderHeight,
                .colorAttachments = {{frameContext.backbuffer,
                                      frameContext.backbufferFormat,
                                      {0.0f, 0.0f, 0.0f, 1.0f}}}}) == RHIResult::Success &&
           frameContext.commandContext->bindGraphicsPipeline(m_fullscreenPipeline) == RHIResult::Success &&
           frameContext.commandContext->bindResourceSet(m_sceneColorSampleSet) == RHIResult::Success &&
           frameContext.commandContext->draw({3, 1, 0, 0}) == RHIResult::Success &&
           frameContext.commandContext->endRendering() == RHIResult::Success;
}

std::shared_ptr<SceneDocument> SceneRenderPipeline::findScene(std::string_view sceneName) const
{
    const auto it = m_scenes.find(std::string(sceneName));
    return it != m_scenes.end() ? it->second.document : nullptr;
}

bool SceneRenderPipeline::ensure_static_resources(IRHIDevice& device)
{
    if (m_sceneBuffer.isValid()) {
        return true;
    }

    const uint8_t whitePixel[] = {255, 255, 255, 255};
    ImageDesc whiteTextureDesc{};
    whiteTextureDesc.width = 1;
    whiteTextureDesc.height = 1;
    whiteTextureDesc.format = PixelFormat::RGBA8Srgb;
    whiteTextureDesc.usage = ImageUsage::Sampled;
    whiteTextureDesc.debugName = "SceneWhiteTexture";
    if (device.createImage(whiteTextureDesc, &m_whiteTexture, whitePixel) != RHIResult::Success) return false;

    SamplerDesc samplerDesc{};
    samplerDesc.debugName = "SceneLinearSampler";
    if (device.createSampler(samplerDesc, &m_linearSampler) != RHIResult::Success) return false;

    const BufferDesc sceneBufferDesc{
        .size = sizeof(SceneUniformData),
        .usage = BufferUsage::Uniform,
        .memoryUsage = MemoryUsage::Upload,
        .debugName = "SceneUniformBuffer",
    };
    if (device.createBuffer(sceneBufferDesc, &m_sceneBuffer) != RHIResult::Success) return false;

    ResourceLayoutDesc computeLayoutDesc{};
    computeLayoutDesc.debugName = "SceneBackgroundComputeLayout";
    computeLayoutDesc.bindings.push_back({0, ResourceType::StorageImage, 1, ShaderType::Compute});
    if (device.createResourceLayout(computeLayoutDesc, &m_computeLayout) != RHIResult::Success ||
        device.createResourceSet(m_computeLayout, &m_computeSet) != RHIResult::Success) {
        return false;
    }

    ResourceLayoutDesc textureLayoutDesc{};
    textureLayoutDesc.debugName = "SceneTextureLayout";
    textureLayoutDesc.bindings.push_back({0, ResourceType::CombinedImageSampler, 1, ShaderType::Fragment});
    if (device.createResourceLayout(textureLayoutDesc, &m_textureLayout) != RHIResult::Success ||
        device.createResourceSet(m_textureLayout, &m_backgroundSampleSet) != RHIResult::Success ||
        device.createResourceSet(m_textureLayout, &m_sceneColorSampleSet) != RHIResult::Success) {
        return false;
    }

    ResourceLayoutDesc sceneLayoutDesc{};
    sceneLayoutDesc.debugName = "SceneMaterialLayout";
    sceneLayoutDesc.bindings.push_back({0, ResourceType::UniformBuffer, 1, ShaderType::Vertex | ShaderType::Fragment});
    sceneLayoutDesc.bindings.push_back({1, ResourceType::UniformBuffer, 1, ShaderType::Fragment});
    sceneLayoutDesc.bindings.push_back({2, ResourceType::CombinedImageSampler, 1, ShaderType::Fragment});
    sceneLayoutDesc.bindings.push_back({3, ResourceType::CombinedImageSampler, 1, ShaderType::Fragment});
    if (device.createResourceLayout(sceneLayoutDesc, &m_sceneLayout) != RHIResult::Success) {
        return false;
    }

    if (m_backgroundEffects.empty()) {
        m_backgroundEffects.push_back({"gradient", {{1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}});
        m_backgroundEffects.push_back({"sky", {{0.1f, 0.2f, 0.4f, 0.97f}}});
        const std::array<std::string, 2> shaderPaths = {
            paths::display(paths::shader("Internal/gradient.spv")),
            paths::display(paths::shader("Internal/sky.spv")),
        };
        for (size_t index = 0; index < shaderPaths.size(); ++index) {
            PipelineHandle pipeline{};
            ComputePipelineDesc pipelineDesc{};
            pipelineDesc.debugName = index == 0 ? "SceneGradientPipeline" : "SceneSkyPipeline";
            pipelineDesc.computeShader = {.stage = ShaderType::Compute, .filePath = shaderPaths[index]};
            pipelineDesc.resourceLayouts.push_back(m_computeLayout);
            pipelineDesc.pushConstantSize = sizeof(SceneBackgroundEffectData);
            pipelineDesc.pushConstantVisibility = ShaderType::Compute;
            if (device.createComputePipeline(pipelineDesc, &pipeline) != RHIResult::Success) {
                return false;
            }
            m_backgroundPipelines.push_back(pipeline);
        }
    }

    return true;
}

bool SceneRenderPipeline::ensure_size_dependent_resources(IRHIDevice& device,
                                                          PixelFormat colorFormat,
                                                          uint32_t width,
                                                          uint32_t height)
{
    if (colorFormat == PixelFormat::Undefined || width == 0 || height == 0) {
        return false;
    }

    if (m_sceneColorImage.isValid() && m_sceneDepthImage.isValid() && m_backgroundImage.isValid() && m_pipelineFormat == colorFormat &&
        m_scaledWidth == width && m_scaledHeight == height && m_scenePipeline.isValid() &&
        m_sceneFullscreenPipeline.isValid() && m_fullscreenPipeline.isValid()) {
        return true;
    }

    if (m_sceneColorImage.isValid() || m_sceneDepthImage.isValid() || m_backgroundImage.isValid() || m_scenePipeline.isValid() ||
        m_sceneFullscreenPipeline.isValid() || m_fullscreenPipeline.isValid()) {
        if (device.waitIdle() != RHIResult::Success) {
            return false;
        }
    }

    if (m_sceneColorImage.isValid()) {
        device.destroyImage(m_sceneColorImage);
        m_sceneColorImage = {};
    }
    if (m_backgroundImage.isValid()) {
        device.destroyImage(m_backgroundImage);
        m_backgroundImage = {};
    }
    if (m_sceneDepthImage.isValid()) {
        device.destroyImage(m_sceneDepthImage);
        m_sceneDepthImage = {};
    }
    if (m_scenePipeline.isValid()) {
        device.destroyPipeline(m_scenePipeline);
        m_scenePipeline = {};
    }
    if (m_sceneFullscreenPipeline.isValid()) {
        device.destroyPipeline(m_sceneFullscreenPipeline);
        m_sceneFullscreenPipeline = {};
    }
    if (m_fullscreenPipeline.isValid()) {
        device.destroyPipeline(m_fullscreenPipeline);
        m_fullscreenPipeline = {};
    }

    ImageDesc backgroundDesc{};
    backgroundDesc.width = width;
    backgroundDesc.height = height;
    backgroundDesc.format = PixelFormat::RGBA16Float;
    backgroundDesc.usage = ImageUsage::Storage | ImageUsage::Sampled;
    backgroundDesc.debugName = "SceneBackgroundImage";
    if (device.createImage(backgroundDesc, &m_backgroundImage) != RHIResult::Success) {
        return false;
    }

    ImageDesc sceneColorDesc{};
    sceneColorDesc.width = width;
    sceneColorDesc.height = height;
    sceneColorDesc.format = colorFormat;
    sceneColorDesc.usage = ImageUsage::ColorAttachment | ImageUsage::Sampled;
    sceneColorDesc.debugName = "SceneColorImage";
    if (device.createImage(sceneColorDesc, &m_sceneColorImage) != RHIResult::Success) {
        return false;
    }

    ImageDesc sceneDepthDesc{};
    sceneDepthDesc.width = width;
    sceneDepthDesc.height = height;
    sceneDepthDesc.format = PixelFormat::D32Float;
    sceneDepthDesc.usage = ImageUsage::DepthStencilAttachment;
    sceneDepthDesc.debugName = "SceneDepthImage";
    if (device.createImage(sceneDepthDesc, &m_sceneDepthImage) != RHIResult::Success) {
        return false;
    }

    ResourceSetWriteDesc computeWrite{};
    computeWrite.images.push_back({.binding = 0, .image = m_backgroundImage, .sampler = {}, .type = ResourceType::StorageImage});
    ResourceSetWriteDesc backgroundSampleWrite{};
    backgroundSampleWrite.images.push_back(
        {.binding = 0, .image = m_backgroundImage, .sampler = m_linearSampler, .type = ResourceType::CombinedImageSampler});
    ResourceSetWriteDesc sceneColorSampleWrite{};
    sceneColorSampleWrite.images.push_back(
        {.binding = 0, .image = m_sceneColorImage, .sampler = m_linearSampler, .type = ResourceType::CombinedImageSampler});
    if (device.updateResourceSet(m_computeSet, computeWrite) != RHIResult::Success ||
        device.updateResourceSet(m_backgroundSampleSet, backgroundSampleWrite) != RHIResult::Success ||
        device.updateResourceSet(m_sceneColorSampleSet, sceneColorSampleWrite) != RHIResult::Success) {
        return false;
    }

    GraphicsPipelineDesc fullscreenPipelineDesc{};
    fullscreenPipelineDesc.debugName = "SceneFullscreenPresentPipeline";
    const std::string fullscreenVertexShaderPath = scene_shader_path("rhi_scene_fullscreen.vert.spv");
    const std::string fullscreenFragmentShaderPath = scene_shader_path("rhi_scene_fullscreen.frag.spv");
    fullscreenPipelineDesc.vertexShader = {.stage = ShaderType::Vertex, .filePath = fullscreenVertexShaderPath};
    fullscreenPipelineDesc.fragmentShader = {.stage = ShaderType::Fragment, .filePath = fullscreenFragmentShaderPath};
    fullscreenPipelineDesc.resourceLayouts.push_back(m_textureLayout);
    fullscreenPipelineDesc.cullMode = CullMode::None;
    fullscreenPipelineDesc.frontFace = FrontFace::Clockwise;
    fullscreenPipelineDesc.colorAttachments.push_back({colorFormat, false});
    if (device.createGraphicsPipeline(fullscreenPipelineDesc, &m_fullscreenPipeline) != RHIResult::Success) {
        return false;
    }

    GraphicsPipelineDesc sceneFullscreenPipelineDesc = fullscreenPipelineDesc;
    sceneFullscreenPipelineDesc.debugName = "SceneFullscreenPipeline";
    sceneFullscreenPipelineDesc.depthStencil = {PixelFormat::D32Float, false, false, CompareOp::LessOrEqual};
    if (device.createGraphicsPipeline(sceneFullscreenPipelineDesc, &m_sceneFullscreenPipeline) != RHIResult::Success) {
        return false;
    }

    GraphicsPipelineDesc scenePipelineDesc{};
    scenePipelineDesc.debugName = "RhiScenePipeline";
    const std::string sceneVertexShaderPath = scene_shader_path("rhi_scene_mesh.vert.spv");
    const std::string sceneFragmentShaderPath = scene_shader_path("rhi_scene_mesh.frag.spv");
    scenePipelineDesc.vertexShader = {.stage = ShaderType::Vertex, .filePath = sceneVertexShaderPath};
    scenePipelineDesc.fragmentShader = {.stage = ShaderType::Fragment, .filePath = sceneFragmentShaderPath};
    scenePipelineDesc.resourceLayouts.push_back(m_sceneLayout);
    scenePipelineDesc.vertexLayout.stride = sizeof(SceneVertex);
    scenePipelineDesc.vertexLayout.attributes.push_back({0, 0, VertexFormat::Float3});
    scenePipelineDesc.vertexLayout.attributes.push_back({1, 12, VertexFormat::Float3});
    scenePipelineDesc.vertexLayout.attributes.push_back({2, 24, VertexFormat::Float2});
    scenePipelineDesc.vertexLayout.attributes.push_back({3, 32, VertexFormat::Float4});
    scenePipelineDesc.cullMode = CullMode::Back;
    scenePipelineDesc.frontFace = FrontFace::CounterClockwise;
    scenePipelineDesc.pushConstantSize = sizeof(glm::mat4);
    scenePipelineDesc.pushConstantVisibility = ShaderType::Vertex;
    scenePipelineDesc.colorAttachments.push_back({colorFormat, false});
    scenePipelineDesc.depthStencil = {PixelFormat::D32Float, true, true, CompareOp::LessOrEqual};
    if (device.createGraphicsPipeline(scenePipelineDesc, &m_scenePipeline) != RHIResult::Success) {
        return false;
    }

    m_pipelineFormat = colorFormat;
    m_scaledWidth = width;
    m_scaledHeight = height;
    return true;
}

void SceneRenderPipeline::collect_draw_items(const std::shared_ptr<SceneNode>& node,
                                             const SceneRecord& scene,
                                             std::vector<DrawItem>& drawItems) const
{
    if (!node) {
        return;
    }

    if (const auto meshNode = std::dynamic_pointer_cast<SceneMeshNode>(node)) {
        const auto meshIt = scene.meshes.find(meshNode->meshName);
        if (meshIt != scene.meshes.end()) {
            for (const MeshSurface& surface : meshIt->second.surfaces) {
                drawItems.push_back(
                    {.vertexBuffer = meshIt->second.vertexBuffer,
                     .indexBuffer = meshIt->second.indexBuffer,
                     .firstIndex = surface.firstIndex,
                     .indexCount = surface.indexCount,
                     .materialIndex = surface.materialIndex,
                     .worldMatrix = meshNode->worldTransform});
            }
        }
    }

    for (const auto& child : node->children) {
        collect_draw_items(child, scene, drawItems);
    }
}

PipelineHandle SceneRenderPipeline::current_effect_pipeline() const
{
    if (m_backgroundPipelines.empty()) {
        return {};
    }

    const size_t clampedIndex =
        static_cast<size_t>(std::clamp(m_currentBackgroundEffect, 0, static_cast<int>(m_backgroundPipelines.size() - 1)));
    return m_backgroundPipelines[clampedIndex];
}

std::shared_ptr<IRenderPipeline> CreateDefaultSceneRenderPipeline()
{
    return std::make_shared<SceneRenderPipeline>();
}

} // namespace luna
