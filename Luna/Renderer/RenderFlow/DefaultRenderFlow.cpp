#include "Renderer/RenderFlow/DefaultRenderFlow.h"

#include "Core/Log.h"
#include "Renderer/RenderFlow/DefaultScene/Feature.h"
#include "Renderer/RenderWorld/RenderWorld.h"
#include "Renderer/RendererUtilities.h"

#include <algorithm>

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

std::vector<render_flow::RenderFeatureInfo> DefaultRenderFlow::featureInfos() const
{
    std::vector<render_flow::RenderFeatureInfo> infos;
    infos.reserve(m_features.size());
    for (const auto& feature : m_features) {
        if (feature) {
            infos.push_back(feature->info());
        }
    }
    return infos;
}

bool DefaultRenderFlow::setFeatureEnabled(std::string_view name, bool enabled)
{
    for (const auto& feature : m_features) {
        if (!feature) {
            continue;
        }

        const render_flow::RenderFeatureInfo info = feature->info();
        if (info.name != name) {
            continue;
        }

        if (!info.runtime_toggleable) {
            LUNA_RENDERER_WARN("Render feature '{}' does not support runtime toggling", name);
            return false;
        }

        if (info.enabled == enabled) {
            return true;
        }

        return feature->setEnabled(enabled);
    }

    LUNA_RENDERER_WARN("Render feature '{}' was not found", name);
    return false;
}

std::vector<render_flow::RenderFeatureParameterInfo> DefaultRenderFlow::featureParameters(std::string_view name) const
{
    for (const auto& feature : m_features) {
        if (!feature) {
            continue;
        }

        if (feature->info().name == name) {
            return feature->parameters();
        }
    }

    return {};
}

bool DefaultRenderFlow::setFeatureParameter(std::string_view feature_name,
                                            std::string_view parameter_name,
                                            const render_flow::RenderFeatureParameterValue& value)
{
    for (const auto& feature : m_features) {
        if (!feature) {
            continue;
        }

        if (feature->info().name != feature_name) {
            continue;
        }

        const auto parameters = feature->parameters();
        const auto parameter_it = std::find_if(parameters.begin(), parameters.end(), [parameter_name](const auto& parameter) {
            return parameter.name == parameter_name;
        });
        if (parameter_it == parameters.end()) {
            LUNA_RENDERER_WARN("Render feature '{}' has no parameter '{}'", feature_name, parameter_name);
            return false;
        }
        if (parameter_it->read_only) {
            LUNA_RENDERER_WARN("Render feature parameter '{}.{}' is read-only", feature_name, parameter_name);
            return false;
        }
        if (parameter_it->type != value.type) {
            LUNA_RENDERER_WARN("Render feature parameter '{}.{}' received a mismatched value type",
                               feature_name,
                               parameter_name);
            return false;
        }

        return feature->setParameter(parameter_name, value);
    }

    LUNA_RENDERER_WARN("Render feature '{}' was not found", feature_name);
    return false;
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
