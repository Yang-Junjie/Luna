#include "Renderer/RenderFlow/DefaultRenderFlow.h"

#include "Core/Log.h"
#include "Renderer/RenderFlow/DefaultScene/Feature.h"
#include "Renderer/RenderFlow/Features/RenderFeatureModules.h"
#include "Renderer/RenderFlow/RenderFeatureRegistry.h"
#include "Renderer/RenderFlow/RenderFeatureSupport.h"
#include "Renderer/RenderWorld/RenderWorld.h"
#include "Renderer/RendererUtilities.h"

#include <algorithm>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace luna {
namespace {

std::string joinStrings(const std::vector<std::string>& values)
{
    std::string result;
    for (size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            result += "; ";
        }
        result += values[index];
    }
    return result;
}

std::string joinFeatureNames(const std::vector<std::string>& values)
{
    std::string result;
    for (size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            result += ", ";
        }
        result += values[index];
    }
    return result;
}

std::vector<render_flow::RenderFeatureGraphResource>
    copyGraphResources(std::span<const render_flow::RenderFeatureGraphResource> resources)
{
    return {resources.begin(), resources.end()};
}

bool isOptional(const render_flow::RenderFeatureGraphResource& resource) noexcept
{
    return resource.flags & render_flow::RenderFeatureGraphResourceFlags::Optional;
}

bool isExternal(const render_flow::RenderFeatureGraphResource& resource) noexcept
{
    return resource.flags & render_flow::RenderFeatureGraphResourceFlags::External;
}

} // namespace

DefaultRenderFlow::DefaultRenderFlow()
{
    render_flow::linkBuiltInRenderFeatureModules();
    addFeature(std::make_unique<render_flow::default_scene::Feature>());
    installRegisteredFeatures();
}

DefaultRenderFlow::~DefaultRenderFlow()
{
    shutdown();
}

void DefaultRenderFlow::shutdown()
{
    m_frame_ready_to_commit = false;
    m_builder.clear();
    m_feature_runtime_states.clear();
    for (const auto& feature : m_features) {
        feature->releasePersistentResources();
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

    const render_flow::RenderFeatureInfo info = feature->info();
    if (info.name.empty()) {
        LUNA_RENDERER_ERROR("Default render flow feature registration failed: feature name is empty");
        return false;
    }

    if (hasFeature(info.name)) {
        LUNA_RENDERER_WARN("Default render flow feature '{}' is already registered; skipping duplicate", info.name);
        return false;
    }

    if (!feature->registerPasses(m_builder)) {
        LUNA_RENDERER_ERROR("Default render flow feature '{}' registration failed: {}", info.name, m_builder.lastError());
        return false;
    }

    const std::string feature_name(info.name);
    auto& runtime_state = m_feature_runtime_states[feature_name];
    runtime_state.owned_passes.clear();
    for (std::string_view pass_name : m_builder.passesForFeature(feature_name)) {
        runtime_state.owned_passes.emplace_back(pass_name);
    }
    runtime_state.supported = true;
    runtime_state.active_this_frame = false;
    runtime_state.prepared_this_frame = false;
    runtime_state.support_summary = "not evaluated";
    runtime_state.graph_contract_valid = true;
    runtime_state.graph_contract_summary = "not evaluated";
    if (runtime_state.owned_passes.empty()) {
        LUNA_RENDERER_WARN("Default render flow feature '{}' registered no owned passes; use RenderFlowBuilder feature pass APIs",
                           info.name);
    }

    LUNA_RENDERER_INFO("Registered default render flow feature '{}'", info.name);
    m_features.push_back(std::move(feature));
    return true;
}

void DefaultRenderFlow::installRegisteredFeatures()
{
    const auto descriptors =
        render_flow::RenderFeatureRegistry::instance().descriptorsForFlow(render_flow::kDefaultRenderFlowName);
    for (const auto& descriptor : descriptors) {
        if (hasFeature(descriptor.name)) {
            LUNA_RENDERER_WARN("Skipping auto-installed render feature '{}' because it is already registered",
                               descriptor.name);
            continue;
        }

        auto feature = render_flow::RenderFeatureRegistry::instance().createFeature(descriptor.name);
        if (!feature) {
            LUNA_RENDERER_WARN("Skipping auto-installed render feature '{}' because its factory returned null",
                               descriptor.name);
            continue;
        }

        if (!descriptor.enabled_by_default && !feature->setEnabled(false)) {
            LUNA_RENDERER_WARN("Render feature '{}' could not apply enabled_by_default=false", descriptor.name);
        }

        if (!addFeature(std::move(feature))) {
            LUNA_RENDERER_WARN("Failed to auto-install render feature '{}'", descriptor.name);
        }
    }
}

bool DefaultRenderFlow::hasFeature(std::string_view name) const
{
    return std::any_of(m_features.begin(), m_features.end(), [name](const auto& feature) {
        return feature && feature->info().name == name;
    });
}

DefaultRenderFlow::FeatureRuntimeState* DefaultRenderFlow::findFeatureRuntimeState(std::string_view name)
{
    const auto state = m_feature_runtime_states.find(std::string(name));
    return state != m_feature_runtime_states.end() ? &state->second : nullptr;
}

const DefaultRenderFlow::FeatureRuntimeState*
    DefaultRenderFlow::findFeatureRuntimeState(std::string_view name) const
{
    const auto state = m_feature_runtime_states.find(std::string(name));
    return state != m_feature_runtime_states.end() ? &state->second : nullptr;
}

bool DefaultRenderFlow::isPassActive(std::string_view name) const
{
    const std::string_view owner = m_builder.passOwner(name);
    if (owner.empty()) {
        return true;
    }

    const FeatureRuntimeState* state = findFeatureRuntimeState(owner);
    return state == nullptr || state->active_this_frame;
}

void DefaultRenderFlow::logFeatureSupportDiagnostics(const render_flow::IRenderFeature& feature,
                                                     const render_flow::RenderFeatureSupportResult& support)
{
    const render_flow::RenderFeatureInfo info = feature.info();
    if (info.name.empty()) {
        return;
    }

    const std::string feature_name(info.name);
    const std::string signature =
        support.supported ? std::string("supported") : render_flow::summarizeRenderFeatureSupport(support);
    auto& state = m_feature_runtime_states[feature_name];
    if (state.support_summary == signature) {
        return;
    }

    const bool had_previous = state.support_summary != "not evaluated";
    state.support_summary = signature;
    state.supported = support.supported;
    if (!support.supported) {
        LUNA_RENDERER_WARN("Render feature '{}' requirements are not satisfied: {}", info.name, signature);
    } else if (had_previous) {
        LUNA_RENDERER_INFO("Render feature '{}' requirements are satisfied", info.name);
    }
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
            render_flow::RenderFeatureInfo info = feature->info();
            if (const FeatureRuntimeState* state = findFeatureRuntimeState(info.name)) {
                info.supported = state->supported;
                info.active = info.enabled && state->supported;
                info.support_summary = state->support_summary;
                info.graph_contract_valid = state->graph_contract_valid;
                info.graph_contract_summary = state->graph_contract_summary;
            } else {
                info.supported = true;
                info.active = info.enabled;
                info.support_summary = "not evaluated";
                info.graph_contract_valid = true;
                info.graph_contract_summary = "not evaluated";
            }
            const render_flow::RenderFeatureRequirements requirements = feature->requirements();
            info.graph_inputs = copyGraphResources(requirements.graph_inputs);
            info.graph_outputs = copyGraphResources(requirements.graph_outputs);
            infos.push_back(std::move(info));
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

        if (!feature->setEnabled(enabled)) {
            return false;
        }

        if (!enabled) {
            if (FeatureRuntimeState* state = findFeatureRuntimeState(info.name)) {
                if (state->active_this_frame) {
                    feature->releasePersistentResources();
                }
                state->active_this_frame = false;
                state->prepared_this_frame = false;
            }
        }
        return true;
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

void DefaultRenderFlow::validateFeatureGraphContracts()
{
    std::unordered_map<std::string, std::vector<std::string>> producers;
    std::unordered_map<std::string, std::vector<std::string>> consumers;

    for (const auto& feature : m_features) {
        if (!feature) {
            continue;
        }

        const render_flow::RenderFeatureInfo info = feature->info();
        const FeatureRuntimeState* state = findFeatureRuntimeState(info.name);
        if (state == nullptr || !state->active_this_frame) {
            continue;
        }

        const render_flow::RenderFeatureRequirements requirements = feature->requirements();
        for (const render_flow::RenderFeatureGraphResource& output : requirements.graph_outputs) {
            if (!output.name.empty()) {
                producers[std::string(output.name)].emplace_back(info.name);
            }
        }
        for (const render_flow::RenderFeatureGraphResource& input : requirements.graph_inputs) {
            if (!input.name.empty()) {
                consumers[std::string(input.name)].emplace_back(info.name);
            }
        }
    }

    for (const auto& feature : m_features) {
        if (!feature) {
            continue;
        }

        const render_flow::RenderFeatureInfo info = feature->info();
        FeatureRuntimeState* state = findFeatureRuntimeState(info.name);
        if (state == nullptr) {
            state = &m_feature_runtime_states[std::string(info.name)];
        }

        if (!state->active_this_frame) {
            state->graph_contract_valid = true;
            state->graph_contract_summary = "inactive";
            continue;
        }

        std::vector<std::string> issues;
        const render_flow::RenderFeatureRequirements requirements = feature->requirements();
        for (const render_flow::RenderFeatureGraphResource& input : requirements.graph_inputs) {
            if (input.name.empty() || isOptional(input)) {
                continue;
            }

            if (!producers.contains(std::string(input.name))) {
                issues.push_back("missing producer for graph input '" + std::string(input.name) + "'");
            }
        }

        for (const render_flow::RenderFeatureGraphResource& output : requirements.graph_outputs) {
            if (output.name.empty()) {
                continue;
            }

            const auto producer_it = producers.find(std::string(output.name));
            if (producer_it != producers.end() && producer_it->second.size() > 1) {
                issues.push_back("graph output '" + std::string(output.name) +
                                 "' has multiple active producers: " + joinFeatureNames(producer_it->second));
            }

            if (!isExternal(output) && !consumers.contains(std::string(output.name))) {
                issues.push_back("graph output '" + std::string(output.name) + "' has no active consumer");
            }
        }

        const bool valid = issues.empty();
        const std::string summary = valid ? std::string("ok") : joinStrings(issues);
        if (state->graph_contract_summary == summary) {
            state->graph_contract_valid = valid;
            continue;
        }

        const bool had_previous = state->graph_contract_summary != "not evaluated" &&
                                  state->graph_contract_summary != "inactive";
        state->graph_contract_valid = valid;
        state->graph_contract_summary = summary;
        if (!valid) {
            LUNA_RENDERER_WARN("Render feature '{}' graph contract diagnostics: {}", info.name, summary);
        } else if (had_previous) {
            LUNA_RENDERER_INFO("Render feature '{}' graph contract diagnostics are clear", info.name);
        }
    }
}

void DefaultRenderFlow::render(RenderFlowContext& context)
{
    m_frame_ready_to_commit = false;

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

    for (const auto& feature : m_features) {
        if (!feature) {
            continue;
        }

        const render_flow::RenderFeatureInfo info = feature->info();
        const render_flow::RenderFeatureSupportResult support =
            render_flow::evaluateRenderFeatureSupport(*feature, scene_context);
        logFeatureSupportDiagnostics(*feature, support);

        FeatureRuntimeState* state = findFeatureRuntimeState(info.name);
        if (state == nullptr) {
            state = &m_feature_runtime_states[std::string(info.name)];
        }

        const bool was_active = state->active_this_frame;
        state->supported = support.supported;
        state->active_this_frame = info.enabled && support.supported;
        state->prepared_this_frame = false;

        if (was_active && !state->active_this_frame) {
            feature->releasePersistentResources();
        }
    }

    validateFeatureGraphContracts();

    m_blackboard.clear();
    for (const auto& feature : m_features) {
        if (!feature) {
            continue;
        }

        const render_flow::RenderFeatureInfo info = feature->info();
        FeatureRuntimeState* state = findFeatureRuntimeState(info.name);
        if (state == nullptr) {
            continue;
        }
        if (!state->active_this_frame) {
            continue;
        }

        feature->prepareFrame(world, scene_context, context.featureFrameContext(), m_blackboard);
        state->prepared_this_frame = true;
    }

    render_flow::RenderPassContext pass_context(context.graph(), world, scene_context, m_blackboard);
    const render_flow::RenderFlowBuilder::CompileResult compiled_flow = m_builder.compile();
    if (!compiled_flow.success) {
        LUNA_RENDERER_ERROR("Default render flow compile failed: {}", compiled_flow.error);
        return;
    }

    for (const auto& entry : compiled_flow.passes) {
        if (entry.pass && isPassActive(entry.name)) {
            entry.pass->setup(pass_context);
        }
    }

    m_frame_ready_to_commit = true;
}

bool DefaultRenderFlow::commitFrame()
{
    if (!m_frame_ready_to_commit) {
        return false;
    }

    m_frame_ready_to_commit = false;
    for (const auto& feature : m_features) {
        if (!feature) {
            continue;
        }

        const render_flow::RenderFeatureInfo info = feature->info();
        const FeatureRuntimeState* state = findFeatureRuntimeState(info.name);
        if (state == nullptr || (state->active_this_frame && state->prepared_this_frame)) {
            feature->commitFrame();
        }
    }
    return true;
}

} // namespace luna
