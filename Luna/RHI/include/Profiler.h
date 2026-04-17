#ifndef LUNA_RHI_PROFILER_H
#define LUNA_RHI_PROFILER_H
#include "CommandBufferEncoder.h"
#include "Core.h"
#include "QueryPool.h"

#include <memory>
#include <string>
#include <vector>

namespace luna::RHI {
struct GPUTimingResult {
    std::string Name;
    double ElapsedMs = 0.0;
};

class LUNA_RHI_API GPUProfiler {
public:
    virtual ~GPUProfiler() = default;

    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;

    virtual void BeginScope(const Ref<CommandBufferEncoder>& encoder, const std::string& name) = 0;
    virtual void EndScope(const Ref<CommandBufferEncoder>& encoder) = 0;

    virtual std::vector<GPUTimingResult> GetResults() const = 0;
    virtual double GetTotalGPUTimeMs() const = 0;
    virtual void Reset() = 0;
};

struct GPUProfilerCreateInfo {
    uint32_t MaxScopes = 256;
    Ref<class Device> Device;
};
} // namespace luna::RHI

#endif
