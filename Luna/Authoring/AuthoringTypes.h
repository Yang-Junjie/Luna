#pragma once

#include "Core/UUID.h"

#include <cstdint>

#include <filesystem>
#include <string>

namespace luna::authoring {

enum class AuthoringEventType : uint8_t {
    SceneReset,
    SceneCreated,
    SceneLoaded,
    SceneSaved,
    SceneDirtyChanged,
    SceneSettingsChanged,
    EntityCreated,
    EntityModified,
    EntityDestroyed,
    EntityReparented,
};

struct AuthoringEvent {
    AuthoringEventType type{AuthoringEventType::EntityModified};
    UUID entity_id{0};
    std::filesystem::path path;
    std::string message;
};

} // namespace luna::authoring
