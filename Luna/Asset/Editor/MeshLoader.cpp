#include "Asset/Editor/MeshLoader.h"

#include "Project/ProjectManager.h"

#include <cstdint>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>
#include <optional>
#include <unordered_map>
#include <vector>

#if defined(_MSC_VER)
#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#endif

#define TINYOBJLOADER_IMPLEMENTATION
#include "third_party/tinyobjloader/tiny_obj_loader.h"

namespace luna::mesh_loader_detail {

glm::vec2 toVec2(const fastgltf::math::fvec2& value)
{
    return {value[0], value[1]};
}

glm::vec3 toVec3(const fastgltf::math::fvec3& value)
{
    return {value[0], value[1], value[2]};
}

bool isObjModel(const std::filesystem::path& path)
{
    return path.extension() == ".obj";
}

bool isGltfModel(const std::filesystem::path& path)
{
    const auto extension = path.extension();
    return extension == ".gltf" || extension == ".glb";
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

void computeTangents(SubMesh& sub_mesh)
{
    for (auto& vertex : sub_mesh.Vertices) {
        vertex.Tangent = glm::vec3(0.0f);
        vertex.Bitangent = glm::vec3(0.0f);
    }

    for (size_t i = 0; i + 2 < sub_mesh.Indices.size(); i += 3) {
        const auto i0 = sub_mesh.Indices[i + 0];
        const auto i1 = sub_mesh.Indices[i + 1];
        const auto i2 = sub_mesh.Indices[i + 2];
        if (i0 >= sub_mesh.Vertices.size() || i1 >= sub_mesh.Vertices.size() || i2 >= sub_mesh.Vertices.size()) {
            continue;
        }

        auto& v0 = sub_mesh.Vertices[i0];
        auto& v1 = sub_mesh.Vertices[i1];
        auto& v2 = sub_mesh.Vertices[i2];
        const auto tangent_space = computeTangentSpace(
            v0.Position, v1.Position, v2.Position, v0.TexCoord, v1.TexCoord, v2.TexCoord);

        v0.Tangent += tangent_space.first;
        v1.Tangent += tangent_space.first;
        v2.Tangent += tangent_space.first;
        v0.Bitangent += tangent_space.second;
        v1.Bitangent += tangent_space.second;
        v2.Bitangent += tangent_space.second;
    }

    for (auto& vertex : sub_mesh.Vertices) {
        if (glm::length2(vertex.Tangent) > 0.0f) {
            vertex.Tangent = glm::normalize(vertex.Tangent);
        } else {
            vertex.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        }

        if (glm::length2(vertex.Bitangent) > 0.0f) {
            vertex.Bitangent = glm::normalize(vertex.Bitangent);
        } else {
            vertex.Bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }
}

void computeTangents(std::vector<SubMesh>& sub_meshes)
{
    for (auto& sub_mesh : sub_meshes) {
        if (!sub_mesh.Indices.empty() && !sub_mesh.Vertices.empty()) {
            computeTangents(sub_mesh);
        }
    }
}

std::shared_ptr<Mesh> buildMesh(std::string asset_name, std::vector<SubMesh> sub_meshes)
{
    if (sub_meshes.empty()) {
        return {};
    }

    if (asset_name.empty()) {
        asset_name = "Mesh";
    }

    computeTangents(sub_meshes);
    return Mesh::create(std::move(asset_name), std::move(sub_meshes));
}

} // namespace luna::mesh_loader_detail

namespace luna {

std::shared_ptr<Asset> MeshLoader::load(const AssetMetadata& meta_data)
{
    const auto project_root_path = ProjectManager::instance().getProjectRootPath();
    if (!project_root_path) {
        return {};
    }

    return loadFromFile(*project_root_path / meta_data.FilePath, meta_data.Name);
}

std::shared_ptr<Mesh> MeshLoader::loadFromFile(const std::filesystem::path& path, std::string asset_name)
{
    if (asset_name.empty()) {
        asset_name = path.stem().string();
    }

    if (mesh_loader_detail::isObjModel(path)) {
        return loadFromObj(path, std::move(asset_name));
    }
    if (mesh_loader_detail::isGltfModel(path)) {
        return loadFromGltf(path, std::move(asset_name));
    }

    return {};
}

std::shared_ptr<Mesh> MeshLoader::loadFromObj(const std::filesystem::path& path, std::string asset_name)
{
    tinyobj::ObjReader reader;
    tinyobj::ObjReaderConfig config;
    config.triangulate = true;
    config.mtl_search_path = path.parent_path().string();

    if (!reader.ParseFromFile(path.string(), config)) {
        return {};
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();

    std::vector<SubMesh> sub_meshes;

    for (size_t shape_index = 0; shape_index < shapes.size(); ++shape_index) {
        const auto& shape = shapes[shape_index];
        std::unordered_map<int, size_t> material_to_sub_mesh;

        auto get_or_create_sub_mesh = [&](int material_id) -> SubMesh& {
            if (const auto it = material_to_sub_mesh.find(material_id); it != material_to_sub_mesh.end()) {
                return sub_meshes[it->second];
            }

            SubMesh sub_mesh;
            sub_mesh.MaterialIndex = material_id >= 0 ? static_cast<uint32_t>(material_id) : UINT32_MAX;

            const std::string shape_name =
                shape.name.empty() ? asset_name + "_Shape_" + std::to_string(shape_index) : shape.name;
            sub_mesh.Name = material_id >= 0 ? shape_name + "_Mat_" + std::to_string(material_id) : shape_name;

            sub_meshes.push_back(std::move(sub_mesh));
            material_to_sub_mesh[material_id] = sub_meshes.size() - 1;
            return sub_meshes.back();
        };

        size_t index_offset = 0;
        for (size_t face_index = 0; face_index < shape.mesh.num_face_vertices.size(); ++face_index) {
            const auto face_vertex_count = shape.mesh.num_face_vertices[face_index];
            const int material_id =
                face_index < shape.mesh.material_ids.size() ? shape.mesh.material_ids[face_index] : -1;

            if (face_vertex_count != 3) {
                index_offset += face_vertex_count;
                continue;
            }

            SubMesh& sub_mesh = get_or_create_sub_mesh(material_id);
            const uint32_t base_index = static_cast<uint32_t>(sub_mesh.Vertices.size());

            for (size_t vertex_index = 0; vertex_index < face_vertex_count; ++vertex_index) {
                const tinyobj::index_t idx = shape.mesh.indices[index_offset + vertex_index];
                StaticMeshVertex vertex{};
                vertex.Tangent = glm::vec3(0.0f);
                vertex.Bitangent = glm::vec3(0.0f);

                if (idx.vertex_index >= 0 && (3 * size_t(idx.vertex_index) + 2) < attrib.vertices.size()) {
                    vertex.Position = glm::vec3(attrib.vertices[3 * size_t(idx.vertex_index) + 0],
                                                attrib.vertices[3 * size_t(idx.vertex_index) + 1],
                                                attrib.vertices[3 * size_t(idx.vertex_index) + 2]);
                }

                if (idx.normal_index >= 0 && (3 * size_t(idx.normal_index) + 2) < attrib.normals.size()) {
                    vertex.Normal = glm::vec3(attrib.normals[3 * size_t(idx.normal_index) + 0],
                                              attrib.normals[3 * size_t(idx.normal_index) + 1],
                                              attrib.normals[3 * size_t(idx.normal_index) + 2]);
                }

                if (idx.texcoord_index >= 0 && (2 * size_t(idx.texcoord_index) + 1) < attrib.texcoords.size()) {
                    vertex.TexCoord = glm::vec2(attrib.texcoords[2 * size_t(idx.texcoord_index) + 0],
                                                1.0f - attrib.texcoords[2 * size_t(idx.texcoord_index) + 1]);
                }

                sub_mesh.Vertices.push_back(vertex);
                sub_mesh.Indices.push_back(base_index + static_cast<uint32_t>(vertex_index));
            }

            index_offset += face_vertex_count;
        }
    }

    return mesh_loader_detail::buildMesh(std::move(asset_name), std::move(sub_meshes));
}

std::shared_ptr<Mesh> MeshLoader::loadFromGltf(const std::filesystem::path& path, std::string asset_name)
{
    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None) {
        return {};
    }

    fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_transform |
                            fastgltf::Extensions::KHR_mesh_quantization |
                            fastgltf::Extensions::MSFT_texture_dds |
                            fastgltf::Extensions::EXT_texture_webp);

    auto asset = parser.loadGltf(
        data.get(),
        path.parent_path(),
        fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages |
            fastgltf::Options::GenerateMeshIndices,
        fastgltf::Category::Buffers | fastgltf::Category::BufferViews | fastgltf::Category::Accessors |
            fastgltf::Category::Images | fastgltf::Category::Textures | fastgltf::Category::Materials |
            fastgltf::Category::Meshes);
    if (asset.error() != fastgltf::Error::None) {
        return {};
    }

    std::vector<SubMesh> sub_meshes;

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

            SubMesh sub_mesh;
            sub_mesh.Name = mesh.name.empty() ? asset_name + "_Primitive_" + std::to_string(sub_meshes.size())
                                              : std::string(mesh.name) + "_" + std::to_string(primitive_index);
            if (primitive.materialIndex.has_value()) {
                sub_mesh.MaterialIndex = static_cast<uint32_t>(*primitive.materialIndex);
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
                sub_mesh.Indices.resize(index_accessor.count);
                fastgltf::copyFromAccessor<uint32_t>(asset.get(), index_accessor, sub_mesh.Indices.data());
            } else {
                sub_mesh.Indices.resize(positions.size());
                for (size_t i = 0; i < sub_mesh.Indices.size(); ++i) {
                    sub_mesh.Indices[i] = static_cast<uint32_t>(i);
                }
            }

            sub_mesh.Vertices.resize(positions.size());
            for (size_t i = 0; i < sub_mesh.Vertices.size(); ++i) {
                auto& vertex = sub_mesh.Vertices[i];
                vertex.Position = mesh_loader_detail::toVec3(positions[i]);
                vertex.Tangent = glm::vec3(0.0f);
                vertex.Bitangent = glm::vec3(0.0f);

                if (tex_coords.size() == sub_mesh.Vertices.size()) {
                    vertex.TexCoord = mesh_loader_detail::toVec2(tex_coords[i]);
                }

                if (normals.size() == sub_mesh.Vertices.size()) {
                    vertex.Normal = mesh_loader_detail::toVec3(normals[i]);
                }
            }

            sub_meshes.push_back(std::move(sub_mesh));
        }
    }

    return mesh_loader_detail::buildMesh(std::move(asset_name), std::move(sub_meshes));
}

} // namespace luna
