#pragma once

#include "Core/Application.h"

namespace luna::samples::triangle {

class TriangleApp final : public Application {
public:
    TriangleApp();

protected:
    VulkanRenderer::InitializationOptions getRendererInitializationOptions() override;
    void onInit() override;
};

} // namespace luna::samples::triangle
