#include "CommandBufferEncoder.h"
namespace Cacao
{
    void CommandBufferEncoder::PipelineBarrier(PipelineStage srcStage, PipelineStage dstStage,
                                               std::span<const TextureBarrier> textureBarriers)
    {
        PipelineBarrier(srcStage, dstStage, {}, {}, textureBarriers);
    }
    void CommandBufferEncoder::PipelineBarrier(PipelineStage srcStage, PipelineStage dstStage,
                                               std::span<const BufferBarrier> bufferBarriers)
    {
        PipelineBarrier(srcStage, dstStage, {}, bufferBarriers, {});
    }
}
