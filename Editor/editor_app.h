#pragma once

#include "Core/application.h"

namespace luna::editor {

struct EditorAppOptions {
    bool runResizeSelfTest = false;
};

class EditorApp final : public Application {
public:
    explicit EditorApp(const EditorAppOptions& options = {});

    bool isSelfTestMode() const
    {
        return m_options.runResizeSelfTest;
    }

    bool selfTestPassed() const
    {
        return m_selfTestPassed;
    }

    void setSelfTestPassed(bool passed)
    {
        m_selfTestPassed = passed;
    }

protected:
    void onInit() override;

private:
    EditorAppOptions m_options;
    bool m_selfTestPassed = false;
};

} // namespace luna::editor
