#include "Renderer/RenderFlow/RenderPassGraphContract.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace luna::render_flow {
namespace {

struct ResourceDeclaration {
    RenderFeatureGraphResourceKind kind{RenderFeatureGraphResourceKind::Texture};
    RenderFeatureGraphResourceFlags flags{RenderFeatureGraphResourceFlags::None};
    bool declared_input{false};
    bool declared_output{false};
};

struct PassResourceUse {
    std::string feature_name;
    std::string pass_name;
    RenderFeatureGraphResourceKind kind{RenderFeatureGraphResourceKind::Texture};
    RenderPassResourceAccess access{RenderPassResourceAccess::Read};
    RenderFeatureGraphResourceFlags flags{RenderFeatureGraphResourceFlags::None};
};

struct FeatureValidationState {
    std::string feature_name;
    bool active{false};
    RenderFeatureRequirements requirements{};
    std::span<const RenderPassGraphContractPassInput> passes;
    std::vector<std::string> issues;
};

using ResourceDeclarationMap = std::unordered_map<std::string, ResourceDeclaration>;
using ResourceUseMap = std::unordered_map<std::string, std::vector<PassResourceUse>>;

bool hasFlag(RenderFeatureGraphResourceFlags flags, RenderFeatureGraphResourceFlags flag) noexcept
{
    return flags & flag;
}

bool isOptional(RenderFeatureGraphResourceFlags flags) noexcept
{
    return hasFlag(flags, RenderFeatureGraphResourceFlags::Optional);
}

bool isExternal(RenderFeatureGraphResourceFlags flags) noexcept
{
    return hasFlag(flags, RenderFeatureGraphResourceFlags::External);
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

const char* passResourceAccessLabel(RenderPassResourceAccess access) noexcept
{
    switch (access) {
        case RenderPassResourceAccess::Read:
            return "read";
        case RenderPassResourceAccess::Write:
            return "write";
        case RenderPassResourceAccess::ReadWrite:
            return "read/write";
    }
    return "unknown";
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

ResourceDeclarationMap buildFeatureDeclarations(const RenderFeatureRequirements& requirements)
{
    ResourceDeclarationMap declarations;
    for (const RenderFeatureGraphResource& input : requirements.graph_inputs) {
        if (input.name.empty()) {
            continue;
        }

        ResourceDeclaration& declaration = declarations[std::string(input.name)];
        declaration.kind = input.kind;
        declaration.flags |= input.flags;
        declaration.declared_input = true;
    }

    for (const RenderFeatureGraphResource& output : requirements.graph_outputs) {
        if (output.name.empty()) {
            continue;
        }

        ResourceDeclaration& declaration = declarations[std::string(output.name)];
        declaration.kind = output.kind;
        declaration.flags |= output.flags;
        declaration.declared_output = true;
    }
    return declarations;
}

const ResourceDeclaration* findDeclaration(const ResourceDeclarationMap& declarations, std::string_view name)
{
    const auto it = declarations.find(std::string(name));
    return it != declarations.end() ? &it->second : nullptr;
}

bool readAllowedByFeatureContract(const ResourceDeclaration& declaration) noexcept
{
    return declaration.declared_input || declaration.declared_output;
}

bool writeAllowedByFeatureContract(const ResourceDeclaration& declaration) noexcept
{
    return declaration.declared_output;
}

void addPassResourceIssues(const FeatureValidationState& state,
                           const ResourceDeclarationMap& declarations,
                           ResourceUseMap& producers,
                           ResourceUseMap& consumers,
                           std::vector<std::string>& issues)
{
    for (const RenderPassGraphContractPassInput& pass : state.passes) {
        if (pass.pass_name.empty()) {
            issues.push_back("pass contract has empty pass name");
            continue;
        }

        std::unordered_map<std::string, RenderFeatureGraphResourceKind> seen;
        for (size_t resource_index = 0; resource_index < pass.resources.size(); ++resource_index) {
            const RenderPassResourceUsage& resource = pass.resources[resource_index];
            if (resource.name.empty()) {
                issues.push_back("pass '" + std::string(pass.pass_name) + "' resource #" +
                                 std::to_string(resource_index) + " has empty name");
                continue;
            }

            const std::string resource_name(resource.name);
            const auto [seen_it, inserted] = seen.emplace(resource_name, resource.kind);
            if (!inserted && seen_it->second != resource.kind) {
                issues.push_back("pass '" + std::string(pass.pass_name) + "' resource '" + resource_name +
                                 "' is declared with different kinds: " +
                                 graphResourceKindLabel(seen_it->second) + " and " +
                                 graphResourceKindLabel(resource.kind));
            }

            const ResourceDeclaration* declaration = findDeclaration(declarations, resource.name);
            if (declaration == nullptr) {
                issues.push_back("pass '" + std::string(pass.pass_name) + "' " +
                                 passResourceAccessLabel(resource.access) + "s graph resource '" +
                                 resource_name + "' that is not declared by feature contract");
            } else {
                if (declaration->kind != resource.kind) {
                    issues.push_back("pass '" + std::string(pass.pass_name) + "' resource '" + resource_name +
                                     "' kind mismatch: pass declares " +
                                     graphResourceKindLabel(resource.kind) + " but feature contract declares " +
                                     graphResourceKindLabel(declaration->kind));
                }
                if (readsResource(resource.access) && !readAllowedByFeatureContract(*declaration)) {
                    issues.push_back("pass '" + std::string(pass.pass_name) + "' reads graph resource '" +
                                     resource_name + "' that is not readable by feature contract");
                }
                if (writesResource(resource.access) && !writeAllowedByFeatureContract(*declaration)) {
                    issues.push_back("pass '" + std::string(pass.pass_name) + "' writes graph resource '" +
                                     resource_name + "' that is not an output of feature contract");
                }
            }

            const PassResourceUse use{
                .feature_name = state.feature_name,
                .pass_name = std::string(pass.pass_name),
                .kind = resource.kind,
                .access = resource.access,
                .flags = resource.flags,
            };
            if (readsResource(resource.access)) {
                consumers[resource_name].push_back(use);
            }
            if (writesResource(resource.access)) {
                producers[resource_name].push_back(use);
            }
        }
    }
}

void addFeatureCoverageIssues(const FeatureValidationState& state,
                              const ResourceUseMap& producers,
                              const ResourceUseMap& consumers,
                              std::vector<std::string>& issues)
{
    for (const RenderFeatureGraphResource& input : state.requirements.graph_inputs) {
        if (input.name.empty() || isOptional(input.flags) || isExternal(input.flags)) {
            continue;
        }

        if (!consumers.contains(std::string(input.name))) {
            issues.push_back("feature graph input '" + std::string(input.name) +
                             "' is not read by any declared pass");
        }
    }

    for (const RenderFeatureGraphResource& output : state.requirements.graph_outputs) {
        if (output.name.empty() || isOptional(output.flags) || isExternal(output.flags)) {
            continue;
        }

        if (!producers.contains(std::string(output.name))) {
            issues.push_back("feature graph output '" + std::string(output.name) +
                             "' is not written by any declared pass");
        }
    }
}

void addProducerIssues(const FeatureValidationState& state,
                       const ResourceDeclarationMap& declarations,
                       const ResourceUseMap& producers,
                       const ResourceUseMap& consumers,
                       std::vector<std::string>& issues)
{
    for (const RenderPassGraphContractPassInput& pass : state.passes) {
        for (const RenderPassResourceUsage& resource : pass.resources) {
            if (resource.name.empty() || !readsResource(resource.access)) {
                continue;
            }

            const std::string resource_name(resource.name);
            const ResourceDeclaration* declaration = findDeclaration(declarations, resource.name);
            const RenderFeatureGraphResourceFlags flags =
                declaration != nullptr ? (declaration->flags | resource.flags) : resource.flags;
            if (isOptional(flags) || isExternal(flags)) {
                continue;
            }

            if (!producers.contains(resource_name)) {
                issues.push_back("pass '" + std::string(pass.pass_name) + "' reads graph resource '" +
                                 resource_name + "' with no active pass producer");
            }
        }
    }

    for (const auto& [resource_name, resource_producers] : producers) {
        const auto consumer_it = consumers.find(resource_name);
        if (consumer_it != consumers.end()) {
            continue;
        }

        for (const PassResourceUse& producer : resource_producers) {
            const ResourceDeclaration* declaration = findDeclaration(declarations, resource_name);
            const RenderFeatureGraphResourceFlags flags =
                declaration != nullptr ? (declaration->flags | producer.flags) : producer.flags;
            if (producer.feature_name != state.feature_name || isOptional(flags) || isExternal(flags)) {
                continue;
            }

            issues.push_back("pass '" + producer.pass_name + "' writes graph resource '" + resource_name +
                             "' with no active pass consumer");
        }
    }
}

} // namespace

std::vector<RenderPassGraphContractResult> validateRenderPassGraphContracts(
    std::span<const RenderPassGraphContractFeatureInput> features)
{
    std::vector<FeatureValidationState> states;
    states.reserve(features.size());

    ResourceUseMap producers;
    ResourceUseMap consumers;
    for (const RenderPassGraphContractFeatureInput& feature : features) {
        FeatureValidationState& state = states.emplace_back(FeatureValidationState{
            .feature_name = std::string(feature.feature_name),
            .active = feature.active,
            .requirements = feature.requirements,
            .passes = feature.passes,
        });

        if (!state.active) {
            continue;
        }

        const ResourceDeclarationMap declarations = buildFeatureDeclarations(state.requirements);
        addPassResourceIssues(state, declarations, producers, consumers, state.issues);
    }

    std::vector<RenderPassGraphContractResult> results;
    results.reserve(states.size());
    for (FeatureValidationState& state : states) {
        if (!state.active) {
            results.push_back(RenderPassGraphContractResult{
                .feature_name = std::move(state.feature_name),
                .valid = true,
                .summary = "inactive",
            });
            continue;
        }

        const ResourceDeclarationMap declarations = buildFeatureDeclarations(state.requirements);
        addFeatureCoverageIssues(state, producers, consumers, state.issues);
        addProducerIssues(state, declarations, producers, consumers, state.issues);

        const bool valid = state.issues.empty();
        results.push_back(RenderPassGraphContractResult{
            .feature_name = std::move(state.feature_name),
            .valid = valid,
            .summary = valid ? std::string("ok") : joinStrings(state.issues),
        });
    }

    return results;
}

} // namespace luna::render_flow
