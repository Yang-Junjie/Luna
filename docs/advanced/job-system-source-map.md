# JobSystem 源码地图

> **提示 (Note):**
> 这篇文章是“读源码前的地图”，重点是回答 `Luna/JobSystem` 里每个文件负责什么、几个核心对象如何配合，以及它在整个应用生命周期中的位置。若你更关心调用方式，请先看 [manual/job-system-manual.md](../manual/job-system-manual.md)。

## 先看结论

`Luna/JobSystem` 不是一个从零实现的调度器，而是对 [enkiTS `TaskScheduler.h`](../../third_party/enkiTS/src/TaskScheduler.h) 的一层工程化封装。它做了四件事:

1. 把 enkiTS 的原生 `ITaskSet` / `IPinnedTask` 包装成更容易提交的 lambda 任务。
2. 给任务补上一层 `TaskHandle`，让上层代码能查询状态、等待完成、串联 continuation。
3. 在 enkiTS 原生依赖机制之上，额外实现了“动态依赖图推进”，也就是 `then()` / `whenAll()` 这类更像 DAG 的接口。
4. 提供一个偏资源管线的薄封装 `ResourceLoadQueue`，把“后台加载 -> 主线程提交”写成固定套路。

从源码角度看，它的真正核心不是某个单一类，而是下面这组对象的配合:

| 组件 | 位置 | 作用 |
| --- | --- | --- |
| `TaskSystem` | [Luna/JobSystem/TaskSystem.h](../../Luna/JobSystem/TaskSystem.h) / [Luna/JobSystem/TaskSystem.cpp](../../Luna/JobSystem/TaskSystem.cpp) | 拥有 `enki::TaskScheduler`，负责初始化、线程注册、任务提交、等待、IO 线程管理 |
| `TaskHandle` | [Luna/JobSystem/TaskHandle.h](../../Luna/JobSystem/TaskHandle.h) / [Luna/JobSystem/TaskHandle.cpp](../../Luna/JobSystem/TaskHandle.cpp) | 对外暴露的轻量句柄，负责状态查询、等待和 continuation 语法糖 |
| `TaskCompletionState` | [Luna/JobSystem/TaskHandleState.h](../../Luna/JobSystem/TaskHandleState.h) / [Luna/JobSystem/TaskSystem.cpp](../../Luna/JobSystem/TaskSystem.cpp) | 任务完成状态与依赖中继器，既记录是否完成/失败，也负责唤醒后继任务 |
| `PendingLaunch` | [Luna/JobSystem/TaskHandleState.h](../../Luna/JobSystem/TaskHandleState.h) / [Luna/JobSystem/TaskSystem.cpp](../../Luna/JobSystem/TaskSystem.cpp) | 表示“还有 N 个依赖没完成，等全部完成后再真正调度”的待发射任务 |
| `ResourceLoadQueue` | [Luna/JobSystem/ResourceLoadQueue.h](../../Luna/JobSystem/ResourceLoadQueue.h) | 面向资源加载场景的模板封装，默认走 `IO -> MainThread` |

## 文件怎么读

如果你准备顺着代码走一遍，建议按这个顺序:

1. 先看 [Luna/JobSystem/TaskHandle.h](../../Luna/JobSystem/TaskHandle.h)，理解对外 API 面。
2. 再看 [Luna/JobSystem/TaskSystem.h](../../Luna/JobSystem/TaskSystem.h)，确认 `TaskSystem` 暴露了哪些能力。
3. 然后直接读 [Luna/JobSystem/TaskSystem.cpp](../../Luna/JobSystem/TaskSystem.cpp)，这里包含了几乎全部关键行为。
4. 最后看 [Luna/JobSystem/ResourceLoadQueue.h](../../Luna/JobSystem/ResourceLoadQueue.h)，理解项目如何把任务系统落到资源加载场景里。

这样读的原因很简单:

- `TaskHandle.h` 决定了上层“怎么看这个系统”。
- `TaskSystem.cpp` 决定了这个系统“实际上怎么运行”。
- `ResourceLoadQueue.h` 则展示了“这个系统在项目里最自然的一种用法”。

## 三种任务目标

对上层来说，最重要的概念不是 enkiTS 的类型名，而是 `TaskTarget`:

| `TaskTarget` | 底层类型 | 执行位置 | 典型用途 |
| --- | --- | --- | --- |
| `Worker` | `enki::ITaskSet` | enkiTS worker 线程池 | CPU 计算、并行数据处理 |
| `MainThread` | `enki::IPinnedTask` | 注册号为 0 的应用主线程 | 主线程回写、渲染资源 commit |
| `IO` | `enki::IPinnedTask` | `TaskSystem` 自己创建并注册的 IO 线程 | 阻塞文件读取、解码前置加载 |

这个划分很关键，因为它解释了为什么源码里同时存在三种具体任务包装体:

- `FunctionTask`
- `ParallelFunctionTask`
- `PinnedFunctionTask`

它们都继承 `ManagedTaskBase`，差别在于“最终要交给 enkiTS 的是什么对象”。

## 源码中的四层结构

把 `TaskSystem.cpp` 拆开看，它大致有四层:

### 第一层: 任务壳对象

文件开头的三个 `struct` 是真正被 enkiTS 执行的任务壳:

- `FunctionTask`: 包装单次 lambda，执行时只在 `range.start == 0` 的那次回调里调用用户函数。
- `ParallelFunctionTask`: 包装分片并行 lambda，直接把 `TaskSetPartition` 转发给用户代码。
- `PinnedFunctionTask`: 包装必须固定在线程号上的任务，执行入口是 `Execute()`。

它们共同实现了 `ManagedTaskBase::schedule()`，也就是“等真正可以启动时，应该调用 `TaskSystem` 的哪个提交入口”。

### 第二层: 完成态与依赖推进

中间这层是这个封装最有价值的部分:

- `TaskCompletionState` 本身继承 `enki::ICompletable`
- `PendingLaunch` 用原子计数器等待多个依赖同时完成

这里的设计重点不是“任务本身”，而是“任务完成之后，怎么继续推进依赖图”。也正因为如此，`TaskHandle` 里持有的不是原始 enkiTS 任务，而是 `TaskCompletionState`。

### 第三层: 调度器所有权与线程管理

`TaskSystem` 自己拥有:

- `std::unique_ptr<enki::TaskScheduler> m_scheduler`
- `std::vector<std::thread> m_io_threads`
- `TaskSystemConfig m_config`
- `m_managed_tasks`，用于延长任务对象生命周期

它同时负责三类线程相关工作:

1. 初始化 enkiTS worker 线程池。
2. 启动并注册内部 IO 线程。
3. 为项目中的外部线程提供受控注册接口 `registerExternalThread()`。

### 第四层: 业务语义封装

`ResourceLoadQueue` 不拥有调度器，也不维护自己的线程。它只是把 `TaskSystem` 的两个能力组合起来:

1. 提交一个返回资源值的加载任务。
2. 在资源准备好后，再提交一个主线程 commit 任务。

这说明 `JobSystem` 的设计目标很明确: 先提供通用机制，再由更高层的封装把它固定成项目想要的工作流。

## `TaskHandle` 为什么只是一层状态句柄

很多任务系统会把“句柄”直接绑定到 future、promise 或原始任务对象上。Luna 这里没有这么做，而是把 `TaskHandle` 设计成一层很薄的状态引用。

`TaskHandle` 内部只有一个成员:

```cpp
std::shared_ptr<detail::TaskCompletionState> m_state;
```

这导致它有几个很重要的特征:

1. `TaskHandle` 很轻，可以像值类型一样拷贝。
2. `TaskHandle` 不负责保存任务函数本身，只负责观察“这个任务链走到哪了”。
3. continuation 依赖的是“前一个任务何时完成”，不是“前一个任务对象本体还在不在”。

这种设计和 `m_managed_tasks` 配合起来，等于把“用户看到的句柄”和“调度器真正持有的任务壳对象”分离开了。

## `m_managed_tasks` 的真实作用

第一次读代码时，最容易忽略的是 `TaskSystem::m_managed_tasks`。但如果少了它，整个封装会非常危险。

原因是:

- `submit()` / `submitParallel()` / `submitMainThreadJob()` 最终都把某个 `ManagedTaskBase` 子类对象交给 enkiTS。
- 这些对象里又保存着用户 lambda。
- 一旦 `TaskSystem` 在任务尚未完成时丢失对这些对象的所有权，enkiTS 就会在悬空对象上继续执行。

所以 `trackManagedTask()` 做的是“把任务对象和 completion state 一起保活”，而 `reapCompletedManagedTasks()` 则在任务完成后清理掉它们。

这也是为什么 `TaskSystem::waitForAll()`、`waitForTask()`、`runPinnedTasksForCurrentThread()` 后面都会顺手调用一次 `reapCompletedManagedTasks()`。

## 它在应用主循环里的位置

只看 `JobSystem` 目录还不够，必须把它放回应用生命周期里理解。`Application` 的关键接线点在:

- [Luna/Core/Application.cpp](../../Luna/Core/Application.cpp) 的 `Application::initialize()`
- [Luna/Core/Application.cpp](../../Luna/Core/Application.cpp) 的 `Application::run()`

从调用顺序看:

1. `Application::initialize()` 先初始化 `m_task_system`。
2. 每帧 `onUpdate()` 和 layer 更新结束后，`Application::run()` 会调用 `m_task_system.waitForAll()`。
3. 程序退出或 `Application` 析构时，会调用 `m_task_system.shutdown()`。

这意味着:

- 主线程 pinned task 不是凭空自动执行的，它依赖主线程在合适时机调用 `waitForAll()` / `waitForTask()` / `runPinnedTasksForCurrentThread()` 来“泵”任务。
- JobSystem 在项目里不是一个后台孤岛，而是被主循环主动驱动的。

## 读源码时最值得盯住的三条控制流

如果你只想抓住最关键的实现，建议重点跟踪下面三条路径。

### 路径一: 无依赖 worker task

```text
TaskSystem::submit()
-> FunctionTask
-> submitManagedTask()
-> submitTask()
-> enki::TaskScheduler::AddTaskSetToPipe()
-> FunctionTask::ExecuteRange()
-> TaskCompletionState::OnDependenciesComplete()
```

这是最基本、也最容易验证的一条路径。

### 路径二: 有依赖的 continuation

```text
TaskHandle::then()
-> TaskSystem::submit(..., dependencies)
-> submitManagedTask()
-> PendingLaunch
-> TaskCompletionState::addPendingLaunch()
-> dependencyResolved()
-> schedule()
```

这一条解释了 `then()` 和 `whenAll()` 为什么能工作。

### 路径三: 资源加载流水线

```text
ResourceLoadQueue::submitLoadWithCommit()
-> submitLoad()
-> IO task 写入 ResourceLoadState
-> load_handle.task().then(...)
-> MainThread commit task take() 资源
```

这一条体现了 JobSystem 在项目里的典型落地方式。

## 最后用一句话概括

如果把 `Luna/JobSystem` 当成一个整体来看，它本质上是:

“一个以 enkiTS 为执行后端、以 `TaskCompletionState` 为状态中继、以 `TaskHandle` 为对外抽象、以 `ResourceLoadQueue` 为业务示例的任务编排层。”

后面的几篇文章会分别沿着这个地图，把“任务怎么落地”“依赖怎么推进”“线程边界为什么这样设计”“资源加载如何借它搭出来”展开讲清楚。
