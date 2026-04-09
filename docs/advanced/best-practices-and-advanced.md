# 第六部分: 最佳实践与进阶

## 性能优化指南

### 1. 理解当前帧模型

Luna 使用 `VirtualFrameProvider` 管理多帧资源。每个虚拟帧都拥有:

- 一个 `CommandBuffer`
- 一个 `StageBuffer`
- 一个 image-available semaphore
- 一个 command-queue fence

### 2. 重点性能陷阱

| 陷阱 | 原因 | 建议 |
| --- | --- | --- |
| 每帧重建 RenderGraph | 会重建 pass 原生对象与附件 | 仅在 resize 或图结构变化时重建 |
| `device.waitIdle()` 使用过多 | 会打断 GPU 并行性 | 除 resize/销毁外尽量避免 |
| `DescriptorCache::GetDescriptor()` 频繁调用 | 当前实现每次都会新建 layout 和 set | 为长期存在的 shader/pipeline 缓存 descriptor 结果 |
| 大资源反复通过 `StageBuffer` 上传 | 每帧 staging 容量有限 | 静态资源应在加载期上传到 GPU 常驻资源 |
| 运行时频繁从源码编译 shader | glslang 编译成本高 | 发布环境优先使用 `.spv` |

> **警告 (Warning):**
> 当前 `GraphicShader` 与 `ComputeShader` 中都通过 `assert(...DescriptorSets.size() < 2)` 限制了只支持单 descriptor set。复杂材质系统落地前，先解决这一点。

## 二次开发指南

### 扩展原则 1: 新业务优先写成 Layer

适用于:

- 编辑器面板
- 输入交互
- 调试显示
- 场景逻辑

### 扩展原则 2: 新 GPU 功能优先写成 RenderPass

适用于:

- 阴影
- GBuffer
- 后处理
- 离屏渲染

```cpp
class ShadowPass final : public VulkanAbstractionLayer::RenderPass {
public:
    void SetupPipeline(PipelineState pipeline) override;
    void ResolveResources(ResolveState resolve) override;
    void OnRender(RenderPassState state) override;
};
```

### 扩展原则 3: 资源导入和 GPU 上传分两步

建议保持:

1. CPU 侧导入: `ModelLoader` / `ImageLoader` / `ShaderLoader`
2. GPU 侧上传: `Buffer` / `Image` / `StageBuffer`

### 扩展原则 4: 统一命名资源

`ResolveInfo` 的价值就在于“pass 只认识逻辑名，不认识句柄来源”。继续扩展时，尽量坚持:

- pass 内使用逻辑资源名
- 图装配阶段注入真实资源

## 推荐扩展路线

### 路线 A: 把当前编辑器做成真正场景预览器

1. 新增 mesh pass
2. 用 `ModelLoader` 导入 `assets/basicmesh.glb`
3. 用 `ImageLoader` 上传材质纹理
4. 将 `Camera::getViewMatrix()` 接入 uniform buffer
5. 把输出继续串给 ImGui pass

### 路线 B: 做渲染框架强化

1. 支持多 descriptor set
2. 为 `DescriptorCache` 增加 layout 复用
3. 引入 pipeline cache
4. 缓存 RenderGraph 节点构建结果

### 路线 C: 做编辑器能力增强

1. 为 `EditorLayer` 拆出多个面板类
2. 加资源浏览器
3. 加场景层级面板
4. 加 shader 重载入口

## 未来重构热点

从当前代码形态看，后续最值得优先投资的点有:

1. 去掉单 descriptor set 限制
2. 收敛 `GetCurrentVulkanContext()` 的全局依赖
3. 给 `DescriptorCache` 做真正缓存
4. 让资源导入层真正接入默认渲染路径

## 进阶总结

> 应用层负责调度，Layer 负责业务，RenderPass 负责 GPU 阶段，RenderGraph 负责同步，VulkanContext 负责底层资源。
