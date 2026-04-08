#include "VkLoader.h"
#include "VkEngine.h"

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
    vertex.m_color = {1.0f, 1.0f, 1.0f, 1.0f};
    return vertex;
}

glm::mat4 toGlmMatrix(const fastgltf::math::fmat4x4& matrix)
{
    glm::mat4 result{1.0f};
    for (int column = 0; column < 4; column++) {
        for (int row = 0; row < 4; row++) {
            result[column][row] = matrix[column][row];
        }
    }
    return result;
}

std::string makeMeshName(const fastgltf::Mesh& mesh, size_t index)
{
    if (!mesh.name.empty()) {
        return std::string(mesh.name);
    }

    return "mesh_" + std::to_string(index);
}

std::string makeNodeName(const fastgltf::Node& node, size_t index)
{
    if (!node.name.empty()) {
        return std::string(node.name);
    }

    return "node_" + std::to_string(index);
}

vk::SamplerAddressMode extractWrap(fastgltf::Wrap wrap)
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

vk::Filter extractFilter(fastgltf::Filter filter)
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

vk::SamplerMipmapMode extractMipmapMode(fastgltf::Filter filter)
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

std::vector<std::byte> readBinaryFile(const std::filesystem::path& path, size_t offset = 0)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    const size_t file_size = static_cast<size_t>(file.tellg());
    if (offset >= file_size) {
        return {};
    }

    std::vector<std::byte> bytes(file_size - offset);
    file.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return bytes;
}

std::vector<std::byte> extractBytesFromDataSource(const fastgltf::Asset& asset,
                                                      const fastgltf::DataSource& source,
                                                      const std::filesystem::path& base_path);

std::vector<std::byte> extractBytesFromBufferView(const fastgltf::Asset& asset,
                                                      size_t buffer_view_index,
                                                      const std::filesystem::path& base_path)
{
    if (buffer_view_index >= asset.bufferViews.size()) {
        return {};
    }

    const auto& buffer_view = asset.bufferViews[buffer_view_index];
    if (buffer_view.bufferIndex >= asset.buffers.size()) {
        return {};
    }

    std::vector<std::byte> bytes = extractBytesFromDataSource(asset, asset.buffers[buffer_view.bufferIndex].data, base_path);
    if (bytes.empty()) {
        return {};
    }

    if (buffer_view.byteOffset >= bytes.size()) {
        return {};
    }

    const size_t available_size = bytes.size() - buffer_view.byteOffset;
    const size_t copy_size = std::min(buffer_view.byteLength, available_size);
    return std::vector<std::byte>(bytes.begin() + static_cast<std::ptrdiff_t>(buffer_view.byteOffset),
                                  bytes.begin() + static_cast<std::ptrdiff_t>(buffer_view.byteOffset + copy_size));
}

std::vector<std::byte> extractBytesFromDataSource(const fastgltf::Asset& asset,
                                                      const fastgltf::DataSource& source,
                                                      const std::filesystem::path& base_path)
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
        const auto& file_path = std::get<fastgltf::sources::URI>(source);
        if (file_path.uri.isDataUri()) {
            return {};
        }

        return readBinaryFile(base_path / file_path.uri.fspath(), file_path.fileByteOffset);
    }

    if (std::holds_alternative<fastgltf::sources::BufferView>(source)) {
        const auto& buffer_view = std::get<fastgltf::sources::BufferView>(source);
        return extractBytesFromBufferView(asset, buffer_view.bufferViewIndex, base_path);
    }

    return {};
}

std::optional<AllocatedImage> loadImageFromDataSource(VulkanEngine* engine,
                                                          const fastgltf::Asset& asset,
                                                          const fastgltf::DataSource& source,
                                                          const std::filesystem::path& base_path,
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
            engine->createImage(
                pixels,
                vk::Extent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
                format,
                vk::ImageUsageFlagBits::eSampled);
        stbi_image_free(pixels);
        return image;
    };

    if (std::holds_alternative<fastgltf::sources::URI>(source)) {
        const auto& file_path = std::get<fastgltf::sources::URI>(source);
        if (!file_path.uri.isDataUri()) {
            const std::filesystem::path path = base_path / file_path.uri.fspath();
            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
            if (pixels != nullptr) {
                AllocatedImage image = engine->createImage(
                    pixels,
                    vk::Extent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
                    format,
                    vk::ImageUsageFlagBits::eSampled);
                stbi_image_free(pixels);
                return image;
            }
        }
    }

    const std::vector<std::byte> bytes = extractBytesFromDataSource(asset, source, base_path);
    if (bytes.empty()) {
        return std::nullopt;
    }

    return decode_memory(bytes.data(), bytes.size());
}

vk::Sampler createSampler(VulkanEngine* engine, const fastgltf::Sampler& sampler)
{
    vk::SamplerCreateInfo sampler_info{};
    sampler_info.magFilter =
        sampler.magFilter.has_value() ? extractFilter(sampler.magFilter.value()) : vk::Filter::eLinear;
    sampler_info.minFilter =
        sampler.minFilter.has_value() ? extractFilter(sampler.minFilter.value()) : vk::Filter::eLinear;
    sampler_info.mipmapMode =
        sampler.minFilter.has_value() ? extractMipmapMode(sampler.minFilter.value()) : vk::SamplerMipmapMode::eLinear;
    sampler_info.addressModeU = extractWrap(sampler.wrapS);
    sampler_info.addressModeV = extractWrap(sampler.wrapT);
    sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = VK_LOD_CLAMP_NONE;

    vk::Sampler new_sampler{};
    VK_CHECK(engine->m_device.createSampler(&sampler_info, nullptr, &new_sampler));
    return new_sampler;
}

glm::vec4 toGlmVec4(const fastgltf::math::nvec4& value)
{
    return glm::vec4(value.x(), value.y(), value.z(), value.w());
}

glm::vec3 toGlmVec3(const fastgltf::math::fvec3& value)
{
    return glm::vec3(value.x(), value.y(), value.z());
}

glm::vec3 quaternionToEulerDegrees(const glm::quat& q)
{
    const double sinr_cosp = 2.0 * (q.w * q.x + q.y * q.z);
    const double cosr_cosp = 1.0 - 2.0 * (q.x * q.x + q.y * q.y);
    const double roll_x = std::atan2(sinr_cosp, cosr_cosp);

    const double sinp = 2.0 * (q.w * q.y - q.z * q.x);
    double pitch_y = 0.0;
    if (std::abs(sinp) >= 1.0) {
        pitch_y = std::copysign(glm::half_pi<double>(), sinp);
    } else {
        pitch_y = std::asin(sinp);
    }

    const double siny_cosp = 2.0 * (q.w * q.z + q.x * q.y);
    const double cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
    const double yaw_z = std::atan2(siny_cosp, cosy_cosp);

    return glm::degrees(glm::vec3(static_cast<float>(roll_x), static_cast<float>(pitch_y), static_cast<float>(yaw_z)));
}

} // namespace

void DrawContext::clear()
{
    m_opaque_surfaces.clear();
    m_transparent_surfaces.clear();
}

void Node::updateLocalTransform()
{
    const glm::vec3 rotation_radians = glm::radians(m_rotation_euler_degrees);
    const glm::quat rotation_x = glm::angleAxis(rotation_radians.x, glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::quat rotation_y = glm::angleAxis(rotation_radians.y, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::quat rotation_z = glm::angleAxis(rotation_radians.z, glm::vec3(0.0f, 0.0f, 1.0f));
    const glm::quat rotation = rotation_z * rotation_y * rotation_x;

    m_local_transform = glm::translate(glm::mat4(1.0f), m_translation) * glm::mat4_cast(rotation) *
                     glm::scale(glm::mat4(1.0f), m_scale);
}

void Node::refreshTransform(const glm::mat4& parent_matrix)
{
    m_world_transform = parent_matrix * m_local_transform;

    for (const auto& child : m_children) {
        if (child) {
            child->refreshTransform(m_world_transform);
        }
    }
}

void Node::draw(const glm::mat4& top_matrix, DrawContext& ctx)
{
    for (const auto& child : m_children) {
        if (child) {
            child->draw(top_matrix, ctx);
        }
    }
}

void MeshNode::draw(const glm::mat4& top_matrix, DrawContext& ctx)
{
    const glm::mat4 node_matrix = top_matrix * m_world_transform;

    if (m_mesh) {
        for (const GeoSurface& surface : m_mesh->m_surfaces) {
            RenderObject draw;
            draw.m_index_count = surface.m_count;
            draw.m_first_index = surface.m_start_index;
            draw.m_index_buffer = m_mesh->m_mesh_buffers.m_index_buffer.m_buffer;
            draw.m_material = surface.m_material;
            draw.m_transform = node_matrix;
            draw.m_vertex_buffer_address = m_mesh->m_mesh_buffers.m_vertex_buffer_address;

            if (draw.m_material != nullptr && draw.m_material->m_pass_type == MaterialPass::Transparent) {
                ctx.m_transparent_surfaces.push_back(draw);
            } else {
                ctx.m_opaque_surfaces.push_back(draw);
            }
        }
    }

    for (const auto& child : m_children) {
        if (child) {
            child->draw(top_matrix, ctx);
        }
    }
}

LoadedGLTF::~LoadedGLTF()
{
    clearAll();
}

void LoadedGLTF::draw(const glm::mat4& top_matrix, DrawContext& ctx)
{
    for (const auto& node : m_top_nodes) {
        if (!node) {
            continue;
        }

        node->refreshTransform(glm::mat4(1.0f));
        node->draw(top_matrix, ctx);
    }
}

void LoadedGLTF::clearAll()
{
    if (m_creator != nullptr) {
        for (auto& [_, mesh] : m_meshes) {
            if (!mesh) {
                continue;
            }

            m_creator->destroyBuffer(mesh->m_mesh_buffers.m_index_buffer);
            m_creator->destroyBuffer(mesh->m_mesh_buffers.m_vertex_buffer);
        }

        for (auto& buffer : m_material_data_buffers) {
            if (buffer.m_buffer && buffer.m_allocation != VK_NULL_HANDLE) {
                m_creator->destroyBuffer(buffer);
            }
        }

        for (const vk::Sampler sampler : m_samplers) {
            if (!sampler || sampler == m_creator->m_default_sampler_linear ||
                sampler == m_creator->m_default_sampler_nearest) {
                continue;
            }

            m_creator->m_device.destroySampler(sampler, nullptr);
        }

        for (const auto& image : m_images) {
            if (!image.m_image || image.m_image == m_creator->m_white_image.m_image ||
                image.m_image == m_creator->m_black_image.m_image || image.m_image == m_creator->m_grey_image.m_image ||
                image.m_image == m_creator->m_error_checkerboard_image.m_image) {
                continue;
            }

            m_creator->destroyImage(image);
        }

        m_material_descriptor_pool.destroyPool(m_creator->m_device);
    }

    m_top_nodes.clear();
    m_nodes.clear();
    m_meshes.clear();
    m_materials.clear();
    m_material_data_buffers.clear();
    m_samplers.clear();
    m_images.clear();
}

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine* engine, const std::filesystem::path& file_path)
{
    if (engine == nullptr) {
        LUNA_CORE_ERROR("Cannot load glTF scene because VulkanEngine is null");
        return std::nullopt;
    }

    auto gltf_data = fastgltf::GltfDataBuffer::FromPath(file_path);
    if (gltf_data.error() != fastgltf::Error::None) {
        LUNA_CORE_ERROR("Failed to read glTF file '{}': {} ({})",
                        file_path.string(),
                        fastgltf::getErrorName(gltf_data.error()),
                        fastgltf::getErrorMessage(gltf_data.error()));
        return std::nullopt;
    }

    constexpr auto gltf_options =
        fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages |
        fastgltf::Options::DecomposeNodeMatrices;

    fastgltf::Parser parser{};
    auto asset = parser.loadGltf(gltf_data.get(), file_path.parent_path(), gltf_options, fastgltf::Category::All);
    if (asset.error() != fastgltf::Error::None) {
        LUNA_CORE_ERROR("Failed to parse glTF file '{}': {} ({})",
                        file_path.string(),
                        fastgltf::getErrorName(asset.error()),
                        fastgltf::getErrorMessage(asset.error()));
        return std::nullopt;
    }

    auto& gltf = asset.get();
    auto scene = std::make_shared<LoadedGLTF>();
    scene->m_creator = engine;

    scene->m_samplers.reserve(gltf.samplers.size());
    for (const auto& sampler : gltf.samplers) {
        scene->m_samplers.push_back(createSampler(engine, sampler));
    }

    scene->m_images.resize(gltf.images.size());
    for (size_t image_index = 0; image_index < gltf.images.size(); image_index++) {
        const auto& image = gltf.images[image_index];
        const auto loaded_image =
            loadImageFromDataSource(engine, gltf, image.data, file_path.parent_path(), vk::Format::eR8G8B8A8Srgb);
        if (loaded_image.has_value()) {
            scene->m_images[image_index] = loaded_image.value();
        } else {
            scene->m_images[image_index] = engine->m_error_checkerboard_image;
            LUNA_CORE_WARN("Falling back to error texture for glTF image {}", image_index);
        }
    }

    {
        std::vector<DescriptorAllocator::PoolSizeRatio> material_pool_ratios = {
            {vk::DescriptorType::eUniformBuffer, 1.0f},
            {vk::DescriptorType::eCombinedImageSampler, 2.0f},
        };
        scene->m_material_descriptor_pool.initPool(
            engine->m_device, std::max(1u, static_cast<uint32_t>(gltf.materials.size())), material_pool_ratios);
    }

    scene->m_materials.resize(gltf.materials.size());
    scene->m_material_data_buffers.reserve(gltf.materials.size());
    for (size_t material_index = 0; material_index < gltf.materials.size(); material_index++) {
        const auto& gltf_material = gltf.materials[material_index];

        MaterialConstants constants{};
        constants.m_color_factors = toGlmVec4(gltf_material.pbrData.baseColorFactor);
        constants.m_metal_rough_factors =
            glm::vec4(static_cast<float>(gltf_material.pbrData.metallicFactor),
                      static_cast<float>(gltf_material.pbrData.roughnessFactor),
                      0.0f,
                      0.0f);

        AllocatedBuffer material_buffer =
            engine->createBuffer(
                sizeof(MaterialConstants), vk::BufferUsageFlagBits::eUniformBuffer, VMA_MEMORY_USAGE_CPU_TO_GPU);
        std::memcpy(material_buffer.m_info.pMappedData, &constants, sizeof(MaterialConstants));
        scene->m_material_data_buffers.push_back(material_buffer);

        MaterialResources resources{};
        resources.m_data_buffer = scene->m_material_data_buffers.back();

        resources.m_color_image = engine->m_white_image;
        resources.m_color_sampler = engine->m_default_sampler_linear;
        if (gltf_material.pbrData.baseColorTexture.has_value()) {
            const auto& texture_info = gltf_material.pbrData.baseColorTexture.value();
            if (texture_info.textureIndex < gltf.textures.size()) {
                const auto& texture = gltf.textures[texture_info.textureIndex];
                if (texture.imageIndex.has_value() && texture.imageIndex.value() < scene->m_images.size() &&
                    scene->m_images[texture.imageIndex.value()].m_image) {
                    resources.m_color_image = scene->m_images[texture.imageIndex.value()];
                }
                if (texture.samplerIndex.has_value() && texture.samplerIndex.value() < scene->m_samplers.size()) {
                    resources.m_color_sampler = scene->m_samplers[texture.samplerIndex.value()];
                }
            }
        }

        resources.m_metal_rough_image = engine->m_white_image;
        resources.m_metal_rough_sampler = engine->m_default_sampler_linear;
        if (gltf_material.pbrData.metallicRoughnessTexture.has_value()) {
            const auto& texture_info = gltf_material.pbrData.metallicRoughnessTexture.value();
            if (texture_info.textureIndex < gltf.textures.size()) {
                const auto& texture = gltf.textures[texture_info.textureIndex];
                if (texture.imageIndex.has_value() && texture.imageIndex.value() < scene->m_images.size() &&
                    scene->m_images[texture.imageIndex.value()].m_image) {
                    resources.m_metal_rough_image = scene->m_images[texture.imageIndex.value()];
                }
                if (texture.samplerIndex.has_value() && texture.samplerIndex.value() < scene->m_samplers.size()) {
                    resources.m_metal_rough_sampler = scene->m_samplers[texture.samplerIndex.value()];
                }
            }
        }

        const MaterialPass pass =
            gltf_material.alphaMode == fastgltf::AlphaMode::Blend ? MaterialPass::Transparent : MaterialPass::MainColor;
        scene->m_materials[material_index] =
            std::make_shared<MaterialInstance>(
                engine->m_metal_rough_material.writeMaterial(engine->m_device, pass, resources, scene->m_material_descriptor_pool));
    }

    std::vector<std::shared_ptr<MeshAsset>> meshes_by_index;
    meshes_by_index.reserve(gltf.meshes.size());

    for (size_t mesh_index = 0; mesh_index < gltf.meshes.size(); mesh_index++) {
        const auto& gltf_mesh = gltf.meshes[mesh_index];
        auto mesh = std::make_shared<MeshAsset>();
        mesh->m_name = makeMeshName(gltf_mesh, mesh_index);

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        for (const auto& primitive : gltf_mesh.primitives) {
            if (primitive.type != fastgltf::PrimitiveType::Triangles) {
                LUNA_CORE_WARN("Skipping non-triangle primitive in mesh '{}'", mesh->m_name);
                continue;
            }

            const auto position_it = primitive.findAttribute("POSITION");
            if (position_it == primitive.attributes.end()) {
                LUNA_CORE_WARN("Skipping primitive without POSITION attribute in mesh '{}'", mesh->m_name);
                continue;
            }

            const auto& position_accessor = gltf.accessors[position_it->accessorIndex];
            const auto initial_vertex = static_cast<uint32_t>(vertices.size());

            vertices.resize(vertices.size() + position_accessor.count, makeDefaultVertex());
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                gltf, position_accessor, [&](fastgltf::math::fvec3 position, std::size_t index) {
                    auto& vertex = vertices[initial_vertex + index];
                    vertex.m_position = {position.x(), position.y(), position.z()};
                });

            if (const auto normal_it = primitive.findAttribute("NORMAL"); normal_it != primitive.attributes.end()) {
                const auto& normal_accessor = gltf.accessors[normal_it->accessorIndex];
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                    gltf, normal_accessor, [&](fastgltf::math::fvec3 normal, std::size_t index) {
                        auto& vertex = vertices[initial_vertex + index];
                        vertex.m_normal = {normal.x(), normal.y(), normal.z()};
                        vertex.m_color = {(normal.x() + 1.0f) * 0.5f,
                                        (normal.y() + 1.0f) * 0.5f,
                                        (normal.z() + 1.0f) * 0.5f,
                                        1.0f};
                    });
            }

            if (const auto uv_it = primitive.findAttribute("TEXCOORD_0"); uv_it != primitive.attributes.end()) {
                const auto& uv_accessor = gltf.accessors[uv_it->accessorIndex];
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                    gltf, uv_accessor, [&](fastgltf::math::fvec2 uv, std::size_t index) {
                        auto& vertex = vertices[initial_vertex + index];
                        vertex.m_uv_x = uv.x();
                        vertex.m_uv_y = uv.y();
                    });
            }

            GeoSurface surface{};
            surface.m_start_index = static_cast<uint32_t>(indices.size());
            surface.m_material = &engine->m_default_material_instance;
            if (primitive.materialIndex.has_value() && primitive.materialIndex.value() < scene->m_materials.size() &&
                scene->m_materials[primitive.materialIndex.value()]) {
                surface.m_material = scene->m_materials[primitive.materialIndex.value()].get();
            }

            if (primitive.indicesAccessor.has_value()) {
                const auto& index_accessor = gltf.accessors[primitive.indicesAccessor.value()];
                std::vector<uint32_t> primitive_indices(index_accessor.count);
                fastgltf::copyFromAccessor<uint32_t>(gltf, index_accessor, primitive_indices.data());

                for (const auto index : primitive_indices) {
                    indices.push_back(initial_vertex + index);
                }

                surface.m_count = static_cast<uint32_t>(primitive_indices.size());
            } else {
                surface.m_count = static_cast<uint32_t>(position_accessor.count);
                for (uint32_t index = 0; index < surface.m_count; index++) {
                    indices.push_back(initial_vertex + index);
                }
            }

            mesh->m_surfaces.push_back(surface);
        }

        if (mesh->m_surfaces.empty()) {
            LUNA_CORE_WARN("Mesh '{}' did not contain any renderable triangle primitives", mesh->m_name);
            meshes_by_index.push_back(nullptr);
            continue;
        }

        mesh->m_mesh_buffers = engine->uploadMesh(indices, vertices);
        std::string mesh_name = mesh->m_name;
        if (scene->m_meshes.contains(mesh_name)) {
            mesh_name += "_" + std::to_string(mesh_index);
            mesh->m_name = mesh_name;
        }
        scene->m_meshes[mesh_name] = mesh;
        meshes_by_index.push_back(std::move(mesh));
    }

    if (scene->m_meshes.empty()) {
        LUNA_CORE_ERROR("No renderable meshes were loaded from '{}'", file_path.string());
        return std::nullopt;
    }

    std::vector<std::shared_ptr<Node>> nodes_by_index(gltf.nodes.size());
    for (size_t node_index = 0; node_index < gltf.nodes.size(); node_index++) {
        const auto& gltf_node = gltf.nodes[node_index];

        std::shared_ptr<Node> new_node;
        if (gltf_node.meshIndex.has_value()) {
            auto mesh_node = std::make_shared<MeshNode>();
            const size_t mesh_index = gltf_node.meshIndex.value();
            if (mesh_index < meshes_by_index.size()) {
                mesh_node->m_mesh = meshes_by_index[mesh_index];
            }
            new_node = mesh_node;
        } else {
            new_node = std::make_shared<Node>();
        }

        std::string node_name = makeNodeName(gltf_node, node_index);
        if (scene->m_nodes.contains(node_name)) {
            node_name += "_" + std::to_string(node_index);
        }
        new_node->m_name = node_name;

        if (const auto* trs = std::get_if<fastgltf::TRS>(&gltf_node.transform)) {
            new_node->m_translation = toGlmVec3(trs->translation);
            new_node->m_scale = toGlmVec3(trs->scale);

            const glm::quat rotation_quat(
                trs->rotation.w(), trs->rotation.x(), trs->rotation.y(), trs->rotation.z());
            new_node->m_rotation_euler_degrees = quaternionToEulerDegrees(rotation_quat);
            new_node->m_initial_translation = new_node->m_translation;
            new_node->m_initial_rotation_euler_degrees = new_node->m_rotation_euler_degrees;
            new_node->m_initial_scale = new_node->m_scale;
            new_node->updateLocalTransform();
        } else {
            new_node->m_local_transform = toGlmMatrix(std::get<fastgltf::math::fmat4x4>(gltf_node.transform));
        }

        new_node->m_world_transform = new_node->m_local_transform;
        scene->m_nodes[node_name] = new_node;
        nodes_by_index[node_index] = new_node;
    }

    for (size_t node_index = 0; node_index < gltf.nodes.size(); node_index++) {
        const auto& gltf_node = gltf.nodes[node_index];
        const auto& parent_node = nodes_by_index[node_index];
        if (!parent_node) {
            continue;
        }

        for (const size_t child_index : gltf_node.children) {
            if (child_index >= nodes_by_index.size() || !nodes_by_index[child_index]) {
                continue;
            }

            parent_node->m_children.push_back(nodes_by_index[child_index]);
            nodes_by_index[child_index]->m_parent = parent_node;
        }
    }

    if (gltf.defaultScene.has_value() && gltf.defaultScene.value() < gltf.scenes.size()) {
        const auto& default_scene = gltf.scenes[gltf.defaultScene.value()];
        for (const size_t node_index : default_scene.nodeIndices) {
            if (node_index < nodes_by_index.size() && nodes_by_index[node_index]) {
                scene->m_top_nodes.push_back(nodes_by_index[node_index]);
            }
        }
    } else {
        for (const auto& node : nodes_by_index) {
            if (node && node->m_parent.expired()) {
                scene->m_top_nodes.push_back(node);
            }
        }
    }

    if (scene->m_top_nodes.empty()) {
        LUNA_CORE_ERROR("No scene nodes were loaded from '{}'", file_path.string());
        return std::nullopt;
    }

    LUNA_CORE_INFO("Loaded glTF scene '{}' with {} mesh asset(s) and {} node(s)",
                   file_path.filename().string(),
                   scene->m_meshes.size(),
                   scene->m_nodes.size());
    return scene;
}


