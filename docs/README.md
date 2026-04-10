# Luna Wiki

Luna 是一个基于 C++20、GLFW、ImGui 和 Vulkan 的渲染基础设施项目。按照当前代码快照来看，它已经具备一套清晰的应用生命周期、层系统、事件输入桥接、Vulkan 上下文管理、RenderGraph 框架，以及着色器/模型/图片导入能力；当前默认编辑器运行时则聚焦于一个最小可工作的渲染闭环: 清屏 + ImGui 编辑器界面 + 自由摄像机控制。

> **提示 (Note):**
> 本文档基于 `F:\Beisent\Luna` 源码逆向整理，而不是基于外部产品说明书。凡是“当前已实现/尚未接线”的描述，均以仓库中的实际代码为准。

## 文档导航

| 类型 | 文档 | 说明 |
| --- | --- | --- |
| Manual | [manual/introduction-and-core-concepts.md](./manual/introduction-and-core-concepts.md) | 项目定位、目标、核心术语 |
| Manual | [manual/architecture-in-depth.md](./manual/architecture-in-depth.md) | 系统总览、目录结构、生命周期、设计模式 |
| Manual | [manual/getting-started.md](./manual/getting-started.md) | 环境准备、构建、运行、最小示例 |
| Manual | [manual/building-an-application-with-rendergraph.md](./manual/building-an-application-with-rendergraph.md) | 如何基于现有框架组装应用并接入自定义 RenderGraph |
| Manual | [manual/subsystems-manual.md](./manual/subsystems-manual.md) | 应用层、平台层、渲染层、资源层、ImGui 集成详解 |
| Reference | [reference/class-and-api-reference.md](./reference/class-and-api-reference.md) | 核心类与 API 参考 |
| Advanced | [advanced/best-practices-and-advanced.md](./advanced/best-practices-and-advanced.md) | 性能、扩展、二次开发建议 |

## 推荐阅读路径

1. 初次接触 Luna: 先读“简介与核心概念”与“快速入门”。
2. 准备从零组装一个新应用: 接着读“使用现有框架与 RenderGraph 构建应用”。
3. 需要修改运行时框架: 再读“架构设计深度剖析”。
4. 需要新增渲染通道或资源类型: 重点读“子系统解析”和“最佳实践与进阶”。
5. 需要查类和函数: 直接跳到 API 参考。

## 当前架构结论

从源码来看，Luna 当前更准确的定位是:

- 一个编辑器壳 `LunaEditor`
- 一个通用引擎核心库 `LunaCore`
- 一套内嵌的 `luna::val`
- 一条最小渲染路径: `clear -> imgui -> present`

换句话说，Luna 已经完成了“引擎骨架”和“渲染框架”的大部分基础设施，但默认应用尚未把 `ModelLoader`、`ShaderLoader`、复杂场景绘制等功能接进每帧渲染图中。这不是缺点，而是理解源码时必须接受的事实: 代码库的重心目前是“打底层能力”，不是“交付完整内容管线”。

## 仓库文档结构

```text
docs/
├─ README.md
├─ manual/
│  ├─ introduction-and-core-concepts.md
│  ├─ architecture-in-depth.md
│  ├─ getting-started.md
│  ├─ building-an-application-with-rendergraph.md
│  └─ subsystems-manual.md
├─ reference/
│  └─ class-and-api-reference.md
└─ advanced/
   └─ best-practices-and-advanced.md
```
