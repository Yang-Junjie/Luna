#include "Core/Log.h"
#include "TaskSystem.h"

#include <algorithm>
#include <limits>
#include <system_error>

namespace luna {

namespace {

uint32_t computeWorkerThreadCount(const TaskSystemConfig& config)
{
    if (config.worker_thread_count > 0) {
        return config.worker_thread_count;
    }

    const uint32_t hardware_threads = std::max(enki::GetNumHardwareThreads(), 1u);
    const uint32_t reserved_threads = 1 + config.io_thread_count;

    if (hardware_threads > reserved_threads) {
        return hardware_threads - reserved_threads;
    }

    return 1;
}

} // namespace

TaskSystem::~TaskSystem()
{
    shutdown();
}

bool TaskSystem::initialize(const TaskSystemConfig& config)
{
    if (m_initialized) {
        return true;
    }

    m_config = config;
    m_config.worker_thread_count = computeWorkerThreadCount(config);
    m_next_io_thread_index.store(0, std::memory_order_relaxed);
    m_ready_io_threads = 0;
    m_io_startup_failed = false;

    m_scheduler = std::make_unique<enki::TaskScheduler>();

    enki::TaskSchedulerConfig scheduler_config;
    scheduler_config.numTaskThreadsToCreate = m_config.worker_thread_count;
    scheduler_config.numExternalTaskThreads = m_config.io_thread_count;

    m_scheduler->Initialize(scheduler_config);

    try {
        for (uint32_t io_index = 0; io_index < m_config.io_thread_count; ++io_index) {
            const uint32_t thread_number = getFirstIOThreadNumber() + io_index;
            m_io_threads.emplace_back([this, thread_number]() {
                ioThreadMain(thread_number);
            });
        }
    } catch (const std::system_error& error) {
        if (Logger::isInitialized()) {
            LUNA_CORE_ERROR("Task system failed to create IO thread: {}", error.what());
        }
        shutdown();
        return false;
    }

    if (m_config.io_thread_count > 0) {
        std::unique_lock<std::mutex> lock(m_io_startup_mutex);
        m_io_startup_cv.wait(lock, [this]() {
            return m_io_startup_failed || m_ready_io_threads == m_config.io_thread_count;
        });

        if (m_io_startup_failed) {
            if (Logger::isInitialized()) {
                LUNA_CORE_ERROR("Task system failed to register one or more IO threads");
            }
            shutdown();
            return false;
        }
    }

    m_initialized = true;

    if (Logger::isInitialized()) {
        LUNA_CORE_INFO("Task system initialized with {} worker threads and {} IO threads ({} total registered threads)",
                       m_config.worker_thread_count,
                       m_config.io_thread_count,
                       m_scheduler->GetNumTaskThreads());
    }

    return true;
}

void TaskSystem::shutdown()
{
    if (m_scheduler == nullptr && m_io_threads.empty()) {
        return;
    }

    if (m_scheduler != nullptr) {
        m_scheduler->WaitforAllAndShutdown();
    }

    for (std::thread& io_thread : m_io_threads) {
        if (io_thread.joinable()) {
            io_thread.join();
        }
    }
    m_io_threads.clear();

    m_scheduler.reset();
    m_next_io_thread_index.store(0, std::memory_order_relaxed);
    m_ready_io_threads = 0;
    m_io_startup_failed = false;
    m_config.worker_thread_count = 0;
    m_config.io_thread_count = 0;
    m_initialized = false;

    if (Logger::isInitialized()) {
        LUNA_CORE_INFO("Task system shutdown");
    }
}

bool TaskSystem::isInitialized() const
{
    return m_initialized;
}

void TaskSystem::submitTask(enki::ITaskSet* task)
{
    if (!m_scheduler || !task) {
        return;
    }

    m_scheduler->AddTaskSetToPipe(task);
}

void TaskSystem::waitForAll()
{
    if (m_scheduler) {
        m_scheduler->WaitforAll();
    }
}

void TaskSystem::waitForTask(const enki::ICompletable* task)
{
    if (m_scheduler && task) {
        m_scheduler->WaitforTask(task);
    }
}

void TaskSystem::runPinnedTasksForCurrentThread()
{
    if (m_scheduler) {
        m_scheduler->RunPinnedTasks();
    }
}

void TaskSystem::submitIOJob(enki::IPinnedTask* task)
{
    if (!m_scheduler || !task) {
        return;
    }

    if (m_config.io_thread_count == 0) {
        if (Logger::isInitialized()) {
            LUNA_CORE_WARN("Dropping IO pinned task because the task system has no IO threads");
        }
        return;
    }

    const uint32_t io_index = m_next_io_thread_index.fetch_add(1, std::memory_order_relaxed) % m_config.io_thread_count;
    task->threadNum = getFirstIOThreadNumber() + io_index;
    m_scheduler->AddPinnedTask(task);
}

void TaskSystem::submitMainThreadJob(enki::IPinnedTask* task)
{
    if (!m_scheduler || !task) {
        return;
    }

    task->threadNum = 0;
    m_scheduler->AddPinnedTask(task);
}

uint32_t TaskSystem::getWorkerThreadCount() const
{
    return m_config.worker_thread_count;
}

uint32_t TaskSystem::getIOThreadCount() const
{
    return m_config.io_thread_count;
}

uint32_t TaskSystem::getFirstIOThreadNumber() const
{
    return enki::TaskScheduler::GetNumFirstExternalTaskThread();
}

enki::TaskScheduler& TaskSystem::getScheduler()
{
    return *m_scheduler;
}

const enki::TaskScheduler& TaskSystem::getScheduler() const
{
    return *m_scheduler;
}

void TaskSystem::ioThreadMain(uint32_t thread_number)
{
    const bool registered = m_scheduler != nullptr && m_scheduler->RegisterExternalTaskThread(thread_number);

    {
        std::lock_guard<std::mutex> lock(m_io_startup_mutex);
        if (registered) {
            ++m_ready_io_threads;
        } else {
            m_io_startup_failed = true;
        }
    }
    m_io_startup_cv.notify_one();

    if (!registered || m_scheduler == nullptr) {
        return;
    }

    while (!m_scheduler->GetIsShutdownRequested()) {
        m_scheduler->WaitForNewPinnedTasks();

        if (m_scheduler->GetIsShutdownRequested()) {
            break;
        }

        m_scheduler->RunPinnedTasks();
    }

    m_scheduler->DeRegisterExternalTaskThread();
}

} // namespace luna
