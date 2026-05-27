#include "Renderer/Mesh.h"
#include "Renderer/RenderFlow/DefaultScene/DrawQueue.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/VisibilityBoundsOverlayPass.h"
#include "Renderer/Visibility/Frustum.h"

#include <cmath>

#include <glm/geometric.hpp>
#include <iostream>
#include <memory>
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

std::shared_ptr<luna::Mesh> makeMesh()
{
    luna::SubMesh sub_mesh{};
    sub_mesh.Name = "CullingSubmesh";
    sub_mesh.Vertices = {
        makeVertex({-0.5f, -0.5f, 0.0f}),
        makeVertex({0.5f, -0.5f, 0.0f}),
        makeVertex({0.0f, 0.5f, 0.0f}),
    };
    sub_mesh.Indices = {0, 1, 2};
    return luna::Mesh::create("CullingMesh", {std::move(sub_mesh)});
}

luna::MeshBounds makeBounds(const glm::vec3& center, const glm::vec3& extents)
{
    return luna::MeshBounds{
        .Min = center - extents,
        .Max = center + extents,
        .Center = center,
        .Extents = extents,
        .Radius = glm::length(extents),
        .Valid = true,
    };
}

luna::RenderDrawPacket makePacket(const std::shared_ptr<luna::Mesh>& mesh,
                                  const luna::MeshBounds& world_bounds,
                                  luna::RenderPhaseMask phases)
{
    return luna::RenderDrawPacket{
        .mesh = mesh,
        .submesh_index = 0,
        .local_bounds = mesh ? mesh->getSubMeshes().front().Bounds : luna::MeshBounds{},
        .world_bounds = world_bounds,
        .phases = phases,
    };
}

void testDrawQueueKeepsShadowCastersUnculled(TestContext& context)
{
    const auto mesh = makeMesh();
    context.expect(mesh && mesh->isValid(), "test mesh should be valid");
    if (!mesh || !mesh->isValid()) {
        return;
    }

    luna::Camera camera;
    camera.setPerspective(1.0f, 0.1f, 50.0f);

    luna::render_flow::default_scene::DrawQueue draw_queue;
    draw_queue.beginScene(camera, 1.0f);

    constexpr luna::RenderPhaseMask gbuffer_shadow =
        luna::renderPhaseBit(luna::RenderPhase::GBuffer) | luna::renderPhaseBit(luna::RenderPhase::ShadowCaster);
    constexpr luna::RenderPhaseMask transparent_shadow =
        luna::renderPhaseBit(luna::RenderPhase::Transparent) | luna::renderPhaseBit(luna::RenderPhase::ShadowCaster);

    draw_queue.submitDrawPacket(makePacket(mesh, makeBounds({0.0f, 0.0f, -5.0f}, {0.5f, 0.5f, 0.5f}), gbuffer_shadow));
    draw_queue.submitDrawPacket(
        makePacket(mesh, makeBounds({100.0f, 0.0f, -5.0f}, {0.5f, 0.5f, 0.5f}), gbuffer_shadow));
    draw_queue.submitDrawPacket(makePacket(mesh, {}, gbuffer_shadow));
    draw_queue.submitDrawPacket(
        makePacket(mesh, makeBounds({-100.0f, 0.0f, -5.0f}, {0.5f, 0.5f, 0.5f}), transparent_shadow));
    draw_queue.submitDrawPacket(makePacket(mesh,
                                           makeBounds({0.0f, 0.0f, -6.0f}, {0.5f, 0.5f, 0.5f}),
                                           luna::renderPhaseBit(luna::RenderPhase::Transparent)));
    draw_queue.submitDrawPacket(makePacket(mesh,
                                           makeBounds({0.0f, 0.0f, -7.0f}, {0.5f, 0.5f, 0.5f}),
                                           luna::renderPhaseBit(luna::RenderPhase::DepthOnly) |
                                               luna::renderPhaseBit(luna::RenderPhase::Picking)));
    draw_queue.submitDrawPacket(makePacket(mesh,
                                           makeBounds({100.0f, 0.0f, -7.0f}, {0.5f, 0.5f, 0.5f}),
                                           luna::renderPhaseBit(luna::RenderPhase::DepthOnly) |
                                               luna::renderPhaseBit(luna::RenderPhase::Picking)));

    const auto& depth_draws = draw_queue.drawCommands(luna::RenderPhase::DepthOnly);
    const auto& gbuffer_draws = draw_queue.drawCommands(luna::RenderPhase::GBuffer);
    const auto& transparent_draws = draw_queue.drawCommands(luna::RenderPhase::Transparent);
    const auto& shadow_draws = draw_queue.drawCommands(luna::RenderPhase::ShadowCaster);
    const auto& picking_draws = draw_queue.drawCommands(luna::RenderPhase::Picking);
    const auto& stats = draw_queue.stats();

    context.expect(draw_queue.drawCommands().size() == 4, "camera-visible draw view should exclude culled draws");
    context.expect(depth_draws.size() == 1, "DepthOnly draws should use the cached camera-visible phase list");
    context.expect(gbuffer_draws.size() == 2, "GBuffer draws should include visible and invalid-bounds packets");
    context.expect(transparent_draws.size() == 1, "Transparent draws should exclude camera-culled packets");
    context.expect(shadow_draws.size() == 4, "ShadowCaster draws should use the unculled draw set");
    context.expect(picking_draws.size() == 1, "Picking draws should use the cached camera-visible phase list");
    context.expect(stats.submitted == 7, "stats should count valid submitted draws");
    context.expect(stats.camera_visible == 4, "stats should count camera-visible draws");
    context.expect(stats.camera_culled == 3, "stats should count camera-culled draws");
    context.expect(stats.invalid_bounds == 1, "stats should count invalid bounds");
    context.expect(stats.shadow_unculled == 4, "stats should count unculled shadow casters");
    context.expect(draw_queue.visibilityDebugItems().empty(), "visibility debug capture should be disabled by default");
    context.expect(draw_queue.visibilityDebugStats().captured == 0,
                   "visibility debug stats should be empty when disabled");
}

void testDrawQueueCapturesVisibilityDebugItems(TestContext& context)
{
    const auto mesh = makeMesh();
    context.expect(mesh && mesh->isValid(), "test mesh should be valid");
    if (!mesh || !mesh->isValid()) {
        return;
    }

    luna::Camera camera;
    camera.setPerspective(1.0f, 0.1f, 50.0f);

    luna::render_flow::default_scene::DrawQueue draw_queue;
    draw_queue.beginScene(camera,
                          1.0f,
                          luna::render_flow::default_scene::VisibilityDebugCaptureOptions{
                              .capture_visible_bounds = true,
                              .capture_culled_bounds = true,
                          });

    constexpr luna::RenderPhaseMask gbuffer =
        luna::renderPhaseBit(luna::RenderPhase::GBuffer) | luna::renderPhaseBit(luna::RenderPhase::ShadowCaster);

    draw_queue.submitDrawPacket(makePacket(mesh, makeBounds({0.0f, 0.0f, -5.0f}, {0.5f, 0.5f, 0.5f}), gbuffer));
    draw_queue.submitDrawPacket(makePacket(mesh, {}, gbuffer));
    draw_queue.submitDrawPacket(makePacket(mesh, makeBounds({100.0f, 0.0f, -5.0f}, {0.5f, 0.5f, 0.5f}), gbuffer));

    const auto& items = draw_queue.visibilityDebugItems();
    const auto& stats = draw_queue.visibilityDebugStats();

    context.expect(items.size() == 3, "visibility debug should capture visible, invalid, and culled bounds");
    context.expect(stats.captured == 3, "visibility debug stats should count captured items");
    context.expect(stats.camera_visible == 1, "visibility debug should count camera-visible items");
    context.expect(stats.invalid_bounds == 1, "visibility debug should count invalid bounds separately");
    context.expect(stats.camera_culled == 1, "visibility debug should count camera-culled items");
    if (items.size() == 3) {
        context.expect(items[0].classification ==
                           luna::render_flow::default_scene::VisibilityDebugClassification::CameraVisible,
                       "first visibility debug item should be camera-visible");
        context.expect(items[1].classification ==
                           luna::render_flow::default_scene::VisibilityDebugClassification::InvalidBounds,
                       "second visibility debug item should be invalid-bounds");
        context.expect(items[2].classification ==
                           luna::render_flow::default_scene::VisibilityDebugClassification::CameraCulled,
                       "third visibility debug item should be camera-culled");
    }
}

void testDrawQueueUsesSeparateCullingCamera(TestContext& context)
{
    const auto mesh = makeMesh();
    context.expect(mesh && mesh->isValid(), "test mesh should be valid");
    if (!mesh || !mesh->isValid()) {
        return;
    }

    luna::Camera view_camera;
    view_camera.setPerspective(1.0f, 0.1f, 50.0f);

    luna::Camera culling_camera;
    culling_camera.setPerspective(1.0f, 0.1f, 50.0f);
    culling_camera.setPosition({100.0f, 0.0f, 0.0f});

    luna::render_flow::default_scene::DrawQueue draw_queue;
    draw_queue.beginScene(view_camera,
                          1.0f,
                          culling_camera,
                          1.0f,
                          luna::render_flow::default_scene::VisibilityDebugCaptureOptions{
                              .capture_visible_bounds = true,
                              .capture_culled_bounds = true,
                              .capture_culling_frustum = true,
                              .culling_frustum_frozen = true,
                          });

    draw_queue.submitDrawPacket(makePacket(
        mesh, makeBounds({0.0f, 0.0f, -5.0f}, {0.5f, 0.5f, 0.5f}), luna::renderPhaseBit(luna::RenderPhase::GBuffer)));

    const auto& stats = draw_queue.stats();
    const auto& debug_stats = draw_queue.visibilityDebugStats();
    const auto& frustums = draw_queue.visibilityDebugFrustums();

    context.expect(draw_queue.drawCommands().empty(),
                   "separate culling camera should control camera-visible draw commands");
    context.expect(stats.camera_culled == 1, "separate culling camera should cull bounds outside its frustum");
    context.expect(debug_stats.camera_culled == 1, "visibility debug should capture culled item from culling camera");
    context.expect(debug_stats.culling_frustums == 1, "visibility debug should capture the culling frustum");
    context.expect(frustums.size() == 1, "draw queue should store one frustum debug item");
    if (!frustums.empty()) {
        context.expect(frustums.front().frozen, "frustum debug item should preserve frozen flag");
    }
}

void testCameraFrustumCorners(TestContext& context)
{
    luna::Camera perspective_camera;
    perspective_camera.setPerspective(1.0f, 0.1f, 10.0f);

    const luna::FrustumCorners perspective_corners = luna::cameraFrustumCorners(perspective_camera, 2.0f);
    const float perspective_near_half_height = std::tan(0.5f) * 0.1f;
    const float perspective_near_half_width = perspective_near_half_height * 2.0f;
    const float perspective_far_half_height = std::tan(0.5f) * 10.0f;
    const float perspective_far_half_width = perspective_far_half_height * 2.0f;

    context.expect(
        sameVec3(perspective_corners[0], glm::vec3{-perspective_near_half_width, -perspective_near_half_height, -0.1f}),
        "perspective frustum near-min corner should match camera settings");
    context.expect(
        sameVec3(perspective_corners[6], glm::vec3{perspective_far_half_width, perspective_far_half_height, -10.0f}),
        "perspective frustum far-max corner should match camera settings");

    luna::Camera orthographic_camera;
    orthographic_camera.setOrthographic(4.0f, 0.5f, 8.0f);

    const luna::FrustumCorners orthographic_corners = luna::cameraFrustumCorners(orthographic_camera, 1.5f);

    context.expect(sameVec3(orthographic_corners[0], glm::vec3{-3.0f, -2.0f, -0.5f}),
                   "orthographic frustum near-min corner should match camera settings");
    context.expect(sameVec3(orthographic_corners[6], glm::vec3{3.0f, 2.0f, -8.0f}),
                   "orthographic frustum far-max corner should match camera settings");
}

void testVisibilityBoundsOverlayBuildsLineVertices(TestContext& context)
{
    std::vector<luna::render_flow::default_scene::VisibilityDebugItem> items;
    items.push_back(luna::render_flow::default_scene::VisibilityDebugItem{
        .world_bounds = makeBounds({0.0f, 0.0f, 0.0f}, {1.0f, 2.0f, 3.0f}),
        .world_origin = {0.0f, 0.0f, 0.0f},
        .classification = luna::render_flow::default_scene::VisibilityDebugClassification::CameraVisible,
    });
    items.push_back(luna::render_flow::default_scene::VisibilityDebugItem{
        .world_bounds = {},
        .world_origin = {10.0f, 20.0f, 30.0f},
        .classification = luna::render_flow::default_scene::VisibilityDebugClassification::InvalidBounds,
    });

    std::vector<luna::render_flow::default_scene::VisibilityDebugFrustumItem> frustums;
    frustums.push_back(luna::render_flow::default_scene::VisibilityDebugFrustumItem{
        .corners =
            {
                glm::vec3{-1.0f, -1.0f, -1.0f},
                glm::vec3{1.0f, -1.0f, -1.0f},
                glm::vec3{1.0f, 1.0f, -1.0f},
                glm::vec3{-1.0f, 1.0f, -1.0f},
                glm::vec3{-2.0f, -2.0f, -2.0f},
                glm::vec3{2.0f, -2.0f, -2.0f},
                glm::vec3{2.0f, 2.0f, -2.0f},
                glm::vec3{-2.0f, 2.0f, -2.0f},
            },
        .color = {0.25f, 0.50f, 1.0f, 1.0f},
        .frozen = true,
    });

    std::vector<luna::render_flow::default_scene::VisibilityBoundsOverlayVertex> vertices;
    const auto stats =
        luna::render_flow::default_scene::buildVisibilityBoundsOverlayVertices(items, frustums, vertices);

    context.expect(stats.items == 2, "overlay builder should count source debug items");
    context.expect(stats.bounds == 1, "overlay builder should count valid bounds");
    context.expect(stats.invalid_markers == 1, "overlay builder should emit an invalid-bounds marker");
    context.expect(stats.frustums == 1, "overlay builder should count frustum debug items");
    context.expect(stats.vertices == 72, "overlay builder should emit 24 vertices per bounds/frustum box");
    context.expect(vertices.size() == 72, "overlay vertex list should match stats");
    if (vertices.size() == 72) {
        context.expect(sameVec3(vertices.front().world_position, glm::vec3(-1.0f, -2.0f, -3.0f)),
                       "first overlay vertex should be the valid bounds minimum corner in world space");
        const float marker_extent = luna::render_flow::default_scene::kVisibilityBoundsInvalidMarkerHalfExtent;
        context.expect(sameVec3(vertices[24].world_position,
                                glm::vec3(10.0f - marker_extent, 20.0f - marker_extent, 30.0f - marker_extent)),
                       "invalid bounds marker should be centered on the draw origin");
        context.expect(sameVec3(vertices[48].world_position, glm::vec3(-1.0f, -1.0f, -1.0f)),
                       "frustum line vertices should be appended after bounds markers");
    }
}

} // namespace

int main()
{
    TestContext context;
    testDrawQueueKeepsShadowCastersUnculled(context);
    testDrawQueueCapturesVisibilityDebugItems(context);
    testDrawQueueUsesSeparateCullingCamera(context);
    testCameraFrustumCorners(context);
    testVisibilityBoundsOverlayBuildsLineVertices(context);
    return context.failures() == 0 ? 0 : 1;
}
