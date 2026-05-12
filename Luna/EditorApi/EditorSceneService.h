#pragma once

#include "EditorApi/EditorTypes.h"

#include <cstddef>
#include <string>

namespace luna::editor {

class SceneService {
public:
    virtual ~SceneService() = default;

    virtual std::string sceneLabel() const = 0;
    virtual size_t entityCount() const = 0;
    virtual bool canEditScene() const noexcept = 0;

    virtual EntityId createEntity(std::string name) = 0;
};

} // namespace luna::editor
