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

std::string makeParameterScopedLabel(std::string_view label,
                                     std::string_view feature_name,
                                     std::string_view parameter_name)
{
    std::string scoped_label = makeFeatureScopedLabel(label, feature_name);
    scoped_label += ".";
    scoped_label.append(parameter_name.data(), parameter_name.size());
    return scoped_label;
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

    ImGui::SetNextWindowSize(ImVec2(380.0f, 420.0f), ImGuiCond_FirstUseEver);
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
        if (!feature.runtime_toggleable) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Checkbox(feature_label.c_str(), &enabled) && feature.runtime_toggleable &&
            set_feature_enabled) {
            set_feature_enabled(feature.name, enabled);
        }

        if (!feature.runtime_toggleable) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        ImGui::TextDisabled("[%.*s]", static_cast<int>(feature.category.size()), feature.category.data());

        const auto parameters = get_parameters ? get_parameters(feature.name) : ParameterList{};
        if (!parameters.empty()) {
            ImGui::Indent();
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
            ImGui::Unindent();
        }
    }

    ImGui::End();
}

} // namespace luna
