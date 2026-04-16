#include "Impls/OpenGL/GLSwapchain.h"
#include "Impls/OpenGL/GLSurface.h"
#include "Impls/OpenGL/GLCommon.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Cacao
{
    GLSwapchain::GLSwapchain(const SwapchainCreateInfo& info)
        : m_createInfo(info)
    {
        if (info.CompatibleSurface)
        {
            if (auto glSurface = std::dynamic_pointer_cast<GLSurface>(info.CompatibleSurface))
                m_hdc = glSurface->GetNativeHDC();
        }
    }

    Ref<GLSwapchain> GLSwapchain::Create(const SwapchainCreateInfo& info)
    {
        return std::make_shared<GLSwapchain>(info);
    }

    GLSwapchain::~GLSwapchain() = default;

    Result GLSwapchain::Present(const Ref<Queue>&, const Ref<Synchronization>&, uint32_t)
    {
#ifdef _WIN32
        if (m_hdc)
            SwapBuffers(static_cast<HDC>(m_hdc));
#endif
        m_currentIndex = (m_currentIndex + 1) % GetImageCount();
        return Result::Success;
    }

    Result GLSwapchain::AcquireNextImage(const Ref<Synchronization>&, int, int& out)
    {
        out = static_cast<int>(m_currentIndex);
        return Result::Success;
    }

    uint32_t GLSwapchain::GetImageCount() const { return 2; }

    Ref<Texture> GLSwapchain::GetBackBuffer(uint32_t) const { return nullptr; }

    Extent2D GLSwapchain::GetExtent() const
    {
        return m_createInfo.Extent;
    }

    Format GLSwapchain::GetFormat() const { return m_createInfo.Format; }

    PresentMode GLSwapchain::GetPresentMode() const { return m_createInfo.PresentMode; }
}
