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

RHI::Ref<RHI::ShaderModule> loadShaderModule(const RHI::Ref<RHI::Device>& device,
                                             const RHI::Ref<RHI::ShaderCompiler>& compiler,
                                             const std::filesystem::path& path,
                                             std::string_view entry_point,
                                             RHI::ShaderStage stage);

} // namespace luna::renderer_detail
