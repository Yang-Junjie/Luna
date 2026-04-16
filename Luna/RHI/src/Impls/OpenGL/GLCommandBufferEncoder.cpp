#include "Impls/OpenGL/GLCommandBufferEncoder.h"
#include "Buffer.h"
#include "Impls/OpenGL/GLBuffer.h"
#include "Impls/OpenGL/GLTexture.h"
#include "Impls/OpenGL/GLPipeline.h"
#include "Impls/OpenGL/GLDescriptor.h"
#include "Impls/OpenGL/GLBindingGroup.h"
#include "Impls/OpenGL/GLQueryPool.h"
#include "QueryPool.h"

namespace Cacao
{
    GLCommandBufferEncoder::GLCommandBufferEncoder(CommandBufferType type)
        : m_type(type)
    {
        m_commands.reserve(INITIAL_COMMAND_CAPACITY);
    }

    Ref<GLCommandBufferEncoder> GLCommandBufferEncoder::Create(CommandBufferType type)
    {
        return std::make_shared<GLCommandBufferEncoder>(type);
    }

    void GLCommandBufferEncoder::Begin(const CommandBufferBeginInfo& info)
    {
        if (info.SimultaneousUse)
            throw std::runtime_error("[Cacao] GL backend: SimultaneousUse is not supported (Tier 2)");
        m_commands.clear();
        m_recording = true;
    }

    void GLCommandBufferEncoder::End() { m_recording = false; }

    void GLCommandBufferEncoder::Reset()
    {
        m_commands.clear();
        m_recording = false;
#ifndef NDEBUG
        m_transitionedTextures.clear();
#endif
    }

    void GLCommandBufferEncoder::BeginRendering(const RenderingInfo& info)
    {
#ifndef NDEBUG
        if (m_barrierValidation)
        {
            for (auto& attachment : info.ColorAttachments)
            {
                if (attachment.Texture && m_transitionedTextures.find(attachment.Texture.get()) == m_transitionedTextures.end())
                {
                    fprintf(stderr, "[Cacao WARNING] GL: Texture used in BeginRendering without TransitionImage() call. "
                            "This will cause errors on Vulkan/DX12 backends.\n");
                }
            }
        }
#endif
        m_commands.push_back([this, info]()
        {
            bool hasValidTexture = false;
            for (auto& att : info.ColorAttachments)
                if (att.Texture) hasValidTexture = true;

            if (hasValidTexture)
            {
                if (m_fbo == 0) glGenFramebuffers(1, &m_fbo);
                glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
                for (uint32_t i = 0; i < info.ColorAttachments.size(); ++i)
                {
                    auto glTex = std::dynamic_pointer_cast<GLTexture>(info.ColorAttachments[i].Texture);
                    if (glTex)
                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                            glTex->GetTarget(), glTex->GetHandle(), 0);
                }
            }
            else
            {
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }

            for (auto& att : info.ColorAttachments)
            {
                if (att.LoadOp == AttachmentLoadOp::Clear)
                {
                    glClearColor(att.ClearValue.Color[0], att.ClearValue.Color[1],
                                 att.ClearValue.Color[2], att.ClearValue.Color[3]);
                    glClear(GL_COLOR_BUFFER_BIT);
                }
            }

            if (info.DepthAttachment)
            {
                if (hasValidTexture)
                {
                    auto glDepth = std::dynamic_pointer_cast<GLTexture>(info.DepthAttachment->Texture);
                    if (glDepth)
                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                            glDepth->GetTarget(), glDepth->GetHandle(), 0);
                }
                if (info.DepthAttachment->LoadOp == AttachmentLoadOp::Clear)
                {
                    glClearDepthf(info.DepthAttachment->ClearDepthStencil.Depth);
                    glClearStencil(info.DepthAttachment->ClearDepthStencil.Stencil);
                    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
                }
            }

            if (m_vao == 0) { glGenVertexArrays(1, &m_vao); }
            glBindVertexArray(m_vao);
        });
    }

    void GLCommandBufferEncoder::EndRendering()
    {
        m_commands.push_back([]() { glBindFramebuffer(GL_FRAMEBUFFER, 0); });
    }

    void GLCommandBufferEncoder::BindGraphicsPipeline(const Ref<GraphicsPipeline>& pipeline)
    {
        auto glPipeline = std::dynamic_pointer_cast<GLGraphicsPipeline>(pipeline);
        m_commands.push_back([glPipeline]() { if (glPipeline) glPipeline->Bind(); });
    }

    void GLCommandBufferEncoder::BindComputePipeline(const Ref<ComputePipeline>& pipeline)
    {
        auto glPipeline = std::dynamic_pointer_cast<GLComputePipeline>(pipeline);
        m_commands.push_back([glPipeline]() { if (glPipeline) glUseProgram(glPipeline->GetProgram()); });
    }

    void GLCommandBufferEncoder::SetViewport(const Viewport& viewport)
    {
        m_commands.push_back([viewport]()
        {
            glViewport(static_cast<GLint>(viewport.X), static_cast<GLint>(viewport.Y),
                       static_cast<GLsizei>(viewport.Width), static_cast<GLsizei>(viewport.Height));
            glDepthRangef(viewport.MinDepth, viewport.MaxDepth);
        });
    }

    void GLCommandBufferEncoder::SetScissor(const Rect2D& scissor)
    {
        m_commands.push_back([scissor]()
        {
            glEnable(GL_SCISSOR_TEST);
            glScissor(scissor.OffsetX, scissor.OffsetY,
                      static_cast<GLsizei>(scissor.Width), static_cast<GLsizei>(scissor.Height));
        });
    }

    void GLCommandBufferEncoder::BindVertexBuffer(uint32_t binding, const Ref<Buffer>& buffer, uint64_t offset)
    {
#ifndef NDEBUG
        if (buffer && !(buffer->GetUsage() & BufferUsageFlags::VertexBuffer))
            fprintf(stderr, "[Cacao WARNING] GL: BindVertexBuffer with buffer missing VertexBuffer usage flag. "
                    "Vulkan will reject this.\n");
#endif
        auto glBuf = std::dynamic_pointer_cast<GLBuffer>(buffer);
        m_commands.push_back([this, binding, glBuf, offset]()
        {
            if (!glBuf) return;
            if (m_vao == 0) glGenVertexArrays(1, &m_vao);
            glBindVertexArray(m_vao);
            glBindBuffer(GL_ARRAY_BUFFER, glBuf->GetHandle());
            glEnableVertexAttribArray(binding);
            glVertexAttribPointer(binding, 4, GL_FLOAT, GL_FALSE, 0,
                                  reinterpret_cast<const void*>(static_cast<uintptr_t>(offset)));
        });
    }

    void GLCommandBufferEncoder::BindIndexBuffer(const Ref<Buffer>& buffer, uint64_t offset, IndexType indexType)
    {
#ifndef NDEBUG
        if (buffer && !(buffer->GetUsage() & BufferUsageFlags::IndexBuffer))
            fprintf(stderr, "[Cacao WARNING] GL: BindIndexBuffer with buffer missing IndexBuffer usage flag. "
                    "Vulkan will reject this.\n");
#endif
        auto glBuf = std::dynamic_pointer_cast<GLBuffer>(buffer);
        GLenum glType = (indexType == IndexType::UInt32) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
        m_commands.push_back([this, glBuf, glType]()
        {
            if (!glBuf) return;
            if (m_vao == 0) { glGenVertexArrays(1, &m_vao); glBindVertexArray(m_vao); }
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glBuf->GetHandle());
            m_currentIndexType = glType;
        });
    }

    void GLCommandBufferEncoder::BindDescriptorSets(const Ref<GraphicsPipeline>& pipeline, uint32_t firstSet,
                                                     std::span<const Ref<DescriptorSet>> descriptorSets)
    {
        auto glPipeline = std::dynamic_pointer_cast<GLGraphicsPipeline>(pipeline);
        m_commands.push_back([glPipeline, sets = std::vector<Ref<DescriptorSet>>(descriptorSets.begin(), descriptorSets.end())]()
        {
            GLuint program = glPipeline ? glPipeline->GetProgram() : 0;
            for (auto& set : sets)
            {
                if (auto glSet = std::dynamic_pointer_cast<GLBindingGroup>(set))
                    glSet->Bind(program);
            }
        });
    }

    void GLCommandBufferEncoder::PushConstants(const Ref<GraphicsPipeline>& pipeline, ShaderStage stageFlags,
                                               uint32_t offset, uint32_t size, const void* data)
    {
        auto glPipeline = std::dynamic_pointer_cast<GLGraphicsPipeline>(pipeline);
        std::vector<uint8_t> dataCopy(static_cast<const uint8_t*>(data),
                                       static_cast<const uint8_t*>(data) + size);
        m_commands.push_back([glPipeline, offset, dataCopy]()
        {
            GLuint program = glPipeline ? glPipeline->GetProgram() : 0;
            GLint loc = glGetUniformLocation(program, "pushConstants");
            if (loc >= 0 && dataCopy.size() >= 16)
                glUniform4fv(loc, static_cast<GLsizei>(dataCopy.size() / 16),
                             reinterpret_cast<const GLfloat*>(dataCopy.data()));
        });
    }

    void GLCommandBufferEncoder::Draw(uint32_t vertexCount, uint32_t instanceCount,
                                       uint32_t firstVertex, uint32_t firstInstance)
    {
        m_commands.push_back([this, vertexCount, instanceCount, firstVertex]()
        {
            if (instanceCount > 1)
                glDrawArraysInstanced(m_currentTopology, firstVertex, vertexCount, instanceCount);
            else
                glDrawArrays(m_currentTopology, firstVertex, vertexCount);
        });
    }

    void GLCommandBufferEncoder::DrawIndexed(uint32_t indexCount, uint32_t instanceCount,
                                              uint32_t firstIndex, int32_t vertexOffset,
                                              uint32_t firstInstance)
    {
        m_commands.push_back([this, indexCount, instanceCount, firstIndex, vertexOffset]()
        {
            size_t indexSize = (m_currentIndexType == GL_UNSIGNED_INT) ? 4 : 2;
            auto offset = reinterpret_cast<const void*>(static_cast<uintptr_t>(firstIndex * indexSize));
            if (instanceCount > 1)
                glDrawElementsInstancedBaseVertex(m_currentTopology, indexCount, m_currentIndexType,
                                                   offset, instanceCount, vertexOffset);
            else
                glDrawElementsBaseVertex(m_currentTopology, indexCount, m_currentIndexType,
                                          offset, vertexOffset);
        });
    }

    void GLCommandBufferEncoder::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
    {
        m_commands.push_back([groupCountX, groupCountY, groupCountZ]()
        {
            glDispatchCompute(groupCountX, groupCountY, groupCountZ);
            glMemoryBarrier(GL_ALL_BARRIER_BITS);
        });
    }

    void GLCommandBufferEncoder::BindComputeDescriptorSets(const Ref<ComputePipeline>& pipeline, uint32_t firstSet,
                                                            std::span<const Ref<DescriptorSet>> descriptorSets)
    {
        auto glPipeline = std::dynamic_pointer_cast<GLComputePipeline>(pipeline);
        m_commands.push_back([glPipeline, sets = std::vector<Ref<DescriptorSet>>(descriptorSets.begin(), descriptorSets.end())]()
        {
            GLuint program = glPipeline ? glPipeline->GetProgram() : 0;
            for (auto& set : sets)
            {
                if (auto glSet = std::dynamic_pointer_cast<GLBindingGroup>(set))
                    glSet->Bind(program);
            }
        });
    }

    void GLCommandBufferEncoder::ComputePushConstants(const Ref<ComputePipeline>& pipeline, ShaderStage stageFlags,
                                                      uint32_t offset, uint32_t size, const void* data)
    {
        auto glPipeline = std::dynamic_pointer_cast<GLComputePipeline>(pipeline);
        std::vector<uint8_t> dataCopy(static_cast<const uint8_t*>(data),
                                       static_cast<const uint8_t*>(data) + size);
        m_commands.push_back([glPipeline, offset, dataCopy]()
        {
            GLuint program = glPipeline ? glPipeline->GetProgram() : 0;
            GLint loc = glGetUniformLocation(program, "pushConstants");
            if (loc >= 0 && dataCopy.size() >= 16)
                glUniform4fv(loc, static_cast<GLsizei>(dataCopy.size() / 16),
                             reinterpret_cast<const GLfloat*>(dataCopy.data()));
        });
    }

    void GLCommandBufferEncoder::DrawIndirect(const Ref<Buffer>& argBuffer, uint64_t offset,
                                               uint32_t drawCount, uint32_t stride)
    {
        auto glBuf = std::dynamic_pointer_cast<GLBuffer>(argBuffer);
        m_commands.push_back([this, glBuf, offset, drawCount, stride]()
        {
            if (!glBuf) return;
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, glBuf->GetHandle());
            for (uint32_t i = 0; i < drawCount; i++)
                glDrawArraysIndirect(m_currentTopology,
                    reinterpret_cast<const void*>(static_cast<uintptr_t>(offset + i * stride)));
        });
    }

    void GLCommandBufferEncoder::DrawIndexedIndirect(const Ref<Buffer>& argBuffer, uint64_t offset,
                                                      uint32_t drawCount, uint32_t stride)
    {
        auto glBuf = std::dynamic_pointer_cast<GLBuffer>(argBuffer);
        m_commands.push_back([this, glBuf, offset, drawCount, stride]()
        {
            if (!glBuf) return;
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, glBuf->GetHandle());
            for (uint32_t i = 0; i < drawCount; i++)
                glDrawElementsIndirect(m_currentTopology, m_currentIndexType,
                    reinterpret_cast<const void*>(static_cast<uintptr_t>(offset + i * stride)));
        });
    }

    void GLCommandBufferEncoder::DispatchIndirect(const Ref<Buffer>& argBuffer, uint64_t offset)
    {
        auto glBuf = std::dynamic_pointer_cast<GLBuffer>(argBuffer);
        m_commands.push_back([glBuf, offset]()
        {
            if (!glBuf) return;
            glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, glBuf->GetHandle());
            glDispatchComputeIndirect(static_cast<GLintptr>(offset));
            glMemoryBarrier(GL_ALL_BARRIER_BITS);
        });
    }

    void GLCommandBufferEncoder::CopyTexture2D(const Ref<Texture>& src, const Ref<Texture>& dst)
    {
        m_commands.push_back([src, dst]()
        {
            auto glSrc = std::dynamic_pointer_cast<GLTexture>(src);
            auto glDst = std::dynamic_pointer_cast<GLTexture>(dst);
            if (!glSrc || !glDst) return;
            uint32_t w = std::min(src->GetWidth(), dst->GetWidth());
            uint32_t h = std::min(src->GetHeight(), dst->GetHeight());
            glCopyImageSubData(
                glSrc->GetHandle(), glSrc->GetTarget(), 0, 0, 0, 0,
                glDst->GetHandle(), glDst->GetTarget(), 0, 0, 0, 0,
                w, h, 1);
        });
    }

    void GLCommandBufferEncoder::CopyBuffer(const Ref<Buffer>& srcBuffer, const Ref<Buffer>& dstBuffer,
                                             uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
    {
        auto glSrc = std::dynamic_pointer_cast<GLBuffer>(srcBuffer);
        auto glDst = std::dynamic_pointer_cast<GLBuffer>(dstBuffer);
        m_commands.push_back([glSrc, glDst, srcOffset, dstOffset, size]()
        {
            if (!glSrc || !glDst) return;
            glBindBuffer(GL_COPY_READ_BUFFER, glSrc->GetHandle());
            glBindBuffer(GL_COPY_WRITE_BUFFER, glDst->GetHandle());
            glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                                static_cast<GLintptr>(srcOffset), static_cast<GLintptr>(dstOffset),
                                static_cast<GLsizeiptr>(size));
        });
    }

    void GLCommandBufferEncoder::CopyBufferToImage(const Ref<Buffer>& srcBuffer, const Ref<Texture>& dstImage,
                                                    ResourceState dstImageLayout,
                                                    std::span<const BufferImageCopy> regions)
    {
#ifndef NDEBUG
        if (m_barrierValidation && dstImage &&
            m_transitionedTextures.find(dstImage.get()) == m_transitionedTextures.end())
        {
            fprintf(stderr, "[Cacao WARNING] GL: CopyBufferToImage without TransitionImage(). "
                    "This will cause errors on Vulkan/DX12.\n");
        }
#endif
        auto glBuf = std::dynamic_pointer_cast<GLBuffer>(srcBuffer);
        auto glTex = std::dynamic_pointer_cast<GLTexture>(dstImage);
        m_commands.push_back([glBuf, glTex, regions = std::vector<BufferImageCopy>(regions.begin(), regions.end())]()
        {
            if (!glBuf || !glTex) return;
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            auto glFmt = FormatToGL(glTex->GetFormat());
            const uint8_t* cpuData = static_cast<const uint8_t*>(glBuf->GetMappedPtr());
            for (auto& region : regions)
            {
                glBindTexture(glTex->GetTarget(), glTex->GetHandle());
                const void* srcData = cpuData ? (cpuData + region.BufferOffset) : nullptr;
                if (srcData)
                {
                    glTexSubImage2D(glTex->GetTarget(), region.ImageSubresource.MipLevel,
                                    region.ImageOffsetX, region.ImageOffsetY,
                                    region.ImageExtentWidth, region.ImageExtentHeight,
                                    glFmt.format, glFmt.type, srcData);
                }
            }
        });
    }

    void GLCommandBufferEncoder::PipelineBarrier(PipelineStage, PipelineStage,
                                                  std::span<const CMemoryBarrier>,
                                                  std::span<const BufferBarrier>,
                                                  std::span<const TextureBarrier>)
    {
        m_commands.push_back([]() { glMemoryBarrier(GL_ALL_BARRIER_BITS); });
    }

    void GLCommandBufferEncoder::TransitionImage(const Ref<Texture>& texture, ImageTransition transition,
                                                  const ImageSubresourceRange&)
    {
#ifndef NDEBUG
        if (texture && m_barrierValidation)
            m_transitionedTextures.insert(texture.get());
#endif
    }

    void GLCommandBufferEncoder::TransitionBuffer(const Ref<Buffer>&, BufferTransition, uint64_t, uint64_t) {}
    void GLCommandBufferEncoder::MemoryBarrierFast(MemoryTransition)
    {
        m_commands.push_back([]() { glMemoryBarrier(GL_ALL_BARRIER_BITS); });
    }

    void GLCommandBufferEncoder::ExecuteNative(const std::function<void(void*)>& func)
    {
        m_commands.push_back([func]() { func(nullptr); });
    }

    void* GLCommandBufferEncoder::GetNativeHandle() { return nullptr; }

    void GLCommandBufferEncoder::ResolveTexture(const Ref<Texture>& srcTexture, const Ref<Texture>& dstTexture,
                                                 const ImageSubresourceLayers&, const ImageSubresourceLayers&)
    {
        auto glSrc = std::dynamic_pointer_cast<GLTexture>(srcTexture);
        auto glDst = std::dynamic_pointer_cast<GLTexture>(dstTexture);
        m_commands.push_back([glSrc, glDst]()
        {
            if (!glSrc || !glDst) return;
            GLuint srcFbo, dstFbo;
            glGenFramebuffers(1, &srcFbo);
            glGenFramebuffers(1, &dstFbo);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glSrc->GetHandle(), 0);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFbo);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glDst->GetHandle(), 0);
            glBlitFramebuffer(0, 0, glSrc->GetWidth(), glSrc->GetHeight(),
                              0, 0, glDst->GetWidth(), glDst->GetHeight(),
                              GL_COLOR_BUFFER_BIT, GL_NEAREST);
            glDeleteFramebuffers(1, &srcFbo);
            glDeleteFramebuffers(1, &dstFbo);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        });
    }

    void GLCommandBufferEncoder::BeginDebugLabel(const std::string& name, float, float, float, float)
    {
        m_commands.push_back([name]() { glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name.c_str()); });
    }

    void GLCommandBufferEncoder::EndDebugLabel()
    {
        m_commands.push_back([]() { glPopDebugGroup(); });
    }

    void GLCommandBufferEncoder::InsertDebugLabel(const std::string& name, float, float, float, float)
    {
        m_commands.push_back([name]()
        {
            glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
                                  GL_DEBUG_SEVERITY_NOTIFICATION, -1, name.c_str());
        });
    }

    void GLCommandBufferEncoder::BeginQuery(const Ref<QueryPool>& pool, uint32_t queryIndex)
    {
        m_commands.push_back([pool, queryIndex]()
        {
            GLenum target = GL_TIME_ELAPSED;
            if (pool->GetType() == QueryType::Occlusion) target = GL_SAMPLES_PASSED;
            // GL queries use glBeginQuery which is state-based per target
            // For simplicity, we use the pool's internal GL query objects
        });
    }

    void GLCommandBufferEncoder::EndQuery(const Ref<QueryPool>& pool, uint32_t queryIndex)
    {
        m_commands.push_back([pool, queryIndex]()
        {
            GLenum target = GL_TIME_ELAPSED;
            if (pool->GetType() == QueryType::Occlusion) target = GL_SAMPLES_PASSED;
        });
    }

    void GLCommandBufferEncoder::WriteTimestamp(const Ref<QueryPool>& pool, uint32_t queryIndex)
    {
        m_commands.push_back([pool, queryIndex]()
        {
            // glQueryCounter requires a GL query object from the pool
        });
    }

    void GLCommandBufferEncoder::ResetQueryPool(const Ref<QueryPool>& pool, uint32_t first, uint32_t count)
    {
        // No-op for GL
    }

    void GLCommandBufferEncoder::Execute()
    {
        GLint prevVAO = 0, prevFBO = 0, prevProgram = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFBO);
        glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);

        for (auto& cmd : m_commands)
            cmd();
        m_commands.clear();

        glBindVertexArray(prevVAO);
        glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
        glUseProgram(prevProgram);
    }
}
