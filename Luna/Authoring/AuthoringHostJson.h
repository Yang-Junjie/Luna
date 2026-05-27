#pragma once

#include "Authoring/AuthoringHost.h"
#include "Authoring/AuthoringJsonTypes.h"

#include <string_view>
#include <vector>

namespace luna::authoring {

[[nodiscard]] std::string_view authoringEventTypeName(AuthoringEventType type) noexcept;

[[nodiscard]] Json authoringSceneSnapshotJson(const AuthoringSceneSnapshot& snapshot);
[[nodiscard]] Json authoringEventJson(const AuthoringEvent& event);
[[nodiscard]] Json authoringEventsJson(const std::vector<AuthoringEvent>& events);
[[nodiscard]] Json authoringHostSessionStateJson(const AuthoringHostSessionState& state);
[[nodiscard]] Json authoringHostWireManifestJson();

} // namespace luna::authoring
