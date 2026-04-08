#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

namespace luna::vkcore {

class ReleaseQueue {
public:
    void push(std::function<void()>&& function);
    void flush();
    void clear();

    bool empty() const
    {
        return m_callbacks.empty();
    }

private:
    std::deque<std::function<void()>> m_callbacks;
};

class DeferredRelease {
public:
    void initialize(uint32_t frames_in_flight);
    void reset();

    bool isInitialized() const
    {
        return !m_queues.empty();
    }

    void defer(uint64_t frame_number, std::function<void()>&& function);
    void flush(uint64_t frame_number);
    void flushAll();

private:
    std::vector<ReleaseQueue> m_queues;
};

} // namespace luna::vkcore
