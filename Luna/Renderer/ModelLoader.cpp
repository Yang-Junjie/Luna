#include "Renderer/ModelLoader.h"

#include <cassert>
#include <cmath>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <optional>
#include <string_view>

#if defined(_MSC_VER)
#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#endif

#define TINYOBJLOADER_IMPLEMENTATION
#include "third_party/tinyobjloader/tiny_obj_loader.h"

namespace luna::rhi {
namespace {

std::string getAbsolutePathToObjResource(const std::string& obj_path, const std::string& relative_path)
{
    return (std::filesystem::path(obj_path).parent_path() / relative_path).string();
}

ImageData createStubTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return ImageData{
        .ByteData = {r, g, b, a},
        .ImageFormat = Cacao::Format::RGBA8_UNORM,
        .Width = 1,
        .Height = 1,
    };
}

glm::vec2 toVec2(const fastgltf::math::fvec2& value)
{
    return {value[0], value[1]};
}

glm::vec3 toVec3(const fastgltf::math::fvec3& value)
{
    return {value[0], value[1], value[2]};
}

std::pair<glm::vec3, glm::vec3> computeTangentSpace(const glm::vec3& p1,
                                                    const glm::vec3& p2,
                                                    const glm::vec3& p3,
                                                    const glm::vec2& uv1,
                                                    const glm::vec2& uv2,
                                                    const glm::vec2& uv3)
{
    const glm::vec3 edge1 = p2 - p1;
    const glm::vec3 edge2 = p3 - p1;
    const glm::vec2 delta_uv1 = uv2 - uv1;
    const glm::vec2 delta_uv2 = uv3 - uv1;
    const float determinant = delta_uv1.x * delta_uv2.y - delta_uv2.x * delta_uv1.y;

    if (std::abs(determinant) < 1e-6f) {
        return {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    }

    const float inv_determinant = 1.0f / determinant;
    glm::vec3 tangent = inv_determinant * (delta_uv2.y * edge1 - delta_uv1.y * edge2);
    glm::vec3 bitangent = inv_determinant * (-delta_uv2.x * edge1 + delta_uv1.x * edge2);

    if (glm::length2(tangent) > 0.0f) {
        tangent = glm::normalize(tangent);
    }
    if (glm::length2(bitangent) > 0.0f) {
        bitangent = glm::normalize(bitangent);
    }

    return {tangent, bitangent};
}

std::vector<std::pair<glm::vec3, glm::vec3>> computeTangentsBitangents(const tinyobj::mesh_t& mesh,
                                                                       const tinyobj::attrib_t& attrib)
{
    std::vector<std::pair<glm::vec3, glm::vec3>> tangents_bitangents(attrib.vertices.size() / 3,
                                                                     {glm::vec3(0.0f), glm::vec3(0.0f)});

    ModelData::Index index_offset = 0;
    for (const size_t face_vertex_count : mesh.num_face_vertices) {
        if (face_vertex_count != 3) {
            index_offset += static_cast<ModelData::Index>(face_vertex_count);
            continue;
        }

        const tinyobj::index_t idx1 = mesh.indices[index_offset + 0];
        const tinyobj::index_t idx2 = mesh.indices[index_offset + 1];
        const tinyobj::index_t idx3 = mesh.indices[index_offset + 2];

        if (idx1.vertex_index < 0 || idx2.vertex_index < 0 || idx3.vertex_index < 0 || idx1.texcoord_index < 0 ||
            idx2.texcoord_index < 0 || idx3.texcoord_index < 0) {
            index_offset += 3;
            continue;
        }

        const glm::vec3 position1{
            attrib.vertices[3 * size_t(idx1.vertex_index) + 0],
            attrib.vertices[3 * size_t(idx1.vertex_index) + 1],
            attrib.vertices[3 * size_t(idx1.vertex_index) + 2],
        };
        const glm::vec3 position2{
            attrib.vertices[3 * size_t(idx2.vertex_index) + 0],
            attrib.vertices[3 * size_t(idx2.vertex_index) + 1],
            attrib.vertices[3 * size_t(idx2.vertex_index) + 2],
        };
        const glm::vec3 position3{
            attrib.vertices[3 * size_t(idx3.vertex_index) + 0],
            attrib.vertices[3 * size_t(idx3.vertex_index) + 1],
            attrib.vertices[3 * size_t(idx3.vertex_index) + 2],
        };

        const glm::vec2 tex_coord1{
            attrib.texcoords[2 * size_t(idx1.texcoord_index) + 0],
            attrib.texcoords[2 * size_t(idx1.texcoord_index) + 1],
        };
        const glm::vec2 tex_coord2{
            attrib.texcoords[2 * size_t(idx2.texcoord_index) + 0],
            attrib.texcoords[2 * size_t(idx2.texcoord_index) + 1],
        };
        const glm::vec2 tex_coord3{
            attrib.texcoords[2 * size_t(idx3.texcoord_index) + 0],
            attrib.texcoords[2 * size_t(idx3.texcoord_index) + 1],
        };

        const auto tangent_space =
            computeTangentSpace(position1, position2, position3, tex_coord1, tex_coord2, tex_coord3);
        tangents_bitangents[idx1.vertex_index].first += tangent_space.first;
        tangents_bitangents[idx1.vertex_index].second += tangent_space.second;
        tangents_bitangents[idx2.vertex_index].first += tangent_space.first;
        tangents_bitangents[idx2.vertex_index].second += tangent_space.second;
        tangents_bitangents[idx3.vertex_index].first += tangent_space.first;
        tangents_bitangents[idx3.vertex_index].second += tangent_space.second;

        index_offset += 3;
    }

    for (auto& [tangent, bitangent] : tangents_bitangents) {
        if (glm::length2(tangent) > 0.0f) {
            tangent = glm::normalize(tangent);
        }
        if (glm::length2(bitangent) > 0.0f) {
            bitangent = glm::normalize(bitangent);
        }
    }

    return tangents_bitangents;
}

std::vector<std::pair<glm::vec3, glm::vec3>> computeTangentsBitangents(const std::vector<ModelData::Index>& indices,
                                                                       const std::vector<glm::vec3>& positions,
                                                                       const std::vector<glm::vec2>& tex_coords)
{
    std::vector<std::pair<glm::vec3, glm::vec3>> tangents_bitangents(positions.size(),
                                                                     {glm::vec3(0.0f), glm::vec3(0.0f)});

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const auto i0 = indices[i + 0];
        const auto i1 = indices[i + 1];
        const auto i2 = indices[i + 2];
        if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size() || i0 >= tex_coords.size() ||
            i1 >= tex_coords.size() || i2 >= tex_coords.size()) {
            continue;
        }

        const auto tangent_space = computeTangentSpace(
            positions[i0], positions[i1], positions[i2], tex_coords[i0], tex_coords[i1], tex_coords[i2]);

        tangents_bitangents[i0].first += tangent_space.first;
        tangents_bitangents[i0].second += tangent_space.second;
        tangents_bitangents[i1].first += tangent_space.first;
        tangents_bitangents[i1].second += tangent_space.second;
        tangents_bitangents[i2].first += tangent_space.first;
        tangents_bitangents[i2].second += tangent_space.second;
    }

    for (auto& [tangent, bitangent] : tangents_bitangents) {
        if (glm::length2(tangent) > 0.0f) {
            tangent = glm::normalize(tangent);
        }
        if (glm::length2(bitangent) > 0.0f) {
            bitangent = glm::normalize(bitangent);
        }
    }

    return tangents_bitangents;
}

const std::byte* getBufferData(const fastgltf::Buffer& buffer)
{
    return std::visit(fastgltf::visitor{
                          [](const fastgltf::sources::Array& source) -> const std::byte* {
                              return source.bytes.data();
                          },
                          [](const fastgltf::sources::ByteView& source) -> const std::byte* {
                              return source.bytes.data();
                          },
                          [](const auto&) -> const std::byte* {
                              return nullptr;
                          },
                      },
                      buffer.data);
}

ImageData loadFastGltfImageSource(const fastgltf::Asset& asset,
                                  const fastgltf::Image& image,
                                  const std::filesystem::path& base_path)
{
    return std::visit(
        fastgltf::visitor{
            [&](const fastgltf::sources::URI& source) -> ImageData {
                if (!source.uri.isLocalPath()) {
                    return {};
                }
                return ImageLoader::LoadImageFromFile((base_path / source.uri.fspath()).string());
            },
            [&](const fastgltf::sources::Array& source) -> ImageData {
                return ImageLoader::LoadImageFromMemory(reinterpret_cast<const uint8_t*>(source.bytes.data()),
                                                        source.bytes.size(),
                                                        std::string(fastgltf::getMimeTypeString(source.mimeType)));
            },
            [&](const fastgltf::sources::ByteView& source) -> ImageData {
                return ImageLoader::LoadImageFromMemory(reinterpret_cast<const uint8_t*>(source.bytes.data()),
                                                        source.bytes.size(),
                                                        std::string(fastgltf::getMimeTypeString(source.mimeType)));
            },
            [&](const fastgltf::sources::BufferView& source) -> ImageData {
                if (source.bufferViewIndex >= asset.bufferViews.size()) {
                    return {};
                }

                const auto& view = asset.bufferViews[source.bufferViewIndex];
                if (view.bufferIndex >= asset.buffers.size()) {
                    return {};
                }

                const std::byte* buffer_data = getBufferData(asset.buffers[view.bufferIndex]);
                if (buffer_data == nullptr) {
                    return {};
                }

                return ImageLoader::LoadImageFromMemory(reinterpret_cast<const uint8_t*>(buffer_data + view.byteOffset),
                                                        view.byteLength,
                                                        std::string(fastgltf::getMimeTypeString(source.mimeType)));
            },
            [](const auto&) -> ImageData {
                return {};
            },
        },
        image.data);
}

std::optional<std::size_t> getTextureImageIndex(const fastgltf::Texture& texture)
{
    if (texture.ddsImageIndex.has_value()) {
        return texture.ddsImageIndex;
    }
    if (texture.imageIndex.has_value()) {
        return texture.imageIndex;
    }
    if (texture.webpImageIndex.has_value()) {
        return texture.webpImageIndex;
    }
    if (texture.basisuImageIndex.has_value()) {
        return texture.basisuImageIndex;
    }
    return std::nullopt;
}

template <typename OptionalTextureInfoType>
ImageData loadMaterialTexture(const fastgltf::Asset& asset,
                              const OptionalTextureInfoType& texture_info,
                              const std::filesystem::path& base_path)
{
    if (!texture_info.has_value() || texture_info->textureIndex >= asset.textures.size()) {
        return {};
    }

    const auto image_index = getTextureImageIndex(asset.textures[texture_info->textureIndex]);
    if (!image_index.has_value() || *image_index >= asset.images.size()) {
        return {};
    }

    return loadFastGltfImageSource(asset, asset.images[*image_index], base_path);
}

bool isGLTFModel(const std::string& filepath)
{
    const auto extension = std::filesystem::path(filepath).extension();
    return extension == ".gltf" || extension == ".glb";
}

bool isObjModel(const std::string& filepath)
{
    return std::filesystem::path(filepath).extension() == ".obj";
}

} // namespace

ModelData ModelLoader::LoadFromObj(const std::string& filepath)
{
    ModelData result;

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(filepath, tinyobj::ObjReaderConfig{})) {
        return result;
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();
    const auto& materials = reader.GetMaterials();

    result.Materials.reserve(materials.size());
    for (const auto& material : materials) {
        auto& result_material = result.Materials.emplace_back();
        result_material.Name = material.name;
        result_material.AlbedoTexture =
            material.diffuse_texname.empty()
                ? createStubTexture(255, 255, 255, 255)
                : ImageLoader::LoadImageFromFile(getAbsolutePathToObjResource(filepath, material.diffuse_texname));
        result_material.NormalTexture =
            material.normal_texname.empty()
                ? createStubTexture(127, 127, 255, 255)
                : ImageLoader::LoadImageFromFile(getAbsolutePathToObjResource(filepath, material.normal_texname));
        result_material.MetallicRoughness = createStubTexture(0, 255, 0, 255);
    }

    result.Shapes.reserve(shapes.size());
    for (const auto& shape : shapes) {
        auto& result_shape = result.Shapes.emplace_back();
        result_shape.Name = shape.name;

        if (!shape.mesh.material_ids.empty() && shape.mesh.material_ids.front() >= 0) {
            result_shape.MaterialIndex = static_cast<uint32_t>(shape.mesh.material_ids.front());
        }

        std::vector<std::pair<glm::vec3, glm::vec3>> tangents_bitangents;
        if (!attrib.normals.empty() && !attrib.texcoords.empty()) {
            tangents_bitangents = computeTangentsBitangents(shape.mesh, attrib);
        }

        result_shape.Vertices.reserve(shape.mesh.indices.size());
        result_shape.Indices.reserve(shape.mesh.indices.size());

        ModelData::Index index_offset = 0;
        for (const size_t face_vertex_count : shape.mesh.num_face_vertices) {
            if (face_vertex_count != 3) {
                index_offset += static_cast<ModelData::Index>(face_vertex_count);
                continue;
            }

            for (size_t vertex_index = 0; vertex_index < face_vertex_count; ++vertex_index, ++index_offset) {
                const tinyobj::index_t idx = shape.mesh.indices[index_offset];
                if (idx.vertex_index < 0) {
                    continue;
                }

                auto& vertex = result_shape.Vertices.emplace_back();
                result_shape.Indices.push_back(static_cast<ModelData::Index>(result_shape.Indices.size()));

                vertex.Position = {
                    attrib.vertices[3 * size_t(idx.vertex_index) + 0],
                    attrib.vertices[3 * size_t(idx.vertex_index) + 1],
                    attrib.vertices[3 * size_t(idx.vertex_index) + 2],
                };

                if (idx.texcoord_index >= 0 && (2 * size_t(idx.texcoord_index) + 1) < attrib.texcoords.size()) {
                    vertex.TexCoord = {
                        attrib.texcoords[2 * size_t(idx.texcoord_index) + 0],
                        attrib.texcoords[2 * size_t(idx.texcoord_index) + 1],
                    };
                }

                if (idx.normal_index >= 0 && (3 * size_t(idx.normal_index) + 2) < attrib.normals.size()) {
                    vertex.Normal = {
                        attrib.normals[3 * size_t(idx.normal_index) + 0],
                        attrib.normals[3 * size_t(idx.normal_index) + 1],
                        attrib.normals[3 * size_t(idx.normal_index) + 2],
                    };
                }

                if (!tangents_bitangents.empty() && size_t(idx.vertex_index) < tangents_bitangents.size()) {
                    vertex.Tangent = tangents_bitangents[idx.vertex_index].first;
                    vertex.Bitangent = tangents_bitangents[idx.vertex_index].second;
                }
            }
        }
    }

    return result;
}

ModelData ModelLoader::LoadFromGltf(const std::string& filepath)
{
    ModelData result;

    const std::filesystem::path path(filepath);
    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None) {
        return result;
    }

    fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_transform | fastgltf::Extensions::KHR_mesh_quantization |
                            fastgltf::Extensions::MSFT_texture_dds | fastgltf::Extensions::EXT_texture_webp);

    auto asset =
        parser.loadGltf(data.get(),
                        path.parent_path(),
                        fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages |
                            fastgltf::Options::GenerateMeshIndices,
                        fastgltf::Category::Buffers | fastgltf::Category::BufferViews | fastgltf::Category::Accessors |
                            fastgltf::Category::Images | fastgltf::Category::Textures | fastgltf::Category::Materials |
                            fastgltf::Category::Meshes);

    if (asset.error() != fastgltf::Error::None) {
        return result;
    }

    result.Materials.reserve(asset->materials.size());
    for (const auto& material : asset->materials) {
        auto& result_material = result.Materials.emplace_back();
        result_material.Name = material.name;
        result_material.RoughnessScale = material.pbrData.roughnessFactor;
        result_material.MetallicScale = material.pbrData.metallicFactor;
        result_material.AlbedoTexture =
            loadMaterialTexture(asset.get(), material.pbrData.baseColorTexture, path.parent_path());
        result_material.NormalTexture = loadMaterialTexture(asset.get(), material.normalTexture, path.parent_path());
        result_material.MetallicRoughness =
            loadMaterialTexture(asset.get(), material.pbrData.metallicRoughnessTexture, path.parent_path());

        if (!result_material.AlbedoTexture.isValid()) {
            result_material.AlbedoTexture = createStubTexture(255, 255, 255, 255);
        }
        if (!result_material.NormalTexture.isValid()) {
            result_material.NormalTexture = createStubTexture(127, 127, 255, 255);
        }
        if (!result_material.MetallicRoughness.isValid()) {
            result_material.MetallicRoughness = createStubTexture(0, 255, 0, 255);
        }
    }

    for (const auto& mesh : asset->meshes) {
        for (size_t primitive_index = 0; primitive_index < mesh.primitives.size(); ++primitive_index) {
            const auto& primitive = mesh.primitives[primitive_index];
            if (primitive.type != fastgltf::PrimitiveType::Triangles) {
                continue;
            }

            const auto position_attribute = primitive.findAttribute("POSITION");
            if (position_attribute == primitive.attributes.end()) {
                continue;
            }

            auto& shape = result.Shapes.emplace_back();
            shape.Name = mesh.name.empty() ? "shape_" + std::to_string(result.Shapes.size())
                                           : std::string(mesh.name) + "_" + std::to_string(primitive_index);
            if (primitive.materialIndex.has_value()) {
                shape.MaterialIndex = static_cast<uint32_t>(*primitive.materialIndex);
            }

            const auto& position_accessor = asset->accessors[position_attribute->accessorIndex];
            std::vector<fastgltf::math::fvec3> positions(position_accessor.count);
            fastgltf::copyFromAccessor<fastgltf::math::fvec3>(asset.get(), position_accessor, positions.data());

            std::vector<fastgltf::math::fvec2> tex_coords;
            if (const auto tex_coord_attribute = primitive.findAttribute("TEXCOORD_0");
                tex_coord_attribute != primitive.attributes.end()) {
                const auto& tex_coord_accessor = asset->accessors[tex_coord_attribute->accessorIndex];
                tex_coords.resize(tex_coord_accessor.count);
                fastgltf::copyFromAccessor<fastgltf::math::fvec2>(asset.get(), tex_coord_accessor, tex_coords.data());
            }

            std::vector<fastgltf::math::fvec3> normals;
            if (const auto normal_attribute = primitive.findAttribute("NORMAL");
                normal_attribute != primitive.attributes.end()) {
                const auto& normal_accessor = asset->accessors[normal_attribute->accessorIndex];
                normals.resize(normal_accessor.count);
                fastgltf::copyFromAccessor<fastgltf::math::fvec3>(asset.get(), normal_accessor, normals.data());
            }

            if (primitive.indicesAccessor.has_value()) {
                const auto& index_accessor = asset->accessors[*primitive.indicesAccessor];
                shape.Indices.resize(index_accessor.count);
                fastgltf::copyFromAccessor<uint32_t>(asset.get(), index_accessor, shape.Indices.data());
            } else {
                shape.Indices.resize(positions.size());
                for (size_t i = 0; i < shape.Indices.size(); ++i) {
                    shape.Indices[i] = static_cast<ModelData::Index>(i);
                }
            }

            shape.Vertices.resize(positions.size());
            std::vector<glm::vec3> vertex_positions(shape.Vertices.size());
            std::vector<glm::vec2> vertex_tex_coords(shape.Vertices.size());
            const bool has_tex_coords = tex_coords.size() == shape.Vertices.size();
            const bool has_normals = normals.size() == shape.Vertices.size();

            for (size_t i = 0; i < shape.Vertices.size(); ++i) {
                auto& vertex = shape.Vertices[i];
                vertex.Position = toVec3(positions[i]);
                vertex_positions[i] = vertex.Position;

                if (has_tex_coords) {
                    vertex.TexCoord = toVec2(tex_coords[i]);
                    vertex_tex_coords[i] = vertex.TexCoord;
                }

                if (has_normals) {
                    vertex.Normal = toVec3(normals[i]);
                }
            }

            if (has_tex_coords && !shape.Indices.empty()) {
                const auto tangents_bitangents =
                    computeTangentsBitangents(shape.Indices, vertex_positions, vertex_tex_coords);
                for (size_t i = 0; i < shape.Vertices.size() && i < tangents_bitangents.size(); ++i) {
                    shape.Vertices[i].Tangent = tangents_bitangents[i].first;
                    shape.Vertices[i].Bitangent = tangents_bitangents[i].second;
                }
            }
        }
    }

    return result;
}

ModelData ModelLoader::Load(const std::string& filepath)
{
    if (isGLTFModel(filepath)) {
        return LoadFromGltf(filepath);
    }
    if (isObjModel(filepath)) {
        return LoadFromObj(filepath);
    }
    return {};
}

} // namespace luna::rhi
