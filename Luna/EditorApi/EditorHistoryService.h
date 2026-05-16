#pragma once

#include <string>

namespace luna::editor {

class HistoryService {
public:
    virtual ~HistoryService() = default;

    [[nodiscard]] virtual bool canUndo() const noexcept = 0;
    [[nodiscard]] virtual bool canRedo() const noexcept = 0;
    [[nodiscard]] virtual bool hasOpenTransaction() const noexcept = 0;
    virtual bool beginTransaction(std::string name) = 0;
    virtual bool commitTransaction() = 0;
    virtual bool rollbackTransaction() = 0;
    virtual bool undo() = 0;
    virtual bool redo() = 0;
};

} // namespace luna::editor
