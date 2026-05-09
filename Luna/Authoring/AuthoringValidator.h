#pragma once

#include "Authoring/AuthoringProtocol.h"
#include "Core/UUID.h"

#include <string>
#include <unordered_map>

namespace luna::authoring {

using AuthoringAliasMap = std::unordered_map<std::string, UUID>;

struct AuthoringValidationOptions {
    bool check_file_system{true};
    bool warn_on_file_overwrite{true};
    const AuthoringAliasMap* known_aliases{nullptr};
};

[[nodiscard]] bool validateAuthoringPlan(const AuthoringPlan& plan,
                                         const AuthoringSession& session,
                                         AuthoringReport& report,
                                         const AuthoringValidationOptions& options = {});

} // namespace luna::authoring
