#include "Renderer/RenderFlow/DefaultRenderFlow.h"

#include "Core/Log.h"
#include "Renderer/RenderFlow/DefaultScene/Feature.h"
#include "Renderer/RenderWorld/RenderWorld.h"
#include "Renderer/RendererUtilities.h"

namespace luna {

DefaultRenderFlow::DefaultRenderFlow()
{
    addFeature(std::make_unique<render_flow::default_scene::Feature>());
}

DefaultRenderFlow::~DefaultRenderFlow()
{
    shutdown();
}

void DefaultRenderFlow::shutdown()
{
    m_builder.clear();
    for (const auto& feature : m_features) {
        feature->shutdown();
    }
    m_features.clear();
}

bool DefaultRenderFlow::addFeature(std::unique_ptr<render_flow::IRenderFeature> feature)
{
    if (!feature) {
        LUNA_RENDERER_ERROR("Default render flow feature registration failed: feature is null");
        return false;
    }

    if (!feature->registerPasses(m_builder)) {
        LUNA_RENDERER_ERROR("Default render flow feature registration failed: {}", m_builder.lastError());
        return false;
    }

    m_features.push_back(std::move(feature));
    return true;
}

bool DefaultRenderFlow::configure(const ConfigureFunction& configure_function)
{
    if (!configure_function) {
        LUNA_RENDERER_ERROR("Default render flow configuration failed: configure function is empty");
        return false;
    }

    configure_function(m_builder);

    if (!m_builder.lastError().empty()) {
        LUNA_RENDERER_ERROR("Default render flow configuration failed: {}", m_builder.lastError());
        return false;
    }

    const render_flow::RenderFlowBuilder::CompileResult compiled_flow = m_builder.compile();
    if (!compiled_flow.success) {
        LUNA_RENDERER_ERROR("Default render flow configuration failed: {}", compiled_flow.error);
        return false;
    }

    return true;
}

render_flow::RenderFlowBuilder& DefaultRenderFlow::builder() noexcept
{
    return m_builder;
}

const render_flow::RenderFlowBuilder& DefaultRenderFlow::builder() const noexcept
{
    return m_builder;
}

void DefaultRenderFlow::render(RenderFlowContext& context)
{
    const RenderWorld& world = context.world();
    const SceneRenderContext& scene_context = context.sceneContext();

    if (!world.hasCamera()) {
        return;
    }

    if (!scene_context.isValid()) {
        LUNA_RENDERER_WARN("Scene render graph build skipped because context is invalid: device={} color={} depth={} size={}x{}",
                           static_cast<bool>(scene_context.device),
                           scene_context.color_target.isValid(),
                           scene_context.depth_target.isValid(),
                           scene_context.framebuffer_width,
                           scene_context.framebuffer_height);
        return;
    }

    LUNA_RENDERER_FRAME_DEBUG(
        "Building default render flow: size={}x{} backend={} color_format={} ({}) draw_packets={} pick_debug={}",
        scene_context.framebuffer_width,
        scene_context.framebuffer_height,
        renderer_detail::backendTypeToString(scene_context.backend_type),
        renderer_detail::formatToString(scene_context.color_format),
        static_cast<int>(scene_context.color_format),
        world.drawPackets().size(),
        scene_context.show_pick_debug_visualization);

    m_blackboard.clear();
    for (const auto& feature : m_features) {
        feature->prepareFrame(world, scene_context, m_blackboard);
    }

    render_flow::RenderPassContext pass_context(context.graph(), world, scene_context, m_blackboard);
    const render_flow::RenderFlowBuilder::CompileResult compiled_flow = m_builder.compile();
    if (!compiled_flow.success) {
        LUNA_RENDERER_ERROR("Default render flow compile failed: {}", compiled_flow.error);
        return;
    }

    for (const auto& entry : compiled_flow.passes) {
        if (entry.pass) {
            entry.pass->setup(pass_context);
        }
    }
}

} // namespace luna
