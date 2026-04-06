void DescriptorWriter::write_image(uint32_t binding,
                                   vk::ImageView image,
                                   vk::Sampler sampler,
                                   vk::ImageLayout layout,
                                   vk::DescriptorType type,
                                   uint32_t arrayElement)
{
    const vk::DescriptorImageInfo imageInfo{
        .sampler = sampler,
        .imageView = image,
        .imageLayout = layout,
    };
    write_images(binding, std::span<const vk::DescriptorImageInfo>(&imageInfo, 1), type, arrayElement);
}

void DescriptorWriter::write_images(uint32_t binding,
                                    std::span<const vk::DescriptorImageInfo> infos,
                                    vk::DescriptorType type,
                                    uint32_t firstArrayElement)
{
    imageInfos.emplace_back(infos.begin(), infos.end());
    vk::WriteDescriptorSet write{};
    write.dstBinding = binding;
    write.dstArrayElement = firstArrayElement;
    write.descriptorCount = static_cast<uint32_t>(imageInfos.back().size());
    write.descriptorType = type;
    write.pImageInfo = imageInfos.back().data();
    writes.push_back(write);
}

void DescriptorWriter::write_buffer(uint32_t binding,
                                    vk::Buffer buffer,
                                    size_t size,
                                    size_t offset,
                                    vk::DescriptorType type,
                                    uint32_t arrayElement)
{
    const vk::DescriptorBufferInfo bufferInfo{
        .buffer = buffer,
        .offset = static_cast<vk::DeviceSize>(offset),
        .range = static_cast<vk::DeviceSize>(size),
    };
    write_buffers(binding, std::span<const vk::DescriptorBufferInfo>(&bufferInfo, 1), type, arrayElement);
}

void DescriptorWriter::write_buffers(uint32_t binding,
                                     std::span<const vk::DescriptorBufferInfo> infos,
                                     vk::DescriptorType type,
                                     uint32_t firstArrayElement)
{
    bufferInfos.emplace_back(infos.begin(), infos.end());
    vk::WriteDescriptorSet write{};
    write.dstBinding = binding;
    write.dstArrayElement = firstArrayElement;
    write.descriptorCount = static_cast<uint32_t>(bufferInfos.back().size());
    write.descriptorType = type;
    write.pBufferInfo = bufferInfos.back().data();
    writes.push_back(write);
}

void DescriptorWriter::clear()
{
    imageInfos.clear();
    bufferInfos.clear();
    writes.clear();
}

void DescriptorWriter::update_set(vk::Device device, vk::DescriptorSet set)
{
    for (vk::WriteDescriptorSet& write : writes) {
        write.dstSet = set;
    }

    device.updateDescriptorSets(static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}
