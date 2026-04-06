luna::BufferHandle VulkanResourceBindingRegistry::register_buffer(vk::Buffer buffer)
{
    if (!buffer) {
        return {};
    }

    const uint64_t id = m_nextBufferId++;
    m_buffers.insert_or_assign(id, buffer);
    return luna::BufferHandle::fromRaw(id);
}

luna::ImageViewHandle VulkanResourceBindingRegistry::register_image_view(vk::ImageView imageView)
{
    if (!imageView) {
        return {};
    }

    const uint64_t id = m_nextImageViewId++;
    m_imageViews.insert_or_assign(id, imageView);
    return luna::ImageViewHandle::fromRaw(id);
}

bool VulkanResourceBindingRegistry::register_image_view(luna::ImageViewHandle handle, vk::ImageView imageView)
{
    if (!handle.isValid() || !imageView) {
        return false;
    }

    m_imageViews.insert_or_assign(handle.value, imageView);
    return true;
}

luna::SamplerHandle VulkanResourceBindingRegistry::register_sampler(vk::Sampler sampler)
{
    if (!sampler) {
        return {};
    }

    const uint64_t id = m_nextSamplerId++;
    m_samplers.insert_or_assign(id, sampler);
    return luna::SamplerHandle::fromRaw(id);
}

void VulkanResourceBindingRegistry::unregister_buffer(luna::BufferHandle handle)
{
    if (!handle.isValid()) {
        return;
    }

    m_buffers.erase(handle.value);
}

void VulkanResourceBindingRegistry::unregister_image_view(luna::ImageViewHandle handle)
{
    if (!handle.isValid()) {
        return;
    }

    m_imageViews.erase(handle.value);
}

void VulkanResourceBindingRegistry::unregister_sampler(luna::SamplerHandle handle)
{
    if (!handle.isValid()) {
        return;
    }

    m_samplers.erase(handle.value);
}

vk::Buffer VulkanResourceBindingRegistry::resolve_buffer(luna::BufferHandle handle) const
{
    const auto it = m_buffers.find(handle.value);
    return it != m_buffers.end() ? it->second : vk::Buffer{};
}

vk::ImageView VulkanResourceBindingRegistry::resolve_image_view(luna::ImageViewHandle handle) const
{
    const auto it = m_imageViews.find(handle.value);
    return it != m_imageViews.end() ? it->second : vk::ImageView{};
}

vk::Sampler VulkanResourceBindingRegistry::resolve_sampler(luna::SamplerHandle handle) const
{
    const auto it = m_samplers.find(handle.value);
    return it != m_samplers.end() ? it->second : vk::Sampler{};
}

bool update_resource_set(vk::Device device,
                         const VulkanResourceBindingRegistry& registry,
                         vk::DescriptorSet set,
                         const luna::ResourceSetWriteDesc& writeDesc)
{
    DescriptorWriter writer;

    for (const luna::BufferBindingWriteDesc& bufferWrite : writeDesc.buffers) {
        if (bufferWrite.elements.empty()) {
            const vk::Buffer buffer = registry.resolve_buffer(bufferWrite.buffer);
            if (!buffer) {
                LUNA_CORE_ERROR("Failed to resolve RHI buffer binding {}", bufferWrite.binding);
                return false;
            }

            writer.write_buffer(bufferWrite.binding,
                                buffer,
                                static_cast<size_t>(bufferWrite.size),
                                static_cast<size_t>(bufferWrite.offset),
                                to_vulkan_descriptor_type(bufferWrite.type),
                                bufferWrite.firstArrayElement);
            continue;
        }

        std::vector<vk::DescriptorBufferInfo> infos;
        infos.reserve(bufferWrite.elements.size());
        for (const luna::BufferBindingElementWriteDesc& element : bufferWrite.elements) {
            const vk::Buffer buffer = registry.resolve_buffer(element.buffer);
            if (!buffer) {
                LUNA_CORE_ERROR("Failed to resolve RHI buffer binding {}", bufferWrite.binding);
                return false;
            }

            infos.push_back(vk::DescriptorBufferInfo{
                .buffer = buffer,
                .offset = static_cast<vk::DeviceSize>(element.offset),
                .range = static_cast<vk::DeviceSize>(element.size),
            });
        }

        writer.write_buffers(
            bufferWrite.binding, infos, to_vulkan_descriptor_type(bufferWrite.type), bufferWrite.firstArrayElement);
    }

    for (const luna::ImageBindingWriteDesc& imageWrite : writeDesc.images) {
        if (imageWrite.elements.empty()) {
            luna::ImageViewHandle imageViewHandle = imageWrite.imageView;
            if (!imageViewHandle.isValid() && imageWrite.image.isValid()) {
                imageViewHandle = luna::ImageViewHandle::fromRaw(imageWrite.image.value);
            }

            const vk::ImageView imageView = registry.resolve_image_view(imageViewHandle);
            if (!imageView) {
                LUNA_CORE_ERROR("Failed to resolve RHI image binding {}", imageWrite.binding);
                return false;
            }

            const vk::Sampler sampler = registry.resolve_sampler(imageWrite.sampler);
            if (imageWrite.type == luna::ResourceType::CombinedImageSampler && !sampler) {
                LUNA_CORE_ERROR("Failed to resolve RHI sampler binding {}", imageWrite.binding);
                return false;
            }

            writer.write_image(imageWrite.binding,
                               imageView,
                               sampler,
                               to_vulkan_image_layout(imageWrite.type),
                               to_vulkan_descriptor_type(imageWrite.type),
                               imageWrite.firstArrayElement);
            continue;
        }

        std::vector<vk::DescriptorImageInfo> infos;
        infos.reserve(imageWrite.elements.size());
        for (const luna::ImageBindingElementWriteDesc& element : imageWrite.elements) {
            luna::ImageViewHandle imageViewHandle = element.imageView;
            if (!imageViewHandle.isValid() && element.image.isValid()) {
                imageViewHandle = luna::ImageViewHandle::fromRaw(element.image.value);
            }

            const vk::ImageView imageView = registry.resolve_image_view(imageViewHandle);
            if (!imageView) {
                LUNA_CORE_ERROR("Failed to resolve RHI image binding {}", imageWrite.binding);
                return false;
            }

            const vk::Sampler sampler = registry.resolve_sampler(element.sampler);
            if (imageWrite.type == luna::ResourceType::CombinedImageSampler && !sampler) {
                LUNA_CORE_ERROR("Failed to resolve RHI sampler binding {}", imageWrite.binding);
                return false;
            }

            infos.push_back(vk::DescriptorImageInfo{
                .sampler = sampler,
                .imageView = imageView,
                .imageLayout = to_vulkan_image_layout(imageWrite.type),
            });
        }

        writer.write_images(
            imageWrite.binding, infos, to_vulkan_descriptor_type(imageWrite.type), imageWrite.firstArrayElement);
    }

    writer.update_set(device, set);
    return true;
}
