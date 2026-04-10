# 第五部分: 类与 API 参考手册

> **提示 (Note):**
> 本节聚焦核心类，而不是机械罗列所有类型。属性表优先列出外部开发者真正需要知道的公有状态或概念状态。

## 类名: `luna::Application`

- **继承/实现**: 无
- **简述**: Luna 的应用宿主，负责窗口、事件、主循环、ImGui 与渲染器生命周期。

### 属性 (Properties)

| 类型 | 名称 | 默认值 | 描述 |
| --- | --- | --- | --- |
| `ApplicationSpecification` | `m_specification` | `{ name=Luna, 1600x900 }` | 应用名、窗口大小、多视口等配置 |
| `bool` | `m_initialized` | `false` | 初始化是否成功 |
| `bool` | `m_running` | `true` | 主循环是否继续 |
| `VulkanRenderer` | `m_renderer` | 内部构造 | 当前应用绑定的渲染器 |

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `void` | `pushLayer(std::unique_ptr<Layer> layer)` | 压入普通层并调用 `onAttach()` |
| `void` | `pushOverlay(std::unique_ptr<Layer> overlay)` | 压入覆盖层并调用 `onAttach()` |
| `void` | `run()` | 启动主循环 |
| `void` | `close()` | 请求关闭应用 |
| `ImGuiLayer*` | `getImGuiLayer() const` | 获取 ImGui 层 |
| `VulkanRenderer&` | `getRenderer()` | 获取渲染器 |
| `bool` | `isInitialized() const` | 判断初始化是否成功 |
| `static Application&` | `get()` | 获取全局应用实例 |

### 方法详细说明

#### `run()`

这是模板方法模式的核心实现。它完成:

1. 调用 `onInit()`
2. 进入 while 主循环
3. 计算 `Timestep`
4. 依次调用应用钩子和各层更新
5. 开启 ImGui 帧
6. 驱动渲染器执行一帧
7. 收尾时调用 `onShutdown()`

```cpp
class MyApp final : public luna::Application {
public:
    MyApp() : Application({ .m_name = "My App" }) {}

protected:
    void onInit() override {
        pushLayer(std::make_unique<MyLayer>());
    }
};
```

## 类名: `luna::Layer`

- **继承/实现**: 无
- **简述**: 业务逻辑层的统一接口。

### 属性 (Properties)

| 类型 | 名称 | 默认值 | 描述 |
| --- | --- | --- | --- |
| `std::string` | `m_name` | `"Layer"` | 当前 Layer 的逻辑名称 |

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `void` | `onAttach()` | 层被挂载时调用 |
| `void` | `onDetach()` | 层被移除时调用 |
| `void` | `onUpdate(Timestep dt)` | 每帧更新 |
| `void` | `onEvent(Event& event)` | 事件回调 |
| `void` | `onImGuiRender()` | ImGui 绘制 |
| `void` | `onRender()` | 渲染阶段逻辑 |
| `const std::string&` | `getName() const` | 获取名称 |

### 方法详细说明

#### `onImGuiRender()`

用于绘制本层的编辑器或调试 UI:

```cpp
void onImGuiRender() override {
    ImGui::Begin("Stats");
    ImGui::TextUnformatted("Hello from a custom layer.");
    ImGui::End();
}
```

## 类名: `luna::VulkanRenderer`

- **继承/实现**: 无
- **简述**: 引擎侧渲染入口，负责 Vulkan 上下文初始化、RenderGraph 构建与每帧执行。

### 属性 (Properties)

| 类型 | 名称 | 默认值 | 描述 |
| --- | --- | --- | --- |
| `Window*` | `m_window` | `nullptr` | 绑定窗口 |
| `GLFWwindow*` | `m_native_window` | `nullptr` | 原生 GLFW 句柄 |
| `std::unique_ptr<VulkanContext>` | `m_context` | `nullptr` | Vulkan 上下文 |
| `std::unique_ptr<RenderGraph>` | `m_render_graph` | `nullptr` | 当前渲染图 |
| `Camera` | `m_main_camera` | 默认构造 | 主摄像机 |
| `glm::vec4` | `m_clear_color` | `(0.10, 0.10, 0.12, 1.0)` | 清屏颜色 |

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `bool` | `init(Window& window)` | 初始化渲染器 |
| `void` | `shutdown()` | 释放渲染器资源 |
| `void` | `requestResize()` | 请求交换链重建 |
| `void` | `startFrame()` | 启动一帧 |
| `void` | `renderFrame()` | 执行 RenderGraph 并 present |
| `void` | `endFrame()` | 结束一帧 |
| `bool` | `isInitialized() const` | 判断渲染器是否初始化 |
| `bool` | `isRenderingEnabled() const` | 判断当前帧是否可渲染 |
| `Camera&` | `getMainCamera()` | 访问主摄像机 |
| `glm::vec4&` | `getClearColor()` | 访问清屏颜色 |

### 方法详细说明

#### `init(Window& window)`

该方法会:

1. 查询 GLFW 所需 Vulkan 实例扩展
2. 创建 `VulkanContext`
3. 创建窗口 Surface
4. 初始化虚拟帧和交换链
5. 设置主摄像机默认位置
6. 构建默认 RenderGraph

#### `renderFrame()`

当前实现会执行整张图并把输出图像 blit 到交换链图像:

```cpp
auto& commands = m_context->GetCurrentCommandBuffer();
m_render_graph->Execute(commands);
m_render_graph->Present(
    commands,
    m_context->AcquireCurrentSwapchainImage(luna::val::ImageUsage::TRANSFER_DISTINATION));
```

## 类名: `luna::val::RenderGraphBuilder`

- **继承/实现**: 无
- **简述**: 按声明式方式构建 RenderGraph，并自动推导附件分配和资源屏障。

### 属性 (Properties)

| 类型 | 名称 | 默认值 | 描述 |
| --- | --- | --- | --- |
| `std::vector<RenderPassReference>` | `renderPassReferences` | 空 | Pass 注册表 |
| `std::string` | `outputName` | 空字符串 | 最终输出附件名 |

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `RenderGraphBuilder&` | `AddRenderPass(const std::string& name, std::unique_ptr<RenderPass> renderPass)` | 注册一个 pass |
| `RenderGraphBuilder&` | `SetOutputName(const std::string& name)` | 指定最终输出附件 |
| `std::unique_ptr<RenderGraph>` | `Build()` | 构建执行图 |

### 方法详细说明

#### `Build()`

这是渲染架构的关键入口。它会:

1. 调用每个 pass 的 `SetupPipeline()`
2. 收集 descriptor 依赖并补全资源依赖
3. 计算所有资源的初始/最终 usage
4. 分配附件图像
5. 构建原生 RenderPass、Framebuffer、Pipeline
6. 生成 pipeline barrier 回调
7. 产出 `RenderGraph`

## 类名: `luna::val::RenderGraph`

- **继承/实现**: 无
- **简述**: 执行已经构建完成的图，串联每个节点的资源解析、屏障、pass 执行与呈现。

### 属性 (Properties)

| 类型 | 名称 | 默认值 | 描述 |
| --- | --- | --- | --- |
| `std::vector<RenderGraphNode>` | `nodes` | 构造传入 | 图节点 |
| `std::unordered_map<std::string, Image>` | `attachments` | 构造传入 | 命名附件表 |
| `std::string` | `outputName` | 构造传入 | 最终输出附件 |

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `void` | `Execute(CommandBuffer& commandBuffer)` | 执行所有 pass |
| `void` | `Present(CommandBuffer& commandBuffer, const Image& presentImage)` | 执行最终呈现 |
| `const RenderGraphNode&` | `GetNodeByName(const std::string& name) const` | 按名称取节点 |
| `const Image&` | `GetAttachmentByName(const std::string& name) const` | 按名称取附件 |

## 类名: `luna::val::VulkanContext`

- **继承/实现**: 无
- **简述**: Vulkan 实例、设备、交换链、队列、描述符池、虚拟帧和即时命令提交的中心对象。

### 属性 (Properties)

| 类型 | 名称 | 默认值 | 描述 |
| --- | --- | --- | --- |
| `vk::Instance` | `instance` | 空 | Vulkan 实例 |
| `vk::Device` | `device` | 空 | 逻辑设备 |
| `vk::SwapchainKHR` | `swapchain` | 空 | 交换链 |
| `DescriptorCache` | `descriptorCache` | 默认构造 | descriptor 池与布局分配器 |
| `VirtualFrameProvider` | `virtualFrames` | 默认构造 | 多帧并行资源容器 |

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `void` | `InitializeContext(const WindowSurface& surface, const ContextInitializeOptions& options)` | 完成设备与交换链初始化 |
| `void` | `RecreateSwapchain(uint32_t surfaceWidth, uint32_t surfaceHeight)` | 重建交换链 |
| `void` | `StartFrame()` | 开始虚拟帧 |
| `void` | `EndFrame()` | 结束虚拟帧 |
| `CommandBuffer&` | `GetCurrentCommandBuffer()` | 获取当前帧命令缓冲 |
| `StageBuffer&` | `GetCurrentStageBuffer()` | 获取当前帧暂存缓冲 |
| `const Image&` | `AcquireCurrentSwapchainImage(ImageUsage::Bits usage)` | 获取当前交换链图像 |
| `DescriptorCache&` | `GetDescriptorCache()` | 获取 descriptor 缓存 |

## 类名: `luna::val::RenderPass`

- **继承/实现**: 基类接口
- **简述**: 声明渲染/计算通道行为的抽象接口。

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `void` | `SetupPipeline(PipelineState state)` | 声明附件、依赖、Shader、descriptor |
| `void` | `ResolveResources(ResolveState resolve)` | 将逻辑资源名解析为运行时资源 |
| `void` | `BeforeRender(RenderPassState state)` | 开始 pass 前钩子 |
| `void` | `OnRender(RenderPassState state)` | 真正记录绘制/Dispatch 命令 |
| `void` | `AfterRender(RenderPassState state)` | pass 收尾钩子 |

### 方法详细说明

```cpp
void SetupPipeline(PipelineState pipeline) override {
    pipeline.DeclareAttachment("scene_color", Format::R8G8B8A8_UNORM, 0, 0);
    pipeline.AddOutputAttachment("scene_color", ClearColor{0, 0, 0, 1});
}
```

## 类名: `luna::val::CommandBuffer`

- **继承/实现**: 无
- **简述**: 对原生 `vk::CommandBuffer` 的轻量封装。

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `void` | `Begin()` | 开始记录 |
| `void` | `End()` | 结束记录 |
| `void` | `BeginPass(const PassNative& renderPass)` | 开始 render pass |
| `void` | `EndPass(const PassNative& renderPass)` | 结束 render pass |
| `void` | `Draw(...)` | 非索引绘制 |
| `void` | `DrawIndexed(...)` | 索引绘制 |
| `void` | `Dispatch(uint32_t x, uint32_t y, uint32_t z)` | 计算着色器调度 |
| `void` | `CopyBuffer(...)` | Buffer 拷贝 |
| `void` | `CopyBufferToImage(...)` | 上传图片 |
| `void` | `BlitImage(...)` | 图像 blit |
| `void` | `GenerateMipLevels(...)` | 生成 mipmap |
| `void` | `TransferLayout(...)` | 图像 layout 变更 |

## 类名: `luna::val::DescriptorBinding`

- **继承/实现**: 无
- **简述**: 把资源名字和 descriptor binding 规则连接起来，并在每帧写入 descriptor set。

### 属性 (Properties)

| 类型 | 名称 | 默认值 | 描述 |
| --- | --- | --- | --- |
| `ResolveOptions` | `options` | `RESOLVE_EACH_FRAME` | descriptor 写入策略 |

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `DescriptorBinding&` | `Bind(uint32_t binding, const std::string& name, UniformType type)` | 绑定命名 Buffer 或 Image |
| `DescriptorBinding&` | `Bind(uint32_t binding, const std::string& name, const Sampler& sampler, UniformType type)` | 绑定命名 Image + Sampler |
| `DescriptorBinding&` | `Bind(uint32_t binding, const Sampler& sampler, UniformType type)` | 绑定纯 Sampler |
| `void` | `SetOptions(ResolveOptions options)` | 设置 descriptor 写入策略 |
| `void` | `Resolve(const ResolveInfo& resolveInfo)` | 解析命名资源 |
| `void` | `Write(const vk::DescriptorSet& descriptorSet)` | 把解析结果写入 descriptor set |

### 方法详细说明

```cpp
binding.Bind(0, "scene_color", UniformType::SAMPLED_IMAGE);
resolve.Resolve("scene_color", image);
binding.Resolve(resolve);
binding.Write(descriptorSet);
```

## 类名: `luna::val::ShaderLoader`

- **继承/实现**: 无
- **简述**: 负责着色器编译、SPIR-V 读取和反射。

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `ShaderData` | `LoadFromSourceFile(const std::string& filepath, ShaderType type, ShaderLanguage language)` | 从源码文件编译并反射 |
| `ShaderData` | `LoadFromBinaryFile(const std::string& filepath)` | 从 SPIR-V 文件读取并反射 |
| `ShaderData` | `LoadFromBinary(std::vector<uint32_t> bytecode)` | 从内存中的 SPIR-V 反射 |
| `ShaderData` | `LoadFromSource(const std::string& code, ShaderType type, ShaderLanguage language)` | 从源码字符串编译并反射 |

## 类名: `luna::val::ModelLoader`

- **继承/实现**: 无
- **简述**: 负责 OBJ 与 glTF/GLB 模型导入。

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `ModelData` | `LoadFromObj(const std::string& filepath)` | 导入 OBJ |
| `ModelData` | `LoadFromGltf(const std::string& filepath)` | 导入 glTF/GLB |
| `ModelData` | `Load(const std::string& filepath)` | 按扩展名自动选择导入器 |

## 类名: `luna::val::ImageLoader`

- **继承/实现**: 无
- **简述**: 负责普通图片、DDS、zlib 包装 DDS 以及立方体贴图读取。

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `ImageData` | `LoadImageFromFile(const std::string& filepath)` | 从文件加载图片 |
| `ImageData` | `LoadImageFromMemory(const uint8_t* data, std::size_t size, const std::string& mimeType = {})` | 从内存加载图片 |
| `CubemapData` | `LoadCubemapImageFromFile(const std::string& filepath)` | 从单张十字排布图片生成 cubemap |

## 类名: `luna::ImGuiLayer`

- **继承/实现**: `luna::Layer`
- **简述**: 管理 ImGui 生命周期、DockSpace、多视口以及事件拦截。

### 属性 (Properties)

| 类型 | 名称 | 默认值 | 描述 |
| --- | --- | --- | --- |
| `bool` | `m_block_events` | `true` | 是否阻止 ImGui 捕获到的事件继续下发 |
| `bool` | `m_attached` | `false` | ImGui 是否已初始化 |
| `bool` | `m_enable_multi_viewport` | `false` | 是否启用多视口 |

### 方法 (Methods)

| 返回类型 | 方法名(参数列表) | 描述 |
| --- | --- | --- |
| `void` | `onAttach()` | 初始化 ImGui |
| `void` | `onDetach()` | 销毁 ImGui |
| `void` | `onEvent(Event& event)` | 处理事件捕获 |
| `void` | `begin()` | 开启 ImGui 帧 |
| `void` | `end()` | 结束逻辑帧 |
| `void` | `renderPlatformWindows()` | 渲染平台窗口 |
| `void` | `blockEvents(bool block)` | 开关事件拦截 |
| `bool` | `isInitialized() const` | 判断初始化状态 |
