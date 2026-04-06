#include "RHI/CommandContext.h"
#include "RHI/Descriptors.h"
#include "RHI/RHIDevice.h"
#include "RHI/ResourceLayout.h"
#include "RHI/Types.h"

int main()
{
    luna::DeviceCreateInfo deviceInfo{};
    deviceInfo.backend = luna::RHIBackend::Vulkan;
    deviceInfo.swapchain = {.width = 1280, .height = 720};

    luna::BufferDesc bufferDesc{};
    bufferDesc.size = 256;
    bufferDesc.usage = luna::BufferUsage::Vertex;

    luna::ImageViewDesc imageViewDesc{};
    imageViewDesc.image = luna::ImageHandle::fromRaw(1);
    imageViewDesc.type = luna::ImageViewType::Image2D;
    imageViewDesc.aspect = luna::ImageAspect::Color;
    imageViewDesc.baseMipLevel = 0;
    imageViewDesc.mipCount = 1;
    imageViewDesc.baseArrayLayer = 0;
    imageViewDesc.layerCount = 1;

    luna::ResourceLayoutDesc resourceLayoutDesc{};
    resourceLayoutDesc.bindings.push_back({0, luna::ResourceType::UniformBuffer, 1, luna::ShaderType::Vertex});

    luna::RenderingInfo renderingInfo{};
    renderingInfo.width = 1280;
    renderingInfo.height = 720;
    renderingInfo.colorAttachments.push_back(
        {.image = luna::ImageHandle::fromRaw(1), .format = luna::PixelFormat::BGRA8Unorm, .clearColor = {}});
    renderingInfo.colorAttachments.back().loadOp = luna::AttachmentLoadOp::Load;
    renderingInfo.colorAttachments.back().storeOp = luna::AttachmentStoreOp::Store;

    luna::IndexedDrawArguments indexedDraw{};
    indexedDraw.indexCount = 3;

    luna::Viewport viewport{};
    viewport.width = 1280.0f;
    viewport.height = 720.0f;

    luna::ScissorRect scissor{};
    scissor.width = 1280;
    scissor.height = 720;

    const bool ok = deviceInfo.backend == luna::RHIBackend::Vulkan && bufferDesc.size == 256 &&
                    imageViewDesc.mipCount == 1 &&
                    resourceLayoutDesc.bindings.size() == 1 && renderingInfo.colorAttachments.size() == 1 &&
                    renderingInfo.colorAttachments.front().loadOp == luna::AttachmentLoadOp::Load &&
                    viewport.width == 1280.0f && scissor.width == 1280 && indexedDraw.indexCount == 3;
    return ok ? 0 : 1;
}
