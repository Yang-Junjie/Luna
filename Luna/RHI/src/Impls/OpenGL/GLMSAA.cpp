#include "Impls/OpenGL/GLMSAA.h"

namespace Cacao {
GLMSAARenderTarget GLMSAA::Create(uint32_t width, uint32_t height, uint32_t samples, GLenum colorFormat, bool withDepth)
{
    GLMSAARenderTarget target;
    target.width = width;
    target.height = height;
    target.samples = samples;

    glGenFramebuffers(1, &target.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);

    glGenRenderbuffers(1, &target.colorRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, target.colorRenderbuffer);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, colorFormat, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, target.colorRenderbuffer);

    if (withDepth) {
        glGenRenderbuffers(1, &target.depthRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, target.depthRenderbuffer);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(
            GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, target.depthRenderbuffer);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return target;
}

void GLMSAA::Resolve(const GLMSAARenderTarget& msaaTarget, GLuint resolveFBO, uint32_t width, uint32_t height)
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaTarget.fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFBO);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLMSAA::Destroy(GLMSAARenderTarget& target)
{
    if (target.colorRenderbuffer) {
        glDeleteRenderbuffers(1, &target.colorRenderbuffer);
    }
    if (target.depthRenderbuffer) {
        glDeleteRenderbuffers(1, &target.depthRenderbuffer);
    }
    if (target.fbo) {
        glDeleteFramebuffers(1, &target.fbo);
    }
    target = {};
}

GLint GLMSAA::GetMaxSamples()
{
    GLint maxSamples = 1;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    return maxSamples;
}
} // namespace Cacao
