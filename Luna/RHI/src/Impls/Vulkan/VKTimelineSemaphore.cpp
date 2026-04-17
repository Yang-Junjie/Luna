#include "Impls/Vulkan/VKDevice.h"
#include "Impls/Vulkan/VKTimelineSemaphore.h"

namespace luna::RHI {
VKTimelineSemaphore::VKTimelineSemaphore(Ref<VKDevice> device, uint64_t initialValue)
    : m_device(std::move(device))
{
    vk::SemaphoreTypeCreateInfo typeInfo{};
    typeInfo.semaphoreType = vk::SemaphoreType::eTimeline;
    typeInfo.initialValue = initialValue;

    vk::SemaphoreCreateInfo ci{};
    ci.pNext = &typeInfo;

    m_semaphore = m_device->GetHandle().createSemaphore(ci);
}

VKTimelineSemaphore::~VKTimelineSemaphore()
{
    if (m_semaphore) {
        m_device->GetHandle().destroySemaphore(m_semaphore);
    }
}

void VKTimelineSemaphore::Signal(uint64_t value)
{
    vk::SemaphoreSignalInfo signalInfo{};
    signalInfo.semaphore = m_semaphore;
    signalInfo.value = value;
    m_device->GetHandle().signalSemaphore(signalInfo);
}

bool VKTimelineSemaphore::Wait(uint64_t value, uint64_t timeoutNs)
{
    vk::SemaphoreWaitInfo waitInfo{};
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &m_semaphore;
    waitInfo.pValues = &value;

    auto result = m_device->GetHandle().waitSemaphores(waitInfo, timeoutNs);
    return result == vk::Result::eSuccess;
}

uint64_t VKTimelineSemaphore::GetValue() const
{
    return m_device->GetHandle().getSemaphoreCounterValue(m_semaphore);
}
} // namespace luna::RHI
