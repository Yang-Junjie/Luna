#include "Renderer/Resources/ShaderModuleLoader.h"

#include <ShaderCompiler.h>
#include <ShaderModule.h>

namespace luna::renderer_detail {

luna::RHI::Ref<luna::RHI::ShaderModule> loadShaderModule(const luna::RHI::Ref<luna::RHI::Device>& device,
                                                         const luna::RHI::Ref<luna::RHI::ShaderCompiler>& compiler,
                                                         const std::filesystem::path& path,
                                                         std::string_view entry_point,
                                                         luna::RHI::ShaderStage stage)
{
    if (!device || !compiler) {
        return {};
    }

    luna::RHI::ShaderCreateInfo create_info;
    create_info.SourcePath = path.string();
    create_info.EntryPoint = std::string(entry_point);
    create_info.Stage = stage;
    return compiler->CompileOrLoad(device, create_info);
}

} // namespace luna::renderer_detail
