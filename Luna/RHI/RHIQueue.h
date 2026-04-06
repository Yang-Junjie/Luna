#pragma once

#include "RHIDeviceTypes.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace luna {

enum class RHIQueueType : uint32_t {
    Graphics = 0,
    Compute,
    Transfer
};

constexpr std::string_view to_string(RHIQueueType queueType) noexcept
{
    switch (queueType) {
        case RHIQueueType::Graphics:
            return "Graphics";
        case RHIQueueType::Compute:
            return "Compute";
        case RHIQueueType::Transfer:
            return "Transfer";
        default:
            return "Unknown";
    }
}

class IRHIFence {
public:
    virtual ~IRHIFence() = default;

    virtual RHIResult wait(uint64_t timeoutNanoseconds = 1'000'000'000ull) = 0;
    virtual RHIResult reset() = 0;
    virtual bool isSignaled() const = 0;
};

class IRHICommandList {
public:
    virtual ~IRHICommandList() = default;

    virtual RHIQueueType getQueueType() const = 0;
    virtual RHIResult begin() = 0;
    virtual RHIResult end() = 0;
    virtual IRHICommandContext* getContext() = 0;
    virtual const IRHICommandContext* getContext() const = 0;
};

class IRHICommandQueue {
public:
    virtual ~IRHICommandQueue() = default;

    virtual RHIQueueType getQueueType() const = 0;
    virtual RHIResult submit(IRHICommandList& commandList,
                             IRHIFence* signalFence = nullptr,
                             std::string_view label = {}) = 0;
    virtual RHIResult waitIdle() = 0;
};

} // namespace luna
