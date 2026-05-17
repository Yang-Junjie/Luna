#include "Renderer/RenderFlow/DefaultScene/ShadowCascadeBounds.h"
#include "Renderer/Visibility/Frustum.h"

#include <cmath>

#include <Capabilities.h>
#include <glm/vec4.hpp>
#include <iostream>
#include <string_view>

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

void testOffscreenCasterExtendsCascadeDepth(TestContext& context)
{
    using namespace luna::render_flow::default_scene_detail;

    const std::array<glm::vec3, 8> receiver_corners{{
        {-2.0f, -2.0f, 0.0f},
        {2.0f, -2.0f, 0.0f},
        {2.0f, 2.0f, 0.0f},
        {-2.0f, 2.0f, 0.0f},
        {-2.0f, -2.0f, 2.0f},
        {2.0f, -2.0f, 2.0f},
        {2.0f, 2.0f, 2.0f},
        {-2.0f, 2.0f, 2.0f},
    }};
    const glm::vec3 light_direction{0.0f, 0.0f, -1.0f};
    ShadowCascadeLightBounds bounds = buildShadowCascadeReceiverBounds(receiver_corners, light_direction, 1024);
    const float original_min_z = bounds.min_bounds.z;
    const float original_max_z = bounds.max_bounds.z;

    const luna::MeshBounds offscreen_caster = makeBounds({0.0f, 0.0f, -30.0f}, {0.5f, 0.5f, 0.5f});
    context.expect(expandShadowCascadeDepthForCaster(bounds, offscreen_caster),
                   "caster overlapping receiver light-space xy should be included");
    context.expect(bounds.min_bounds.z < original_min_z || bounds.max_bounds.z > original_max_z,
                   "included caster should expand cascade depth");

    padShadowCascadeDepth(bounds, 0.1f);
    const glm::mat4 view_projection = buildShadowCascadeViewProjection(bounds, luna::RHI::RHIConventions{});
    const luna::Frustum shadow_frustum = luna::Frustum::fromViewProjection(view_projection);
    context.expect(shadow_frustum.intersects(offscreen_caster),
                   "expanded cascade frustum should include the offscreen caster");
}

void testLateralCasterDoesNotExpandCascade(TestContext& context)
{
    using namespace luna::render_flow::default_scene_detail;

    const std::array<glm::vec3, 8> receiver_corners{{
        {-2.0f, -2.0f, 0.0f},
        {2.0f, -2.0f, 0.0f},
        {2.0f, 2.0f, 0.0f},
        {-2.0f, 2.0f, 0.0f},
        {-2.0f, -2.0f, 2.0f},
        {2.0f, -2.0f, 2.0f},
        {2.0f, 2.0f, 2.0f},
        {-2.0f, 2.0f, 2.0f},
    }};
    const glm::vec3 light_direction{0.0f, 0.0f, -1.0f};
    ShadowCascadeLightBounds bounds = buildShadowCascadeReceiverBounds(receiver_corners, light_direction, 1024);
    const glm::vec3 original_min = bounds.min_bounds;
    const glm::vec3 original_max = bounds.max_bounds;

    const luna::MeshBounds lateral_caster = makeBounds({30.0f, 0.0f, -30.0f}, {0.5f, 0.5f, 0.5f});
    context.expect(!expandShadowCascadeDepthForCaster(bounds, lateral_caster),
                   "caster outside receiver light-space xy should be culled");
    context.expect(bounds.min_bounds == original_min && bounds.max_bounds == original_max,
                   "culled caster should not change cascade bounds");
}

} // namespace

int main()
{
    TestContext context;
    testOffscreenCasterExtendsCascadeDepth(context);
    testLateralCasterDoesNotExpandCascade(context);
    return context.failures() == 0 ? 0 : 1;
}
