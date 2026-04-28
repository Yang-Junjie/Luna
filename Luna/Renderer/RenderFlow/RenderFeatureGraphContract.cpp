#include "Renderer/RenderFlow/RenderFeatureGraphContract.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace luna::render_flow {
namespace {

struct GraphResourceUse {
    std::string feature_name;
    RenderFeatureGraphResourceKind kind{RenderFeatureGraphResourceKind::Texture};
};

struct FeatureValidationState {
    std::string feature_name;
    bool active{false};
    RenderFeatureRequirements requirements{};
    std::vector<std::string> issues;
};

using ResourceUseMap = std::unordered_map<std::string, std::vector<GraphResourceUse>>;

bool isOptional(const RenderFeatureGraphResource& resource) noexcept
{
    return resource.flags & RenderFeatureGraphResourceFlags::Optional;
}

bool isExternal(const RenderFeatureGraphResource& resource) noexcept
{
    return resource.flags & RenderFeatureGraphResourceFlags::External;
}

bool requiresProducer(const RenderFeatureGraphResource& resource) noexcept
{
    return !isOptional(resource) && !isExternal(resource);
}

bool requiresConsumer(const RenderFeatureGraphResource& resource) noexcept
{
    return !isOptional(resource) && !isExternal(resource);
}

const char* graphResourceKindLabel(RenderFeatureGraphResourceKind kind) noexcept
{
    switch (kind) {
        case RenderFeatureGraphResourceKind::Texture:
            return "Texture";
        case RenderFeatureGraphResourceKind::Buffer:
            return "Buffer";
    }
    return "Unknown";
}

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

std::string joinFeatureNames(const std::vector<GraphResourceUse>& values)
{
    std::string result;
    for (size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            result += ", ";
        }
        result += values[index].feature_name;
    }
    return result;
}

void addDeclarationIssues(std::string_view role,
                          std::span<const RenderFeatureGraphResource> resources,
                          std::vector<std::string>& issues)
{
    std::unordered_map<std::string, RenderFeatureGraphResourceKind> seen;
    for (size_t index = 0; index < resources.size(); ++index) {
        const RenderFeatureGraphResource& resource = resources[index];
        if (resource.name.empty()) {
            issues.push_back("graph " + std::string(role) + " #" + std::to_string(index) + " has empty name");
            continue;
        }

        const std::string resource_name(resource.name);
        const auto [seen_it, inserted] = seen.emplace(resource_name, resource.kind);
        if (inserted) {
            continue;
        }

        if (seen_it->second == resource.kind) {
            issues.push_back("graph " + std::string(role) + " '" + resource_name +
                             "' is declared more than once");
        } else {
            issues.push_back("graph " + std::string(role) + " '" + resource_name +
                             "' is declared more than once with different kinds: " +
                             graphResourceKindLabel(seen_it->second) + " and " +
                             graphResourceKindLabel(resource.kind));
        }
    }
}

void addResourceUses(const std::string& feature_name,
                     std::span<const RenderFeatureGraphResource> resources,
                     ResourceUseMap& uses)
{
    for (const RenderFeatureGraphResource& resource : resources) {
        if (resource.name.empty()) {
            continue;
        }

        uses[std::string(resource.name)].push_back(GraphResourceUse{
            .feature_name = feature_name,
            .kind = resource.kind,
        });
    }
}

void addProducerCompatibilityIssues(const RenderFeatureGraphResource& input,
                                    const ResourceUseMap& producers,
                                    std::vector<std::string>& issues)
{
    if (input.name.empty()) {
        return;
    }

    const std::string resource_name(input.name);
    const auto producer_it = producers.find(resource_name);
    if (producer_it == producers.end()) {
        if (requiresProducer(input)) {
            issues.push_back("missing producer for graph input '" + resource_name + "'");
        }
        return;
    }

    for (const GraphResourceUse& producer : producer_it->second) {
        if (producer.kind != input.kind) {
            issues.push_back("graph input '" + resource_name + "' kind mismatch: declared " +
                             graphResourceKindLabel(input.kind) + " but producer '" + producer.feature_name +
                             "' outputs " + graphResourceKindLabel(producer.kind));
        }
    }
}

void addConsumerCompatibilityIssues(const RenderFeatureGraphResource& output,
                                    const ResourceUseMap& producers,
                                    const ResourceUseMap& consumers,
                                    std::vector<std::string>& issues)
{
    if (output.name.empty()) {
        return;
    }

    const std::string resource_name(output.name);
    const auto producer_it = producers.find(resource_name);
    if (producer_it != producers.end() && producer_it->second.size() > 1) {
        issues.push_back("graph output '" + resource_name +
                         "' has multiple active producers: " + joinFeatureNames(producer_it->second));
    }

    const auto consumer_it = consumers.find(resource_name);
    if (consumer_it == consumers.end()) {
        if (requiresConsumer(output)) {
            issues.push_back("graph output '" + resource_name + "' has no active consumer");
        }
        return;
    }

    for (const GraphResourceUse& consumer : consumer_it->second) {
        if (consumer.kind != output.kind) {
            issues.push_back("graph output '" + resource_name + "' kind mismatch: declared " +
                             graphResourceKindLabel(output.kind) + " but consumer '" + consumer.feature_name +
                             "' expects " + graphResourceKindLabel(consumer.kind));
        }
    }
}

} // namespace

std::vector<RenderFeatureGraphContractResult> validateRenderFeatureGraphContracts(
    std::span<const RenderFeatureGraphContractInput> features)
{
    std::vector<FeatureValidationState> states;
    states.reserve(features.size());

    ResourceUseMap producers;
    ResourceUseMap consumers;

    for (const RenderFeatureGraphContractInput& feature : features) {
        FeatureValidationState& state = states.emplace_back(FeatureValidationState{
            .feature_name = std::string(feature.feature_name),
            .active = feature.active,
            .requirements = feature.requirements,
        });

        if (!state.active) {
            continue;
        }

        addDeclarationIssues("input", state.requirements.graph_inputs, state.issues);
        addDeclarationIssues("output", state.requirements.graph_outputs, state.issues);
        addResourceUses(state.feature_name, state.requirements.graph_outputs, producers);
        addResourceUses(state.feature_name, state.requirements.graph_inputs, consumers);
    }

    std::vector<RenderFeatureGraphContractResult> results;
    results.reserve(states.size());
    for (FeatureValidationState& state : states) {
        if (!state.active) {
            results.push_back(RenderFeatureGraphContractResult{
                .feature_name = std::move(state.feature_name),
                .valid = true,
                .summary = "inactive",
            });
            continue;
        }

        for (const RenderFeatureGraphResource& input : state.requirements.graph_inputs) {
            addProducerCompatibilityIssues(input, producers, state.issues);
        }

        for (const RenderFeatureGraphResource& output : state.requirements.graph_outputs) {
            addConsumerCompatibilityIssues(output, producers, consumers, state.issues);
        }

        const bool valid = state.issues.empty();
        results.push_back(RenderFeatureGraphContractResult{
            .feature_name = std::move(state.feature_name),
            .valid = valid,
            .summary = valid ? std::string("ok") : joinStrings(state.issues),
        });
    }

    return results;
}

} // namespace luna::render_flow
