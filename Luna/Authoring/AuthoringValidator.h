#pragma once

#include "Authoring/AuthoringProtocol.h"

namespace luna::authoring {

struct AuthoringValidationOptions {
    bool check_file_system{true};
    bool warn_on_file_overwrite{true};
};

[[nodiscard]] bool validateAuthoringPlan(const AuthoringPlan& plan,
                                         const AuthoringSession& session,
                                         AuthoringReport& report,
                                         const AuthoringValidationOptions& options = {});

} // namespace luna::authoring
