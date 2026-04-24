#include "Renderer/PrimitiveMeshFactory.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace luna {
namespace {

constexpr float kPi = 3.14159265358979323846f;

StaticMeshVertex makeVertex(const glm::vec3& position,
                            const glm::vec2& tex_coord,
                            const glm::vec3& normal,
                            const glm::vec3& tangent,
                            const glm::vec3& bitangent)
{
    StaticMeshVertex vertex{};
    vertex.Position = position;
    vertex.TexCoord = tex_coord;
    vertex.Normal = normal;
    vertex.Tangent = tangent;
    vertex.Bitangent = bitangent;
    return vertex;
}

void addQuad(SubMesh& submesh,
             const glm::vec3& v0,
             const glm::vec3& v1,
             const glm::vec3& v2,
             const glm::vec3& v3,
             const glm::vec3& normal,
             const glm::vec3& tangent,
             const glm::vec3& bitangent)
{
    const uint32_t base_index = static_cast<uint32_t>(submesh.Vertices.size());
    submesh.Vertices.push_back(makeVertex(v0, {0.0f, 0.0f}, normal, tangent, bitangent));
    submesh.Vertices.push_back(makeVertex(v1, {1.0f, 0.0f}, normal, tangent, bitangent));
    submesh.Vertices.push_back(makeVertex(v2, {1.0f, 1.0f}, normal, tangent, bitangent));
    submesh.Vertices.push_back(makeVertex(v3, {0.0f, 1.0f}, normal, tangent, bitangent));

    submesh.Indices.push_back(base_index + 0);
    submesh.Indices.push_back(base_index + 1);
    submesh.Indices.push_back(base_index + 2);
    submesh.Indices.push_back(base_index + 2);
    submesh.Indices.push_back(base_index + 3);
    submesh.Indices.push_back(base_index + 0);
}

void addDisk(SubMesh& submesh, float y, float radius, const glm::vec3& normal, uint32_t segments)
{
    const uint32_t center_index = static_cast<uint32_t>(submesh.Vertices.size());
    const glm::vec3 tangent{1.0f, 0.0f, 0.0f};
    const glm::vec3 bitangent{0.0f, 0.0f, normal.y > 0.0f ? -1.0f : 1.0f};
    submesh.Vertices.push_back(makeVertex({0.0f, y, 0.0f}, {0.5f, 0.5f}, normal, tangent, bitangent));

    for (uint32_t segment = 0; segment <= segments; ++segment) {
        const float u = static_cast<float>(segment) / static_cast<float>(segments);
        const float angle = u * kPi * 2.0f;
        const float x = std::cos(angle) * radius;
        const float z = std::sin(angle) * radius;
        submesh.Vertices.push_back(
            makeVertex({x, y, z}, {x / (radius * 2.0f) + 0.5f, z / (radius * 2.0f) + 0.5f}, normal, tangent, bitangent));
    }

    for (uint32_t segment = 0; segment < segments; ++segment) {
        const uint32_t i0 = center_index;
        const uint32_t i1 = center_index + 1 + segment;
        const uint32_t i2 = i1 + 1;
        if (normal.y > 0.0f) {
            submesh.Indices.push_back(i0);
            submesh.Indices.push_back(i2);
            submesh.Indices.push_back(i1);
        } else {
            submesh.Indices.push_back(i0);
            submesh.Indices.push_back(i1);
            submesh.Indices.push_back(i2);
        }
    }
}

void addCylinderSide(SubMesh& submesh, float radius, float half_height, uint32_t segments)
{
    const uint32_t base_index = static_cast<uint32_t>(submesh.Vertices.size());
    for (uint32_t segment = 0; segment <= segments; ++segment) {
        const float u = static_cast<float>(segment) / static_cast<float>(segments);
        const float angle = u * kPi * 2.0f;
        const float sin_angle = std::sin(angle);
        const float cos_angle = std::cos(angle);
        const glm::vec3 normal{cos_angle, 0.0f, sin_angle};
        const glm::vec3 tangent{-sin_angle, 0.0f, cos_angle};
        const glm::vec3 bitangent{0.0f, 1.0f, 0.0f};
        const float x = cos_angle * radius;
        const float z = sin_angle * radius;
        submesh.Vertices.push_back(makeVertex({x, -half_height, z}, {u, 1.0f}, normal, tangent, bitangent));
        submesh.Vertices.push_back(makeVertex({x, half_height, z}, {u, 0.0f}, normal, tangent, bitangent));
    }

    for (uint32_t segment = 0; segment < segments; ++segment) {
        const uint32_t i0 = base_index + segment * 2;
        const uint32_t i1 = i0 + 1;
        const uint32_t i2 = i0 + 2;
        const uint32_t i3 = i0 + 3;
        submesh.Indices.push_back(i0);
        submesh.Indices.push_back(i2);
        submesh.Indices.push_back(i1);
        submesh.Indices.push_back(i1);
        submesh.Indices.push_back(i2);
        submesh.Indices.push_back(i3);
    }
}

void addConeSide(SubMesh& submesh, float radius, float half_height, uint32_t segments)
{
    const uint32_t base_index = static_cast<uint32_t>(submesh.Vertices.size());
    const float slope = radius / (half_height * 2.0f);
    for (uint32_t segment = 0; segment <= segments; ++segment) {
        const float u = static_cast<float>(segment) / static_cast<float>(segments);
        const float angle = u * kPi * 2.0f;
        const float sin_angle = std::sin(angle);
        const float cos_angle = std::cos(angle);
        const glm::vec3 normal = glm::normalize(glm::vec3{cos_angle, slope, sin_angle});
        const glm::vec3 tangent{-sin_angle, 0.0f, cos_angle};
        const glm::vec3 bitangent = glm::normalize(glm::cross(normal, tangent));
        const float x = cos_angle * radius;
        const float z = sin_angle * radius;
        submesh.Vertices.push_back(makeVertex({x, -half_height, z}, {u, 1.0f}, normal, tangent, bitangent));
        submesh.Vertices.push_back(makeVertex({0.0f, half_height, 0.0f}, {u, 0.0f}, normal, tangent, bitangent));
    }

    for (uint32_t segment = 0; segment < segments; ++segment) {
        const uint32_t i0 = base_index + segment * 2;
        const uint32_t i1 = i0 + 1;
        const uint32_t i2 = i0 + 2;
        const uint32_t i3 = i0 + 3;
        submesh.Indices.push_back(i0);
        submesh.Indices.push_back(i2);
        submesh.Indices.push_back(i1);
        submesh.Indices.push_back(i1);
        submesh.Indices.push_back(i2);
        submesh.Indices.push_back(i3);
    }
}

} // namespace

std::shared_ptr<Mesh> PrimitiveMeshFactory::createCube()
{
    SubMesh cube{};
    cube.Name = "Cube";
    cube.MaterialIndex = 0;

    constexpr float half_extent = 0.5f;
    addQuad(cube,
            {-half_extent, -half_extent, half_extent},
            {half_extent, -half_extent, half_extent},
            {half_extent, half_extent, half_extent},
            {-half_extent, half_extent, half_extent},
            {0.0f, 0.0f, 1.0f},
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f});
    addQuad(cube,
            {half_extent, -half_extent, -half_extent},
            {-half_extent, -half_extent, -half_extent},
            {-half_extent, half_extent, -half_extent},
            {half_extent, half_extent, -half_extent},
            {0.0f, 0.0f, -1.0f},
            {-1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f});
    addQuad(cube,
            {-half_extent, -half_extent, -half_extent},
            {-half_extent, -half_extent, half_extent},
            {-half_extent, half_extent, half_extent},
            {-half_extent, half_extent, -half_extent},
            {-1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 1.0f, 0.0f});
    addQuad(cube,
            {half_extent, -half_extent, half_extent},
            {half_extent, -half_extent, -half_extent},
            {half_extent, half_extent, -half_extent},
            {half_extent, half_extent, half_extent},
            {1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, -1.0f},
            {0.0f, 1.0f, 0.0f});
    addQuad(cube,
            {-half_extent, half_extent, half_extent},
            {half_extent, half_extent, half_extent},
            {half_extent, half_extent, -half_extent},
            {-half_extent, half_extent, -half_extent},
            {0.0f, 1.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, -1.0f});
    addQuad(cube,
            {-half_extent, -half_extent, -half_extent},
            {half_extent, -half_extent, -half_extent},
            {half_extent, -half_extent, half_extent},
            {-half_extent, -half_extent, half_extent},
            {0.0f, -1.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f});

    return Mesh::create("Cube", {std::move(cube)});
}

std::shared_ptr<Mesh> PrimitiveMeshFactory::createPlane()
{
    SubMesh plane{};
    plane.Name = "Plane";
    plane.MaterialIndex = 0;

    constexpr float half_extent = 0.5f;
    addQuad(plane,
            {-half_extent, 0.0f, -half_extent},
            {half_extent, 0.0f, -half_extent},
            {half_extent, 0.0f, half_extent},
            {-half_extent, 0.0f, half_extent},
            {0.0f, 1.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f});

    return Mesh::create("Plane", {std::move(plane)});
}

std::shared_ptr<Mesh> PrimitiveMeshFactory::createSphere(uint32_t segments, uint32_t rings)
{
    segments = (std::max)(segments, 3u);
    rings = (std::max)(rings, 2u);

    SubMesh sphere{};
    sphere.Name = "Sphere";
    sphere.MaterialIndex = 0;

    constexpr float radius = 0.5f;
    for (uint32_t ring = 0; ring <= rings; ++ring) {
        const float v = static_cast<float>(ring) / static_cast<float>(rings);
        const float theta = v * kPi;
        const float sin_theta = std::sin(theta);
        const float cos_theta = std::cos(theta);

        for (uint32_t segment = 0; segment <= segments; ++segment) {
            const float u = static_cast<float>(segment) / static_cast<float>(segments);
            const float phi = u * kPi * 2.0f;
            const float sin_phi = std::sin(phi);
            const float cos_phi = std::cos(phi);

            const glm::vec3 normal{sin_theta * cos_phi, cos_theta, sin_theta * sin_phi};
            const glm::vec3 position = normal * radius;
            const glm::vec3 tangent{-sin_phi, 0.0f, cos_phi};
            const glm::vec3 bitangent{cos_theta * cos_phi, -sin_theta, cos_theta * sin_phi};
            sphere.Vertices.push_back(makeVertex(position, {u, v}, normal, tangent, bitangent));
        }
    }

    const uint32_t stride = segments + 1;
    for (uint32_t ring = 0; ring < rings; ++ring) {
        for (uint32_t segment = 0; segment < segments; ++segment) {
            const uint32_t i0 = ring * stride + segment;
            const uint32_t i1 = i0 + 1;
            const uint32_t i2 = i0 + stride;
            const uint32_t i3 = i2 + 1;

            sphere.Indices.push_back(i0);
            sphere.Indices.push_back(i2);
            sphere.Indices.push_back(i1);
            sphere.Indices.push_back(i1);
            sphere.Indices.push_back(i2);
            sphere.Indices.push_back(i3);
        }
    }

    return Mesh::create("Sphere", {std::move(sphere)});
}

std::shared_ptr<Mesh> PrimitiveMeshFactory::createCylinder(uint32_t segments)
{
    segments = (std::max)(segments, 3u);

    SubMesh cylinder{};
    cylinder.Name = "Cylinder";
    cylinder.MaterialIndex = 0;

    constexpr float radius = 0.5f;
    constexpr float half_height = 0.5f;
    addCylinderSide(cylinder, radius, half_height, segments);
    addDisk(cylinder, half_height, radius, {0.0f, 1.0f, 0.0f}, segments);
    addDisk(cylinder, -half_height, radius, {0.0f, -1.0f, 0.0f}, segments);

    return Mesh::create("Cylinder", {std::move(cylinder)});
}

std::shared_ptr<Mesh> PrimitiveMeshFactory::createCone(uint32_t segments)
{
    segments = (std::max)(segments, 3u);

    SubMesh cone{};
    cone.Name = "Cone";
    cone.MaterialIndex = 0;

    constexpr float radius = 0.5f;
    constexpr float half_height = 0.5f;
    addConeSide(cone, radius, half_height, segments);
    addDisk(cone, -half_height, radius, {0.0f, -1.0f, 0.0f}, segments);

    return Mesh::create("Cone", {std::move(cone)});
}

} // namespace luna
