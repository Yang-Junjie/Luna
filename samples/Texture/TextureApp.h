#pragma once

#include "Core/Application.h"

namespace luna::samples::texture {

class TextureApp final : public Application {
public:
    TextureApp();

protected:
    void onInit() override;
};

} // namespace luna::samples::texture
