#pragma once

#include "Core/Application.h"

namespace luna::samples::model {

class ModelApp final : public Application {
public:
    ModelApp();

protected:
    void onInit() override;
};

} // namespace luna::samples::model
