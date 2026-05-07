#include "EditorUI.h"
#include "RenderFeaturesPanel.h"

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <string>

namespace luna {
namespace {

std::string makeFeatureScopedLabel(std::string_view label, std::string_view feature_name)
{
    std::string scoped_label(label.data(), label.size());
    scoped_label += "##";
    scoped_label.append(feature_name.data(), feature_name.size());
    return scoped_label;
}

std::string
    makeParameterScopedLabel(std::string_view label, std::string_view feature_name, std::string_view parameter_name)
{
    std::string scoped_label = makeFeatureScopedLabel(label, feature_name);
    scoped_label += ".";
    scoped_label.append(parameter_name.data(), parameter_name.size());
    return scoped_label;
}

const char* featureStatusLabel(const render_flow::RenderFeatureInfo& feature) noexcept
{
    if (!feature.supported) {
        return "Unsupported";
    }
    if (!feature.enabled) {
        return "Disabled";
    }
    if (feature.active) {
        return "Active";
    }
    return "Inactive";
}

void drawFeatureStatusTooltip(const render_flow::RenderFeatureInfo& feature)
{
    if (!ImGui::IsItemHovered()) {
        return;
    }

    if (!feature.supported) {
        const char* summary =
            feature.support_summary.empty() ? "Requirements are not satisfied." : feature.support_summary.c_str();
        ImGui::SetTooltip("%s", summary);
        return;
    }
    if (!feature.enabled) {
        ImGui::SetTooltip("Feature is manually disabled.");
        return;
    }
    if (feature.active) {
        ImGui::SetTooltip("Feature is enabled and all evaluated requirements are satisfied.");
        return;
    }

    ImGui::SetTooltip("Feature is not active this frame.");
}

const char* graphResourceKindLabel(render_flow::RenderFeatureGraphResourceKind kind) noexcept
{
    switch (kind) {
        case render_flow::RenderFeatureGraphResourceKind::Texture:
            return "Texture";
        case render_flow::RenderFeatureGraphResourceKind::Buffer:
            return "Buffer";
    }
    return "Unknown";
}

const char* passResourceAccessLabel(render_flow::RenderPassResourceAccess access) noexcept
{
    switch (access) {
        case render_flow::RenderPassResourceAccess::Read:
            return "read";
        case render_flow::RenderPassResourceAccess::Write:
            return "write";
        case render_flow::RenderPassResourceAccess::ReadWrite:
            return "read/write";
    }
    return "unknown";
}

const char* statusValueLabel(bool valid, const std::string& summary) noexcept
{
    if (summary.empty() || summary == "not evaluated") {
        return "Not Evaluated";
    }
    if (summary == "inactive") {
        return "Inactive";
    }
    return valid ? "OK" : "Issues";
}

void drawGraphResourceList(const char* label, const std::vector<render_flow::RenderFeatureGraphResource>& resources)
{
    ImGui::TextDisabled("%s", label);
    ImGui::Indent();
    if (resources.empty()) {
        ImGui::TextDisabled("none");
        ImGui::Unindent();
        return;
    }

    for (const render_flow::RenderFeatureGraphResource& resource : resources) {
        const bool optional = resource.flags & render_flow::RenderFeatureGraphResourceFlags::Optional;
        const bool external = resource.flags & render_flow::RenderFeatureGraphResourceFlags::External;
        ImGui::BulletText("%.*s  [%s]%s%s",
                          static_cast<int>(resource.name.size()),
                          resource.name.data(),
                          graphResourceKindLabel(resource.kind),
                          optional ? " optional" : "",
                          external ? " external" : "");
    }
    ImGui::Unindent();
}

void drawPassResourceList(const std::vector<render_flow::RenderPassResourceUsage>& resources)
{
    ImGui::Indent();
    if (resources.empty()) {
        ImGui::TextDisabled("no declared graph resources");
        ImGui::Unindent();
        return;
    }

    for (const render_flow::RenderPassResourceUsage& resource : resources) {
        const bool optional = resource.flags & render_flow::RenderFeatureGraphResourceFlags::Optional;
        const bool external = resource.flags & render_flow::RenderFeatureGraphResourceFlags::External;
        ImGui::BulletText("%.*s  [%s %s]%s%s",
                          static_cast<int>(resource.name.size()),
                          resource.name.data(),
                          graphResourceKindLabel(resource.kind),
                          passResourceAccessLabel(resource.access),
                          optional ? " optional" : "",
                          external ? " external" : "");
    }
    ImGui::Unindent();
}

void drawStatusLine(const char* label, bool valid, const std::string& summary)
{
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", statusValueLabel(valid, summary));
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", summary.empty() ? "not evaluated" : summary.c_str());
    }
}

void drawFeatureArchitectureSummary(const render_flow::RenderFeatureInfo& feature)
{
    const std::string label = makeFeatureScopedLabel("Architecture", feature.name);
    if (!ImGui::TreeNode(label.c_str())) {
        return;
    }

    drawStatusLine("Support", feature.supported, feature.support_summary);
    drawStatusLine("Graph Contract", feature.graph_contract_valid, feature.graph_contract_summary);
    drawStatusLine("Pass Contract", feature.pass_contract_valid, feature.pass_contract_summary);
    drawStatusLine(
        "Binding Contract", feature.diagnostics.binding_contract_valid, feature.diagnostics.binding_contract_summary);
    drawStatusLine("Pipeline Resources",
                   feature.diagnostics.pipeline_resources_valid,
                   feature.diagnostics.pipeline_resources_summary);
    if (!feature.diagnostics.persistent_resources_summary.empty() ||
        !feature.diagnostics.persistent_resources.empty()) {
        drawStatusLine("Persistent Resources",
                       feature.diagnostics.persistent_resources_valid,
                       feature.diagnostics.persistent_resources_summary);
    }
    if (!feature.diagnostics.history_resources_summary.empty() || !feature.diagnostics.history_resources.empty()) {
        drawStatusLine("History Resources",
                       feature.diagnostics.history_resources_valid,
                       feature.diagnostics.history_resources_summary);
    }
    ImGui::TreePop();
}

void drawFeatureGraphContract(const render_flow::RenderFeatureInfo& feature)
{
    if (feature.graph_inputs.empty() && feature.graph_outputs.empty()) {
        return;
    }

    const std::string label = makeFeatureScopedLabel("Graph Contract", feature.name);
    if (!ImGui::TreeNode(label.c_str())) {
        return;
    }

    ImGui::TextDisabled("Status");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", statusValueLabel(feature.graph_contract_valid, feature.graph_contract_summary));
    if (ImGui::IsItemHovered()) {
        const char* summary =
            feature.graph_contract_summary.empty() ? "not evaluated" : feature.graph_contract_summary.c_str();
        ImGui::SetTooltip("%s", summary);
    }

    drawGraphResourceList("Inputs", feature.graph_inputs);
    drawGraphResourceList("Outputs", feature.graph_outputs);
    ImGui::TreePop();
}

void drawFeaturePassContract(const render_flow::RenderFeatureInfo& feature)
{
    if (feature.pass_contract_summary.empty() && feature.passes.empty()) {
        return;
    }

    const std::string label = makeFeatureScopedLabel("Pass Contract", feature.name);
    if (!ImGui::TreeNode(label.c_str())) {
        return;
    }

    ImGui::TextDisabled("Status");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", statusValueLabel(feature.pass_contract_valid, feature.pass_contract_summary));
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "%s", feature.pass_contract_summary.empty() ? "not evaluated" : feature.pass_contract_summary.c_str());
    }

    ImGui::TextDisabled("Passes");
    ImGui::Indent();
    if (feature.passes.empty()) {
        ImGui::TextDisabled("none");
    } else {
        for (const render_flow::RenderFeaturePassInfo& pass : feature.passes) {
            const std::string pass_label = makeFeatureScopedLabel(pass.name, feature.name);
            if (ImGui::TreeNode(pass_label.c_str())) {
                drawPassResourceList(pass.resources);
                ImGui::TreePop();
            }
        }
    }
    ImGui::Unindent();
    ImGui::TreePop();
}

void drawStatusEntryList(const std::vector<render_flow::RenderFeatureStatusEntry>& entries)
{
    ImGui::Indent();
    if (entries.empty()) {
        ImGui::TextDisabled("none");
        ImGui::Unindent();
        return;
    }

    for (const render_flow::RenderFeatureStatusEntry& entry : entries) {
        ImGui::BulletText("%s: %s", entry.name.c_str(), entry.ready ? "OK" : "Missing");
    }
    ImGui::Unindent();
}

void drawFeatureResourceDiagnostics(const render_flow::RenderFeatureInfo& feature)
{
    const auto& diagnostics = feature.diagnostics;
    const bool has_diagnostics =
        !diagnostics.binding_contract_summary.empty() || !diagnostics.pipeline_resources_summary.empty() ||
        !diagnostics.pipeline_resources.empty() || !diagnostics.persistent_resources_summary.empty() ||
        !diagnostics.persistent_resources.empty() || !diagnostics.history_resources_summary.empty() ||
        !diagnostics.history_resources.empty();
    if (!has_diagnostics) {
        return;
    }

    const std::string label = makeFeatureScopedLabel("Resource Diagnostics", feature.name);
    if (!ImGui::TreeNode(label.c_str())) {
        return;
    }

    drawStatusLine("Binding Contract", diagnostics.binding_contract_valid, diagnostics.binding_contract_summary);
    drawStatusLine("Pipeline Resources", diagnostics.pipeline_resources_valid, diagnostics.pipeline_resources_summary);
    drawStatusEntryList(diagnostics.pipeline_resources);
    if (!diagnostics.persistent_resources_summary.empty() || !diagnostics.persistent_resources.empty()) {
        drawStatusLine(
            "Persistent Resources", diagnostics.persistent_resources_valid, diagnostics.persistent_resources_summary);
        drawStatusEntryList(diagnostics.persistent_resources);
    }
    if (!diagnostics.history_resources_summary.empty() || !diagnostics.history_resources.empty()) {
        drawStatusLine("History Resources", diagnostics.history_resources_valid, diagnostics.history_resources_summary);
        drawStatusEntryList(diagnostics.history_resources);
    }
    ImGui::TreePop();
}

} // namespace

void RenderFeaturesPanel::onImGuiRender(bool& open,
                                        const FeatureList& features,
                                        const GetParametersFunction& get_parameters,
                                        const SetFeatureEnabledFunction& set_feature_enabled,
                                        const SetParameterFunction& set_parameter)
{
    if (!open) {
        return;
    }

    ImGui::SetNextWindowSize(editor::ui::scaled(380.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Render Features", &open)) {
        ImGui::End();
        return;
    }

    if (features.empty()) {
        ImGui::TextUnformatted("No render features registered.");
        ImGui::End();
        return;
    }

    for (const auto& feature : features) {
        const std::string feature_label = makeFeatureScopedLabel(feature.display_name, feature.name);

        bool enabled = feature.enabled;
        const bool can_toggle = feature.runtime_toggleable && feature.supported;
        if (!can_toggle) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Checkbox(feature_label.c_str(), &enabled) && can_toggle && set_feature_enabled) {
            set_feature_enabled(feature.name, enabled);
        }

        if (!can_toggle) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        ImGui::TextDisabled("[%.*s]", static_cast<int>(feature.category.size()), feature.category.data());
        ImGui::SameLine();
        ImGui::TextDisabled("%s", featureStatusLabel(feature));
        drawFeatureStatusTooltip(feature);

        ImGui::Indent();
        drawFeatureArchitectureSummary(feature);
        drawFeatureGraphContract(feature);
        drawFeaturePassContract(feature);
        drawFeatureResourceDiagnostics(feature);
        ImGui::Unindent();

        const auto parameters = get_parameters ? get_parameters(feature.name) : ParameterList{};
        if (!parameters.empty()) {
            ImGui::Indent();
            if (!feature.supported) {
                ImGui::BeginDisabled();
            }
            for (const auto& parameter : parameters) {
                const std::string parameter_label =
                    makeParameterScopedLabel(parameter.display_name, feature.name, parameter.name);

                if (parameter.read_only) {
                    ImGui::BeginDisabled();
                }

                bool changed = false;
                auto value = parameter.value;
                switch (parameter.type) {
                    case render_flow::RenderFeatureParameterType::Bool: {
                        changed = ImGui::Checkbox(parameter_label.c_str(), &value.bool_value);
                        break;
                    }
                    case render_flow::RenderFeatureParameterType::Int: {
                        changed = ImGui::DragInt(parameter_label.c_str(),
                                                 &value.int_value,
                                                 parameter.step,
                                                 parameter.min.int_value,
                                                 parameter.max.int_value);
                        break;
                    }
                    case render_flow::RenderFeatureParameterType::Float: {
                        changed = ImGui::DragFloat(parameter_label.c_str(),
                                                   &value.float_value,
                                                   parameter.step,
                                                   parameter.min.float_value,
                                                   parameter.max.float_value,
                                                   "%.3f");
                        break;
                    }
                    case render_flow::RenderFeatureParameterType::Color: {
                        changed = ImGui::ColorEdit4(parameter_label.c_str(), glm::value_ptr(value.color_value));
                        break;
                    }
                }

                if (changed && !parameter.read_only && set_parameter) {
                    set_parameter(feature.name, parameter.name, value);
                }

                if (parameter.read_only) {
                    ImGui::EndDisabled();
                }
            }
            if (!feature.supported) {
                ImGui::EndDisabled();
            }
            ImGui::Unindent();
        }
    }

    ImGui::End();
}

} // namespace luna
