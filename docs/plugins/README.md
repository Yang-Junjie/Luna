# Luna 插件系统文档

这一组文档专门描述 Luna 当前的插件系统实现。

这些文档不是“理想中的未来设计稿”，而是基于当前仓库源码整理出来的实现说明。文中的“已经支持”“目前限制”“后续建议”都以 `F:\Beisent\Luna` 代码库当前状态为准。

## 这组文档解决什么问题

如果你当前关心的是下面这些问题，这一组文档就是给你准备的:

- Luna 现在的插件系统到底已经实现到了什么程度
- `Bundle -> sync.py -> Generated -> CMake -> 宿主 -> 注册表 -> 运行时` 这条链路到底怎么工作
- `Editor/` 和 `Plugins/builtin/...` 的职责边界是什么
- `Runtime/` 和 `Editor/` 现在分别承担什么宿主职责
- 新插件应该怎么组织目录、怎么写 manifest、怎么写注册函数、怎么接入 Bundle
- 为什么会生成 host 专属的 `PluginList.cmake`、`ResolvedPlugins.cpp` 和 lock 文件
- 当前哪些功能只是骨架，哪些功能已经真的工作

## 文档导航

| 文档 | 说明 |
| --- | --- |
| [plugin-system-manual.md](./plugin-system-manual.md) | 插件系统总览、目录布局、构建期流程、运行时流程、生成文件说明 |
| [writing-your-first-plugin.md](./writing-your-first-plugin.md) | 从零创建一个新插件的完整步骤，包括 manifest、CMake、注册函数、Bundle 接入 |
| [builtin-editor-core-plugin-walkthrough.md](./builtin-editor-core-plugin-walkthrough.md) | 以当前内置插件 `luna.editor.core` 为例，逐文件讲解它是怎么工作的 |
| [current-status-and-roadmap.md](./current-status-and-roadmap.md) | 当前实现状态、明确限制、未完成项以及下一阶段最合理的推进方向 |

## 推荐阅读顺序

1. 先读 [plugin-system-manual.md](./plugin-system-manual.md)，建立整体模型。
2. 再读 [builtin-editor-core-plugin-walkthrough.md](./builtin-editor-core-plugin-walkthrough.md)，把抽象概念对回真实代码。
3. 准备自己写插件时，读 [writing-your-first-plugin.md](./writing-your-first-plugin.md)。
4. 准备继续扩展整套系统时，读 [current-status-and-roadmap.md](./current-status-and-roadmap.md)。

## 当前一句话总结

Luna 当前的插件系统本质上是:

> 本地源码插件 + Python `sync.py` 生成构建清单 + 宿主启动时显式注册

它还不是一个完整的包管理生态，也不是一个二进制热插拔系统。
它现在已经能稳定完成下面这件事:

- 从 Bundle 里选中插件
- 解析本地插件 manifest
- 生成插件 CMake 接入清单和注册代码
- 构建宿主并在启动时注册插件贡献

这条链路已经打通，当前默认例子是:

- Bundle: `Bundles/EditorDefault/luna.bundle.toml`
- Bundle: `Bundles/RuntimeDefault/luna.bundle.toml`
- 插件: `Plugins/builtin/luna.editor.core`
- 插件: `Plugins/builtin/luna.example.hello`
- 插件: `Plugins/builtin/luna.example.imgui_demo`
- 插件: `Plugins/builtin/luna.runtime.core`
- 宿主: `LunaEditorApp`
- 宿主: `LunaRuntimeApp`

## 当前最重要的边界

请先记住这条边界，它决定了后面整个插件系统是否会长歪:

- `Editor/` 负责“编辑器宿主框架与扩展点”
- `Runtime/` 负责“运行时宿主框架”
- `Plugins/...` 负责“具体插件实现与能力贡献”

也就是说:

- `Editor/` 不应该继续塞越来越多具体功能
- 具体面板、工具、命令、工作流能力，应该逐步迁移到 `Plugins/` 下面

后面的文档会详细解释这条边界为什么成立，以及当前代码是如何按这条边界工作的。
