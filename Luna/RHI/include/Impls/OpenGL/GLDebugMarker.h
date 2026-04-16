#ifndef CACAO_GLDEBUGMARKER_H
#define CACAO_GLDEBUGMARKER_H
#include "GLCommon.h"

#include <string>

namespace Cacao {
class GLDebugMarker {
public:
    static bool IsSupported();

    static void PushGroup(const std::string& name)
    {
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, static_cast<GLsizei>(name.size()), name.c_str());
    }

    static void PopGroup()
    {
        glPopDebugGroup();
    }

    static void InsertMessage(const std::string& msg)
    {
        glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION,
                             GL_DEBUG_TYPE_MARKER,
                             0,
                             GL_DEBUG_SEVERITY_NOTIFICATION,
                             static_cast<GLsizei>(msg.size()),
                             msg.c_str());
    }
};
} // namespace Cacao

#endif
