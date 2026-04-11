# JobSystem 线程模型与调度边界

> **提示 (Note):**
> `TaskSystem` 最容易被误用的地方不是 API 形状，而是线程边界。这篇文章专门回答三个问题: 哪些线程是“已注册线程”、为什么某些 API 只能在已注册线程上调用，以及主线程和 IO 线程在这个设计里分别扮演什么角色。

## 先把线程角色分清

在 `Luna/JobSystem` 里，线程不是一个统一概念，而是被分成四类角色:

| 角色 | 来源 | 是否由 `TaskSystem` 创建 | 主要职责 |
| --- | --- | --- | --- |
| 应用主线程 | 调用 `TaskSystem::initialize()` 的线程 | 否 | 主循环、主线程 pinned task、等待与泵任务 |
| worker 线程 | enkiTS 内部线程池 | 是，由 `enki::TaskScheduler` 创建 | 执行普通 `Worker` task 与并行分片任务 |
| IO 线程 | `TaskSystem` 自己创建的 `std::thread` | 是 | 执行 `TaskTarget::IO` pinned task |
| external 线程 | 项目里其他自建线程 | 否 | 在显式注册后，才能安全提交/等待 worker task |

Luna 对这四类线程的态度并不一样:

- worker 和主线程天然属于 enkiTS 的“注册线程世界”。
- IO 线程由 `TaskSystem` 启动后主动注册进去。
- external 线程默认不属于这个世界，必须显式注册。

## 初始化时到底创建了什么

`TaskSystem::initialize()` 做的不是简单的 “new 一个调度器”，它大致分成四步:

1. 保存 `TaskSystemConfig`
2. 计算实际 `worker_thread_count`
3. 初始化 `enki::TaskScheduler`
4. 创建并等待内部 IO 线程完成注册

最重要的一步是 worker 数量计算。若用户没有显式指定 `worker_thread_count`，代码会走:

```text
hardware_threads - (1 + io_thread_count + external_thread_count)
```

其中额外减掉的 `1` 是给主线程预留的位置。若结果不够，仍至少保留 1 个 worker。

这说明它的思路不是“尽量把所有硬件线程都变成 worker”，而是“先给主线程、IO 线程、外部注册槽预留空间，再把剩余预算给 worker 池”。

## 为什么 `numExternalTaskThreads = io + external`

初始化 enkiTS 时，Luna 传入:

- `numTaskThreadsToCreate = worker_thread_count`
- `numExternalTaskThreads = io_thread_count + external_thread_count`

这里的 `external` 不是只给用户线程准备的。Luna 把内部 IO 线程也视为 enkiTS 的 external thread。

这样设计有两个直接结果:

1. IO 线程不需要成为 worker 池的一部分，也不会被工作窃取逻辑误用成通用计算线程。
2. 用户如果还想让自己的 `std::thread` 调用 `submit()` 或 `wait()`，就必须在配置里额外留出 external slot。

## IO 线程是怎么注册进去的

`TaskSystem::initialize()` 在调度器初始化后，会启动 `m_config.io_thread_count` 个 `std::thread`，每个线程入口都是 `ioThreadMain(thread_number)`。

这里的 `thread_number` 不是操作系统线程 ID，而是 enkiTS 的注册编号。Luna 通过:

```cpp
getFirstIOThreadNumber() + io_index
```

给每个 IO 线程分配固定编号。当前 `getFirstIOThreadNumber()` 直接返回 enkiTS 的 `GetNumFirstExternalTaskThread()`，也就是外部线程编号段的起点。

`ioThreadMain()` 的执行步骤非常明确:

1. 调用 `RegisterExternalTaskThread(thread_number)` 把当前线程注册成 enkiTS external thread。
2. 用条件变量告诉初始化线程“我注册成功了”或“我失败了”。
3. 进入循环，等待新 pinned task。
4. 每次被唤醒后调用 `RunPinnedTasks()` 执行属于自己线程号的任务。
5. 收到 shutdown 请求后反注册并退出。

所以 IO 线程本质上是:

“一组被固定 thread number 的 external thread，它们只负责跑 pinned task，不参与 worker 池那套普通 task 调度。”

## 主线程 pinned task 为什么能跑起来

`TaskTarget::MainThread` 最终会走到 `TaskSystem::submitMainThreadJob()`，那里只做了一件关键事:

```cpp
task->threadNum = 0;
```

也就是说，主线程任务会被绑定到 enkiTS 线程号 0。

但这还不够。绑定到主线程不等于会被自动执行，主线程还必须主动“泵” pinned task。Luna 里负责这件事的是:

- `TaskSystem::waitForAll()`
- `TaskSystem::waitForTask()`
- `TaskSystem::runPinnedTasksForCurrentThread()`

而项目正常运行路径里，真正稳定驱动它的是 [Luna/Core/Application.cpp](../../Luna/Core/Application.cpp) 的 `Application::run()`:

```text
每帧更新结束 -> m_task_system.waitForAll() -> 主线程顺便执行 pinned task
```

这也解释了为什么资源 commit 这类主线程任务能在应用主循环里自然工作。

## 为什么有些 API 只能在已注册线程上调用

`TaskSystem` 对线程边界的判断最终集中在两个函数:

- `isCurrentThreadRegistered()`
- `requireSchedulerApiThread()`

`isCurrentThreadRegistered()` 的标准很简单:

```cpp
m_scheduler != nullptr && m_scheduler->GetThreadNum() != enki::NO_THREAD_NUM
```

换句话说，只要当前线程在 enkiTS 看来有合法线程号，就算已注册线程。

在这个前提下，Luna 要求下面这些 API 只能在已注册线程上调用:

- `submitTask()`
- `waitForAll()`
- `waitForTask()`
- `runPinnedTasksForCurrentThread()`
- 间接地，所有 `Worker` 目标的高层提交接口

原因不只是“作者想加限制”，而是这些 API 本来就依赖 enkiTS 对调用线程的认识。一个完全未注册的普通线程既不能安全参与等待，也不应该被允许向 worker 调度器提交通用任务。

## 外部线程如何接入系统

如果项目里有自己的 `std::thread`，它想做两件事:

1. 提交 `Worker` task
2. 等待某个 `TaskHandle`

那么这条线程必须先通过 `TaskSystem::registerExternalThread()` 注册。

这个 API 有三个前置条件:

1. `TaskSystem` 已初始化
2. `external_thread_count > 0`
3. 当前线程还没注册过

成功后返回的是一个 `ExternalThreadScope`。它是一个 RAII 对象:

- 构造成功表示当前线程已注册
- 析构时自动调用 `deregisterExternalThread()`
- 支持 move，不支持拷贝

这层 RAII 很重要，因为 external thread 的注册不是“全局开关”，而是和具体线程生命周期绑定的。

## `deregisterExternalThread()` 为什么要检查编号区间

反注册逻辑不是无条件执行的。`TaskSystem::deregisterExternalThread()` 先取当前线程号，然后检查它是否落在:

```text
[first_external, first_external + io_thread_count + external_thread_count)
```

这么做的意图是区分:

- 主线程和 worker 线程不该走 external 反注册路径
- 只有 IO 线程和用户注册的 external 线程需要走 `DeRegisterExternalTaskThread()`

这也是 `ExternalThreadScope` 能安全地被用户线程和内部 IO 线程共用同一套外部线程语义的原因。

## `submitIOJob()` 与 `submitMainThreadJob()` 的区别

这两个接口都提交 pinned task，但线程路由不同。

### `submitMainThreadJob()`

- 永远设 `threadNum = 0`
- 不做轮转
- 依赖主线程主动 pump

### `submitIOJob()`

- 要求 `io_thread_count > 0`
- 用 `m_next_io_thread_index` 做轮转分发
- 每次把任务绑到某个固定 IO 线程号上

所以 IO 任务虽然看起来是“后台任务”，但它并不是自由浮动在 worker 池里，而是固定投递到一组专门的 pinned 线程上。

这很适合阻塞式文件 IO，因为:

- 它不会占用通用 worker 线程
- 它的执行位置可控
- 调度策略简单直接

## 为什么 `canSubmitWorkerTasksFromCurrentThread()` 这么严格

这个函数当前实现非常保守:

```cpp
return isCurrentThreadRegistered();
```

这意味着只有 enkiTS 已注册线程才允许提交 `TaskTarget::Worker` 任务。表面上看，这似乎限制了“普通线程随手丢任务”的便利性，但换来的好处是边界非常清晰:

- 哪些线程参与调度器生态，一眼就知道
- 哪些线程允许等待或继续推进任务，不会含糊
- 出问题时日志和断言更容易定位

对于引擎基础设施来说，这种保守设计通常比宽松但语义模糊的接口更稳。

## 关闭流程如何收尾

`TaskSystem::shutdown()` 的顺序是:

1. `WaitforAllAndShutdown()`
2. `join()` 所有 IO 线程
3. 清空 `m_managed_tasks`
4. 销毁调度器并重置配置状态

这一顺序背后的假设是:

- 先通知 enkiTS 停止并等待任务结束
- 再等 IO 线程从 `WaitForNewPinnedTasks()` 醒来并退出
- 最后才释放任务对象和调度器资源

如果顺序反过来，例如先销毁调度器再等 IO 线程退出，就很容易把还在等待的 IO 线程留在悬空状态里。

## 用 sample 验证线程模型

[samples/TaskSystem/TaskSystemSmoke.cpp](../../samples/TaskSystem/TaskSystemSmoke.cpp) 很适合作为阅读佐证，因为它直接检查了三件事:

1. `MainThread` 任务执行时 `GetThreadNum() == 0`
2. `IO` 任务执行在线程号 `getFirstIOThreadNumber()`
3. 外部 `std::thread` 在注册后可以安全提交并等待 worker task

这说明当前封装的线程模型不是文档约定，而是 sample 明确验证过的行为。

## 最后用一句话概括

Luna 的线程模型不是“所有线程都能随便碰调度器”，而是:

“主线程、worker、IO、external 四类线程分工明确；只有被 enkiTS 认可的注册线程才能真正参与任务提交、等待与 pinned task 驱动。”

理解这一点之后，`TaskTarget::MainThread`、`TaskTarget::IO`、`registerExternalThread()` 和 `Application::run()` 之间的关系就会非常清楚。
