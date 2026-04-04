#pragma once

#include "RHI/RHIDevice.h"

namespace luna {

class IRenderPipeline {
public:
    virtual ~IRenderPipeline() = default;

    virtual bool init(IRHIDevice& device) = 0;
    virtual void shutdown(IRHIDevice& device) = 0;
    virtual bool render(IRHIDevice& device, const FrameContext& frameContext) = 0;
};

} // namespace luna
