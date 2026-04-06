#pragma once

#include "Core/window.h"
#include "RHI/Descriptors.h"
#include "vk_descriptors.h"
#include "vk_types.h"
#include <deque>
#include <functional>

struct DeletionQueue {
    std::deque<std::function<void()>> deletors;

    // TODO(Yang) : need to be optimalized, because the std::funcation is too slow
    void push_function(std::function<void()>&& function)
    {
        deletors.push_back(function);
    }

    void flush()
    {
        // reverse iterate the deletion queue to execute all the functions
        for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
            (*it)(); // call functors
        }

        deletors.clear();
    }
};

struct FrameData {
    vk::Semaphore _swapchainSemaphore{};
    vk::Semaphore _renderSemaphore{};
    vk::Fence _renderFence{};

    vk::CommandPool _commandPool{};
    vk::CommandBuffer _mainCommandBuffer{};
    DescriptorAllocatorGrowable _frameDescriptors;
    AllocatedBuffer _uploadStagingBuffer{};
    size_t _uploadStagingCapacity{0};
    size_t _uploadStagingOffset{0};
    size_t _uploadBatchBytes{0};
    uint32_t _uploadBatchOps{0};
    uint64_t _submitSerial{0};

    DeletionQueue _deletionQueue;
};

struct ImmediateSubmitContext {
    vk::Fence _fence{};
    vk::CommandPool _commandPool{};
    vk::CommandBuffer _commandBuffer{};
};

constexpr unsigned int FRAME_OVERLAP = 2;

