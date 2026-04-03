#include "vk_loader.h"
#include "vk_engine.h"

#include <stb_image.h>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/tools.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

namespace {

Vertex makeDefaultVertex()
{
    Vertex vertex{};
    vertex.color = {1.0f, 1.0f, 1.0f, 1.0f};
    return vertex;
}

glm::mat4 to_glm_matrix(const fastgltf::math::fmat4x4& matrix)
{
    glm::mat4 result{1.0f};
    for (int column = 0; column < 4; column++) {
        for (int row = 0; row < 4; row++) {
            result[column][row] = matrix[column][row];
        }
    }
    return result;
}

std::string make_mesh_name(const fastgltf::Mesh& mesh, size_t index)
{
    if (!mesh.name.empty()) {
        return std::string(mesh.name);
    }

    return "mesh_" + std::to_string(index);
}

std::string make_node_name(const fastgltf::Node& node, size_t index)
{
    if (!node.name.empty()) {
        return std::string(node.name);
    }

    return "node_" + std::to_string(index);
}

vk::SamplerAddressMode extract_wrap(fastgltf::Wrap wrap)
{
    switch (wrap) {
        case fastgltf::Wrap::ClampToEdge:
            return vk::SamplerAddressMode::eClampToEdge;
        case fastgltf::Wrap::MirroredRepeat:
            return vk::SamplerAddressMode::eMirroredRepeat;
        case fastgltf::Wrap::Repeat:
        default:
            return vk::SamplerAddressMode::eRepeat;
    }
}

vk::Filter extract_filter(fastgltf::Filter filter)
{
    switch (filter) {
        case fastgltf::Filter::Nearest:
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::NearestMipMapLinear:
            return vk::Filter::eNearest;
        case fastgltf::Filter::Linear:
        case fastgltf::Filter::LinearMipMapNearest:
        case fastgltf::Filter::LinearMipMapLinear:
        default:
            return vk::Filter::eLinear;
    }
}

vk::SamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter)
{
    switch (filter) {
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::LinearMipMapNearest:
            return vk::SamplerMipmapMode::eNearest;
        case fastgltf::Filter::NearestMipMapLinear:
        case fastgltf::Filter::LinearMipMapLinear:
            return vk::SamplerMipmapMode::eLinear;
        case fastgltf::Filter::Nearest:
        case fastgltf::Filter::Linear:
        default:
            return vk::SamplerMipmapMode::eLinear;
    }
}

std::vector<std::byte> read_binary_file(const std::filesystem::path& path, size_t offset = 0)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    if (offset >= fileSize) {
        return {};
    }

    std::vector<std::byte> bytes(fileSize - offset);
    file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return bytes;
}

std::vector<std::byte> extract_bytes_from_data_source(const fastgltf::Asset& asset,
                                                      const fastgltf::DataSource& source,
                                                      const std::filesystem::path& basePath);

std::vector<std::byte> extract_bytes_from_buffer_view(const fastgltf::Asset& asset,
                                                      size_t bufferViewIndex,
                                                      const std::filesystem::path& basePath)
{
    if (bufferViewIndex >= asset.bufferViews.size()) {
        return {};
    }

    const auto& bufferView = asset.bufferViews[bufferViewIndex];
    if (bufferView.bufferIndex >= asset.buffers.size()) {
        return {};
    }

    std::vector<std::byte> bytes = extract_bytes_from_data_source(asset, asset.buffers[bufferView.bufferIndex].data, basePath);
    if (bytes.empty()) {
        return {};
    }

    if (bufferView.byteOffset >= bytes.size()) {
        return {};
    }

    const size_t availableSize = bytes.size() - bufferView.byteOffset;
    const size_t copySize = std::min(bufferView.byteLength, availableSize);
    return std::vector<std::byte>(bytes.begin() + static_cast<std::ptrdiff_t>(bufferView.byteOffset),
                                  bytes.begin() + static_cast<std::ptrdiff_t>(bufferView.byteOffset + copySize));
}

std::vector<std::byte> extract_bytes_from_data_source(const fastgltf::Asset& asset,
                                                      const fastgltf::DataSource& source,
                                                      const std::filesystem::path& basePath)
{
    if (std::holds_alternative<fastgltf::sources::Array>(source)) {
        const auto& array = std::get<fastgltf::sources::Array>(source);
        return std::vector<std::byte>(array.bytes.begin(), array.bytes.end());
    }

    if (std::holds_alternative<fastgltf::sources::Vector>(source)) {
        const auto& vector = std::get<fastgltf::sources::Vector>(source);
        return vector.bytes;
    }

    if (std::holds_alternative<fastgltf::sources::ByteView>(source)) {
        const auto& view = std::get<fastgltf::sources::ByteView>(source);
        return std::vector<std::byte>(view.bytes.begin(), view.bytes.end());
    }

    if (std::holds_alternative<fastgltf::sources::URI>(source)) {
        const auto& filePath = std::get<fastgltf::sources::URI>(source);
        if (filePath.uri.isDataUri()) {
            return {};
        }

        return read_binary_file(basePath / filePath.uri.fspath(), filePath.fileByteOffset);
    }

    if (std::holds_alternative<fastgltf::sources::BufferView>(source)) {
        const auto& bufferView = std::get<fastgltf::sources::BufferView>(source);
        return extract_bytes_from_buffer_view(asset, bufferView.bufferViewIndex, basePath);
    }

    return {};
}

std::optional<AllocatedImage> load_image_from_data_source(VulkanEngine* engine,
                                                          const fastgltf::Asset& asset,
                                                          const fastgltf::DataSource& source,
                                                          const std::filesystem::path& basePath,
                                                          vk::Format format)
{
    if (engine == nullptr) {
        return std::nullopt;
    }

    auto decode_memory = [&](const std::byte* data, size_t size) -> std::optional<AllocatedImage> {
        if (data == nullptr || size == 0) {
            return std::nullopt;
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* pixels = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(data), static_cast<int>(size), &width, &height, &channels, 4);
        if (pixels == nullptr) {
            return std::nullopt;
        }

        AllocatedImage image =
            engine->create_image(
                pixels,
                vk::Extent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
                format,
                vk::ImageUsageFlagBits::eSampled);
        stbi_image_free(pixels);
        return image;
    };

    if (std::holds_alternative<fastgltf::sources::URI>(source)) {
        const auto& filePath = std::get<fastgltf::sources::URI>(source);
        if (!filePath.uri.isDataUri()) {
            const std::filesystem::path path = basePath / filePath.uri.fspath();
            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
            if (pixels != nullptr) {
                AllocatedImage image = engine->create_image(
                    pixels,
                    vk::Extent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
                    format,
                    vk::ImageUsageFlagBits::eSampled);
                stbi_image_free(pixels);
                return image;
            }
        }
    }

    const std::vector<std::byte> bytes = extract_bytes_from_data_source(asset, source, basePath);
    if (bytes.empty()) {
        return std::nullopt;
    }

    return decode_memory(bytes.data(), bytes.size());
}

vk::Sampler create_sampler(VulkanEngine* engine, const fastgltf::Sampler& sampler)
{
    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter =
        sampler.magFilter.has_value() ? extract_filter(sampler.magFilter.value()) : vk::Filter::eLinear;
    samplerInfo.minFilter =
        sampler.minFilter.has_value() ? extract_filter(sampler.minFilter.value()) : vk::Filter::eLinear;
    samplerInfo.mipmapMode =
        sampler.minFilter.has_value() ? extract_mipmap_mode(sampler.minFilter.value()) : vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU = extract_wrap(sampler.wrapS);
    samplerInfo.addressModeV = extract_wrap(sampler.wrapT);
    samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

    vk::Sampler newSampler{};
    VK_CHECK(engine->_device.createSampler(&samplerInfo, nullptr, &newSampler));
    return newSampler;
}

glm::vec4 to_glm_vec4(const fastgltf::math::nvec4& value)
{
    return glm::vec4(value.x(), value.y(), value.z(), value.w());
}

glm::vec3 to_glm_vec3(const fastgltf::math::fvec3& value)
{
    return glm::vec3(value.x(), value.y(), value.z());
}

glm::vec3 quaternion_to_euler_degrees(const glm::quat& q)
{
    const double sinr_cosp = 2.0 * (q.w * q.x + q.y * q.z);
    const double cosr_cosp = 1.0 - 2.0 * (q.x * q.x + q.y * q.y);
    const double rollX = std::atan2(sinr_cosp, cosr_cosp);

    const double sinp = 2.0 * (q.w * q.y - q.z * q.x);
    double pitchY = 0.0;
    if (std::abs(sinp) >= 1.0) {
        pitchY = std::copysign(glm::half_pi<double>(), sinp);
    } else {
        pitchY = std::asin(sinp);
    }

    const double siny_cosp = 2.0 * (q.w * q.z + q.x * q.y);
    const double cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
    const double yawZ = std::atan2(siny_cosp, cosy_cosp);

    return glm::degrees(glm::vec3(static_cast<float>(rollX), static_cast<float>(pitchY), static_cast<float>(yawZ)));
}

} // namespace

void DrawContext::clear()
{
    opaqueSurfaces.clear();
    transparentSurfaces.clear();
}

void Node::updateLocalTransform()
{
    const glm::vec3 rotationRadians = glm::radians(rotationEulerDegrees);
    const glm::quat rotationX = glm::angleAxis(rotationRadians.x, glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::quat rotationY = glm::angleAxis(rotationRadians.y, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::quat rotationZ = glm::angleAxis(rotationRadians.z, glm::vec3(0.0f, 0.0f, 1.0f));
    const glm::quat rotation = rotationZ * rotationY * rotationX;

    localTransform = glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) *
                     glm::scale(glm::mat4(1.0f), scale);
}

void Node::refreshTransform(const glm::mat4& parentMatrix)
{
    worldTransform = parentMatrix * localTransform;

    for (const auto& child : children) {
        if (child) {
            child->refreshTransform(worldTransform);
        }
    }
}

void Node::Draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
    for (const auto& child : children) {
        if (child) {
            child->Draw(topMatrix, ctx);
        }
    }
}

void MeshNode::Draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
    const glm::mat4 nodeMatrix = topMatrix * worldTransform;

    if (mesh) {
        for (const GeoSurface& surface : mesh->surfaces) {
            RenderObject draw;
            draw.indexCount = surface.count;
            draw.firstIndex = surface.startIndex;
            draw.indexBuffer = mesh->meshBuffers.indexBuffer.buffer;
            draw.material = surface.material;
            draw.transform = nodeMatrix;
            draw.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;

            if (draw.material != nullptr && draw.material->passType == MaterialPass::Transparent) {
                ctx.transparentSurfaces.push_back(draw);
            } else {
                ctx.opaqueSurfaces.push_back(draw);
            }
        }
    }

    for (const auto& child : children) {
        if (child) {
            child->Draw(topMatrix, ctx);
        }
    }
}

LoadedGLTF::~LoadedGLTF()
{
    clearAll();
}

void LoadedGLTF::Draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
    for (const auto& node : topNodes) {
        if (!node) {
            continue;
        }

        node->refreshTransform(glm::mat4(1.0f));
        node->Draw(topMatrix, ctx);
    }
}

void LoadedGLTF::clearAll()
{
    if (creator != nullptr) {
        for (auto& [_, mesh] : meshes) {
            if (!mesh) {
                continue;
            }

            creator->destroy_buffer(mesh->meshBuffers.indexBuffer);
            creator->destroy_buffer(mesh->meshBuffers.vertexBuffer);
        }

        for (auto& buffer : materialDataBuffers) {
            if (buffer.buffer && buffer.allocation != VK_NULL_HANDLE) {
                creator->destroy_buffer(buffer);
            }
        }

        for (const vk::Sampler sampler : samplers) {
            if (!sampler || sampler == creator->_defaultSamplerLinear ||
                sampler == creator->_defaultSamplerNearest) {
                continue;
            }

            creator->_device.destroySampler(sampler, nullptr);
        }

        for (const auto& image : images) {
            if (!image.image || image.image == creator->_whiteImage.image ||
                image.image == creator->_blackImage.image || image.image == creator->_greyImage.image ||
                image.image == creator->_errorCheckerboardImage.image) {
                continue;
            }

            creator->destroy_image(image);
        }

        materialDescriptorPool.destroy_pool(creator->_device);
    }

    topNodes.clear();
    nodes.clear();
    meshes.clear();
    materials.clear();
    materialDataBuffers.clear();
    samplers.clear();
    images.clear();
}

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine* engine, const std::filesystem::path& filePath)
{
    if (engine == nullptr) {
        LUNA_CORE_ERROR("Cannot load glTF scene because VulkanEngine is null");
        return std::nullopt;
    }

    auto gltfData = fastgltf::GltfDataBuffer::FromPath(filePath);
    if (gltfData.error() != fastgltf::Error::None) {
        LUNA_CORE_ERROR("Failed to read glTF file '{}': {} ({})",
                        filePath.string(),
                        fastgltf::getErrorName(gltfData.error()),
                        fastgltf::getErrorMessage(gltfData.error()));
        return std::nullopt;
    }

    constexpr auto gltfOptions =
        fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages |
        fastgltf::Options::DecomposeNodeMatrices;

    fastgltf::Parser parser{};
    auto asset = parser.loadGltf(gltfData.get(), filePath.parent_path(), gltfOptions, fastgltf::Category::All);
    if (asset.error() != fastgltf::Error::None) {
        LUNA_CORE_ERROR("Failed to parse glTF file '{}': {} ({})",
                        filePath.string(),
                        fastgltf::getErrorName(asset.error()),
                        fastgltf::getErrorMessage(asset.error()));
        return std::nullopt;
    }

    auto& gltf = asset.get();
    auto scene = std::make_shared<LoadedGLTF>();
    scene->creator = engine;

    scene->samplers.reserve(gltf.samplers.size());
    for (const auto& sampler : gltf.samplers) {
        scene->samplers.push_back(create_sampler(engine, sampler));
    }

    scene->images.resize(gltf.images.size());
    for (size_t imageIndex = 0; imageIndex < gltf.images.size(); imageIndex++) {
        const auto& image = gltf.images[imageIndex];
        const auto loadedImage =
            load_image_from_data_source(engine, gltf, image.data, filePath.parent_path(), vk::Format::eR8G8B8A8Srgb);
        if (loadedImage.has_value()) {
            scene->images[imageIndex] = loadedImage.value();
        } else {
            scene->images[imageIndex] = engine->_errorCheckerboardImage;
            LUNA_CORE_WARN("Falling back to error texture for glTF image {}", imageIndex);
        }
    }

    {
        std::vector<DescriptorAllocator::PoolSizeRatio> materialPoolRatios = {
            {vk::DescriptorType::eUniformBuffer, 1.0f},
            {vk::DescriptorType::eCombinedImageSampler, 2.0f},
        };
        scene->materialDescriptorPool.init_pool(
            engine->_device, std::max(1u, static_cast<uint32_t>(gltf.materials.size())), materialPoolRatios);
    }

    scene->materials.resize(gltf.materials.size());
    scene->materialDataBuffers.reserve(gltf.materials.size());
    for (size_t materialIndex = 0; materialIndex < gltf.materials.size(); materialIndex++) {
        const auto& gltfMaterial = gltf.materials[materialIndex];

        MaterialConstants constants{};
        constants.colorFactors = to_glm_vec4(gltfMaterial.pbrData.baseColorFactor);
        constants.metal_rough_factors =
            glm::vec4(static_cast<float>(gltfMaterial.pbrData.metallicFactor),
                      static_cast<float>(gltfMaterial.pbrData.roughnessFactor),
                      0.0f,
                      0.0f);

        AllocatedBuffer materialBuffer =
            engine->create_buffer(
                sizeof(MaterialConstants), vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
        std::memcpy(materialBuffer.info.pMappedData, &constants, sizeof(MaterialConstants));
        scene->materialDataBuffers.push_back(materialBuffer);

        MaterialResources resources{};
        resources.dataBuffer = scene->materialDataBuffers.back();

        resources.colorImage = engine->_whiteImage;
        resources.colorSampler = engine->_defaultSamplerLinear;
        if (gltfMaterial.pbrData.baseColorTexture.has_value()) {
            const auto& textureInfo = gltfMaterial.pbrData.baseColorTexture.value();
            if (textureInfo.textureIndex < gltf.textures.size()) {
                const auto& texture = gltf.textures[textureInfo.textureIndex];
                if (texture.imageIndex.has_value() && texture.imageIndex.value() < scene->images.size() &&
                    scene->images[texture.imageIndex.value()].image) {
                    resources.colorImage = scene->images[texture.imageIndex.value()];
                }
                if (texture.samplerIndex.has_value() && texture.samplerIndex.value() < scene->samplers.size()) {
                    resources.colorSampler = scene->samplers[texture.samplerIndex.value()];
                }
            }
        }

        resources.metalRoughImage = engine->_whiteImage;
        resources.metalRoughSampler = engine->_defaultSamplerLinear;
        if (gltfMaterial.pbrData.metallicRoughnessTexture.has_value()) {
            const auto& textureInfo = gltfMaterial.pbrData.metallicRoughnessTexture.value();
            if (textureInfo.textureIndex < gltf.textures.size()) {
                const auto& texture = gltf.textures[textureInfo.textureIndex];
                if (texture.imageIndex.has_value() && texture.imageIndex.value() < scene->images.size() &&
                    scene->images[texture.imageIndex.value()].image) {
                    resources.metalRoughImage = scene->images[texture.imageIndex.value()];
                }
                if (texture.samplerIndex.has_value() && texture.samplerIndex.value() < scene->samplers.size()) {
                    resources.metalRoughSampler = scene->samplers[texture.samplerIndex.value()];
                }
            }
        }

        const MaterialPass pass =
            gltfMaterial.alphaMode == fastgltf::AlphaMode::Blend ? MaterialPass::Transparent : MaterialPass::MainColor;
        scene->materials[materialIndex] =
            std::make_shared<MaterialInstance>(
                engine->metalRoughMaterial.write_material(engine->_device, pass, resources, scene->materialDescriptorPool));
    }

    std::vector<std::shared_ptr<MeshAsset>> meshesByIndex;
    meshesByIndex.reserve(gltf.meshes.size());

    for (size_t meshIndex = 0; meshIndex < gltf.meshes.size(); meshIndex++) {
        const auto& gltfMesh = gltf.meshes[meshIndex];
        auto mesh = std::make_shared<MeshAsset>();
        mesh->name = make_mesh_name(gltfMesh, meshIndex);

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        for (const auto& primitive : gltfMesh.primitives) {
            if (primitive.type != fastgltf::PrimitiveType::Triangles) {
                LUNA_CORE_WARN("Skipping non-triangle primitive in mesh '{}'", mesh->name);
                continue;
            }

            const auto positionIt = primitive.findAttribute("POSITION");
            if (positionIt == primitive.attributes.end()) {
                LUNA_CORE_WARN("Skipping primitive without POSITION attribute in mesh '{}'", mesh->name);
                continue;
            }

            const auto& positionAccessor = gltf.accessors[positionIt->accessorIndex];
            const auto initialVertex = static_cast<uint32_t>(vertices.size());

            vertices.resize(vertices.size() + positionAccessor.count, makeDefaultVertex());
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                gltf, positionAccessor, [&](fastgltf::math::fvec3 position, std::size_t index) {
                    auto& vertex = vertices[initialVertex + index];
                    vertex.position = {position.x(), position.y(), position.z()};
                });

            if (const auto normalIt = primitive.findAttribute("NORMAL"); normalIt != primitive.attributes.end()) {
                const auto& normalAccessor = gltf.accessors[normalIt->accessorIndex];
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                    gltf, normalAccessor, [&](fastgltf::math::fvec3 normal, std::size_t index) {
                        auto& vertex = vertices[initialVertex + index];
                        vertex.normal = {normal.x(), normal.y(), normal.z()};
                        vertex.color = {(normal.x() + 1.0f) * 0.5f,
                                        (normal.y() + 1.0f) * 0.5f,
                                        (normal.z() + 1.0f) * 0.5f,
                                        1.0f};
                    });
            }

            if (const auto uvIt = primitive.findAttribute("TEXCOORD_0"); uvIt != primitive.attributes.end()) {
                const auto& uvAccessor = gltf.accessors[uvIt->accessorIndex];
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                    gltf, uvAccessor, [&](fastgltf::math::fvec2 uv, std::size_t index) {
                        auto& vertex = vertices[initialVertex + index];
                        vertex.uv_x = uv.x();
                        vertex.uv_y = uv.y();
                    });
            }

            GeoSurface surface{};
            surface.startIndex = static_cast<uint32_t>(indices.size());
            surface.material = &engine->_defaultMaterialInstance;
            if (primitive.materialIndex.has_value() && primitive.materialIndex.value() < scene->materials.size() &&
                scene->materials[primitive.materialIndex.value()]) {
                surface.material = scene->materials[primitive.materialIndex.value()].get();
            }

            if (primitive.indicesAccessor.has_value()) {
                const auto& indexAccessor = gltf.accessors[primitive.indicesAccessor.value()];
                std::vector<uint32_t> primitiveIndices(indexAccessor.count);
                fastgltf::copyFromAccessor<uint32_t>(gltf, indexAccessor, primitiveIndices.data());

                for (const auto index : primitiveIndices) {
                    indices.push_back(initialVertex + index);
                }

                surface.count = static_cast<uint32_t>(primitiveIndices.size());
            } else {
                surface.count = static_cast<uint32_t>(positionAccessor.count);
                for (uint32_t index = 0; index < surface.count; index++) {
                    indices.push_back(initialVertex + index);
                }
            }

            mesh->surfaces.push_back(surface);
        }

        if (mesh->surfaces.empty()) {
            LUNA_CORE_WARN("Mesh '{}' did not contain any renderable triangle primitives", mesh->name);
            meshesByIndex.push_back(nullptr);
            continue;
        }

        mesh->meshBuffers = engine->uploadMesh(indices, vertices);
        std::string meshName = mesh->name;
        if (scene->meshes.contains(meshName)) {
            meshName += "_" + std::to_string(meshIndex);
            mesh->name = meshName;
        }
        scene->meshes[meshName] = mesh;
        meshesByIndex.push_back(std::move(mesh));
    }

    if (scene->meshes.empty()) {
        LUNA_CORE_ERROR("No renderable meshes were loaded from '{}'", filePath.string());
        return std::nullopt;
    }

    std::vector<std::shared_ptr<Node>> nodesByIndex(gltf.nodes.size());
    for (size_t nodeIndex = 0; nodeIndex < gltf.nodes.size(); nodeIndex++) {
        const auto& gltfNode = gltf.nodes[nodeIndex];

        std::shared_ptr<Node> newNode;
        if (gltfNode.meshIndex.has_value()) {
            auto meshNode = std::make_shared<MeshNode>();
            const size_t meshIndex = gltfNode.meshIndex.value();
            if (meshIndex < meshesByIndex.size()) {
                meshNode->mesh = meshesByIndex[meshIndex];
            }
            newNode = meshNode;
        } else {
            newNode = std::make_shared<Node>();
        }

        std::string nodeName = make_node_name(gltfNode, nodeIndex);
        if (scene->nodes.contains(nodeName)) {
            nodeName += "_" + std::to_string(nodeIndex);
        }
        newNode->name = nodeName;

        if (const auto* trs = std::get_if<fastgltf::TRS>(&gltfNode.transform)) {
            newNode->translation = to_glm_vec3(trs->translation);
            newNode->scale = to_glm_vec3(trs->scale);

            const glm::quat rotationQuat(
                trs->rotation.w(), trs->rotation.x(), trs->rotation.y(), trs->rotation.z());
            newNode->rotationEulerDegrees = quaternion_to_euler_degrees(rotationQuat);
            newNode->initialTranslation = newNode->translation;
            newNode->initialRotationEulerDegrees = newNode->rotationEulerDegrees;
            newNode->initialScale = newNode->scale;
            newNode->updateLocalTransform();
        } else {
            newNode->localTransform = to_glm_matrix(std::get<fastgltf::math::fmat4x4>(gltfNode.transform));
        }

        newNode->worldTransform = newNode->localTransform;
        scene->nodes[nodeName] = newNode;
        nodesByIndex[nodeIndex] = newNode;
    }

    for (size_t nodeIndex = 0; nodeIndex < gltf.nodes.size(); nodeIndex++) {
        const auto& gltfNode = gltf.nodes[nodeIndex];
        const auto& parentNode = nodesByIndex[nodeIndex];
        if (!parentNode) {
            continue;
        }

        for (const size_t childIndex : gltfNode.children) {
            if (childIndex >= nodesByIndex.size() || !nodesByIndex[childIndex]) {
                continue;
            }

            parentNode->children.push_back(nodesByIndex[childIndex]);
            nodesByIndex[childIndex]->parent = parentNode;
        }
    }

    if (gltf.defaultScene.has_value() && gltf.defaultScene.value() < gltf.scenes.size()) {
        const auto& defaultScene = gltf.scenes[gltf.defaultScene.value()];
        for (const size_t nodeIndex : defaultScene.nodeIndices) {
            if (nodeIndex < nodesByIndex.size() && nodesByIndex[nodeIndex]) {
                scene->topNodes.push_back(nodesByIndex[nodeIndex]);
            }
        }
    } else {
        for (const auto& node : nodesByIndex) {
            if (node && node->parent.expired()) {
                scene->topNodes.push_back(node);
            }
        }
    }

    if (scene->topNodes.empty()) {
        LUNA_CORE_ERROR("No scene nodes were loaded from '{}'", filePath.string());
        return std::nullopt;
    }

    LUNA_CORE_INFO("Loaded glTF scene '{}' with {} mesh asset(s) and {} node(s)",
                   filePath.filename().string(),
                   scene->meshes.size(),
                   scene->nodes.size());
    return scene;
}

