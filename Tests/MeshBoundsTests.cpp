#include "Renderer/Mesh.h"

#include <cmath>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

class TestContext {
public:
    bool expect(bool condition, std::string_view message)
    {
        if (!condition) {
            std::cerr << "FAIL: " << message << '\n';
            ++m_failures;
        }
        return condition;
    }

    int failures() const noexcept
    {
        return m_failures;
    }

private:
    int m_failures{0};
};

luna::StaticMeshVertex makeVertex(const glm::vec3& position)
{
    luna::StaticMeshVertex vertex{};
    vertex.Position = position;
    return vertex;
}

bool sameFloat(float lhs, float rhs)
{
    return std::fabs(lhs - rhs) <= 0.0001f;
}

bool sameVec3(const glm::vec3& lhs, const glm::vec3& rhs)
{
    return sameFloat(lhs.x, rhs.x) && sameFloat(lhs.y, rhs.y) && sameFloat(lhs.z, rhs.z);
}

void expectBounds(TestContext& context,
                  const luna::MeshBounds& bounds,
                  const glm::vec3& min,
                  const glm::vec3& max,
                  std::string_view label)
{
    context.expect(bounds.isValid(), std::string(label) + " bounds should be valid");
    context.expect(sameVec3(bounds.Min, min), std::string(label) + " min bounds should match vertices");
    context.expect(sameVec3(bounds.Max, max), std::string(label) + " max bounds should match vertices");
    context.expect(sameVec3(bounds.Center, (min + max) * 0.5f), std::string(label) + " center should match bounds");
    context.expect(sameVec3(bounds.Extents, (max - min) * 0.5f), std::string(label) + " extents should match bounds");
    context.expect(sameFloat(bounds.Radius, glm::length(bounds.Extents)),
                   std::string(label) + " radius should enclose bounds");
}

void testMeshComputesSubmeshAndAggregateBounds(TestContext& context)
{
    luna::SubMesh first{};
    first.Name = "First";
    first.Vertices = {
        makeVertex({-1.0f, 2.0f, 0.0f}),
        makeVertex({3.0f, -1.0f, 4.0f}),
        makeVertex({0.0f, 1.0f, 2.0f}),
    };
    first.Indices = {0, 1, 2};

    luna::SubMesh second{};
    second.Name = "Second";
    second.Vertices = {
        makeVertex({5.0f, 0.0f, -3.0f}),
        makeVertex({2.0f, 4.0f, 6.0f}),
        makeVertex({4.0f, 1.0f, 1.0f}),
    };
    second.Indices = {0, 1, 2};

    std::vector<luna::SubMesh> sub_meshes;
    sub_meshes.push_back(std::move(first));
    sub_meshes.push_back(std::move(second));

    const std::shared_ptr<luna::Mesh> mesh = luna::Mesh::create("BoundsMesh", std::move(sub_meshes));
    if (!context.expect(mesh != nullptr, "mesh should be created")) {
        return;
    }

    const auto& mesh_submeshes = mesh->getSubMeshes();
    if (!context.expect(mesh_submeshes.size() == 2, "mesh should keep both submeshes")) {
        return;
    }

    expectBounds(context, mesh_submeshes[0].Bounds, {-1.0f, -1.0f, 0.0f}, {3.0f, 2.0f, 4.0f}, "first submesh");
    expectBounds(context, mesh_submeshes[1].Bounds, {2.0f, 0.0f, -3.0f}, {5.0f, 4.0f, 6.0f}, "second submesh");
    expectBounds(context, mesh->getBounds(), {-1.0f, -1.0f, -3.0f}, {5.0f, 4.0f, 6.0f}, "mesh");
}

void testEmptySubmeshBoundsAreInvalid(TestContext& context)
{
    luna::SubMesh empty{};
    empty.Name = "Empty";

    const std::shared_ptr<luna::Mesh> mesh = luna::Mesh::create("EmptyMesh", {empty});
    if (!context.expect(mesh != nullptr, "empty mesh should still be created")) {
        return;
    }

    context.expect(!mesh->getBounds().isValid(), "mesh bounds should be invalid when no submesh has vertices");
    context.expect(!mesh->getSubMeshes().front().Bounds.isValid(), "empty submesh bounds should be invalid");
}

void testTransformMeshBounds(TestContext& context)
{
    const luna::MeshBounds local_bounds{
        .Min = {-1.0f, -2.0f, -3.0f},
        .Max = {3.0f, 4.0f, 5.0f},
        .Center = {1.0f, 1.0f, 1.0f},
        .Extents = {2.0f, 3.0f, 4.0f},
        .Radius = glm::length(glm::vec3{2.0f, 3.0f, 4.0f}),
        .Valid = true,
    };

    const glm::mat4 translate_scale =
        glm::translate(glm::mat4(1.0f), {10.0f, -4.0f, 2.0f}) * glm::scale(glm::mat4(1.0f), {2.0f, 3.0f, 0.5f});
    expectBounds(context,
                 luna::transformMeshBounds(local_bounds, translate_scale),
                 {8.0f, -10.0f, 0.5f},
                 {16.0f, 8.0f, 4.5f},
                 "translated scaled mesh");

    const luna::MeshBounds unit_box{
        .Min = {-1.0f, -2.0f, -0.5f},
        .Max = {1.0f, 2.0f, 0.5f},
        .Center = {0.0f, 0.0f, 0.0f},
        .Extents = {1.0f, 2.0f, 0.5f},
        .Radius = glm::length(glm::vec3{1.0f, 2.0f, 0.5f}),
        .Valid = true,
    };
    const glm::mat4 rotate_z = glm::rotate(glm::mat4(1.0f), glm::half_pi<float>(), {0.0f, 0.0f, 1.0f});
    expectBounds(context,
                 luna::transformMeshBounds(unit_box, rotate_z),
                 {-2.0f, -1.0f, -0.5f},
                 {2.0f, 1.0f, 0.5f},
                 "rotated mesh");

    context.expect(!luna::transformMeshBounds({}, glm::mat4(1.0f)).isValid(),
                   "invalid bounds should stay invalid after transform");
}

} // namespace

int main()
{
    TestContext context;
    testMeshComputesSubmeshAndAggregateBounds(context);
    testEmptySubmeshBoundsAreInvalid(context);
    testTransformMeshBounds(context);
    return context.failures() == 0 ? 0 : 1;
}
