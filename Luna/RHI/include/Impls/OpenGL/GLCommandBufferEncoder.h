#ifndef CACAO_GLCOMMANDBUFFERENCODER_H
#define CACAO_GLCOMMANDBUFFERENCODER_H
#include "CommandBufferEncoder.h"
#include "GLCommon.h"
#include <vector>
#include <functional>
#include <unordered_set>

namespace Cacao
{
    using GLCommand = std::function<void()>;

    class CACAO_API GLCommandBufferEncoder final : public CommandBufferEncoder
    {
    public:
        static constexpr size_t INITIAL_COMMAND_CAPACITY = 256;

        GLCommandBufferEncoder(CommandBufferType type);
        static Ref<GLCommandBufferEncoder> Create(CommandBufferType type);

        void Free() override {}
        void Reset() override;
        void ReturnToPool() override {}
        void Begin(const CommandBufferBeginInfo& info = CommandBufferBeginInfo()) override;
        void End() override;

        void BeginRendering(const RenderingInfo& info) override;
        void EndRendering() override;

        void BindGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline) override;
        void BindComputePipeline(const Ref<ComputePipeline>& pipeline) override;

        void SetViewport(const Viewport& viewport) override;
        void SetScissor(const Rect2D& scissor) override;

        void BindVertexBuffer(uint32_t binding, const Ref<Buffer>& buffer, uint64_t offset) override;
        void BindIndexBuffer(const Ref<Buffer>& buffer, uint64_t offset, IndexType indexType) override;

        void BindDescriptorSets(const Ref<GraphicsPipeline>& pipeline, uint32_t firstSet,
                                std::span<const Ref<DescriptorSet>> descriptorSets) override;
        void PushConstants(const Ref<GraphicsPipeline>& pipeline, ShaderStage stageFlags,
                           uint32_t offset, uint32_t size, const void* data) override;

        void Draw(uint32_t vertexCount, uint32_t instanceCount,
                  uint32_t firstVertex, uint32_t firstInstance) override;
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount,
                         uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) override;
        void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;

        void BindComputeDescriptorSets(const Ref<ComputePipeline>& pipeline, uint32_t firstSet,
                                       std::span<const Ref<DescriptorSet>> descriptorSets) override;
        void ComputePushConstants(const Ref<ComputePipeline>& pipeline, ShaderStage stageFlags,
                                  uint32_t offset, uint32_t size, const void* data) override;

        void PipelineBarrier(PipelineStage srcStage, PipelineStage dstStage,
                             std::span<const CMemoryBarrier> globalBarriers,
                             std::span<const BufferBarrier> bufferBarriers,
                             std::span<const TextureBarrier> textureBarriers) override;
        void TransitionImage(const Ref<Texture>& texture, ImageTransition transition,
                             const ImageSubresourceRange& range) override;
        void TransitionBuffer(const Ref<Buffer>& buffer, BufferTransition transition,
                              uint64_t offset, uint64_t size) override;
        void MemoryBarrierFast(MemoryTransition transition) override;
        void ExecuteNative(const std::function<void(void* nativeCommandBuffer)>& func) override;
        void* GetNativeHandle() override;

        void CopyTexture2D(const Ref<Texture>& src, const Ref<Texture>& dst) override;
        void CopyBufferToImage(const Ref<Buffer>& srcBuffer, const Ref<Texture>& dstImage,
                               ResourceState dstImageLayout,
                               std::span<const BufferImageCopy> regions) override;
        void CopyBuffer(const Ref<Buffer>& srcBuffer, const Ref<Buffer>& dstBuffer,
                        uint64_t srcOffset, uint64_t dstOffset, uint64_t size) override;
        CommandBufferType GetCommandBufferType() const override { return m_type; }

        void ResolveTexture(const Ref<Texture>& srcTexture, const Ref<Texture>& dstTexture,
                            const ImageSubresourceLayers& srcSubresource,
                            const ImageSubresourceLayers& dstSubresource) override;

        void DrawIndirect(const Ref<Buffer>& argBuffer, uint64_t offset,
                          uint32_t drawCount, uint32_t stride) override;
        void DrawIndexedIndirect(const Ref<Buffer>& argBuffer, uint64_t offset,
                                 uint32_t drawCount, uint32_t stride) override;
        void DispatchIndirect(const Ref<Buffer>& argBuffer, uint64_t offset) override;

        void BeginDebugLabel(const std::string& name, float r, float g, float b, float a) override;
        void EndDebugLabel() override;
        void InsertDebugLabel(const std::string& name, float r, float g, float b, float a) override;

        void BeginQuery(const Ref<QueryPool>& pool, uint32_t queryIndex) override;
        void EndQuery(const Ref<QueryPool>& pool, uint32_t queryIndex) override;
        void WriteTimestamp(const Ref<QueryPool>& pool, uint32_t queryIndex) override;
        void ResetQueryPool(const Ref<QueryPool>& pool, uint32_t first, uint32_t count) override;

        void Execute();

    private:
        std::vector<GLCommand> m_commands;
        CommandBufferType m_type;
        bool m_recording = false;
        GLuint m_fbo = 0;
        GLuint m_vao = 0;
        GLenum m_currentIndexType = GL_UNSIGNED_SHORT;
        GLenum m_currentTopology = GL_TRIANGLES;

#ifndef NDEBUG
        std::unordered_set<const Texture*> m_transitionedTextures;
        bool m_barrierValidation = true;
#endif
    };
}

#endif
