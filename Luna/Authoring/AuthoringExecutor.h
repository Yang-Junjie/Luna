#pragma once

#include "Authoring/AuthoringProtocol.h"
#include "Core/UUID.h"
#include "Scene/Entity.h"

#include <string>
#include <string_view>
#include <unordered_map>

namespace luna::authoring {

class AuthoringSession;

class AuthoringExecutor {
public:
    explicit AuthoringExecutor(AuthoringSession& session);

    [[nodiscard]] bool execute(const AuthoringPlan& plan, AuthoringReport& report);
    void clearAliases();

private:
    [[nodiscard]] Entity resolveEntity(std::string_view reference) const;
    [[nodiscard]] bool requireEntity(std::string_view reference, Entity& entity, AuthoringReport& report) const;
    [[nodiscard]] bool rememberAlias(const std::string& alias, Entity entity, AuthoringReport& report);
    void refreshEntityBindingName(Entity entity, AuthoringReport& report) const;

private:
    AuthoringSession& m_session;
    std::unordered_map<std::string, UUID> m_aliases;
};

} // namespace luna::authoring
