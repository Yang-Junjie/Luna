#ifndef CACAO_GLMSAA_H
#define CACAO_GLMSAA_H
#include "GLCommon.h"

namespace Cacao
{
    struct GLMSAARenderTarget
    {
        GLuint fbo = 0;
        GLuint colorRenderbuffer = 0;
        GLuint depthRenderbuffer = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t samples = 1;
    };

    class CACAO_API GLMSAA
    {
    public:
        static GLMSAARenderTarget Create(uint32_t width, uint32_t height, uint32_t samples,
                                          GLenum colorFormat, bool withDepth = true);

        static void Resolve(const GLMSAARenderTarget& msaaTarget, GLuint resolveFBO,
                            uint32_t width, uint32_t height);

        static void Destroy(GLMSAARenderTarget& target);

        static GLint GetMaxSamples();
    };
}

#endif
