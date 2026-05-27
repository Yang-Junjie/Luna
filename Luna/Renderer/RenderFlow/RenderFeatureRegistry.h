#pragma once

#include "Renderer/RenderFlow/RenderFeature.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace luna::render_flow {

inline constexpr std::string_view kDefaultRenderFlowName = "DefaultRenderFlow";

struct RenderFeatureDescriptor {
    std::string name;
    std::string display_name;
    std::string category;
    std::string target_flow{"DefaultRenderFlow"};
    bool auto_install{true};
    bool enabled_by_default{true};
    int order{0};
};

using RenderFeatureFactory = std::function<std::unique_ptr<IRenderFeature>()>;

struct RenderFeatureRegistrationDesc {
    RenderFeatureDescriptor descriptor;
    RenderFeatureFactory factory;
};

class RenderFeatureRegistry final {
public:
    static RenderFeatureRegistry& instance();

    bool registerFeature(RenderFeatureRegistrationDesc registration);
    [[nodiscard]] std::vector<RenderFeatureDescriptor> descriptors() const;
    [[nodiscard]] std::vector<RenderFeatureDescriptor> descriptorsForFlow(std::string_view target_flow,
                                                                          bool auto_install_only = true) const;
    [[nodiscard]] std::unique_ptr<IRenderFeature> createFeature(std::string_view name) const;
    [[nodiscard]] std::vector<std::unique_ptr<IRenderFeature>>
        createFeaturesForFlow(std::string_view target_flow, bool auto_install_only = true) const;

private:
    std::vector<RenderFeatureRegistrationDesc> m_registrations;
};

class RenderFeatureRegistration final {
public:
    explicit RenderFeatureRegistration(RenderFeatureRegistrationDesc registration);

    [[nodiscard]] bool registered() const noexcept
    {
        return m_registered;
    }

private:
    bool m_registered{false};
};

} // namespace luna::render_flow

#define LUNA_RENDER_FEATURE_JOIN_IMPL(lhs, rhs) lhs##rhs
#define LUNA_RENDER_FEATURE_JOIN(lhs, rhs)      LUNA_RENDER_FEATURE_JOIN_IMPL(lhs, rhs)

#define LUNA_REGISTER_RENDER_FEATURE_EX(                                                               \
    FeatureType, FeatureName, DisplayName, Category, TargetFlow, AutoInstall, EnabledByDefault, Order) \
    namespace {                                                                                        \
    [[maybe_unused]] const ::luna::render_flow::RenderFeatureRegistration                              \
        LUNA_RENDER_FEATURE_JOIN(g_luna_render_feature_registration_,                                  \
                                 __LINE__){::luna::render_flow::RenderFeatureRegistrationDesc{         \
            .descriptor =                                                                              \
                ::luna::render_flow::RenderFeatureDescriptor{                                          \
                    .name = ::std::string(FeatureName),                                                \
                    .display_name = ::std::string(DisplayName),                                        \
                    .category = ::std::string(Category),                                               \
                    .target_flow = ::std::string(TargetFlow),                                          \
                    .auto_install = AutoInstall,                                                       \
                    .enabled_by_default = EnabledByDefault,                                            \
                    .order = Order,                                                                    \
                },                                                                                     \
            .factory = []() -> std::unique_ptr<::luna::render_flow::IRenderFeature> {                  \
                return std::make_unique<FeatureType>();                                                \
            },                                                                                         \
        }};                                                                                            \
    }

#define LUNA_REGISTER_RENDER_FEATURE(FeatureType, FeatureName, DisplayName, Category) \
    LUNA_REGISTER_RENDER_FEATURE_EX(                                                  \
        FeatureType, FeatureName, DisplayName, Category, ::luna::render_flow::kDefaultRenderFlowName, true, true, 0)
