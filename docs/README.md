# Luna Wiki

Luna 是一个基于 C++20、GLFW、ImGui 与 Vulkan 的可扩展应用框架。它当前的重心不是“交付一个完整游戏编辑器”，而是把下面几件事稳定地组合在一起:

- 单一应用宿主 `LunaApp`
- 基于 Bundle 的源码插件装配
- `Layer` / `Overlay` 生命周期
- Vulkan 上下文与 RenderGraph
- 基础资源导入能力
- Editor shell 这类可选上层功能

> **提示 (Note):**
> 这套文档以 `F:\Beisent\Luna` 当前源码为准。凡是“已经支持”“还未支持”“推荐这样做”的描述，都基于仓库中的真实实现，不基于理想设计稿。

## 当前架构一句话总结

Luna 当前更准确的定位是:

> 一个通过 Bundle 选择插件、通过 `build.py + sync.py` 生成构建输入、最终装配成 runtime 或 editor 形态的单宿主 Vulkan 应用框架。

这意味着:

- `editor` 和 `runtime` 不是两个独立程序架构，而是同一个 `LunaApp` 的两种插件组合。
- 插件系统已经能稳定承载 `Layer`、`Panel`、`Command` 和 ImGui 请求。
- 渲染器已经提供 `RenderGraph` 能力，但当前插件系统**还不能**把自定义 RenderGraph 作为正式扩展点接入活动宿主。

## 文档地图

| 类型 | 文档 | 说明 |
| --- | --- | --- |
| Manual | [manual/introduction-and-core-concepts.md](./manual/introduction-and-core-concepts.md) | 项目定位、核心术语、当前能力边界 |
| Manual | [manual/architecture-in-depth.md](./manual/architecture-in-depth.md) | 系统总览、目录结构、生命周期、设计模式与耦合关系 |
| Manual | [manual/getting-started.md](./manual/getting-started.md) | 环境准备、构建、运行、最小插件示例 |
| Manual | [manual/building-an-application-with-rendergraph.md](./manual/building-an-application-with-rendergraph.md) | 如何像 `Samples/Model` 那样通过自定义宿主接入 RenderGraph |
| Manual | [manual/subsystems-manual.md](./manual/subsystems-manual.md) | App/Plugin Host、Editor、Renderer、Asset、JobSystem 等子系统讲解 |
| Manual | [manual/job-system-manual.md](./manual/job-system-manual.md) | JobSystem 的独立使用手册 |
| Plugins | [plugins/README.md](./plugins/README.md) | 插件系统文档导航 |
| Plugins | [plugins/plugin-system-manual.md](./plugins/plugin-system-manual.md) | 插件构建期与运行时工作流、生成文件、注册表说明 |
| Plugins | [plugins/writing-your-first-plugin.md](./plugins/writing-your-first-plugin.md) | 从零编写一个 Luna 插件 |
| Plugins | [plugins/builtin-editor-core-plugin-walkthrough.md](./plugins/builtin-editor-core-plugin-walkthrough.md) | `luna.editor.core` 逐文件讲解 |
| Plugins | [plugins/current-status-and-roadmap.md](./plugins/current-status-and-roadmap.md) | 插件系统现状、局限与下一阶段路线 |
| Reference | [reference/class-and-api-reference.md](./reference/class-and-api-reference.md) | 核心类与 API 参考 |
| Advanced | [advanced/job-system-source-map.md](./advanced/job-system-source-map.md) | JobSystem 目录地图与对象关系 |
| Advanced | [advanced/job-system-task-lifecycle.md](./advanced/job-system-task-lifecycle.md) | 任务生命周期 |
| Advanced | [advanced/job-system-thread-model.md](./advanced/job-system-thread-model.md) | 线程模型 |
| Advanced | [advanced/job-system-resource-loading-pipeline.md](./advanced/job-system-resource-loading-pipeline.md) | 资源加载流水线 |
| Advanced | [advanced/best-practices-and-advanced.md](./advanced/best-practices-and-advanced.md) | 性能、扩展与二次开发建议 |

## 推荐阅读路径

### 路线 A: 第一次接触 Luna

1. 读 [简介与核心概念](./manual/introduction-and-core-concepts.md)
2. 读 [快速入门](./manual/getting-started.md)
3. 启动 `editor` 和 `runtime` 两个 profile

### 路线 B: 准备扩展编辑器

1. 读 [架构设计深度剖析](./manual/architecture-in-depth.md)
2. 读 [插件系统手册](./plugins/plugin-system-manual.md)
3. 读 [编写你的第一个插件](./plugins/writing-your-first-plugin.md)
4. 对照 [luna.editor.core walkthrough](./plugins/builtin-editor-core-plugin-walkthrough.md)

### 路线 C: 准备做渲染/资源方向开发

1. 读 [架构设计深度剖析](./manual/architecture-in-depth.md)
2. 读 [子系统解析](./manual/subsystems-manual.md)
3. 读 [使用现有框架与 RenderGraph 构建应用](./manual/building-an-application-with-rendergraph.md)
4. 读 [类与 API 参考](./reference/class-and-api-reference.md)

### 路线 D: 准备修改底层任务系统

1. 读 [job-system-manual.md](./manual/job-system-manual.md)
2. 读 `advanced/` 目录下四篇 JobSystem 深入文档

## 当前代码库的几个关键事实

### 1. 单宿主已经落地

当前真正的应用入口是:

- `Luna/Core/Main.cpp`
- `App/LunaApp.cpp`

不是旧的 `EditorApp` / `RuntimeApp` 双宿主模型。

### 2. 插件已经进入“可用骨架”阶段

当前已经打通:

- `Bundle -> sync.py -> Generated -> CMake -> registerResolvedPlugins() -> PluginRegistry`

这条链路已经足够支撑继续新增 editor/runtime 插件。

### 3. Renderer 能力强于当前插件扩展协议

仓库中已经有:

- `RenderGraphBuilder`
- 自定义 `RenderPass`
- `Samples/Model`

但这些能力现在主要通过**宿主或样例应用**接入，而不是通过正式插件 API 注入当前 `LunaApp`。

### 4. 文档会明确区分“已经支持”和“技术上能硬做”

例如:

- 插件里的 `Layer` 可以直接访问 `Application::get().getRenderer()`
- 但这不等于插件系统已经正式支持“RenderGraph 贡献”

后面的章节会刻意把这种边界讲清楚，避免把“源码同仓可访问”误写成“稳定扩展点”。

## 仓库文档结构

```text
Docs/
├─ README.md
├─ manual/
│  ├─ introduction-and-core-concepts.md
│  ├─ architecture-in-depth.md
│  ├─ getting-started.md
│  ├─ building-an-application-with-rendergraph.md
│  ├─ subsystems-manual.md
│  └─ job-system-manual.md
├─ plugins/
│  ├─ README.md
│  ├─ plugin-system-manual.md
│  ├─ writing-your-first-plugin.md
│  ├─ builtin-editor-core-plugin-walkthrough.md
│  └─ current-status-and-roadmap.md
├─ reference/
│  └─ class-and-api-reference.md
└─ advanced/
   ├─ best-practices-and-advanced.md
   ├─ job-system-source-map.md
   ├─ job-system-task-lifecycle.md
   ├─ job-system-thread-model.md
   └─ job-system-resource-loading-pipeline.md
```

## 下一步该读什么

- 如果你只想先把程序跑起来，直接去看 [快速入门](./manual/getting-started.md)。
- 如果你已经在写插件，直接去看 [插件系统手册](./plugins/plugin-system-manual.md)。
- 如果你现在关心“为什么 `Samples/Model` 能做到，而插件暂时做不到”，直接去看 [使用现有框架与 RenderGraph 构建应用](./manual/building-an-application-with-rendergraph.md)。
