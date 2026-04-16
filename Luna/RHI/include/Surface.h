#ifndef CACAO_CACAOSURFACE_H
#define CACAO_CACAOSURFACE_H
#include "Core.h"

namespace Cacao {
class Device;
class Queue;
class Adapter;
} // namespace Cacao

namespace Cacao {
enum class SurfaceRotation {
    Identity,
    Rotate90,
    Rotate180,
    Rotate270
};

struct SurfaceTransform {
    SurfaceRotation rotation = SurfaceRotation::Identity;
    bool flipHorizontal = false;
    bool flipVertical = false;

    bool IsSwappedDimensions() const
    {
        return rotation == SurfaceRotation::Rotate90 || rotation == SurfaceRotation::Rotate270;
    }
};

struct SurfaceFormat {
    Format format;
    ColorSpace colorSpace;
};

struct SurfaceCapabilities {
    uint32_t minImageCount;
    uint32_t maxImageCount;
    Extent2D currentExtent;
    Extent2D minImageExtent;
    Extent2D maxImageExtent;
    SurfaceTransform currentTransform;
};
enum class PresentMode {
    Immediate,
    Mailbox,
    Fifo,
    FifoRelaxed
};

class CACAO_API Surface : public std::enable_shared_from_this<Surface> {
public:
    virtual ~Surface() = default;
    virtual SurfaceCapabilities GetCapabilities(const Ref<Adapter>& adapter) = 0;
    virtual std::vector<SurfaceFormat> GetSupportedFormats(const Ref<Adapter>& adapter) = 0;
    virtual Ref<Queue> GetPresentQueue(const Ref<Device>& device) = 0;
    virtual std::vector<PresentMode> GetSupportedPresentModes(const Ref<Adapter>& adapter) = 0;
};
} // namespace Cacao
#endif
