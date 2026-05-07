#pragma once

#include "Core/UUID.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace luna::authoring {

enum class AuthoringVerificationKind : uint8_t {
    SceneSaved,
    EntityExists,
    HasComponent,
    EntityCountAtLeast,
};

struct AuthoringVerification {
    AuthoringVerificationKind kind{AuthoringVerificationKind::EntityExists};
    bool ok{false};
    std::string ref;
    UUID entity_id{0};
    std::string component;
    size_t expected_count{0};
    size_t actual_count{0};
    std::string message;
};

} // namespace luna::authoring
