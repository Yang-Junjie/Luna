#include "Renderer/RenderFlow/DefaultScene/Passes/ShadowPass.h"

#include "Core/Log.h"
#include "Math/Math.h"
#include "Renderer/Material.h"
#include "Renderer/RenderFlow/DefaultScene/DrawQueue.h"
#include "Renderer/RenderFlow/DefaultScene/Passes/PassCommon.h"
#include "Renderer/RenderFlow/DefaultScene/Constants.h"
#include "Renderer/RenderFlow/DefaultScene/GpuTypes.h"
#include "Renderer/RenderFlow/DefaultScene/PipelineResources.h"
#include "Renderer/RenderFlow/RenderBlackboardKeys.h"
#include "Renderer/RenderGraphBuilder.h"
#include "Renderer/RenderWorld/RenderWorld.h"
#include "Renderer/RendererUtilities.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/norm.hpp>

namespace luna::render_flow::default_scene {
namespace {

ShadowResources createShadowResources(RenderGraphBuilder& graph)
{
    const uint32_t shadow_map_size = render_flow::default_scene_detail::kShadowMapSize;
    return ShadowResources{
        .shadow_map = graph.CreateTexture(RenderGraphTextureDesc{
            .Name = "SceneShadowMap",
            .Width = shadow_map_size,
            .Height = shadow_map_size,
            .Format = render_flow::default_scene_detail::kShadowMapFormat,
            .Usage = luna::RHI::TextureUsageFlags::ColorAttachment | luna::RHI::TextureUsageFlags::Sampled,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        }),
        .shadow_depth = graph.CreateTexture(RenderGraphTextureDesc{
            .Name = "SceneShadowDepth",
            .Width = shadow_map_size,
            .Height = shadow_map_size,
            .Format = luna::RHI::Format::D32_FLOAT,
            .Usage = luna::RHI::TextureUsageFlags::DepthStencilAttachment,
            .InitialState = luna::RHI::ResourceState::Undefined,
            .SampleCount = luna::RHI::SampleCount::Count1,
        }),
        .render_params = {},
    };
}

glm::mat4 adjustProjectionForBackend(glm::mat4 projection, luna::RHI::BackendType backend_type)
{
    return backend_type == luna::RHI::BackendType::Vulkan ? luna::flipProjectionY(projection) : projection;
}

glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback)
{
    const float length_squared = glm::dot(value, value);
    return length_squared > 1.0e-6f ? glm::normalize(value) : fallback;
}

render_flow::default_scene_detail::ShadowRenderParams
    buildDirectionalShadowParams(const RenderWorld* world, const SceneRenderContext& context, const DrawQueue& draw_queue)
{
    render_flow::default_scene_detail::ShadowRenderParams params{};
    params.params = glm::vec4(0.0f,
                              0.0018f,
                              0.0f,
                              1.0f / static_cast<float>(render_flow::default_scene_detail::kShadowMapSize));
    if (!world || world->directionalLights().empty() ||
        draw_queue.drawCommands(luna::RenderPhase::ShadowCaster).empty()) {
        return params;
    }

    const RenderDirectionalLight& light = world->directionalLights().front();
    if (light.intensity <= 0.0f) {
        return params;
    }

    constexpr float kShadowHalfExtent = 40.0f;
    constexpr float kShadowCameraDistance = 70.0f;
    constexpr float kShadowDepthRange = 160.0f;
    const glm::vec3 light_direction = safeNormalize(light.direction, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec3 camera_position = world->camera().getPosition();
    const glm::vec3 shadow_center = camera_position + world->camera().getForwardDirection() * 20.0f;
    const glm::vec3 shadow_eye = shadow_center + light_direction * kShadowCameraDistance;

    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::length2(glm::cross(-light_direction, up)) <= 1.0e-6f) {
        up = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    const glm::mat4 view = glm::lookAtRH(shadow_eye, shadow_center, up);
    const glm::mat4 projection = glm::orthoRH_ZO(
        -kShadowHalfExtent, kShadowHalfExtent, -kShadowHalfExtent, kShadowHalfExtent, 0.0f, kShadowDepthRange);
    params.view_projection = adjustProjectionForBackend(projection, context.backend_type) * view;
    params.params.x = 1.0f;
    return params;
}

} // namespace

ShadowDepthPass::ShadowDepthPass(PassSharedState& state) : m_state(&state) {}

const char* ShadowDepthPass::name() const noexcept
{
    return "SceneShadowDepth";
}

void ShadowDepthPass::setup(RenderPassContext& context)
{
    ShadowResources shadow = createShadowResources(context.graph());
    shadow.render_params = buildDirectionalShadowParams(m_state->world(), context.sceneContext(), m_state->drawQueue());
    m_state->setShadowParams(shadow.render_params);

    context.blackboard().set(blackboard::ShadowMap, shadow.shadow_map);

    context.graph().AddRasterPass(
        name(),
        [shadow](RenderGraphRasterPassBuilder& pass_builder) {
            pass_builder.WriteColor(shadow.shadow_map,
                                    luna::RHI::AttachmentLoadOp::Clear,
                                    luna::RHI::AttachmentStoreOp::Store,
                                    luna::RHI::ClearValue::ColorFloat(1.0f, 1.0f, 1.0f, 1.0f));
            pass_builder.WriteDepth(shadow.shadow_depth,
                                    luna::RHI::AttachmentLoadOp::Clear,
                                    luna::RHI::AttachmentStoreOp::Store,
                                    {1.0f, 0});
        },
        [this, scene_context = context.sceneContext()](RenderGraphRasterPassContext& pass_context) {
            execute(pass_context, scene_context);
        });
}

void ShadowDepthPass::execute(RenderGraphRasterPassContext& pass_context, const SceneRenderContext& context)
{
    AssetCache& assets = m_state->assets();
    PipelineResources& pipelines = m_state->pipelines();
    DrawQueue& draw_queue = m_state->drawQueue();
    const Material& default_material = m_state->defaultMaterial();
    const auto shadow_draw_commands = draw_queue.drawCommands(luna::RenderPhase::ShadowCaster);
    LUNA_RENDERER_FRAME_DEBUG("Executing scene shadow pass with {} shadow caster draw command(s)",
                              shadow_draw_commands.size());

    const DrawPassResources pass_resources = pipelines.shadowPassResources();
    if (!pass_resources.isValid()) {
        LUNA_RENDERER_ERROR("Scene shadow pass aborted: shadow_pipeline={} scene_descriptor_set={}",
                            static_cast<bool>(pass_resources.pipeline),
                            static_cast<bool>(pass_resources.scene_descriptor_set));
        return;
    }

    auto& commands = pass_context.commandBuffer();
    assets.prepareDraws(commands, shadow_draw_commands, default_material, AssetCache::Bindings{
        .device = pipelines.device(),
        .descriptor_pool = pipelines.descriptorPool(),
        .material_layout = pipelines.materialLayout(),
    });

    pass_context.beginRendering();
    commands.BindGraphicsPipeline(pass_resources.pipeline);
    configureViewportAndScissor(commands, pass_context.framebufferWidth(), pass_context.framebufferHeight());
    const size_t recorded_draw_count =
        recordShadowDrawCommands(commands, pass_resources, shadow_draw_commands, assets, default_material);
    LUNA_RENDERER_FRAME_DEBUG("Scene shadow pass recorded {}/{} draw command(s)",
                              recorded_draw_count,
                              shadow_draw_commands.size());
    pass_context.endRendering();
}

} // namespace luna::render_flow::default_scene
