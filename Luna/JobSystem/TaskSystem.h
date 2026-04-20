#pragma once

#include "JobSystem/TaskHandleState.h"
#include "TaskScheduler.h"

#include <atomic>
#include <condition_variable>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace luna {

struct TaskSystemConfig {
    uint32_t worker_thread_count = 0;
    uint32_t io_thread_count = 1;
    uint32_t external_thread_count = 0;
};

class TaskSystem {
public:
    class ExternalThreadScope {
    public:
        ExternalThreadScope() = default;
        ~ExternalThreadScope();

        ExternalThreadScope(const ExternalThreadScope&) = delete;
        ExternalThreadScope& operator=(const ExternalThreadScope&) = delete;

        ExternalThreadScope(ExternalThreadScope&& other) noexcept;
        ExternalThreadScope& operator=(ExternalThreadScope&& other) noexcept;

        bool isRegistered() const;

    private:
        explicit ExternalThreadScope(TaskSystem* task_system);
        void reset();

    private:
        TaskSystem* m_task_system = nullptr;

        friend class TaskSystem;
    };

    TaskSystem() = default;
    ~TaskSystem();

    TaskSystem(const TaskSystem&) = delete;
    TaskSystem& operator=(const TaskSystem&) = delete;

    TaskSystem(TaskSystem&&) = delete;
    TaskSystem& operator=(TaskSystem&&) = delete;

    bool initialize(const TaskSystemConfig& config = {});
    void shutdown();

    bool isInitialized() const;

    bool submitTask(enki::ITaskSet* task);
    void waitForAll();
    void waitForTask(const enki::ICompletable* task);
    void pollCompletedTasks();
    void runPinnedTasksForCurrentThread();
    bool submitIOJob(enki::IPinnedTask* task);
    bool submitMainThreadJob(enki::IPinnedTask* task);

    TaskHandle submit(std::function<void()> function,
                      TaskSubmitDesc desc = {},
                      std::initializer_list<TaskHandle> dependencies = {});
    TaskHandle
        submit(std::function<void()> function, const std::vector<TaskHandle>& dependencies, TaskSubmitDesc desc = {});
    TaskHandle submitParallel(std::function<void(enki::TaskSetPartition, uint32_t)> function,
                              TaskSubmitDesc desc,
                              std::initializer_list<TaskHandle> dependencies = {});
    TaskHandle submitParallel(std::function<void(enki::TaskSetPartition, uint32_t)> function,
                              const std::vector<TaskHandle>& dependencies,
                              TaskSubmitDesc desc);
    TaskHandle whenAll(std::initializer_list<TaskHandle> dependencies);
    TaskHandle whenAll(const std::vector<TaskHandle>& dependencies);

    uint32_t getWorkerThreadCount() const;
    uint32_t getIOThreadCount() const;
    uint32_t getExternalThreadCount() const;
    uint32_t getFirstIOThreadNumber() const;
    bool isCurrentThreadRegistered() const;
    [[nodiscard]] ExternalThreadScope registerExternalThread();

    enki::TaskScheduler& getScheduler();
    const enki::TaskScheduler& getScheduler() const;

private:
    std::vector<std::shared_ptr<detail::TaskCompletionState>>
        collectDependencyStates(const std::vector<TaskHandle>& dependencies) const;
    TaskHandle submitManagedTask(std::shared_ptr<detail::ManagedTaskBase> task_state,
                                 TaskTarget target,
                                 const std::vector<TaskHandle>& dependencies);
    void trackManagedTask(const std::shared_ptr<detail::ManagedTaskBase>& task_state,
                          const std::shared_ptr<detail::TaskCompletionState>& completion_state);
    void reapCompletedManagedTasks();
    bool canSubmitWorkerTasksFromCurrentThread() const;
    bool requireSchedulerApiThread(const char* operation) const;
    void deregisterExternalThread();
    void ioThreadMain(uint32_t thread_number);

private:
    std::unique_ptr<enki::TaskScheduler> m_scheduler;
    std::vector<std::thread> m_io_threads;
    std::mutex m_io_startup_mutex;
    std::condition_variable m_io_startup_cv;
    TaskSystemConfig m_config;
    std::atomic<uint32_t> m_next_io_thread_index = 0;
    uint32_t m_ready_io_threads = 0;
    bool m_io_startup_failed = false;
    bool m_initialized = false;

    std::mutex m_managed_tasks_mutex;
    std::vector<detail::ManagedTaskRecord> m_managed_tasks;
};

} // namespace luna
