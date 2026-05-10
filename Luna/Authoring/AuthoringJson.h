#pragma once

#include "Authoring/AuthoringJsonTypes.h"
#include "Authoring/AuthoringProtocol.h"

#include <iosfwd>
#include <string>
#include <string_view>

namespace luna::authoring {

[[nodiscard]] std::string escapeAuthoringJsonString(std::string_view value);

[[nodiscard]] Json authoringReportJson(const AuthoringReport& report, bool ok);

void writeAuthoringReportJson(std::ostream& out, const AuthoringReport& report, bool ok);

} // namespace luna::authoring
