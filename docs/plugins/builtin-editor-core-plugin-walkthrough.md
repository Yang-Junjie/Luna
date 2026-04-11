# builtin 插件 `luna.editor.core` 逐文件讲解

## 1. 为什么要专门讲这个插件

因为它是当前仓库里第一个真正跑通整条插件链路的插件。

它不是概念样例，而是当前默认编辑器实际在用的插件。

如果你想真正理解 Luna 当前插件系统是怎么工作的，看这个插件比看抽象设计图更有帮助。

## 2. 这个插件在系统中的位置

它的位置是:

```text
Plugins/builtin/luna.editor.core
```

它的职责不是“充当编辑器宿主”。
它的职责是“给编辑器宿主提供一组默认编辑器能力”。

当前它提供:

- 一个编辑器相机控制 Layer
- 一个 `Renderer` 面板
- 一个 `Reset Camera` 命令

## 3. 目录结构

当前目录如下:

```text
Plugins/builtin/luna.editor.core/
├─ luna.plugin.toml
├─ CMakeLists.txt
└─ src/
   ├─ BuiltinEditorCorePlugin.cpp
   ├─ EditorCameraControllerLayer.h
   ├─ RendererInfoPanel.h
   └─ RendererInfoPanel.cpp
```

## 4. `luna.plugin.toml`

当前 manifest 是:

```toml
id = "luna.editor.core"
name = "Builtin Editor Core"
version = "0.1.0"
sdk = "0.1"
kind = "editor"
cmake_target = "LunaBuiltinEditorCorePlugin"
entry = "luna_register_luna_editor_core"
hosts = ["editor"]

[dependencies]
```

### 4.1 这份 manifest 说明了什么

- 这是一个 `editor` 类插件
- 它只允许被 `editor` 宿主使用
- 它的 CMake target 叫 `LunaBuiltinEditorCorePlugin`
- 它的注册入口是 `luna_register_luna_editor_core`
- 它当前没有声明其他插件依赖

## 5. `CMakeLists.txt`

这个文件定义了插件目标:

```cmake
add_library(
    LunaBuiltinEditorCorePlugin
    STATIC
    luna.plugin.toml
    src/BuiltinEditorCorePlugin.cpp
    src/EditorCameraControllerLayer.h
    src/RendererInfoPanel.h
    src/RendererInfoPanel.cpp
)

target_link_libraries(
    LunaBuiltinEditorCorePlugin
    PUBLIC
        LunaEditorFramework
)
```

### 5.1 这里最关键的一点

这个插件没有直接混进 `Editor/` 宿主源码里。

它是一个单独的静态库 target，然后通过 `PluginList.cmake` 被 `LunaEditor` 链接进去。

这正是“插件是插件，宿主是宿主”的边界开始真正落地的地方。

## 6. `BuiltinEditorCorePlugin.cpp`

这是插件真正的入口文件。

它做的事可以概括成:

1. 提供注册函数
2. 往 `PluginRegistry` 注册 Layer
3. 往 `EditorRegistry` 注册 Panel 和 Command

核心入口长这样:

```cpp
extern "C" void luna_register_luna_editor_core(luna::PluginRegistry& registry)
```

### 6.1 为什么这个函数重要

`sync.py` 读取 manifest 里的 `entry` 字段后，会把它写进生成的 `ResolvedPlugins.cpp`。

最后宿主不是“扫描插件目录”来找它，而是直接调用这个函数。

### 6.2 它当前注册了什么

#### Layer

```cpp
registry.addLayer("luna.editor.camera_controller", [] {
    return std::make_unique<luna::editor::EditorCameraControllerLayer>();
});
```

这表示:

- 插件提交了一个 Layer 工厂
- 宿主稍后会用这个工厂创建真正的 Layer 实例

#### Panel

```cpp
editor_registry.addPanel<luna::editor::RendererInfoPanel>(
    "luna.editor.renderer",
    "Renderer",
    true);
```

这表示:

- 插件往编辑器注册了一个面板
- 面板 id 是 `luna.editor.renderer`
- 窗口标题是 `Renderer`
- 默认打开

#### Command

```cpp
editor_registry.addCommand("luna.editor.reset_camera", "Reset Camera", [] {
    resetMainCamera();
});
```

这表示:

- 插件额外注册了一个命令
- 编辑器主菜单会把它显示在 `Commands` 菜单里

## 7. `EditorCameraControllerLayer.h`

这个文件定义了一个具体 Layer。

它做的事是:

- 监听鼠标右键与键盘
- 驱动主相机旋转和平移
- 在 `onDetach()` 时恢复鼠标状态

### 7.1 为什么它是 Layer 而不是 Panel

因为它本质上不是一个 UI 窗口，而是持续运行的交互逻辑。

它的职责是:

- 每帧更新
- 处理输入状态
- 操作相机

这正是当前 `Layer` 更适合承载的内容。

## 8. `RendererInfoPanel.*`

这是一个具体面板实现。

它做的事比较直接:

- 显示和编辑清屏颜色
- 显示相机位置
- 显示 pitch / yaw
- 显示当前操作提示

### 8.1 为什么它不是 Layer

因为这部分本质上只是 UI 展示内容。

它没有必要接入完整 Layer 生命周期，也不应该为了一个小窗口去占一个 Layer 插槽。

所以它被设计成 `EditorPanel`。

## 9. 它是如何被宿主真正用起来的

链路如下:

1. Bundle 启用了 `luna.editor.core`
2. `sync.py` 扫到它的 manifest
3. `sync.py` 生成 `PluginList.cmake`
4. `Editor/CMakeLists.txt` 通过生成文件把它加入构建
5. `sync.py` 生成 `ResolvedPlugins.cpp`
6. `EditorApp::onInit()` 调用 `registerResolvedPlugins()`
7. `registerResolvedPlugins()` 调用 `luna_register_luna_editor_core()`
8. 插件将 Layer / Panel / Command 注册到 registry
9. 宿主根据注册结果实例化对象并进入主循环

## 10. 它为什么仍然叫 `editor.core`

当前这个插件本质上承担了“默认编辑器最小功能集合”的角色。

它不应该无限扩张成“把所有编辑器功能都塞进去的大杂烩”。

更合理的方向是:

- `luna.editor.core` 只保留非常基础的编辑器默认能力
- 其他功能逐步拆成:
  - `luna.asset.browser`
  - `luna.scene.editor`
  - `luna.viewport`
  - `luna.gizmo`

## 11. 这个插件体现出的架构意义

这个插件之所以重要，不只是因为它能显示一个面板。

它真正说明了三件事:

### 11.1 插件已经是独立目标，不再只是硬编码源文件

这是结构上的巨大差别。

### 11.2 宿主与插件的边界已经开始成形

- `Editor/` 负责宿主框架
- `Plugins/builtin/luna.editor.core` 负责具体能力

### 11.3 插件系统现在确实能影响运行结果

如果你把它从 Bundle 里去掉，再跑 `sync.py`，当前默认编辑器就不会再得到这些贡献。

## 12. 从这个插件学到的最重要经验

如果你准备继续写新插件，当前最值得模仿的是这几点:

- manifest、CMake、注册函数、具体实现分离
- 用注册函数提交贡献，而不是直接改宿主
- 小 UI 做成 `EditorPanel`
- 持续输入逻辑做成 `Layer`
- 让宿主统一实例化和管理生命周期

## 13. 一句话总结

`luna.editor.core` 是 Luna 当前插件系统的第一个“真实插件样板”。

它证明了这条链路已经能工作:

> Bundle 选择插件 -> `sync.py` 生成构建与注册文件 -> 宿主调用插件入口 -> 插件提交贡献 -> 编辑器把贡献变成真实运行时行为
