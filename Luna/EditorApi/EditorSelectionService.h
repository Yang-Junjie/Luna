#pragma once

#include "EditorApi/EditorTypes.h"

namespace luna::editor {

class SelectionService {
public:
    virtual ~SelectionService() = default;

    virtual EntityId selectedEntityId() const noexcept = 0;
    virtual void selectEntity(EntityId entity_id) = 0;
    virtual void clearSelection() = 0;
};

} // namespace luna::editor
