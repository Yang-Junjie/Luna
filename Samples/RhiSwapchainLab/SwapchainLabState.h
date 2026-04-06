#pragma once

#include "RHI/RHIDevice.h"

#include <string>

namespace swapchain_lab {

struct State {
    luna::SwapchainDesc requestedDesc{};
    luna::RHISwapchainState observedState{};
    bool recreateSwapchainRequested = false;
    bool twoWindows = false;
    uint64_t frameCounter = 0;
    std::string status;
};

} // namespace swapchain_lab
