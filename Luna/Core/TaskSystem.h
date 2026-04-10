#pragma once

#include "TaskScheduler.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace luna {

struct TaskSystemConfig {
    uint32_t worker_thread_count = 0;
    uint32_t io_thread_count = 1;
};

class TaskSystem {
public:
    TaskSystem() = default;
    ~TaskSystem();

    TaskSystem(const TaskSystem&) = delete;
    TaskSystem& operator=(const TaskSystem&) = delete;

    TaskSystem(TaskSystem&&) = delete;
    TaskSystem& operator=(TaskSystem&&) = delete;

    bool initialize(const TaskSystemConfig& config = {});
    void shutdown();

    bool isInitialized() const;

    void submitTask(enki::ITaskSet* task);
    void waitForAll();
    void waitForTask(const enki::ICompletable* task);
    void runPinnedTasksForCurrentThread();
    void submitIOJob(enki::IPinnedTask* task);
    void submitMainThreadJob(enki::IPinnedTask* task);

    uint32_t getWorkerThreadCount() const;
    uint32_t getIOThreadCount() const;
    uint32_t getFirstIOThreadNumber() const;

    enki::TaskScheduler& getScheduler();
    const enki::TaskScheduler& getScheduler() const;

private:
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
};

} // namespace luna
