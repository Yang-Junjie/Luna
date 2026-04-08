#include "VkQueue.h"

#include "VkDevice.h"

#include <Core/Log.h>

namespace {

template <typename T> void logVkbError(const char* step, const vkb::Result<T>& result)
{
    LUNA_CORE_ERROR("{} failed: {}", step, result.error().message());
    for (const std::string& reason : result.full_error().detailed_failure_reasons) {
        LUNA_CORE_ERROR("  {}", reason);
    }
}

} // namespace

namespace luna::vkcore {

bool Queue::initialize(const Device& device, vkb::QueueType queue_type)
{
    reset();

    if (!device.isValid()) {
        LUNA_CORE_ERROR("Cannot fetch a Vulkan queue before creating a logical device");
        return false;
    }

    auto queue_ret = device.getBootstrapDevice().get_queue(queue_type);
    if (!queue_ret) {
        logVkbError("Queue fetch", queue_ret);
        return false;
    }

    auto queue_index_ret = device.getBootstrapDevice().get_queue_index(queue_type);
    if (!queue_index_ret) {
        logVkbError("Queue family fetch", queue_index_ret);
        return false;
    }

    m_queue = queue_ret.value();
    m_family_index = queue_index_ret.value();
    m_type = queue_type;
    return true;
}

void Queue::reset()
{
    m_queue = VK_NULL_HANDLE;
    m_family_index = 0;
    m_type = vkb::QueueType::graphics;
}

} // namespace luna::vkcore
