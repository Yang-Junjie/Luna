#include "VkSampler.h"

#include <utility>

namespace luna::vkcore {

Sampler::~Sampler()
{
    reset();
}

Sampler::Sampler(Sampler&& other) noexcept
{
    *this = std::move(other);
}

Sampler& Sampler::operator=(Sampler&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    reset();

    m_handle = other.m_handle;
    m_device = other.m_device;

    other.m_handle = VK_NULL_HANDLE;
    other.m_device = VK_NULL_HANDLE;
    return *this;
}

bool Sampler::create(vk::Device device, const vk::SamplerCreateInfo& create_info)
{
    reset();

    vk::Sampler handle{};
    const vk::Result result = device.createSampler(&create_info, nullptr, &handle);
    if (result != vk::Result::eSuccess) {
        return false;
    }

    assign(device, handle);
    return true;
}

void Sampler::destroy(vk::Device device)
{
    (void) device;
    reset();
}

void Sampler::assign(vk::Device device, vk::Sampler handle)
{
    m_device = device;
    m_handle = handle;
}

void Sampler::reset()
{
    if (m_device != VK_NULL_HANDLE && m_handle != VK_NULL_HANDLE) {
        m_device.destroySampler(m_handle, nullptr);
    }

    m_handle = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
}

} // namespace luna::vkcore
