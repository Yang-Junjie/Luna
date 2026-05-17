#include "Asset/AssetDatabase.h"
#include "Asset/AssetManager.h"
#include "Renderer/Mesh.h"
#include "Renderer/RenderWorld/RenderWorld.h"
#include "Renderer/RenderWorld/RenderWorldExtractor.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <cmath>

#include <glm/geometric.hpp>
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
    context.expect(sameVec3(bounds.Min, min), std::string(label) + " min should match");
    context.expect(sameVec3(bounds.Max, max), std::string(label) + " max should match");
    context.expect(sameVec3(bounds.Center, (min + max) * 0.5f), std::string(label) + " center should match");
    context.expect(sameVec3(bounds.Extents, (max - min) * 0.5f), std::string(label) + " extents should match");
    context.expect(sameFloat(bounds.Radius, glm::length(bounds.Extents)), std::string(label) + " radius should match");
}

std::shared_ptr<luna::Mesh> createTestMesh()
{
    luna::SubMesh first{};
    first.Name = "First";
    first.Vertices = {
        makeVertex({-1.0f, -1.0f, -1.0f}),
        makeVertex({1.0f, 2.0f, 3.0f}),
        makeVertex({0.0f, 0.0f, 1.0f}),
    };
    first.Indices = {0, 1, 2};

    luna::SubMesh second{};
    second.Name = "Second";
    second.Vertices = {
        makeVertex({2.0f, -2.0f, 0.0f}),
        makeVertex({4.0f, 1.0f, 2.0f}),
        makeVertex({3.0f, 0.0f, 1.0f}),
    };
    second.Indices = {0, 1, 2};

    std::vector<luna::SubMesh> sub_meshes;
    sub_meshes.push_back(std::move(first));
    sub_meshes.push_back(std::move(second));
    return luna::Mesh::create("ExtractorBoundsMesh", std::move(sub_meshes));
}

void testExtractorAddsWorldBounds(TestContext& context)
{
    luna::AssetDatabase::clear();
    luna::AssetManager::get().clear();

    const luna::AssetHandle mesh_handle(42'001);
    const auto mesh = createTestMesh();
    context.expect(mesh && mesh->isValid(), "test mesh should be valid");
    if (!mesh || !mesh->isValid()) {
        return;
    }

    luna::AssetManager::get().registerMemoryAsset(mesh_handle, mesh);

    luna::Scene scene;
    auto& entity_manager = scene.entityManager();
    luna::Entity entity = entity_manager.createEntity("BoundsEntity");
    entity.transform().translation = {10.0f, -5.0f, 2.0f};
    entity.transform().scale = {2.0f, 3.0f, 4.0f};

    auto& mesh_component = entity.addComponent<luna::MeshComponent>();
    mesh_component.meshHandle = mesh_handle;

    luna::Camera camera;
    luna::RenderWorld render_world;
    luna::RenderWorldExtractor{}.extract(scene, camera, render_world);

    context.expect(render_world.meshInstances().size() == 1, "extractor should add one mesh instance");
    context.expect(render_world.drawPackets().size() == 2, "extractor should add one packet per submesh");
    if (render_world.meshInstances().size() != 1 || render_world.drawPackets().size() != 2) {
        luna::AssetManager::get().clear();
        luna::AssetDatabase::clear();
        return;
    }

    const auto& instance = render_world.meshInstances().front();
    expectBounds(context, instance.local_bounds, {-1.0f, -2.0f, -1.0f}, {4.0f, 2.0f, 3.0f}, "instance local");
    expectBounds(context, instance.world_bounds, {8.0f, -11.0f, -2.0f}, {18.0f, 1.0f, 14.0f}, "instance world");

    const auto& first_packet = render_world.drawPackets()[0];
    expectBounds(context, first_packet.local_bounds, {-1.0f, -1.0f, -1.0f}, {1.0f, 2.0f, 3.0f}, "first packet local");
    expectBounds(context, first_packet.world_bounds, {8.0f, -8.0f, -2.0f}, {12.0f, 1.0f, 14.0f}, "first packet world");

    const auto& second_packet = render_world.drawPackets()[1];
    expectBounds(context, second_packet.local_bounds, {2.0f, -2.0f, 0.0f}, {4.0f, 1.0f, 2.0f}, "second packet local");
    expectBounds(
        context, second_packet.world_bounds, {14.0f, -11.0f, 2.0f}, {18.0f, -2.0f, 10.0f}, "second packet world");

    luna::AssetManager::get().clear();
    luna::AssetDatabase::clear();
}

} // namespace

int main()
{
    TestContext context;
    testExtractorAddsWorldBounds(context);
    return context.failures() == 0 ? 0 : 1;
}
