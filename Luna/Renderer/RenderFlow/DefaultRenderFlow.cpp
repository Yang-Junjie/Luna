#include "Renderer/RenderFlow/DefaultRenderFlow.h"

#include "Core/Log.h"
#include "Renderer/RenderFlow/DefaultScene/PassNames.h"
#include "Renderer/RenderWorld/RenderWorld.h"
#include "Renderer/RendererUtilities.h"

namespace luna {

DefaultRenderFlow::DefaultRenderFlow()
    : m_scene_state(m_resources, m_draw_queue, m_default_material)
{
    namespace pass_names = render_flow::default_scene::pass_names;

    m_builder.addPass(std::string(pass_names::Geometry),
                      std::make_unique<render_flow::default_scene::DefaultSceneGeometryPass>(m_scene_state));
    m_builder.insertPassAfter(
        pass_names::Geometry,
        std::string(pass_names::Lighting),
        std::make_unique<render_flow::default_scene::DefaultSceneLightingPass>(m_scene_state));
    m_builder.insertPassAfter(
        pass_names::Lighting,
        std::string(pass_names::Transparent),
        std::make_unique<render_flow::default_scene::DefaultSceneTransparentPass>(m_scene_state));
}

DefaultRenderFlow::~DefaultRenderFlow()
{
    shutdown();
}

void DefaultRenderFlow::shutdown()
{
    m_builder.clear();
    m_draw_queue.clear();
    m_resources.shutdown();
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

    m_draw_queue.beginScene(world.camera());
    for (const auto& packet : world.drawPackets()) {
        m_draw_queue.submitDrawPacket(packet);
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
        m_draw_queue.drawCommands().size(),
        scene_context.show_pick_debug_visualization);

    m_scene_state.setWorld(world);
    render_flow::RenderPassContext pass_context(context.graph(), world, scene_context);
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






