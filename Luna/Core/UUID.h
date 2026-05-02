#pragma once
#include <cstdint>

#include <functional>
#include <string>

namespace luna {

class UUID {
public:
    UUID();
    UUID(uint64_t uuid);
    UUID(const UUID&) = default;

    operator uint64_t() const
    {
        return m_UUID;
    }

    bool operator==(const UUID& other) const
    {
        return m_UUID == other.m_UUID;
    }

    std::string toString() const
    {
        return std::to_string(m_UUID);
    }

    bool isValid() const
    {
        return m_UUID != 0;
    }

private:
    uint64_t m_UUID;
};

} // namespace luna

namespace std {
template <typename T> struct hash;

template <> struct hash<luna::UUID> {
    std::size_t operator()(const luna::UUID& uuid) const
    {
        return static_cast<uint64_t>(uuid);
    }
};

} // namespace std
