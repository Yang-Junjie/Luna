namespace {
void add_or_merge_binding(std::vector<vk::DescriptorSetLayoutBinding>& bindings,
                          const vk::DescriptorSetLayoutBinding& binding)
{
    const auto existing =
        std::find_if(bindings.begin(), bindings.end(), [&](const vk::DescriptorSetLayoutBinding& candidate) {
            return candidate.binding == binding.binding;
        });

    if (existing == bindings.end()) {
        bindings.push_back(binding);
        return;
    }

    assert(existing->descriptorType == binding.descriptorType);
    assert(existing->descriptorCount == binding.descriptorCount);
    existing->stageFlags |= binding.stageFlags;
}
} // namespace

vk::DescriptorType to_vulkan_descriptor_type(luna::ResourceType type)
{
    switch (type) {
        case luna::ResourceType::UniformBuffer:
            return vk::DescriptorType::eUniformBuffer;
        case luna::ResourceType::DynamicUniformBuffer:
            return vk::DescriptorType::eUniformBufferDynamic;
        case luna::ResourceType::CombinedImageSampler:
            return vk::DescriptorType::eCombinedImageSampler;
        case luna::ResourceType::SampledImage:
            return vk::DescriptorType::eSampledImage;
        case luna::ResourceType::Sampler:
            return vk::DescriptorType::eSampler;
        case luna::ResourceType::StorageBuffer:
            return vk::DescriptorType::eStorageBuffer;
        case luna::ResourceType::StorageImage:
            return vk::DescriptorType::eStorageImage;
        case luna::ResourceType::InputAttachment:
            return vk::DescriptorType::eInputAttachment;
        default:
            assert(false && "Unsupported resource type.");
            return static_cast<vk::DescriptorType>(VK_DESCRIPTOR_TYPE_MAX_ENUM);
    }
}

vk::ShaderStageFlags to_vulkan_shader_stages(luna::ShaderType visibility)
{
    vk::ShaderStageFlags stages{};
    const uint32_t bits = static_cast<uint32_t>(visibility);

    if ((bits & static_cast<uint32_t>(luna::ShaderType::Vertex)) != 0) {
        stages |= vk::ShaderStageFlagBits::eVertex;
    }
    if ((bits & static_cast<uint32_t>(luna::ShaderType::TessControl)) != 0) {
        stages |= vk::ShaderStageFlagBits::eTessellationControl;
    }
    if ((bits & static_cast<uint32_t>(luna::ShaderType::TessEval)) != 0) {
        stages |= vk::ShaderStageFlagBits::eTessellationEvaluation;
    }
    if ((bits & static_cast<uint32_t>(luna::ShaderType::Geometry)) != 0) {
        stages |= vk::ShaderStageFlagBits::eGeometry;
    }
    if ((bits & static_cast<uint32_t>(luna::ShaderType::Fragment)) != 0) {
        stages |= vk::ShaderStageFlagBits::eFragment;
    }
    if ((bits & static_cast<uint32_t>(luna::ShaderType::Compute)) != 0) {
        stages |= vk::ShaderStageFlagBits::eCompute;
    }

    return stages;
}

vk::ImageLayout to_vulkan_image_layout(luna::ResourceType type)
{
    switch (type) {
        case luna::ResourceType::StorageImage:
            return vk::ImageLayout::eGeneral;
        case luna::ResourceType::CombinedImageSampler:
        case luna::ResourceType::SampledImage:
        case luna::ResourceType::InputAttachment:
            return vk::ImageLayout::eShaderReadOnlyOptimal;
        case luna::ResourceType::Sampler:
            return vk::ImageLayout::eUndefined;
        default:
            return vk::ImageLayout::eShaderReadOnlyOptimal;
    }
}

void DescriptorLayoutBuilder::add_binding_from_reflection(const luna::ShaderReflectionData& data,
                                                          vk::ShaderStageFlags shaderStages)
{
    vk::DescriptorSetLayoutBinding binding{};
    binding.binding = data.binding;
    binding.descriptorType = to_vulkan_descriptor_type(data.type);
    binding.descriptorCount = std::max(1u, data.count);
    binding.stageFlags = shaderStages;
    binding.pImmutableSamplers = nullptr;

    add_or_merge_binding(bindings, binding);
}

void DescriptorLayoutBuilder::add_binding(uint32_t binding, vk::DescriptorType type)
{
    vk::DescriptorSetLayoutBinding newbind{};
    newbind.binding = binding;
    newbind.descriptorType = type;
    newbind.descriptorCount = 1;

    add_or_merge_binding(bindings, newbind);
}

void DescriptorLayoutBuilder::add_bindings_from_reflection(const luna::Shader::ReflectionMap& reflectionMap,
                                                           uint32_t setIndex,
                                                           vk::ShaderStageFlags shaderStages)
{
    const auto setIt = reflectionMap.find(setIndex);
    if (setIt == reflectionMap.end()) {
        return;
    }

    for (const auto& binding : setIt->second) {
        add_binding_from_reflection(binding, shaderStages);
    }
}

void DescriptorLayoutBuilder::clear()
{
    bindings.clear();
}

vk::DescriptorSetLayout DescriptorLayoutBuilder::build(vk::Device device,
                                                       vk::ShaderStageFlags shaderStages,
                                                       const void* pNext,
                                                       vk::DescriptorSetLayoutCreateFlags flags)
{
    for (auto& b : bindings) {
        b.stageFlags |= shaderStages;
    }

    std::sort(bindings.begin(),
              bindings.end(),
              [](const vk::DescriptorSetLayoutBinding& lhs, const vk::DescriptorSetLayoutBinding& rhs) {
                  return lhs.binding < rhs.binding;
              });

    vk::DescriptorSetLayoutCreateInfo info{};
    info.pNext = pNext;
    info.flags = flags;
    info.bindingCount = static_cast<uint32_t>(bindings.size());
    info.pBindings = bindings.data();

    vk::DescriptorSetLayout set{};
    VK_CHECK(device.createDescriptorSetLayout(&info, nullptr, &set));
    return set;
}

vk::DescriptorSetLayout build_resource_layout(vk::Device device,
                                              const luna::ResourceLayoutDesc& desc,
                                              const void* pNext,
                                              vk::DescriptorSetLayoutCreateFlags flags)
{
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    bindings.reserve(desc.bindings.size());

    for (const luna::ResourceBindingDesc& bindingDesc : desc.bindings) {
        vk::DescriptorSetLayoutBinding binding{};
        binding.binding = bindingDesc.binding;
        binding.descriptorType = to_vulkan_descriptor_type(bindingDesc.type);
        binding.descriptorCount = std::max(1u, bindingDesc.count);
        binding.stageFlags = to_vulkan_shader_stages(bindingDesc.visibility);
        binding.pImmutableSamplers = nullptr;
        add_or_merge_binding(bindings, binding);
    }

    std::sort(bindings.begin(),
              bindings.end(),
              [](const vk::DescriptorSetLayoutBinding& lhs, const vk::DescriptorSetLayoutBinding& rhs) {
                  return lhs.binding < rhs.binding;
              });

    vk::DescriptorSetLayoutCreateInfo info{};
    info.pNext = pNext;
    info.flags = flags;
    info.bindingCount = static_cast<uint32_t>(bindings.size());
    info.pBindings = bindings.data();

    vk::DescriptorSetLayout layout{};
    VK_CHECK(device.createDescriptorSetLayout(&info, nullptr, &layout));

    if (!desc.debugName.empty()) {
        LUNA_CORE_INFO("{} created via RHI", desc.debugName);
    }

    return layout;
}
