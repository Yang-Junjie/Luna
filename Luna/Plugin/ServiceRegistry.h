#pragma once

#include <memory>
#include <stdexcept>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace luna {

class ServiceRegistry {
public:
    template<typename Service, typename... Args>
    Service& emplace(Args&&... args)
    {
        auto instance = std::make_shared<Service>(std::forward<Args>(args)...);
        return add<Service>(std::move(instance));
    }

    template<typename Interface, typename Service, typename... Args>
    Interface& emplaceAs(Args&&... args)
    {
        static_assert(std::is_base_of_v<Interface, Service>, "Service must derive from Interface");
        auto instance = std::make_shared<Service>(std::forward<Args>(args)...);
        return add<Interface>(std::static_pointer_cast<Interface>(instance));
    }

    template<typename Service>
    Service& add(std::shared_ptr<Service> service)
    {
        if (service == nullptr) {
            throw std::runtime_error("Cannot register a null service instance");
        }

        m_services[std::type_index(typeid(Service))] = std::move(service);
        return get<Service>();
    }

    template<typename Service>
    bool has() const
    {
        return m_services.contains(std::type_index(typeid(Service)));
    }

    template<typename Service>
    Service& get() const
    {
        const auto it = m_services.find(std::type_index(typeid(Service)));
        if (it == m_services.end() || it->second == nullptr) {
            throw std::runtime_error("Requested service is not registered");
        }

        return *std::static_pointer_cast<Service>(it->second);
    }

private:
    std::unordered_map<std::type_index, std::shared_ptr<void>> m_services;
};

} // namespace luna
