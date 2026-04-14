# 第五部分: 类与 API 参考手册

> **提示 (Note):**
> 本节不是“所有类型的穷举索引”，而是当前对二次开发最重要的一组核心类参考。  
> 表格中的“属性”优先列出外部开发者真正需要理解的状态或配置，而不是机械复制全部私有成员。

## 类名: `luna::ApplicationSpecification`

- **继承/实现**: 无
- **简述**: 应用宿主的基础配置结构，决定窗口标题、尺寸以及 ImGui 开关。

### 属性 (Properties)

| 类型 | 名称 | 默认值 | 描述 |
| --- | --- | --- | --- |
| `std::string` | `m_name` | `"Luna"` | 窗口标题 |
| `uint32_t` | `m_window_width` | `1600` | 初始窗口宽度 |
| `uint32_t` | `m_window_height` | `900` | 初始窗口高度 |
| `bool` | `m_maximized` | `false` | 是否启动即最大化 |
| `bool` | `m_enable_imgui` | `true` | 是否默认启用 ImGui |
| `bool` | `m_enable_multi_viewport` | `false` | 是否启用 ImGui 多视口 |

## 类名: `luna::Application`

- **继承/实现**: 无
- **简述**: 应用主循环与生命周期的模板方法宿主。

### 属性 (Properties)

| 类型 | 名称 | 默认值 | 描述 |
| --- | --- | --- | --- |
| `bool` | `m_initialized` | `false` | 是否已完成 `initialize()` |
| `bool` | `m_running` | `true` | 主循环是否继续 |
| `ApplicationSpecification` | `m_specification` | 见默认值 | 应用配置 |
| `VulkanRenderer` | `m_renderer` | 默认构造 | 当前宿主绑定的渲染器 |
| `TaskSystem` | `m_task_system` | 默认构造 | 当前宿主的任务系统 |

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `bool` | `initialize()` | 创建窗口、初始化 renderer 与 task system |
| `void` | `run()` | 启动主循环 |
| `void` | `close()` | 请求关闭应用 |
| `void` | `pushLayer(std::unique_ptr<Layer> layer)` | 添加普通层并调用 `onAttach()` |
| `void` | `pushOverlay(std::unique_ptr<Layer> overlay)` | 添加 overlay 并调用 `onAttach()` |
| `ImGuiLayer*` | `getImGuiLayer() const` | 获取当前 ImGui 层 |
| `Timestep` | `getTimestep() const` | 获取上一帧时间步长 |
| `bool` | `isInitialized() const` | 查询初始化状态 |
| `VulkanRenderer&` | `getRenderer()` | 获取 renderer |
| `TaskSystem&` | `getTaskSystem()` | 获取 task system |
| `static Application&` | `get()` | 获取当前全局应用实例 |
| `bool` | `enableImGui(bool enable_multi_viewport = false)` | 在初始化前后启用 ImGui |

### 方法详细说明

#### `initialize()`

`initialize()` 的职责是:

1. 初始化 `TaskSystem`
2. 创建 `Window`
3. 初始化 `VulkanRenderer`
4. 根据配置启用 ImGui

```cpp
class MyApp final : public luna::Application {
public:
    MyApp()
        : Application(luna::ApplicationSpecification{
              .m_name = "My App",
              .m_window_width = 1280,
              .m_window_height = 720,
          })
    {}
};
```

#### `run()`

`run()` 固定了整个应用主循环骨架。  
子类只通过 `onInit()`、`onUpdate()`、`onShutdown()` 这几个钩子插入行为。

## 类名: `luna::Layer`

- **继承/实现**: 无
- **简述**: LayerStack 中的统一逻辑单元接口。

### 属性 (Properties)

| 类型 | 名称 | 默认值 | 描述 |
| --- | --- | --- | --- |
| `std::string` | `m_name` | `"Layer"` | 层名称 |

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `void` | `onAttach()` | 层挂载时调用 |
| `void` | `onDetach()` | 层卸载时调用 |
| `void` | `onUpdate(Timestep dt)` | 每帧逻辑更新 |
| `void` | `onEvent(Event& event)` | 事件回调 |
| `void` | `onImGuiRender()` | ImGui UI 绘制 |
| `void` | `onRender()` | 渲染阶段逻辑钩子 |
| `const std::string&` | `getName() const` | 获取层名 |

### 方法详细说明

#### `onImGuiRender()`

```cpp
class StatsLayer final : public luna::Layer {
public:
    StatsLayer() : Layer("StatsLayer") {}

    void onImGuiRender() override
    {
        ImGui::Begin("Stats");
        ImGui::TextUnformatted("Hello from Luna.");
        ImGui::End();
    }
};
```

## 类名: `luna::ServiceRegistry`

- **继承/实现**: 无
- **简述**: 按类型索引共享服务对象的轻量服务容器。

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `Service&` | `emplace< Service >(Args&&...)` | 原位构造并注册服务 |
| `Interface&` | `emplaceAs< Interface, Service >(Args&&...)` | 以接口类型注册派生服务 |
| `Service&` | `add(std::shared_ptr<Service> service)` | 注册已有共享对象 |
| `bool` | `has< Service >() const` | 判断服务是否已注册 |
| `Service&` | `get< Service >() const` | 获取服务实例 |

### 方法详细说明

```cpp
auto& services = registry.services();
services.emplace<MyProjectService>(project_path);

if (services.has<MyProjectService>()) {
    auto& project = services.get<MyProjectService>();
}
```

## 类名: `luna::PluginRegistry`

- **继承/实现**: 无
- **简述**: 插件注册阶段的核心扩展入口。

### 属性 (Properties)

| 类型 | 名称 | 默认值 | 描述 |
| --- | --- | --- | --- |
| `bool` | `m_imgui_requested` | `false` | 是否有插件请求启用 ImGui |
| `bool` | `m_enable_multi_viewport` | `false` | 是否请求启用多视口 |
| `std::vector<LayerContribution>` | `m_layers` | 空 | Layer / Overlay 贡献列表 |

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `ServiceRegistry&` | `services()` | 获取服务容器 |
| `bool` | `hasEditorRegistry() const` | 判断当前宿主是否提供 editor registry |
| `void` | `requestImGui(bool enable_multi_viewport = false)` | 请求启用 ImGui |
| `bool` | `isImGuiRequested() const` | 查询 ImGui 请求状态 |
| `bool` | `requestsMultiViewport() const` | 查询多视口请求状态 |
| `editor::EditorRegistry&` | `editor()` | 获取 editor registry |
| `void` | `addLayer(std::string id, LayerFactory factory)` | 注册普通层 |
| `void` | `addOverlay(std::string id, LayerFactory factory)` | 注册 overlay |
| `const std::vector<LayerContribution>&` | `layers() const` | 获取当前所有 layer 贡献 |

### 方法详细说明

#### `requestImGui()`

```cpp
extern "C" void luna_register_my_plugin(luna::PluginRegistry& registry)
{
    registry.requestImGui();
}
```

#### 当前能力边界

`PluginRegistry` 当前主要负责:

- Layer / Overlay
- ImGui request
- editor registry access
- service access

它当前**不负责**:

- RenderGraph builder 注入
- RenderPass registry
- Asset importer registry

## 类名: `luna::editor::EditorRegistry`

- **继承/实现**: 无
- **简述**: editor shell 的 panel 与 command 注册表。

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `void` | `addPanel(std::string id, std::string display_name, PanelFactory factory, bool open_by_default = true)` | 注册 panel |
| `void` | `addPanel<PanelT>(std::string id, std::string display_name, bool open_by_default = true)` | 注册默认构造 panel |
| `void` | `addCommand(std::string id, std::string display_name, CommandCallback callback)` | 注册命令 |
| `bool` | `invokeCommand(const std::string& id) const` | 触发命令 |
| `const std::vector<PanelRegistration>&` | `panels() const` | 获取 panel 定义 |
| `const std::vector<CommandRegistration>&` | `commands() const` | 获取命令定义 |

### 方法详细说明

```cpp
registry.editor().addPanel<MyPanel>("my.panel", "My Panel", true);
registry.editor().addCommand("my.reset", "Reset Something", [] {
    // do reset
});
```

## 类名: `luna::editor::EditorPanel`

- **继承/实现**: 无
- **简述**: editor panel 的最小生命周期接口。

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `void` | `onAttach()` | 面板创建后调用 |
| `void` | `onDetach()` | 面板销毁前调用 |
| `void` | `onImGuiRender()` | 面板内容绘制 |

## 类名: `luna::editor::EditorShellLayer`

- **继承/实现**: `luna::Layer`
- **简述**: 把 `EditorRegistry` 中注册的 panel 与 command 变成实际 UI 的承载层。

### 属性 (Properties)

| 类型 | 名称 | 默认值 | 描述 |
| --- | --- | --- | --- |
| `EditorRegistry&` | `m_registry` | 构造传入 | 当前 editor registry |
| `std::vector<PanelInstance>` | `m_panels` | 空 | 当前已实例化 panel 列表 |

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `void` | `onAttach()` | 根据 registry 创建 panel 实例 |
| `void` | `onDetach()` | 释放 panel 实例 |
| `void` | `onImGuiRender()` | 渲染主菜单栏与所有已打开 panel |

## 类名: `luna::VulkanRenderer`

- **继承/实现**: 无
- **简述**: 渲染入口对象，持有 `VulkanContext`、活动 `RenderGraph`、默认 `SceneRenderer` 与主摄像机。

### 属性 (Properties)

| 类型 | 名称 | 默认值 | 描述 |
| --- | --- | --- | --- |
| `SceneRenderer` | `m_scene_renderer` | 默认构造 | 默认 scene 渲染路径组织器 |
| `Camera` | `m_main_camera` | 默认构造 | 当前主摄像机 |
| `glm::vec4` | `m_clear_color` | `(0.10, 0.10, 0.12, 1.0)` | 当前 clear color |
| `bool` | `m_imgui_enabled` | `false` | 是否启用 ImGui pass |
| `bool` | `m_resize_requested` | `false` | 是否等待处理 resize |
| `bool` | `m_render_graph_rebuild_requested` | `false` | 是否等待重建默认 RenderGraph |

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `bool` | `init(Window& window, InitializationOptions options = {})` | 初始化 renderer |
| `void` | `shutdown()` | 销毁 renderer 资源 |
| `bool` | `isInitialized() const` | 查询初始化状态 |
| `bool` | `isRenderingEnabled() const` | 查询当前帧是否可渲染 |
| `bool` | `isImGuiEnabled() const` | 查询是否启用 ImGui pass |
| `void` | `requestResize()` | 请求下一帧处理 resize |
| `void` | `requestRenderGraphRebuild()` | 请求下一帧重建当前默认 RenderGraph |
| `bool` | `isResizeRequested() const` | 查询 resize 标记 |
| `void` | `setImGuiEnabled(bool enabled)` | 打开或关闭 ImGui pass |
| `void` | `startFrame()` | 开始一帧 |
| `void` | `renderFrame()` | 执行 RenderGraph |
| `void` | `endFrame()` | 结束一帧 |
| `GLFWwindow*` | `getNativeWindow() const` | 获取原生窗口句柄 |
| `const vk::RenderPass&` | `getImGuiRenderPass() const` | 获取 ImGui render pass |
| `Camera&` | `getMainCamera()` | 获取主摄像机 |
| `const Camera&` | `getMainCamera() const` | 获取只读主摄像机 |
| `SceneRenderer&` | `getSceneRenderer()` | 获取默认 scene 渲染路径组织器 |
| `const SceneRenderer&` | `getSceneRenderer() const` | 获取只读 scene 渲染路径组织器 |
| `glm::vec4&` | `getClearColor()` | 获取 clear color |
| `const glm::vec4&` | `getClearColor() const` | 获取只读 clear color |

### 方法详细说明

#### `InitializationOptions`

`VulkanRenderer::InitializationOptions` 当前最重要的字段是:

| 类型 | 名称 | 描述 |
| --- | --- | --- |
| `RenderGraphBuilderCallback` | `m_render_graph_builder` | 在初始化阶段自定义活动 RenderGraph |

#### 默认 RenderGraph 行为

如果没有提供自定义 `m_render_graph_builder`，`VulkanRenderer` 会把默认图构建委托给 `SceneRenderer::buildRenderGraph()`。

正常情况下默认图是:

- `scene_geometry`
- `scene_lighting`
- optional `imgui`
- present

如果 `SceneRenderer` 的 scene shader 或核心资源没有成功初始化，则会回退到:

- `scene_clear`
- optional `imgui`
- present

#### `requestRenderGraphRebuild()`

当你修改会影响默认 scene 图资源解析的配置时，应显式请求重建:

```cpp
auto& renderer = luna::Application::get().getRenderer();
renderer.getSceneRenderer().setShaderPaths(shader_paths);
renderer.requestRenderGraphRebuild();
```

#### `renderFrame()`

```cpp
auto& commands = m_context->GetCurrentCommandBuffer();
m_render_graph->Execute(commands);
m_render_graph->Present(
    commands,
    m_context->AcquireCurrentSwapchainImage(luna::val::ImageUsage::TRANSFER_DISTINATION));
```

#### 重要说明

当前 `VulkanRenderer` 对“提供全新 RenderGraph builder”的正式入口仍然是初始化阶段。  
插件现在可以通过 `SceneRenderer` 影响默认 scene 渲染路径，但仍然不能正式替换 `RenderGraphBuilderCallback`。

## 类名: `luna::SceneRenderer`

- **继承/实现**: 无
- **简述**: 默认 scene 渲染路径组织器，负责收集静态网格提交、缓存已上传 mesh/material，并构建默认 RenderGraph。

### 结构体: `ShaderPaths`

| 类型 | 名称 | 描述 |
| --- | --- | --- |
| `std::filesystem::path` | `geometry_vertex_path` | geometry pass 顶点 shader 路径 |
| `std::filesystem::path` | `geometry_fragment_path` | geometry pass 片元 shader 路径 |
| `std::filesystem::path` | `lighting_vertex_path` | lighting pass 顶点 shader 路径 |
| `std::filesystem::path` | `lighting_fragment_path` | lighting pass 片元 shader 路径 |

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `void` | `shutdown()` | 销毁缓存的 scene 资源与上传状态 |
| `void` | `beginScene(const Camera& camera)` | 记录当前场景相机并清空本帧提交列表 |
| `void` | `clearSubmittedMeshes()` | 清空当前帧静态网格提交列表 |
| `void` | `setShaderPaths(ShaderPaths shader_paths)` | 覆盖默认 scene shader 路径并使核心资源失效 |
| `void` | `submitStaticMesh(const glm::mat4& transform, std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material = {})` | 提交一个静态网格绘制命令 |
| `std::unique_ptr<val::RenderGraph>` | `buildRenderGraph(val::Format surface_format, uint32_t framebuffer_width, uint32_t framebuffer_height, bool include_imgui_pass)` | 构建默认 scene RenderGraph |

### 方法详细说明

#### `setShaderPaths()`

`setShaderPaths()` 只会更新 `SceneRenderer` 内部记录的 shader 路径，并使已加载 shader / 核心资源失效。  
如果 renderer 已经在运行，你通常还要再调用 `VulkanRenderer::requestRenderGraphRebuild()`，让活动图重新根据新路径构建。

#### `submitStaticMesh()`

`submitStaticMesh()` 会忽略空 mesh 或无效 mesh。  
当前 `Scene::onUpdateRuntime()` 就是通过这条 API，把 `TransformComponent + StaticMeshComponent` 组合转成绘制提交。

#### `buildRenderGraph()`

正常情况下会构建:

- `scene_geometry`
- `scene_lighting`
- optional `imgui`
- present

如果 scene shader 不完整或加载失败，则会回退到只输出 `scene_color` 的 `scene_clear` 图，保证宿主仍然能显示一个有效画面。

## 类名: `luna::Scene`

- **继承/实现**: 无
- **简述**: 基于 `entt::registry` 的最小 runtime 场景容器，负责 entity 生命周期与运行时静态网格提交流程。

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `Entity` | `createEntity(const std::string& name = std::string{})` | 创建一个带默认组件的新实体 |
| `Entity` | `createEntityWithUUID(UUID uuid, const std::string& name = std::string{})` | 创建一个指定 UUID 的实体 |
| `void` | `destroyEntity(Entity entity)` | 销毁属于当前 `Scene` 的实体 |
| `void` | `onUpdateRuntime()` | 遍历可见静态网格并提交给 `SceneRenderer` |

### 方法详细说明

#### `createEntity()`

当前会为新实体自动添加:

- `IDComponent`
- `TagComponent`
- `TransformComponent`

如果没有提供名称，默认 tag 为 `"Entity"`。

#### `onUpdateRuntime()`

`onUpdateRuntime()` 会:

1. 从 `Application::get().getRenderer()` 获取 renderer 和主摄像机。
2. 调用 `SceneRenderer::beginScene()` 开始本帧 scene 提交。
3. 遍历所有同时拥有 `TransformComponent` 与 `StaticMeshComponent` 的实体。
4. 跳过 `visible == false` 或 mesh 为空的实体。
5. 调用 `submitStaticMesh()` 提交最终绘制命令。

## 类名: `luna::Entity`

- **继承/实现**: 无
- **简述**: 指向特定 `Scene` 中某个 `entt::entity` 的轻量句柄，负责封装组件访问与基本标识读取。

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `T&` | `addComponent<T>(Args&&... args)` | 为当前实体添加组件 |
| `T&` | `getComponent<T>()` | 获取可写组件引用 |
| `const T&` | `getComponent<T>() const` | 获取只读组件引用 |
| `bool` | `hasComponent<T>() const` | 判断实体是否拥有某组件 |
| `void` | `removeComponent<T>()` | 如果存在则移除组件 |
| `UUID` | `getUUID() const` | 读取 `IDComponent` 中的 UUID |
| `const std::string&` | `getName() const` | 读取 `TagComponent` 中的名称 |

### 额外行为

`Entity` 还提供:

- `bool` 转换，用于判断句柄是否有效
- 到 `entt::entity` 与 `uint32_t` 的转换
- 基于实体句柄和所属 `Scene` 的相等性比较

## 核心 Scene 组件

| 组件 | 当前字段 / 能力 | 作用 |
| --- | --- | --- |
| `IDComponent` | `UUID id` | 提供稳定实体标识 |
| `TagComponent` | `std::string tag` | 提供名称 |
| `TransformComponent` | `translation`、`rotation`、`scale`、`getTransform()` | 提供局部变换并生成模型矩阵 |
| `StaticMeshComponent` | `mesh`、`material`、`visible` | 声明运行时静态网格与材质引用 |

## 类名: `luna::val::RenderGraphBuilder`

- **继承/实现**: 无
- **简述**: 根据 pass 声明构建 `RenderGraph` 的构建器。

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `RenderGraphBuilder&` | `AddRenderPass(const std::string& name, std::unique_ptr<RenderPass> renderPass)` | 注册 pass |
| `RenderGraphBuilder&` | `SetOutputName(const std::string& name)` | 指定最终输出附件 |
| `std::unique_ptr<RenderGraph>` | `Build()` | 构建可执行图 |

### 方法详细说明

```cpp
luna::val::RenderGraphBuilder builder;
builder
    .AddRenderPass("main", std::make_unique<MyPass>())
    .AddRenderPass("imgui", std::make_unique<luna::val::ImGuiRenderPass>("scene_color"))
    .SetOutputName("scene_color");

auto graph = builder.Build();
```

## 类名: `luna::val::RenderGraph`

- **继承/实现**: 无
- **简述**: 已完成构建的渲染执行图。

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `void` | `Execute(CommandBuffer& commandBuffer)` | 执行所有图节点 |
| `void` | `Present(CommandBuffer& commandBuffer, const Image& presentImage)` | 把输出附件呈现到交换链图像 |
| `const RenderGraphNode&` | `GetNodeByName(const std::string& name) const` | 按名称获取节点 |
| `RenderGraphNode&` | `GetNodeByName(const std::string& name)` | 按名称获取可写节点 |
| `const Image&` | `GetAttachmentByName(const std::string& name) const` | 获取命名附件 |

## 类名: `luna::val::RenderPass`

- **继承/实现**: 抽象基类
- **简述**: RenderGraph 中单个渲染/计算通道的逻辑接口。

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `void` | `SetupPipeline(PipelineState state)` | 声明附件、依赖、shader、descriptor |
| `void` | `ResolveResources(ResolveState resolve)` | 解析命名资源 |
| `void` | `BeforeRender(RenderPassState state)` | pass 开始前钩子 |
| `void` | `OnRender(RenderPassState state)` | 真正记录绘制或计算命令 |
| `void` | `AfterRender(RenderPassState state)` | pass 结束后钩子 |

### 方法详细说明

```cpp
class MyPass final : public luna::val::RenderPass {
public:
    void SetupPipeline(luna::val::PipelineState pipeline) override
    {
        pipeline.DeclareAttachment("scene_color", luna::val::Format::R8G8B8A8_UNORM, 0, 0);
        pipeline.AddOutputAttachment("scene_color", luna::val::ClearColor{0, 0, 0, 1});
    }
};
```

## 类名: `luna::val::VulkanContext`

- **继承/实现**: 无
- **简述**: Vulkan 实例、逻辑设备、交换链、命令池、descriptor cache 与虚拟帧的中心对象。

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `void` | `InitializeContext(const WindowSurface& surface, const ContextInitializeOptions& options)` | 创建设备与交换链 |
| `void` | `RecreateSwapchain(uint32_t surfaceWidth, uint32_t surfaceHeight)` | 重建交换链 |
| `void` | `StartFrame()` | 开始当前虚拟帧 |
| `void` | `EndFrame()` | 提交并结束当前虚拟帧 |
| `bool` | `IsRenderingEnabled() const` | 查询渲染开关 |
| `const Image&` | `AcquireCurrentSwapchainImage(ImageUsage::Bits usage)` | 获取当前交换链图像 |
| `CommandBuffer&` | `GetCurrentCommandBuffer()` | 获取当前命令缓冲 |
| `StageBuffer&` | `GetCurrentStageBuffer()` | 获取当前 stage buffer |
| `DescriptorCache&` | `GetDescriptorCache()` | 获取 descriptor cache |
| `const vk::Device&` | `GetDevice() const` | 获取逻辑设备 |
| `uint32_t` | `GetAPIVersion() const` | 获取 Vulkan API 版本 |

## 类名: `luna::TaskSystem`

- **继承/实现**: 无
- **简述**: 基于 enkiTS 的任务系统包装器，支持 worker / IO / main-thread 目标和任务依赖。

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `bool` | `initialize(const TaskSystemConfig& config = {})` | 初始化任务系统 |
| `void` | `shutdown()` | 关闭任务系统 |
| `bool` | `isInitialized() const` | 查询状态 |
| `TaskHandle` | `submit(std::function<void()> function, TaskSubmitDesc desc = {}, std::initializer_list<TaskHandle> dependencies = {})` | 提交普通任务 |
| `TaskHandle` | `submitParallel(std::function<void(enki::TaskSetPartition, uint32_t)> function, TaskSubmitDesc desc, std::initializer_list<TaskHandle> dependencies = {})` | 提交并行任务 |
| `TaskHandle` | `whenAll(std::initializer_list<TaskHandle> dependencies)` | 组合依赖任务 |
| `void` | `waitForAll()` | 等待所有任务完成 |
| `void` | `waitForTask(const enki::ICompletable* task)` | 等待底层 enki task |
| `bool` | `submitIOJob(enki::IPinnedTask* task)` | 提交 IO pinned job |
| `bool` | `submitMainThreadJob(enki::IPinnedTask* task)` | 提交主线程 pinned job |
| `ExternalThreadScope` | `registerExternalThread()` | 为外部线程注册 task system 上下文 |

### 方法详细说明

```cpp
auto& tasks = luna::Application::get().getTaskSystem();

auto load = tasks.submit([] {
    // background work
});

auto finalize = load.then(tasks, [] {
    // execute after load
});

finalize.wait(tasks);
```

## 类名: `luna::TaskHandle`

- **继承/实现**: 无
- **简述**: 代表一个可等待、可组合的异步任务句柄。

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `bool` | `isValid() const` | 句柄是否有效 |
| `bool` | `isComplete() const` | 是否已完成 |
| `bool` | `hasFailed() const` | 是否失败 |
| `TaskStatus` | `status() const` | 查询状态 |
| `void` | `wait(TaskSystem& task_system) const` | 阻塞等待 |
| `TaskHandle` | `then(TaskSystem& task_system, std::function<void()> function, TaskSubmitDesc desc = {}) const` | 在当前任务之后再提交一个任务 |
| `TaskHandle` | `thenParallel(TaskSystem& task_system, std::function<void(enki::TaskSetPartition, uint32_t)> function, TaskSubmitDesc desc) const` | 在当前任务之后提交并行任务 |

## 当前 API 参考的使用建议

### 什么时候看 Reference，什么时候看 Manual

| 需求 | 更适合看哪里 |
| --- | --- |
| 想知道整体链路与职责边界 | `manual/` |
| 想知道某个类有哪些方法 | `reference/` |
| 想知道当前插件能不能做某件事 | `plugins/` |

### 当前最重要的 API 事实

- `Application` 是宿主骨架
- `PluginRegistry` 是当前插件正式扩展入口
- `EditorRegistry` 是 editor 扩展点容器
- `VulkanRenderer` 是宿主控制的渲染器
- `RenderGraphBuilder` 是当前自定义渲染图的正式入口，但主要由宿主使用
