#pragma once

#include "Core/Application.h"
#include "Plugin/ServiceRegistry.h"

namespace luna::runtime {

class RuntimeApp final : public Application {
public:
    RuntimeApp();

protected:
    void onInit() override;

private:
    luna::ServiceRegistry m_service_registry;
};

} // namespace luna::runtime
