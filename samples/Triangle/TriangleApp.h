#pragma once

#include "Core/Application.h"

namespace luna::samples::triangle {

class TriangleApp final : public Application {
public:
    TriangleApp();

protected:
    void onInit() override;
};

} // namespace luna::samples::triangle
