#include "RenderFeaturesPlugin.h"

#include "EditorApi/EditorApi.h"
#include "Luna/Editor/EditorBuiltinPluginRegistration.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr const char* kPluginId = "luna.editor.render-features";
constexpr const char* kWindowId = "luna.editor.render-features.window";

std::string makeFeatureScopedLabel(std::string_view label, std::string_view feature_name)
{
    std::string scoped_label(label.data(), label.size());
    scoped_label += "##";
    scoped_label.append(feature_name.data(), feature_name.size());
    return scoped_label;
}

std::string makeHiddenParameterLabel(std::string_view feature_name, std::string_view parameter_name)
{
    std::string label{"##"};
    label.append(feature_name.data(), feature_name.size());
    label += ".";
    label.append(parameter_name.data(), parameter_name.size());
    return label;
}

const char* featureStatusLabel(const luna::editor::RenderFeatureInfo& feature) noexcept
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

const char* graphResourceKindLabel(luna::editor::RenderFeatureGraphResourceKind kind) noexcept
{
    switch (kind) {
        case luna::editor::RenderFeatureGraphResourceKind::Texture:
            return "Texture";
        case luna::editor::RenderFeatureGraphResourceKind::Buffer:
            return "Buffer";
    }
    return "Unknown";
}

const char* passResourceAccessLabel(luna::editor::RenderPassResourceAccess access) noexcept
{
    switch (access) {
        case luna::editor::RenderPassResourceAccess::Read:
            return "read";
        case luna::editor::RenderPassResourceAccess::Write:
            return "write";
        case luna::editor::RenderPassResourceAccess::ReadWrite:
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

std::string parameterDisplayName(const luna::editor::RenderFeatureParameterInfo& parameter)
{
    return parameter.display_name.empty() ? parameter.name : parameter.display_name;
}

void drawFeatureStatusTooltip(luna::editor::Ui& ui, const luna::editor::RenderFeatureInfo& feature)
{
    if (!ui.isItemHovered()) {
        return;
    }

    if (!feature.supported) {
        ui.setTooltip(feature.support_summary.empty() ? "Requirements are not satisfied." : feature.support_summary);
        return;
    }
    if (!feature.enabled) {
        ui.setTooltip("Feature is manually disabled.");
        return;
    }
    if (feature.active) {
        ui.setTooltip("Feature is enabled and all evaluated requirements are satisfied.");
        return;
    }

    ui.setTooltip("Feature is not active this frame.");
}

void drawStatusLine(luna::editor::Ui& ui, std::string_view label, bool valid, const std::string& summary)
{
    ui.textDisabled(label);
    ui.sameLine();
    ui.textDisabled(statusValueLabel(valid, summary));
    if (ui.isItemHovered()) {
        ui.setTooltip(summary.empty() ? "not evaluated" : summary);
    }
}

void appendResourceFlags(std::string& text, luna::editor::RenderFeatureGraphResourceFlags flags)
{
    if (flags & luna::editor::RenderFeatureGraphResourceFlags::Optional) {
        text += " optional";
    }
    if (flags & luna::editor::RenderFeatureGraphResourceFlags::External) {
        text += " external";
    }
}

void drawGraphResourceList(luna::editor::Ui& ui,
                           std::string_view label,
                           const std::vector<luna::editor::RenderFeatureGraphResource>& resources)
{
    ui.textDisabled(label);
    ui.indent();
    if (resources.empty()) {
        ui.textDisabled("none");
        ui.unindent();
        return;
    }

    for (const luna::editor::RenderFeatureGraphResource& resource : resources) {
        std::string text = resource.name + "  [" + graphResourceKindLabel(resource.kind) + "]";
        appendResourceFlags(text, resource.flags);
        ui.bulletText(text);
    }
    ui.unindent();
}

void drawPassResourceList(luna::editor::Ui& ui,
                          const std::vector<luna::editor::RenderPassResourceUsage>& resources)
{
    ui.indent();
    if (resources.empty()) {
        ui.textDisabled("no declared graph resources");
        ui.unindent();
        return;
    }

    for (const luna::editor::RenderPassResourceUsage& resource : resources) {
        std::string text = resource.name + "  [" + graphResourceKindLabel(resource.kind) + " " +
                           passResourceAccessLabel(resource.access) + "]";
        appendResourceFlags(text, resource.flags);
        ui.bulletText(text);
    }
    ui.unindent();
}

void drawStatusEntryList(luna::editor::Ui& ui,
                         const std::vector<luna::editor::RenderFeatureStatusEntry>& entries)
{
    ui.indent();
    if (entries.empty()) {
        ui.textDisabled("none");
        ui.unindent();
        return;
    }

    for (const luna::editor::RenderFeatureStatusEntry& entry : entries) {
        ui.bulletText(entry.name + ": " + (entry.ready ? "OK" : "Missing"));
    }
    ui.unindent();
}

void drawFeatureArchitectureSummary(luna::editor::Ui& ui, const luna::editor::RenderFeatureInfo& feature)
{
    const std::string label = makeFeatureScopedLabel("Architecture", feature.name);
    if (!ui.treeNode(label)) {
        return;
    }

    drawStatusLine(ui, "Support", feature.supported, feature.support_summary);
    drawStatusLine(ui, "Graph Contract", feature.graph_contract_valid, feature.graph_contract_summary);
    drawStatusLine(ui, "Pass Contract", feature.pass_contract_valid, feature.pass_contract_summary);
    drawStatusLine(ui,
                   "Binding Contract",
                   feature.diagnostics.binding_contract_valid,
                   feature.diagnostics.binding_contract_summary);
    drawStatusLine(ui,
                   "Pipeline Resources",
                   feature.diagnostics.pipeline_resources_valid,
                   feature.diagnostics.pipeline_resources_summary);
    if (!feature.diagnostics.persistent_resources_summary.empty() ||
        !feature.diagnostics.persistent_resources.empty()) {
        drawStatusLine(ui,
                       "Persistent Resources",
                       feature.diagnostics.persistent_resources_valid,
                       feature.diagnostics.persistent_resources_summary);
    }
    if (!feature.diagnostics.history_resources_summary.empty() || !feature.diagnostics.history_resources.empty()) {
        drawStatusLine(ui,
                       "History Resources",
                       feature.diagnostics.history_resources_valid,
                       feature.diagnostics.history_resources_summary);
    }
    ui.treePop();
}

void drawFeatureGraphContract(luna::editor::Ui& ui, const luna::editor::RenderFeatureInfo& feature)
{
    if (feature.graph_inputs.empty() && feature.graph_outputs.empty()) {
        return;
    }

    const std::string label = makeFeatureScopedLabel("Graph Contract", feature.name);
    if (!ui.treeNode(label)) {
        return;
    }

    drawStatusLine(ui, "Status", feature.graph_contract_valid, feature.graph_contract_summary);
    drawGraphResourceList(ui, "Inputs", feature.graph_inputs);
    drawGraphResourceList(ui, "Outputs", feature.graph_outputs);
    ui.treePop();
}

void drawFeaturePassContract(luna::editor::Ui& ui, const luna::editor::RenderFeatureInfo& feature)
{
    if (feature.pass_contract_summary.empty() && feature.passes.empty()) {
        return;
    }

    const std::string label = makeFeatureScopedLabel("Pass Contract", feature.name);
    if (!ui.treeNode(label)) {
        return;
    }

    drawStatusLine(ui, "Status", feature.pass_contract_valid, feature.pass_contract_summary);
    ui.textDisabled("Passes");
    ui.indent();
    if (feature.passes.empty()) {
        ui.textDisabled("none");
    } else {
        for (const luna::editor::RenderFeaturePassInfo& pass : feature.passes) {
            const std::string pass_label = makeFeatureScopedLabel(pass.name, feature.name);
            if (ui.treeNode(pass_label)) {
                drawPassResourceList(ui, pass.resources);
                ui.treePop();
            }
        }
    }
    ui.unindent();
    ui.treePop();
}

void drawFeatureResourceDiagnostics(luna::editor::Ui& ui, const luna::editor::RenderFeatureInfo& feature)
{
    const auto& diagnostics = feature.diagnostics;
    const bool has_diagnostics = !diagnostics.binding_contract_summary.empty() ||
                                 !diagnostics.pipeline_resources_summary.empty() ||
                                 !diagnostics.pipeline_resources.empty() ||
                                 !diagnostics.persistent_resources_summary.empty() ||
                                 !diagnostics.persistent_resources.empty() ||
                                 !diagnostics.history_resources_summary.empty() ||
                                 !diagnostics.history_resources.empty();
    if (!has_diagnostics) {
        return;
    }

    const std::string label = makeFeatureScopedLabel("Resource Diagnostics", feature.name);
    if (!ui.treeNode(label)) {
        return;
    }

    drawStatusLine(ui, "Binding Contract", diagnostics.binding_contract_valid, diagnostics.binding_contract_summary);
    drawStatusLine(ui, "Pipeline Resources", diagnostics.pipeline_resources_valid, diagnostics.pipeline_resources_summary);
    drawStatusEntryList(ui, diagnostics.pipeline_resources);
    if (!diagnostics.persistent_resources_summary.empty() || !diagnostics.persistent_resources.empty()) {
        drawStatusLine(
            ui, "Persistent Resources", diagnostics.persistent_resources_valid, diagnostics.persistent_resources_summary);
        drawStatusEntryList(ui, diagnostics.persistent_resources);
    }
    if (!diagnostics.history_resources_summary.empty() || !diagnostics.history_resources.empty()) {
        drawStatusLine(ui, "History Resources", diagnostics.history_resources_valid, diagnostics.history_resources_summary);
        drawStatusEntryList(ui, diagnostics.history_resources);
    }
    ui.treePop();
}

bool drawParameterControl(luna::editor::Ui& ui,
                          const luna::editor::RenderFeatureInfo& feature,
                          const luna::editor::RenderFeatureParameterInfo& parameter,
                          luna::editor::RenderFeatureParameterValue& value)
{
    const std::string label = makeHiddenParameterLabel(feature.name, parameter.name);
    switch (parameter.type) {
        case luna::editor::RenderFeatureParameterType::Bool:
            return ui.checkbox(label, value.bool_value);
        case luna::editor::RenderFeatureParameterType::Int: {
            int int_value = static_cast<int>(value.int_value);
            const bool changed =
                ui.dragInt(label, int_value, parameter.step, parameter.min.int_value, parameter.max.int_value);
            if (changed) {
                value.int_value = static_cast<int32_t>(int_value);
            }
            return changed;
        }
        case luna::editor::RenderFeatureParameterType::Float:
            return ui.dragFloat(
                label, value.float_value, parameter.step, parameter.min.float_value, parameter.max.float_value, "%.3f");
        case luna::editor::RenderFeatureParameterType::Color:
            return ui.colorEdit4(label, value.color_value);
    }
    return false;
}

void drawFeatureParameters(luna::editor::Host& host,
                           luna::editor::Ui& ui,
                           const luna::editor::RenderFeatureInfo& feature)
{
    const std::vector<luna::editor::RenderFeatureParameterInfo> parameters =
        host.rendering().defaultRenderFeatureParameters(feature.name);
    if (parameters.empty()) {
        return;
    }

    ui.textDisabled("Parameters");
    ui.indent();

    const luna::editor::TableFlags table_flags = luna::editor::TableFlag::RowBg |
                                                 luna::editor::TableFlag::BordersInnerH |
                                                 luna::editor::TableFlag::SizingStretchProp;
    const std::string table_id = makeFeatureScopedLabel("##RenderFeatureParameters", feature.name);
    if (ui.beginTable(table_id, 2, table_flags)) {
        ui.tableSetupColumn("Parameter",
                            static_cast<luna::editor::TableColumnFlags>(
                                luna::editor::TableColumnFlag::WidthFixed),
                            150.0f);
        ui.tableSetupColumn("Value",
                            static_cast<luna::editor::TableColumnFlags>(
                                luna::editor::TableColumnFlag::WidthStretch));

        for (const luna::editor::RenderFeatureParameterInfo& parameter : parameters) {
            ui.tableNextRow();
            ui.tableNextColumn();

            const bool disabled = !feature.supported || parameter.read_only;
            if (disabled) {
                ui.textDisabled(parameterDisplayName(parameter));
            } else {
                ui.text(parameterDisplayName(parameter));
            }

            ui.tableNextColumn();
            luna::editor::RenderFeatureParameterValue value = parameter.value;
            if (disabled) {
                ui.beginDisabled();
            }
            ui.setNextItemWidth(-1.0f);
            const bool changed = drawParameterControl(ui, feature, parameter, value);
            if (disabled) {
                ui.endDisabled();
            }

            if (changed && !disabled) {
                (void) host.rendering().setDefaultRenderFeatureParameter(feature.name, parameter.name, value);
            }
        }

        ui.endTable();
    }

    ui.unindent();
}

void drawFeature(luna::editor::Host& host,
                 luna::editor::Ui& ui,
                 const luna::editor::RenderFeatureInfo& feature)
{
    const std::string display_name = feature.display_name.empty() ? feature.name : feature.display_name;
    const std::string feature_label = makeFeatureScopedLabel(display_name, feature.name);

    bool enabled = feature.enabled;
    const bool can_toggle = feature.runtime_toggleable && feature.supported;
    if (!can_toggle) {
        ui.beginDisabled();
    }
    if (ui.checkbox(feature_label, enabled) && can_toggle) {
        (void) host.rendering().setDefaultRenderFeatureEnabled(feature.name, enabled);
    }
    if (!can_toggle) {
        ui.endDisabled();
    }

    if (!feature.category.empty()) {
        ui.sameLine();
        ui.textDisabled("[" + feature.category + "]");
    }
    ui.sameLine();
    ui.textDisabled(featureStatusLabel(feature));
    drawFeatureStatusTooltip(ui, feature);

    ui.indent();
    drawFeatureArchitectureSummary(ui, feature);
    drawFeatureGraphContract(ui, feature);
    drawFeaturePassContract(ui, feature);
    drawFeatureResourceDiagnostics(ui, feature);
    drawFeatureParameters(host, ui, feature);
    ui.unindent();
    ui.separator();
}

void drawRenderFeaturesWindow(luna::editor::WindowDrawContext& context)
{
    luna::editor::Host& host = context.host();
    luna::editor::Ui& ui = context.ui();
    const std::vector<luna::editor::RenderFeatureInfo> features = host.rendering().defaultRenderFeatureInfos();

    if (features.empty()) {
        ui.text("No render features registered.");
        return;
    }

    for (const luna::editor::RenderFeatureInfo& feature : features) {
        drawFeature(host, ui, feature);
    }
}

class RenderFeaturesPlugin final : public luna::editor::Plugin {
public:
    [[nodiscard]] luna::editor::PluginDescriptor descriptor() const override
    {
        return luna::editor::PluginDescriptor{
            .id = kPluginId,
            .display_name = "Render Features",
            .version = "0.1.0",
        };
    }

    bool onLoad(luna::editor::Host& host) override
    {
        return host.windows().registerWindow(luna::editor::WindowDescriptor{
            .id = kWindowId,
            .title = "Render Features",
            .default_open = false,
            .default_size = luna::editor::Vec2{.x = 380.0f, .y = 420.0f},
            .draw = drawRenderFeaturesWindow,
        });
    }

    void onUnload(luna::editor::Host& host) override
    {
        host.windows().unregisterWindow(kWindowId);
    }
};

} // namespace

namespace luna::editor {

std::unique_ptr<Plugin> createRenderFeaturesPlugin()
{
    return std::make_unique<RenderFeaturesPlugin>();
}

namespace {

const EditorBuiltinPluginFactoryRegistration kRenderFeaturesPluginRegistration{
    kPluginId,
    createRenderFeaturesPlugin,
};

} // namespace

} // namespace luna::editor
