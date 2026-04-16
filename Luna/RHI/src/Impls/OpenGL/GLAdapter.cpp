#include "Impls/OpenGL/GLAdapter.h"
#include "Impls/OpenGL/GLDevice.h"

namespace Cacao
{
    GLAdapter::GLAdapter(const std::string& renderer, const std::string& vendor,
                         int glMajor, int glMinor)
        : m_renderer(renderer), m_vendor(vendor), m_glMajor(glMajor), m_glMinor(glMinor) {}

    Ref<GLAdapter> GLAdapter::Create(const std::string& renderer, const std::string& vendor,
                                      int glMajor, int glMinor)
    {
        return std::make_shared<GLAdapter>(renderer, vendor, glMajor, glMinor);
    }

    AdapterProperties GLAdapter::GetProperties() const
    {
        AdapterProperties props{};
        props.name = m_renderer;
        props.type = AdapterType::Unknown;
        props.deviceID = 0;
        props.vendorID = 0;
        props.dedicatedVideoMemory = 0;
        return props;
    }

    AdapterType GLAdapter::GetAdapterType() const { return AdapterType::Unknown; }

    bool GLAdapter::IsFeatureSupported(DeviceFeature feature) const
    {
        switch (feature)
        {
        case DeviceFeature::SamplerAnisotropy: return true;
        case DeviceFeature::MultiDrawIndirect: return m_glMajor >= 4 && m_glMinor >= 3;
        case DeviceFeature::DrawIndirectCount: return m_glMajor >= 4 && m_glMinor >= 6;
        default: return false;
        }
    }

    Ref<Device> GLAdapter::CreateDevice(const DeviceCreateInfo& info)
    {
        return GLDevice::Create(shared_from_this(), info);
    }

    DeviceLimits GLAdapter::QueryLimits() const
    {
        DeviceLimits limits;
        GLint val = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &val); limits.maxTextureSize2D = val;
        glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &val); limits.maxTextureSize3D = val;
        glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &val); limits.maxTextureSizeCube = val;
        glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &val); limits.maxTextureArrayLayers = val;
        glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &val); limits.maxColorAttachments = val;
        glGetIntegerv(GL_MAX_VIEWPORTS, &val); limits.maxViewports = val;
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, (GLint*)&limits.maxComputeWorkGroupCountX);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, (GLint*)&limits.maxComputeWorkGroupCountY);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, (GLint*)&limits.maxComputeWorkGroupCountZ);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, (GLint*)&limits.maxComputeWorkGroupSizeX);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, (GLint*)&limits.maxComputeWorkGroupSizeY);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, (GLint*)&limits.maxComputeWorkGroupSizeZ);
        glGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, (GLint*)&limits.maxComputeSharedMemorySize);
        glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, (GLint*)&limits.maxUniformBufferSize);
        GLint maxSamples = 1;
        glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
        limits.maxMSAASamples = maxSamples;
        limits.supportsAsyncCompute = false;
        limits.supportsTransferQueue = false;
        limits.supportsPipelineCacheSerialization = false;
        limits.supportsStorageBufferWriteInGraphics = true;
        return limits;
    }

    uint32_t GLAdapter::FindQueueFamilyIndex(QueueType) const { return 0; }
}
