#pragma once

#include "JobSystem/TaskSystem.h"

#include <functional>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace luna {

struct ResourceLoadQueueDesc {
    TaskSubmitDesc load_task{.target = TaskTarget::IO};
    TaskSubmitDesc commit_task{.target = TaskTarget::MainThread};
};

namespace detail {

template <typename Resource> struct ResourceLoadState {
    std::shared_ptr<Resource> resource;
    mutable std::mutex mutex;

    bool hasValue() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return static_cast<bool>(resource);
    }

    std::optional<Resource> take()
    {
        std::shared_ptr<Resource> resource_copy;
        std::lock_guard<std::mutex> lock(mutex);
        if (!resource) {
            return std::nullopt;
        }

        resource_copy = std::move(resource);
        resource.reset();
        return std::move(*resource_copy);
    }

    template <typename Callback> void withValue(const Callback& callback) const
    {
        std::shared_ptr<Resource> resource_copy;
        {
            std::lock_guard<std::mutex> lock(mutex);
            resource_copy = resource;
        }

        if (resource_copy) {
            callback(*resource_copy);
        }
    }
};

} // namespace detail

template <typename Resource> class ResourceLoadHandle {
public:
    ResourceLoadHandle() = default;

    bool isValid() const
    {
        return m_task.isValid() && static_cast<bool>(m_state);
    }

    bool isReady() const
    {
        return m_task.isComplete();
    }

    void wait(TaskSystem& task_system) const
    {
        m_task.wait(task_system);
    }

    bool hasValue() const
    {
        return m_state && m_state->hasValue();
    }

    template <typename Callback> bool withValue(const Callback& callback) const
    {
        if (!m_state) {
            return false;
        }

        bool invoked = false;
        m_state->withValue([&](const Resource& resource) {
            callback(resource);
            invoked = true;
        });
        return invoked;
    }

    std::optional<Resource> take()
    {
        if (!m_state) {
            return std::nullopt;
        }

        return m_state->take();
    }

    const TaskHandle& task() const
    {
        return m_task;
    }

private:
    ResourceLoadHandle(TaskHandle task_handle, std::shared_ptr<detail::ResourceLoadState<Resource>> state)
        : m_task(std::move(task_handle)),
          m_state(std::move(state))
    {}

private:
    TaskHandle m_task;
    std::shared_ptr<detail::ResourceLoadState<Resource>> m_state;

    friend class ResourceLoadQueue;
};

class ResourceLoadQueue {
public:
    explicit ResourceLoadQueue(TaskSystem& task_system)
        : m_task_system(task_system)
    {}

    template <typename LoadFunction>
    auto submitLoad(LoadFunction&& load_function,
                    std::initializer_list<TaskHandle> dependencies = {},
                    ResourceLoadQueueDesc desc = {})
        -> ResourceLoadHandle<std::invoke_result_t<std::decay_t<LoadFunction>>>
    {
        return submitLoad(std::forward<LoadFunction>(load_function), std::vector<TaskHandle>(dependencies), desc);
    }

    template <typename LoadFunction>
    auto submitLoad(LoadFunction&& load_function,
                    const std::vector<TaskHandle>& dependencies,
                    ResourceLoadQueueDesc desc = {})
        -> ResourceLoadHandle<std::invoke_result_t<std::decay_t<LoadFunction>>>
    {
        using Resource = std::invoke_result_t<std::decay_t<LoadFunction>>;

        auto state = std::make_shared<detail::ResourceLoadState<Resource>>();
        auto task = m_task_system.submit(
            [state, load = std::forward<LoadFunction>(load_function)]() mutable {
                auto resource = std::make_shared<Resource>(load());

                std::lock_guard<std::mutex> lock(state->mutex);
                state->resource = std::move(resource);
            },
            dependencies,
            desc.load_task);

        return ResourceLoadHandle<Resource>(std::move(task), std::move(state));
    }

    template <typename LoadFunction, typename CommitFunction>
    TaskHandle submitLoadWithCommit(LoadFunction&& load_function,
                                    CommitFunction&& commit_function,
                                    std::initializer_list<TaskHandle> dependencies = {},
                                    ResourceLoadQueueDesc desc = {})
    {
        return submitLoadWithCommit(std::forward<LoadFunction>(load_function),
                                    std::forward<CommitFunction>(commit_function),
                                    std::vector<TaskHandle>(dependencies),
                                    desc);
    }

    template <typename LoadFunction, typename CommitFunction>
    TaskHandle submitLoadWithCommit(LoadFunction&& load_function,
                                    CommitFunction&& commit_function,
                                    const std::vector<TaskHandle>& dependencies,
                                    ResourceLoadQueueDesc desc = {})
    {
        auto load_handle = submitLoad(std::forward<LoadFunction>(load_function), dependencies, desc);
        return load_handle.task().then(
            m_task_system,
            [state = load_handle.m_state, commit = std::forward<CommitFunction>(commit_function)]() mutable {
                auto resource = state->take();
                if (resource.has_value()) {
                    commit(std::move(*resource));
                }
            },
            desc.commit_task);
    }

private:
    TaskSystem& m_task_system;
};

} // namespace luna
