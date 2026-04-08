#pragma once

#include <vulkan/vulkan.hpp>

namespace luna::vkcore {

class Sampler {
public:
    Sampler() = default;
    ~Sampler();

    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;

    Sampler(Sampler&& other) noexcept;
    Sampler& operator=(Sampler&& other) noexcept;

    bool create(vk::Device device, const vk::SamplerCreateInfo& create_info);
    void destroy(vk::Device device);

    bool isValid() const
    {
        return m_handle != VK_NULL_HANDLE;
    }

    vk::Sampler get() const
    {
        return m_handle;
    }

    void reset();

    operator vk::Sampler() const
    {
        return m_handle;
    }

    bool operator==(const Sampler& other) const
    {
        return m_handle == other.m_handle;
    }

private:
    void assign(vk::Device device, vk::Sampler handle);

    vk::Sampler m_handle{};
    vk::Device m_device{};
};

} // namespace luna::vkcore
