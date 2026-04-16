#include "Impls/OpenGL/GLDebugMarker.h"

namespace Cacao
{
    bool GLDebugMarker::IsSupported()
    {
        GLint numExtensions = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
        for (GLint i = 0; i < numExtensions; ++i)
        {
            const char* ext = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
            if (ext && std::string(ext) == "GL_KHR_debug")
                return true;
        }
        return false;
    }
}
