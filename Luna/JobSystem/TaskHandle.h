#pragma once

#include "TaskScheduler.h"

#include <cstdint>
#include <functional>
#include <memory>

namespace luna {

class TaskSystem;

namespace detail {
struct ManagedTaskBase;
struct TaskCompletionState;
}

enum class TaskTarget : uint8_t {
    Worker,
    MainThread,
    IO,
};

enum class TaskStatus : uint8_t {
    Invalid,
    Pending,
    Completed,
    Failed,
};

struct TaskSubmitDesc {
    TaskTarget target = TaskTarget::Worker;
    enki::TaskPriority priority = enki::TASK_PRIORITY_HIGH;
    uint32_t set_size = 1;
    uint32_t min_range = 1;
};

class TaskHandle {
public:
    TaskHandle() = default;

    bool isValid() const;
    bool isComplete() const;
    bool hasFailed() const;
    TaskStatus status() const;
    void wait(TaskSystem& task_system) const;

    TaskHandle then(TaskSystem& task_system,
                    std::function<void()> function,
                    TaskSubmitDesc desc = {}) const;
    TaskHandle thenParallel(TaskSystem& task_system,
                            std::function<void(enki::TaskSetPartition, uint32_t)> function,
                            TaskSubmitDesc desc) const;

private:
    explicit TaskHandle(std::shared_ptr<detail::TaskCompletionState> state);

private:
    std::shared_ptr<detail::TaskCompletionState> m_state;

    friend class TaskSystem;
};

} // namespace luna
