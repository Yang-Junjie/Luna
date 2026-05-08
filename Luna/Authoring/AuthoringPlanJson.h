#pragma once

#include "Authoring/AuthoringProtocol.h"

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace luna::authoring {

[[nodiscard]] bool loadAuthoringPlanJson(std::istream& input,
                                         AuthoringPlan& plan,
                                         std::vector<std::string>& errors,
                                         std::vector<AuthoringDiagnostic>* diagnostics = nullptr,
                                         const std::filesystem::path& source_path = {});

[[nodiscard]] bool loadAuthoringPlanJson(const std::filesystem::path& plan_path,
                                         AuthoringPlan& plan,
                                         std::vector<std::string>& errors,
                                         std::vector<AuthoringDiagnostic>* diagnostics = nullptr);

void writeAuthoringPlanJson(std::ostream& out, const AuthoringPlan& plan);

} // namespace luna::authoring
