#include "VkDeferredRelease.h"

namespace luna::vkcore {

void ReleaseQueue::push(std::function<void()>&& function)
{
    if (!function) {
        return;
    }

    m_callbacks.push_back(std::move(function));
}

void ReleaseQueue::flush()
{
    for (auto it = m_callbacks.rbegin(); it != m_callbacks.rend(); ++it) {
        if (*it) {
            (*it)();
        }
    }

    m_callbacks.clear();
}

void ReleaseQueue::clear()
{
    m_callbacks.clear();
}

void DeferredRelease::initialize(uint32_t frames_in_flight)
{
    reset();

    if (frames_in_flight == 0) {
        return;
    }

    m_queues.resize(frames_in_flight);
}

void DeferredRelease::reset()
{
    flushAll();
    m_queues.clear();
}

void DeferredRelease::defer(uint64_t frame_number, std::function<void()>&& function)
{
    if (!function) {
        return;
    }

    if (m_queues.empty()) {
        function();
        return;
    }

    m_queues[frame_number % m_queues.size()].push(std::move(function));
}

void DeferredRelease::flush(uint64_t frame_number)
{
    if (m_queues.empty()) {
        return;
    }

    m_queues[frame_number % m_queues.size()].flush();
}

void DeferredRelease::flushAll()
{
    for (ReleaseQueue& queue : m_queues) {
        queue.flush();
    }
}

} // namespace luna::vkcore
