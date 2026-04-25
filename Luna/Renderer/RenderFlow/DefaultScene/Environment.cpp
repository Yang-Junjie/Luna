#include "Asset/Editor/ImageLoader.h"
#include "Core/Log.h"
#include "Renderer/RenderFlow/DefaultScene/Environment.h"

namespace luna::render_flow::default_scene {

namespace {

std::filesystem::path defaultEnvironmentPath()
{
    return render_flow::default_scene_detail::projectRoot() / "SampleProject" / "Assets" / "Texture" /
           "newport_loft.hdr";
}

} // namespace

void EnvironmentResources::reset()
{
    m_device.reset();
    m_source_texture = {};
    m_irradiance_sh = {};
}

void EnvironmentResources::ensure(const luna::RHI::Ref<luna::RHI::Device>& device)
{
    if (!device) {
        LUNA_RENDERER_WARN("Cannot ensure scene environment without a device");
        return;
    }

    if (m_device != device) {
        reset();
        m_device = device;
    }

    if (m_source_texture.texture) {
        return;
    }

    const std::filesystem::path environment_path = defaultEnvironmentPath();
    ImageData environment_image;
    LUNA_RENDERER_INFO("Loading scene environment map from '{}'", environment_path.string());

    if (std::filesystem::exists(environment_path)) {
        environment_image = ImageLoader::LoadImageFromFile(environment_path.string());
        if (!environment_image.isValid()) {
            LUNA_RENDERER_WARN("Failed to load environment map '{}'; falling back to a neutral environment",
                               environment_path.string());
        }
    } else {
        LUNA_RENDERER_WARN("Environment map '{}' was not found; falling back to a neutral environment",
                           environment_path.string());
    }

    if (!environment_image.isValid() ||
        environment_image.ImageFormat != render_flow::default_scene_detail::kEnvironmentFormat) {
        environment_image = render_flow::default_scene_detail::createFallbackFloatImageData(
            glm::vec4(render_flow::default_scene_detail::kEnvironmentFallbackValue,
                      render_flow::default_scene_detail::kEnvironmentFallbackValue,
                      render_flow::default_scene_detail::kEnvironmentFallbackValue,
                      1.0f));
    }

    m_irradiance_sh = render_flow::default_scene_detail::computeDiffuseIrradianceSH(environment_image);
    environment_image = render_flow::default_scene_detail::generateEnvironmentMipChain(environment_image);
    m_source_texture = render_flow::default_scene_detail::createTextureUpload(
        device, environment_image, Texture::SamplerSettings{}, "SceneEnvironmentSource");

    LUNA_RENDERER_INFO("Prepared scene environment source texture ({}x{}, mips={})",
                       environment_image.Width,
                       environment_image.Height,
                       1u + static_cast<uint32_t>(environment_image.MipLevels.size()));
}

void EnvironmentResources::uploadIfNeeded(luna::RHI::CommandBufferEncoder& commands)
{
    if (!m_source_texture.uploaded) {
        LUNA_RENDERER_DEBUG("Uploading environment source texture '{}'", m_source_texture.debug_name);
    }
    render_flow::default_scene_detail::uploadTextureIfNeeded(
        commands, m_source_texture, luna::RHI::ResourceState::ShaderRead, luna::RHI::SyncScope::FragmentStage);
}

} // namespace luna::render_flow::default_scene
