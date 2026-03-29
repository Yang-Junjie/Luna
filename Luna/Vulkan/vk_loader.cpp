#include "vk_loader.h"
#include "vk_engine.h"

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>

namespace {

Vertex makeDefaultVertex()
{
    Vertex vertex{};
    vertex.color = {1.0f, 1.0f, 1.0f, 1.0f};
    return vertex;
}

} // namespace

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(VulkanEngine* engine,
                                                                      const std::filesystem::path& filePath)
{
    if (engine == nullptr) {
        LUNA_CORE_ERROR("Cannot load glTF meshes because VulkanEngine is null");
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

    constexpr auto gltfOptions = fastgltf::Options::None;

    fastgltf::Parser parser{};
    auto asset = parser.loadGltf(gltfData.get(), filePath.parent_path(), gltfOptions, fastgltf::Category::OnlyRenderable);
    if (asset.error() != fastgltf::Error::None) {
        LUNA_CORE_ERROR("Failed to parse glTF file '{}': {} ({})",
                        filePath.string(),
                        fastgltf::getErrorName(asset.error()),
                        fastgltf::getErrorMessage(asset.error()));
        return std::nullopt;
    }

    auto& gltf = asset.get();
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    meshes.reserve(gltf.meshes.size());

    for (const auto& gltfMesh : gltf.meshes) {
        auto mesh = std::make_shared<MeshAsset>();
        mesh->name = gltfMesh.name.empty() ? "unnamed_mesh" : std::string(gltfMesh.name);

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
            continue;
        }

        mesh->meshBuffers = engine->uploadMesh(indices, vertices);
        meshes.push_back(std::move(mesh));
    }

    if (meshes.empty()) {
        LUNA_CORE_ERROR("No renderable meshes were loaded from '{}'", filePath.string());
        return std::nullopt;
    }

    LUNA_CORE_INFO("Loaded {} mesh asset(s) from '{}'", meshes.size(), filePath.string());
    return meshes;
}

