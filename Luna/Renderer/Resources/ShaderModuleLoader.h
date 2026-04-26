#pragma once

#include <Core.h>

#include <filesystem>
#include <string_view>

namespace luna::RHI {
class Device;
class ShaderCompiler;
class ShaderModule;
} // namespace luna::RHI

namespace luna::renderer_detail {

luna::RHI::Ref<luna::RHI::ShaderModule> loadShaderModule(const luna::RHI::Ref<luna::RHI::Device>& device,
                                                         const luna::RHI::Ref<luna::RHI::ShaderCompiler>& compiler,
                                                         const std::filesystem::path& path,
                                                         std::string_view entry_point,
                                                         luna::RHI::ShaderStage stage);

} // namespace luna::renderer_detail
