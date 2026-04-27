#include "Renderer/RenderFlow/RenderFeatureRegistry.h"

#include <algorithm>
#include <utility>

namespace luna::render_flow {
namespace {

bool descriptorLess(const RenderFeatureDescriptor& lhs, const RenderFeatureDescriptor& rhs)
{
    if (lhs.order != rhs.order) {
        return lhs.order < rhs.order;
    }

    return lhs.name < rhs.name;
}

bool matchesFlow(const RenderFeatureRegistrationDesc& registration,
                 std::string_view target_flow,
                 bool auto_install_only)
{
    return registration.descriptor.target_flow == target_flow &&
           (!auto_install_only || registration.descriptor.auto_install);
}

} // namespace

RenderFeatureRegistry& RenderFeatureRegistry::instance()
{
    static RenderFeatureRegistry registry;
    return registry;
}

bool RenderFeatureRegistry::registerFeature(RenderFeatureRegistrationDesc registration)
{
    if (registration.descriptor.name.empty() || !registration.factory) {
        return false;
    }

    const auto duplicate_it =
        std::find_if(m_registrations.begin(), m_registrations.end(), [&registration](const auto& existing) {
            return existing.descriptor.name == registration.descriptor.name;
        });
    if (duplicate_it != m_registrations.end()) {
        return false;
    }

    m_registrations.push_back(std::move(registration));
    return true;
}

std::vector<RenderFeatureDescriptor> RenderFeatureRegistry::descriptors() const
{
    std::vector<RenderFeatureDescriptor> result;
    result.reserve(m_registrations.size());
    for (const auto& registration : m_registrations) {
        result.push_back(registration.descriptor);
    }

    std::sort(result.begin(), result.end(), descriptorLess);
    return result;
}

std::vector<RenderFeatureDescriptor> RenderFeatureRegistry::descriptorsForFlow(std::string_view target_flow,
                                                                               bool auto_install_only) const
{
    std::vector<RenderFeatureDescriptor> result;
    for (const auto& registration : m_registrations) {
        if (matchesFlow(registration, target_flow, auto_install_only)) {
            result.push_back(registration.descriptor);
        }
    }

    std::sort(result.begin(), result.end(), descriptorLess);
    return result;
}

std::unique_ptr<IRenderFeature> RenderFeatureRegistry::createFeature(std::string_view name) const
{
    const auto feature_it = std::find_if(m_registrations.begin(), m_registrations.end(), [name](const auto& entry) {
        return entry.descriptor.name == name;
    });
    if (feature_it == m_registrations.end() || !feature_it->factory) {
        return {};
    }

    return feature_it->factory();
}

std::vector<std::unique_ptr<IRenderFeature>> RenderFeatureRegistry::createFeaturesForFlow(std::string_view target_flow,
                                                                                          bool auto_install_only) const
{
    std::vector<const RenderFeatureRegistrationDesc*> registrations;
    for (const auto& registration : m_registrations) {
        if (matchesFlow(registration, target_flow, auto_install_only)) {
            registrations.push_back(&registration);
        }
    }

    std::sort(registrations.begin(), registrations.end(), [](const auto* lhs, const auto* rhs) {
        return descriptorLess(lhs->descriptor, rhs->descriptor);
    });

    std::vector<std::unique_ptr<IRenderFeature>> result;
    result.reserve(registrations.size());
    for (const auto* registration : registrations) {
        if (registration != nullptr && registration->factory) {
            if (auto feature = registration->factory()) {
                result.push_back(std::move(feature));
            }
        }
    }
    return result;
}

RenderFeatureRegistration::RenderFeatureRegistration(RenderFeatureRegistrationDesc registration)
    : m_registered(RenderFeatureRegistry::instance().registerFeature(std::move(registration)))
{}

} // namespace luna::render_flow
