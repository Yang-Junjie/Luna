#ifndef CACAO_EASY_H
#define CACAO_EASY_H

#include "Core.h"
#include "Instance.h"
#include "Device.h"
#include "Swapchain.h"
#include "Pipeline.h"
#include "ShaderCompiler.h"
#include "Synchronization.h"
#include "Builders.h"
#include "Texture.h"
#include "Buffer.h"
#include "DescriptorSet.h"
#include "DescriptorPool.h"
#include "DescriptorSetLayout.h"
#include "PipelineLayout.h"
#include "Queue.h"
#include "CommandBufferEncoder.h"
#include "Adapter.h"

namespace Cacao
{
    struct EasyConfig
    {
        std::string appName = "Cacao App";
        uint32_t width = 1280;
        uint32_t height = 720;
        BackendType backend = BackendType::Auto;
        bool vsync = true;
        bool validation = true;
        NativeWindowHandle windowHandle;
    };

    class CACAO_API EasyContext
    {
    public:
        Ref<Instance> instance;
        Ref<Adapter> adapter;
        Ref<Device> device;
        Ref<Surface> surface;
        Ref<Swapchain> swapchain;
        Ref<Queue> graphicsQueue;
        Ref<Synchronization> sync;
        Ref<ShaderCompiler> compiler;
        DeviceLimits limits;

        uint32_t frameIndex = 0;
        uint32_t imageIndex = 0;
        uint32_t framesInFlight = 2;

        static Ref<EasyContext> Create(const EasyConfig& config)
        {
            auto ctx = std::make_shared<EasyContext>();

            InstanceCreateInfo ici;
            ici.type = config.backend;
            ici.applicationName = config.appName;
            ctx->instance = Instance::Create(ici);

            auto adapters = ctx->instance->EnumerateAdapters();
            if (adapters.empty())
                throw std::runtime_error("No GPU adapters found");
            ctx->adapter = adapters[0];
            ctx->limits = ctx->adapter->QueryLimits();

            DeviceCreateInfo dci;
            dci.QueueRequests = {{QueueType::Graphics, 1, 1.0f}};
            ctx->device = ctx->adapter->CreateDevice(dci);

            ctx->surface = ctx->instance->CreateSurface(config.windowHandle);

            auto scInfo = SwapchainBuilder()
                .SetExtent(config.width, config.height)
                .SetFormat(Format::BGRA8_UNORM)
                .SetPresentMode(config.vsync ? PresentMode::Fifo : PresentMode::Mailbox)
                .SetSurface(ctx->surface)
                .Build();
            ctx->swapchain = ctx->device->CreateSwapchain(scInfo);

            ctx->framesInFlight = std::max(1u, ctx->swapchain->GetImageCount() - 1);
            ctx->sync = ctx->device->CreateSynchronization(ctx->framesInFlight);
            ctx->graphicsQueue = ctx->device->GetQueue(QueueType::Graphics, 0);

            ctx->compiler = ShaderCompiler::Create(ctx->instance->GetType());

            return ctx;
        }

        Ref<CommandBufferEncoder> BeginFrame()
        {
            sync->WaitForFrame(frameIndex);
            int idx;
            swapchain->AcquireNextImage(sync, frameIndex, idx);
            imageIndex = static_cast<uint32_t>(idx);
            sync->ResetFrameFence(frameIndex);

            auto cmd = device->CreateCommandBufferEncoder();
            cmd->Begin();
            return cmd;
        }

        void EndFrame(const Ref<CommandBufferEncoder>& cmd)
        {
            cmd->End();
            graphicsQueue->Submit(cmd, sync, frameIndex);
            swapchain->Present(graphicsQueue, sync, frameIndex);
            frameIndex = (frameIndex + 1) % framesInFlight;
        }

        Ref<ShaderModule> LoadShader(const std::string& path, const std::string& entry, ShaderStage stage)
        {
            ShaderCreateInfo sci;
            sci.SourcePath = path;
            sci.EntryPoint = entry;
            sci.Stage = stage;
            return compiler->CompileOrLoad(device, sci);
        }

        Ref<Buffer> CreateVertexBuffer(uint64_t size, const void* data = nullptr)
        {
            auto buf = device->CreateBuffer(
                BufferBuilder()
                .SetSize(size)
                .SetUsage(BufferUsageFlags::VertexBuffer | BufferUsageFlags::TransferDst)
                .SetMemoryUsage(BufferMemoryUsage::CpuToGpu)
                .Build());
            if (data)
            {
                void* mapped = buf->Map();
                memcpy(mapped, data, size);
                buf->Unmap();
            }
            return buf;
        }

        Ref<Buffer> CreateIndexBuffer(uint64_t size, const void* data = nullptr)
        {
            auto buf = device->CreateBuffer(
                BufferBuilder()
                .SetSize(size)
                .SetUsage(BufferUsageFlags::IndexBuffer | BufferUsageFlags::TransferDst)
                .SetMemoryUsage(BufferMemoryUsage::CpuToGpu)
                .Build());
            if (data)
            {
                void* mapped = buf->Map();
                memcpy(mapped, data, size);
                buf->Unmap();
            }
            return buf;
        }

        Ref<Buffer> CreateStorageBuffer(uint64_t size, const void* data = nullptr)
        {
            auto buf = device->CreateBuffer(
                BufferBuilder()
                .SetSize(size)
                .SetUsage(BufferUsageFlags::StorageBuffer | BufferUsageFlags::TransferDst)
                .SetMemoryUsage(BufferMemoryUsage::CpuToGpu)
                .Build());
            if (data)
            {
                void* mapped = buf->Map();
                memcpy(mapped, data, size);
                buf->Unmap();
            }
            return buf;
        }

        Ref<Buffer> CreateUniformBuffer(uint64_t size)
        {
            return device->CreateBuffer(
                BufferBuilder()
                .SetSize(size)
                .SetUsage(BufferUsageFlags::UniformBuffer)
                .SetMemoryUsage(BufferMemoryUsage::CpuToGpu)
                .Build());
        }

        struct QuickPipeline
        {
            Ref<GraphicsPipeline> pipeline;
            Ref<PipelineLayout> layout;
            Ref<DescriptorSetLayout> descriptorLayout;
            Ref<DescriptorPool> descriptorPool;
        };

        QuickPipeline CreateQuickPipeline(
            const std::string& shaderPath,
            const std::string& vsEntry = "mainVS",
            const std::string& fsEntry = "mainPS",
            const std::vector<DescriptorSetLayoutBinding>& bindings = {})
        {
            QuickPipeline qp;
            auto vs = LoadShader(shaderPath, vsEntry, ShaderStage::Vertex);
            auto fs = LoadShader(shaderPath, fsEntry, ShaderStage::Fragment);

            DescriptorSetLayoutCreateInfo layoutInfo;
            layoutInfo.Bindings = bindings;
            qp.descriptorLayout = device->CreateDescriptorSetLayout(layoutInfo);

            PipelineLayoutCreateInfo plInfo;
            plInfo.SetLayouts = {qp.descriptorLayout};
            qp.layout = device->CreatePipelineLayout(plInfo);

            DescriptorPoolCreateInfo poolInfo;
            poolInfo.MaxSets = framesInFlight * 2;
            qp.descriptorPool = device->CreateDescriptorPool(poolInfo);

            auto pipeInfo = GraphicsPipelineBuilder()
                .SetShaders({vs, fs})
                .AddColorFormat(swapchain->GetFormat())
                .SetLayout(qp.layout)
                .Build();
            qp.pipeline = device->CreateGraphicsPipeline(pipeInfo);

            return qp;
        }

        void ClearAndBeginRendering(const Ref<CommandBufferEncoder>& cmd,
                                     float r = 0.1f, float g = 0.1f, float b = 0.1f, float a = 1.0f)
        {
            auto backBuffer = swapchain->GetBackBuffer(imageIndex);
            cmd->TransitionImage(backBuffer, ImageTransition::UndefinedToColorAttachment);

            RenderingInfo ri;
            ri.RenderArea = {0, 0, swapchain->GetExtent().width, swapchain->GetExtent().height};
            RenderingAttachmentInfo colorAtt;
            colorAtt.Texture = backBuffer;
            colorAtt.LoadOp = AttachmentLoadOp::Clear;
            colorAtt.ClearValue = ClearValue::ColorFloat(r, g, b, a);
            ri.ColorAttachments = {colorAtt};
            cmd->BeginRendering(ri);

            Viewport vp = {0, 0,
                static_cast<float>(swapchain->GetExtent().width),
                static_cast<float>(swapchain->GetExtent().height)};
            cmd->SetViewport(vp);
            cmd->SetScissor({0, 0, swapchain->GetExtent().width, swapchain->GetExtent().height});
        }

        void EndRendering(const Ref<CommandBufferEncoder>& cmd)
        {
            cmd->EndRendering();
            auto backBuffer = swapchain->GetBackBuffer(imageIndex);
            cmd->TransitionImage(backBuffer, ImageTransition::ColorAttachmentToPresent);
        }
    };
}

#endif
