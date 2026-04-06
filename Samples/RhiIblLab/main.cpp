#include "Core/application.h"
#include "Core/layer.h"
#include "Core/log.h"
#include "Events/mouse_event.h"
#include "IblLabPipeline.h"
#include "IblLabState.h"
#include "RHI/Descriptors.h"
#include "RHI/ResourceLayout.h"
#include "Vulkan/vk_rhi_device.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <string_view>

namespace {

struct CommandLineOptions {
    bool runSelfTest = false;
    std::string selfTestName = "phase1_boot";
};

struct SelfTestResult {
    bool passed = false;
};

constexpr std::array<std::string_view, 17> kSelfTestNames = {
    "phase1_boot",
    "phase2_enum_contract",
    "phase2_cube_desc_validation",
    "phase2_cube_view_validation",
    "phase2_sampler_contract",
    "phase3_cube_image_create",
    "phase3_cube_full_view",
    "phase3_cube_face_view",
    "phase3_sampler_vk_mapping",
    "phase4_cube_faces_distinct",
    "phase4_skybox_rotation",
    "phase4_face_mip_preview",
    "phase4_face_isolation",
    "phase5_env_mips",
    "phase6_irradiance_smooth",
    "phase6_prefilter_mips",
    "phase6_brdf_lut_preview",
};

const char* bool_text(bool value)
{
    return value ? "true" : "false";
}

bool contains_token(std::string_view text, std::string_view token)
{
    return !token.empty() && text.find(token) != std::string_view::npos;
}

luna::VulkanRHIDevice* get_vulkan_rhi_device()
{
    luna::IRHIDevice* device = luna::Application::get().getRenderService().getRHIDevice();
    return dynamic_cast<luna::VulkanRHIDevice*>(device);
}

luna::ImageDesc make_cube_image_desc(std::string_view debugName, uint32_t size = 256, uint32_t mipLevels = 6)
{
    luna::ImageDesc desc{};
    desc.width = size;
    desc.height = size;
    desc.depth = 1;
    desc.mipLevels = mipLevels;
    desc.arrayLayers = 6;
    desc.type = luna::ImageType::Cube;
    desc.format = luna::PixelFormat::RGBA16Float;
    desc.usage = luna::ImageUsage::Sampled | luna::ImageUsage::ColorAttachment;
    desc.debugName = debugName;
    return desc;
}

bool cube_flag_enabled(vk::ImageCreateFlags flags)
{
    return static_cast<bool>(flags & vk::ImageCreateFlagBits::eCubeCompatible);
}

void destroy_if_valid(luna::VulkanRHIDevice& device, luna::ResourceSetHandle handle)
{
    if (handle.isValid()) {
        device.destroyResourceSet(handle);
    }
}

void destroy_if_valid(luna::VulkanRHIDevice& device, luna::ResourceLayoutHandle handle)
{
    if (handle.isValid()) {
        device.destroyResourceLayout(handle);
    }
}

void destroy_if_valid(luna::VulkanRHIDevice& device, luna::SamplerHandle handle)
{
    if (handle.isValid()) {
        device.destroySampler(handle);
    }
}

void destroy_if_valid(luna::VulkanRHIDevice& device, luna::ImageViewHandle handle)
{
    if (handle.isValid()) {
        device.destroyImageView(handle);
    }
}

void destroy_if_valid(luna::VulkanRHIDevice& device, luna::ImageHandle handle)
{
    if (handle.isValid()) {
        device.destroyImage(handle);
    }
}

bool parse_arguments(int argc, char** argv, CommandLineOptions* options)
{
    if (options == nullptr) {
        return false;
    }

    constexpr std::string_view kSelfTestPrefix = "--self-test=";

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--self-test") {
            options->runSelfTest = true;
            continue;
        }

        if (argument.substr(0, kSelfTestPrefix.size()) == kSelfTestPrefix) {
            const std::string_view selfTestName = argument.substr(kSelfTestPrefix.size());
            if (std::find(kSelfTestNames.begin(), kSelfTestNames.end(), selfTestName) == kSelfTestNames.end()) {
                LUNA_CORE_ERROR("Unknown self-test '{}'", selfTestName);
                return false;
            }

            options->runSelfTest = true;
            options->selfTestName = std::string(selfTestName);
            continue;
        }

        LUNA_CORE_ERROR("Unknown argument '{}'", argument);
        return false;
    }

    return true;
}

bool log_cube_desc_case(std::string_view label,
                        const luna::ImageDesc& desc,
                        bool expectLegal,
                        std::string_view expectedReason = {})
{
    const std::string_view reason = luna::cube_image_desc_contract_error(desc);
    const bool legal = reason.empty();
    if (legal) {
        LUNA_CORE_INFO("{} legal type={} extent={}x{} depth={} arrayLayers={} mips={} format={}",
                       label,
                       luna::to_string(desc.type),
                       desc.width,
                       desc.height,
                       desc.depth,
                       desc.arrayLayers,
                       desc.mipLevels,
                       luna::to_string(desc.format));
    } else {
        LUNA_CORE_INFO("{} illegal reason='{}' type={} extent={}x{} depth={} arrayLayers={} mips={} format={}",
                       label,
                       reason,
                       luna::to_string(desc.type),
                       desc.width,
                       desc.height,
                       desc.depth,
                       desc.arrayLayers,
                       desc.mipLevels,
                       luna::to_string(desc.format));
    }

    return legal == expectLegal && (expectedReason.empty() || contains_token(reason, expectedReason));
}

bool log_cube_view_case(std::string_view label,
                        const luna::ImageDesc& imageDesc,
                        const luna::ImageViewDesc& viewDesc,
                        bool expectLegal,
                        std::string_view expectedReason = {})
{
    const std::string_view reason = luna::cube_view_desc_contract_error(imageDesc, viewDesc);
    const bool legal = reason.empty();
    if (legal) {
        LUNA_CORE_INFO("{} type={} baseMip={} mipCount={} baseLayer={} layerCount={}",
                       label,
                       luna::to_string(viewDesc.type),
                       viewDesc.baseMipLevel,
                       viewDesc.mipCount,
                       viewDesc.baseArrayLayer,
                       viewDesc.layerCount);
    } else {
        LUNA_CORE_INFO("{} rejected reason='{}' type={} baseMip={} mipCount={} baseLayer={} layerCount={}",
                       label,
                       reason,
                       luna::to_string(viewDesc.type),
                       viewDesc.baseMipLevel,
                       viewDesc.mipCount,
                       viewDesc.baseArrayLayer,
                       viewDesc.layerCount);
    }

    return legal == expectLegal && (expectedReason.empty() || contains_token(reason, expectedReason));
}

void log_sampler_attempt(std::string_view label, const luna::SamplerDesc& desc)
{
    LUNA_CORE_INFO(
        "{} magFilter={} minFilter={} mipmapMode={} addressModeU={} addressModeV={} addressModeW={} mipLodBias={} minLod={} maxLod={} anisotropyEnable={} maxAnisotropy={} compareEnable={} compareOp={} borderColor={}",
        label,
        luna::to_string(desc.magFilter),
        luna::to_string(desc.minFilter),
        luna::to_string(desc.mipmapMode),
        luna::to_string(desc.addressModeU),
        luna::to_string(desc.addressModeV),
        luna::to_string(desc.addressModeW),
        desc.mipLodBias,
        desc.minLod,
        desc.maxLod,
        bool_text(desc.anisotropyEnable),
        desc.maxAnisotropy,
        bool_text(desc.compareEnable),
        luna::to_string(desc.compareOp),
        luna::to_string(desc.borderColor));
}

bool run_phase2_enum_contract_self_test()
{
    const bool passed = luna::to_string(luna::ImageType::Cube) == "Cube" &&
                        luna::to_string(luna::ImageViewType::Cube) == "Cube";
    if (passed) {
        LUNA_CORE_INFO("Cube enum contract PASS imageType={} imageViewType={}",
                       luna::to_string(luna::ImageType::Cube),
                       luna::to_string(luna::ImageViewType::Cube));
    } else {
        LUNA_CORE_ERROR("Cube enum contract FAIL imageType={} imageViewType={}",
                        luna::to_string(luna::ImageType::Cube),
                        luna::to_string(luna::ImageViewType::Cube));
    }
    return passed;
}

bool run_phase2_cube_desc_validation_self_test()
{
    luna::ImageDesc legalDesc{};
    legalDesc.width = 512;
    legalDesc.height = 512;
    legalDesc.depth = 1;
    legalDesc.mipLevels = 10;
    legalDesc.arrayLayers = 6;
    legalDesc.type = luna::ImageType::Cube;
    legalDesc.format = luna::PixelFormat::RGBA16Float;
    legalDesc.usage = luna::ImageUsage::Sampled | luna::ImageUsage::ColorAttachment;
    legalDesc.debugName = "Phase2CubeDescLegal";

    luna::ImageDesc badArrayLayers = legalDesc;
    badArrayLayers.arrayLayers = 5;

    luna::ImageDesc badDepth = legalDesc;
    badDepth.depth = 2;

    luna::ImageDesc badType = legalDesc;
    badType.type = luna::ImageType::Image2DArray;

    const bool passed = log_cube_desc_case("Cube desc legal", legalDesc, true) &&
                        log_cube_desc_case("Cube desc illegal arrayLayers", badArrayLayers, false, "arrayLayers != 6") &&
                        log_cube_desc_case("Cube desc illegal depth", badDepth, false, "depth != 1") &&
                        log_cube_desc_case("Cube desc illegal type mismatch", badType, false, "type mismatch");
    if (passed) {
        LUNA_CORE_INFO("Cube image desc validation PASS");
    } else {
        LUNA_CORE_ERROR("Cube image desc validation FAIL");
    }
    return passed;
}

bool run_phase2_cube_view_validation_self_test()
{
    luna::ImageDesc imageDesc{};
    imageDesc.width = 256;
    imageDesc.height = 256;
    imageDesc.depth = 1;
    imageDesc.mipLevels = 6;
    imageDesc.arrayLayers = 6;
    imageDesc.type = luna::ImageType::Cube;
    imageDesc.format = luna::PixelFormat::RGBA16Float;
    imageDesc.usage = luna::ImageUsage::Sampled | luna::ImageUsage::ColorAttachment;

    luna::ImageViewDesc cubeView{};
    cubeView.image = luna::ImageHandle::fromRaw(1);
    cubeView.type = luna::ImageViewType::Cube;
    cubeView.aspect = luna::ImageAspect::Color;
    cubeView.format = imageDesc.format;
    cubeView.baseMipLevel = 0;
    cubeView.mipCount = imageDesc.mipLevels;
    cubeView.baseArrayLayer = 0;
    cubeView.layerCount = 6;

    luna::ImageViewDesc faceView = cubeView;
    faceView.type = luna::ImageViewType::Image2D;
    faceView.baseArrayLayer = 3;
    faceView.layerCount = 1;

    luna::ImageViewDesc badCubeLayerCount = cubeView;
    badCubeLayerCount.layerCount = 5;

    luna::ImageViewDesc badFaceLayerCount = faceView;
    badFaceLayerCount.layerCount = 2;

    const bool passed = log_cube_view_case("Cube view legal", imageDesc, cubeView, true) &&
                        log_cube_view_case("Face 2D view legal", imageDesc, faceView, true) &&
                        log_cube_view_case("Cube view illegal layerCount",
                                           imageDesc,
                                           badCubeLayerCount,
                                           false,
                                           "layer count != 6") &&
                        log_cube_view_case("Face 2D view illegal layerCount",
                                           imageDesc,
                                           badFaceLayerCount,
                                           false,
                                           "face view layer count != 1");
    if (passed) {
        LUNA_CORE_INFO("Cube view contract PASS");
    } else {
        LUNA_CORE_ERROR("Cube view contract FAIL");
    }
    return passed;
}

bool run_phase2_sampler_contract_self_test()
{
    luna::SamplerDesc defaultLinear{};
    defaultLinear.debugName = "Phase2DefaultLinear";

    luna::SamplerDesc anisotropic = defaultLinear;
    anisotropic.debugName = "Phase2Anisotropic";
    anisotropic.anisotropyEnable = true;
    anisotropic.maxAnisotropy = 8.0f;
    anisotropic.addressModeU = luna::SamplerAddressMode::ClampToEdge;
    anisotropic.addressModeV = luna::SamplerAddressMode::ClampToEdge;
    anisotropic.addressModeW = luna::SamplerAddressMode::ClampToEdge;

    luna::SamplerDesc clampToBorder = defaultLinear;
    clampToBorder.debugName = "Phase2ClampToBorder";
    clampToBorder.addressModeU = luna::SamplerAddressMode::ClampToBorder;
    clampToBorder.addressModeV = luna::SamplerAddressMode::ClampToBorder;
    clampToBorder.addressModeW = luna::SamplerAddressMode::ClampToBorder;
    clampToBorder.borderColor = luna::SamplerBorderColor::FloatOpaqueWhite;
    clampToBorder.compareEnable = true;
    clampToBorder.compareOp = luna::CompareOp::LessOrEqual;
    clampToBorder.mipLodBias = 0.75f;

    log_sampler_attempt("Sampler creation attempt: default linear", defaultLinear);
    log_sampler_attempt("Sampler creation attempt: anisotropy", anisotropic);
    log_sampler_attempt("Sampler creation attempt: ClampToBorder", clampToBorder);

    const bool passed = defaultLinear.magFilter == luna::FilterMode::Linear &&
                        defaultLinear.minFilter == luna::FilterMode::Linear &&
                        !defaultLinear.anisotropyEnable &&
                        anisotropic.anisotropyEnable &&
                        anisotropic.maxAnisotropy == 8.0f &&
                        clampToBorder.addressModeU == luna::SamplerAddressMode::ClampToBorder &&
                        clampToBorder.addressModeV == luna::SamplerAddressMode::ClampToBorder &&
                        clampToBorder.addressModeW == luna::SamplerAddressMode::ClampToBorder &&
                        clampToBorder.compareEnable &&
                        clampToBorder.borderColor == luna::SamplerBorderColor::FloatOpaqueWhite;
    if (passed) {
        LUNA_CORE_INFO("Sampler contract PASS");
    } else {
        LUNA_CORE_ERROR("Sampler contract FAIL");
    }
    return passed;
}

bool run_phase3_cube_image_create_self_test()
{
    luna::VulkanRHIDevice* device = get_vulkan_rhi_device();
    if (device == nullptr) {
        LUNA_CORE_ERROR("phase3_cube_image_create requires VulkanRHIDevice");
        return false;
    }

    const luna::ImageDesc imageDesc = make_cube_image_desc("Phase3CubeImage", 256, 6);
    luna::ImageHandle cubeImage{};
    const luna::RHIResult result = device->createImage(imageDesc, &cubeImage);
    if (result != luna::RHIResult::Success) {
        LUNA_CORE_ERROR("phase3_cube_image_create FAIL createImage result={}", luna::to_string(result));
        return false;
    }

    luna::VulkanRHIDevice::DebugImageInfo debugInfo{};
    const bool debugOk = device->debugGetImageInfo(cubeImage, &debugInfo);
    const bool passed = debugOk && debugInfo.desc.type == luna::ImageType::Cube &&
                        debugInfo.backendImageType == vk::ImageType::e2D &&
                        debugInfo.backendDefaultViewType == vk::ImageViewType::eCube &&
                        debugInfo.backendLayerCount == 6 &&
                        cube_flag_enabled(debugInfo.backendCreateFlags);

    LUNA_CORE_INFO("Cube image created size={}x{} mips={} faces={} backendImageType={} defaultViewType={} cubeCompatible={}",
                   imageDesc.width,
                   imageDesc.height,
                   imageDesc.mipLevels,
                   debugInfo.backendLayerCount,
                   vk::to_string(debugInfo.backendImageType),
                   vk::to_string(debugInfo.backendDefaultViewType),
                   bool_text(cube_flag_enabled(debugInfo.backendCreateFlags)));

    destroy_if_valid(*device, cubeImage);

    if (!passed) {
        LUNA_CORE_ERROR("phase3_cube_image_create FAIL debugOk={} backendImageType={} defaultViewType={} faces={} cubeCompatible={}",
                        bool_text(debugOk),
                        debugOk ? vk::to_string(debugInfo.backendImageType) : std::string("Unavailable"),
                        debugOk ? vk::to_string(debugInfo.backendDefaultViewType) : std::string("Unavailable"),
                        debugOk ? debugInfo.backendLayerCount : 0u,
                        debugOk ? bool_text(cube_flag_enabled(debugInfo.backendCreateFlags)) : "false");
        return false;
    }

    LUNA_CORE_INFO("phase3_cube_image_create PASS");
    return true;
}

bool run_phase3_cube_full_view_self_test()
{
    luna::VulkanRHIDevice* device = get_vulkan_rhi_device();
    if (device == nullptr) {
        LUNA_CORE_ERROR("phase3_cube_full_view requires VulkanRHIDevice");
        return false;
    }

    const luna::ImageDesc imageDesc = make_cube_image_desc("Phase3CubeFullViewImage", 256, 6);
    luna::ImageHandle cubeImage{};
    luna::ImageViewHandle cubeView{};
    luna::SamplerHandle sampler{};
    luna::ResourceLayoutHandle layout{};
    luna::ResourceSetHandle set{};

    const auto cleanup = [&]() {
        destroy_if_valid(*device, set);
        destroy_if_valid(*device, layout);
        destroy_if_valid(*device, sampler);
        destroy_if_valid(*device, cubeView);
        destroy_if_valid(*device, cubeImage);
    };

    if (device->createImage(imageDesc, &cubeImage) != luna::RHIResult::Success) {
        LUNA_CORE_ERROR("phase3_cube_full_view FAIL createImage");
        cleanup();
        return false;
    }

    luna::ImageViewDesc cubeViewDesc{};
    cubeViewDesc.image = cubeImage;
    cubeViewDesc.type = luna::ImageViewType::Cube;
    cubeViewDesc.aspect = luna::ImageAspect::Color;
    cubeViewDesc.baseMipLevel = 0;
    cubeViewDesc.mipCount = imageDesc.mipLevels;
    cubeViewDesc.baseArrayLayer = 0;
    cubeViewDesc.layerCount = 6;
    cubeViewDesc.debugName = "Phase3CubeFullView";
    if (device->createImageView(cubeViewDesc, &cubeView) != luna::RHIResult::Success) {
        LUNA_CORE_ERROR("phase3_cube_full_view FAIL createImageView");
        cleanup();
        return false;
    }

    luna::SamplerDesc samplerDesc{};
    samplerDesc.debugName = "Phase3CubeFullViewSampler";
    samplerDesc.addressModeU = luna::SamplerAddressMode::ClampToEdge;
    samplerDesc.addressModeV = luna::SamplerAddressMode::ClampToEdge;
    samplerDesc.addressModeW = luna::SamplerAddressMode::ClampToEdge;
    if (device->createSampler(samplerDesc, &sampler) != luna::RHIResult::Success) {
        LUNA_CORE_ERROR("phase3_cube_full_view FAIL createSampler");
        cleanup();
        return false;
    }

    luna::ResourceLayoutDesc layoutDesc{};
    layoutDesc.debugName = "Phase3CubeFullViewLayout";
    layoutDesc.bindings.push_back({0, luna::ResourceType::CombinedImageSampler, 1, luna::ShaderType::Fragment});
    if (device->createResourceLayout(layoutDesc, &layout) != luna::RHIResult::Success ||
        device->createResourceSet(layout, &set) != luna::RHIResult::Success) {
        LUNA_CORE_ERROR("phase3_cube_full_view FAIL createResourceLayout/set");
        cleanup();
        return false;
    }

    luna::ResourceSetWriteDesc writeDesc{};
    writeDesc.images.push_back({.binding = 0,
                                .imageView = cubeView,
                                .sampler = sampler,
                                .type = luna::ResourceType::CombinedImageSampler});
    const luna::RHIResult updateResult = device->updateResourceSet(set, writeDesc);

    luna::VulkanRHIDevice::DebugImageViewInfo debugInfo{};
    const bool debugOk = device->debugGetImageViewInfo(cubeView, &debugInfo);
    const bool passed = updateResult == luna::RHIResult::Success && debugOk &&
                        debugInfo.backendViewType == vk::ImageViewType::eCube;

    LUNA_CORE_INFO("Cube view created handle={} backendViewType={} descriptorUpdate={}",
                   cubeView.value,
                   debugOk ? vk::to_string(debugInfo.backendViewType) : std::string("Unavailable"),
                   luna::to_string(updateResult));

    cleanup();

    if (!passed) {
        LUNA_CORE_ERROR("phase3_cube_full_view FAIL debugOk={} descriptorUpdate={}",
                        bool_text(debugOk),
                        luna::to_string(updateResult));
        return false;
    }

    LUNA_CORE_INFO("phase3_cube_full_view PASS");
    return true;
}

bool run_phase3_cube_face_view_self_test()
{
    luna::VulkanRHIDevice* device = get_vulkan_rhi_device();
    if (device == nullptr) {
        LUNA_CORE_ERROR("phase3_cube_face_view requires VulkanRHIDevice");
        return false;
    }

    const luna::ImageDesc imageDesc = make_cube_image_desc("Phase3CubeFaceViewImage", 256, 6);
    luna::ImageHandle cubeImage{};
    luna::ImageViewHandle cubeSrvView{};
    luna::ImageViewHandle faceViewA{};
    luna::ImageViewHandle faceViewB{};

    const auto cleanup = [&]() {
        destroy_if_valid(*device, faceViewB);
        destroy_if_valid(*device, faceViewA);
        destroy_if_valid(*device, cubeSrvView);
        destroy_if_valid(*device, cubeImage);
    };

    if (device->createImage(imageDesc, &cubeImage) != luna::RHIResult::Success) {
        LUNA_CORE_ERROR("phase3_cube_face_view FAIL createImage");
        cleanup();
        return false;
    }

    luna::ImageViewDesc cubeSrvDesc{};
    cubeSrvDesc.image = cubeImage;
    cubeSrvDesc.type = luna::ImageViewType::Cube;
    cubeSrvDesc.aspect = luna::ImageAspect::Color;
    cubeSrvDesc.baseMipLevel = 0;
    cubeSrvDesc.mipCount = imageDesc.mipLevels;
    cubeSrvDesc.baseArrayLayer = 0;
    cubeSrvDesc.layerCount = 6;
    cubeSrvDesc.debugName = "Phase3CubeSrvView";
    if (device->createImageView(cubeSrvDesc, &cubeSrvView) != luna::RHIResult::Success) {
        LUNA_CORE_ERROR("phase3_cube_face_view FAIL create cube SRV view");
        cleanup();
        return false;
    }

    luna::ImageViewDesc faceViewDescA{};
    faceViewDescA.image = cubeImage;
    faceViewDescA.type = luna::ImageViewType::Image2D;
    faceViewDescA.aspect = luna::ImageAspect::Color;
    faceViewDescA.baseMipLevel = 0;
    faceViewDescA.mipCount = 1;
    faceViewDescA.baseArrayLayer = 0;
    faceViewDescA.layerCount = 1;
    faceViewDescA.debugName = "Phase3CubeFaceViewA";

    luna::ImageViewDesc faceViewDescB = faceViewDescA;
    faceViewDescB.baseMipLevel = 2;
    faceViewDescB.baseArrayLayer = 4;
    faceViewDescB.debugName = "Phase3CubeFaceViewB";

    if (device->createImageView(faceViewDescA, &faceViewA) != luna::RHIResult::Success ||
        device->createImageView(faceViewDescB, &faceViewB) != luna::RHIResult::Success) {
        LUNA_CORE_ERROR("phase3_cube_face_view FAIL create face attachment views");
        cleanup();
        return false;
    }

    luna::VulkanRHIDevice::DebugImageViewInfo cubeSrvInfo{};
    luna::VulkanRHIDevice::DebugImageViewInfo faceInfoA{};
    luna::VulkanRHIDevice::DebugImageViewInfo faceInfoB{};
    const bool debugOk = device->debugGetImageViewInfo(cubeSrvView, &cubeSrvInfo) &&
                         device->debugGetImageViewInfo(faceViewA, &faceInfoA) &&
                         device->debugGetImageViewInfo(faceViewB, &faceInfoB);
    const bool distinctHandles = cubeSrvView != faceViewA && cubeSrvView != faceViewB && faceViewA != faceViewB;
    const bool passed = debugOk && distinctHandles &&
                        cubeSrvInfo.backendViewType == vk::ImageViewType::eCube &&
                        faceInfoA.backendViewType == vk::ImageViewType::e2D &&
                        faceInfoB.backendViewType == vk::ImageViewType::e2D;

    LUNA_CORE_INFO("Cube SRV view handle={} backendViewType={}",
                   cubeSrvView.value,
                   debugOk ? vk::to_string(cubeSrvInfo.backendViewType) : std::string("Unavailable"));
    LUNA_CORE_INFO("Face attachment view created handle={} face={} mip={} backendViewType={}",
                   faceViewA.value,
                   faceViewDescA.baseArrayLayer,
                   faceViewDescA.baseMipLevel,
                   debugOk ? vk::to_string(faceInfoA.backendViewType) : std::string("Unavailable"));
    LUNA_CORE_INFO("Face attachment view created handle={} face={} mip={} backendViewType={}",
                   faceViewB.value,
                   faceViewDescB.baseArrayLayer,
                   faceViewDescB.baseMipLevel,
                   debugOk ? vk::to_string(faceInfoB.backendViewType) : std::string("Unavailable"));

    cleanup();

    if (!passed) {
        LUNA_CORE_ERROR("phase3_cube_face_view FAIL debugOk={} distinctHandles={}",
                        bool_text(debugOk),
                        bool_text(distinctHandles));
        return false;
    }

    LUNA_CORE_INFO("phase3_cube_face_view PASS");
    return true;
}

bool run_phase3_sampler_vk_mapping_self_test()
{
    luna::VulkanRHIDevice* device = get_vulkan_rhi_device();
    if (device == nullptr) {
        LUNA_CORE_ERROR("phase3_sampler_vk_mapping requires VulkanRHIDevice");
        return false;
    }

    luna::SamplerHandle defaultLinear{};
    luna::SamplerHandle anisotropic{};
    luna::SamplerHandle clampToBorder{};

    const auto cleanup = [&]() {
        destroy_if_valid(*device, clampToBorder);
        destroy_if_valid(*device, anisotropic);
        destroy_if_valid(*device, defaultLinear);
    };

    luna::SamplerDesc defaultLinearDesc{};
    defaultLinearDesc.debugName = "Phase3DefaultLinear";

    luna::SamplerDesc anisotropicDesc = defaultLinearDesc;
    anisotropicDesc.debugName = "Phase3Anisotropic";
    anisotropicDesc.anisotropyEnable = true;
    anisotropicDesc.maxAnisotropy = std::min(8.0f, device->debugGetMaxSamplerAnisotropy());
    anisotropicDesc.addressModeU = luna::SamplerAddressMode::ClampToEdge;
    anisotropicDesc.addressModeV = luna::SamplerAddressMode::ClampToEdge;
    anisotropicDesc.addressModeW = luna::SamplerAddressMode::ClampToEdge;

    luna::SamplerDesc clampDesc = defaultLinearDesc;
    clampDesc.debugName = "Phase3ClampToBorder";
    clampDesc.addressModeU = luna::SamplerAddressMode::ClampToBorder;
    clampDesc.addressModeV = luna::SamplerAddressMode::ClampToBorder;
    clampDesc.addressModeW = luna::SamplerAddressMode::ClampToBorder;
    clampDesc.borderColor = luna::SamplerBorderColor::FloatOpaqueWhite;
    clampDesc.compareEnable = true;
    clampDesc.compareOp = luna::CompareOp::LessOrEqual;
    clampDesc.mipLodBias = 0.5f;

    const luna::RHIResult defaultLinearResult = device->createSampler(defaultLinearDesc, &defaultLinear);
    const luna::RHIResult anisotropicResult = device->createSampler(anisotropicDesc, &anisotropic);
    const luna::RHIResult clampResult = device->createSampler(clampDesc, &clampToBorder);

    luna::VulkanRHIDevice::DebugSamplerInfo defaultLinearInfo{};
    luna::VulkanRHIDevice::DebugSamplerInfo anisotropicInfo{};
    luna::VulkanRHIDevice::DebugSamplerInfo clampInfo{};
    const bool debugOk = defaultLinearResult == luna::RHIResult::Success &&
                         anisotropicResult == luna::RHIResult::Success &&
                         clampResult == luna::RHIResult::Success &&
                         device->debugGetSamplerInfo(defaultLinear, &defaultLinearInfo) &&
                         device->debugGetSamplerInfo(anisotropic, &anisotropicInfo) &&
                         device->debugGetSamplerInfo(clampToBorder, &clampInfo);

    LUNA_CORE_INFO("Vulkan sampler landed defaultLinear addressModeU={} anisotropyEnable={} compareEnable={} borderColor={}",
                   debugOk ? vk::to_string(defaultLinearInfo.addressModeU) : std::string("Unavailable"),
                   debugOk ? bool_text(defaultLinearInfo.anisotropyEnable) : "false",
                   debugOk ? bool_text(defaultLinearInfo.compareEnable) : "false",
                   debugOk ? vk::to_string(defaultLinearInfo.borderColor) : std::string("Unavailable"));
    LUNA_CORE_INFO("Vulkan sampler landed anisotropic addressModeU={} anisotropyEnable={} maxAnisotropy={}",
                   debugOk ? vk::to_string(anisotropicInfo.addressModeU) : std::string("Unavailable"),
                   debugOk ? bool_text(anisotropicInfo.anisotropyEnable) : "false",
                   debugOk ? anisotropicInfo.maxAnisotropy : 0.0f);
    LUNA_CORE_INFO("Vulkan sampler landed clampToBorder addressModeU={} compareEnable={} borderColor={} mipLodBias={}",
                   debugOk ? vk::to_string(clampInfo.addressModeU) : std::string("Unavailable"),
                   debugOk ? bool_text(clampInfo.compareEnable) : "false",
                   debugOk ? vk::to_string(clampInfo.borderColor) : std::string("Unavailable"),
                   debugOk ? clampInfo.mipLodBias : 0.0f);

    luna::SamplerDesc overLimitDesc = anisotropicDesc;
    overLimitDesc.debugName = "Phase3OverLimitAnisotropy";
    overLimitDesc.maxAnisotropy = device->debugGetMaxSamplerAnisotropy() + 1.0f;
    luna::SamplerHandle overLimitHandle{};
    const luna::RHIResult overLimitResult = device->createSampler(overLimitDesc, &overLimitHandle);
    if (overLimitHandle.isValid()) {
        destroy_if_valid(*device, overLimitHandle);
    }
    LUNA_CORE_INFO("Over-limit anisotropy test requested={} deviceLimit={} result={}",
                   overLimitDesc.maxAnisotropy,
                   device->debugGetMaxSamplerAnisotropy(),
                   luna::to_string(overLimitResult));

    const bool passed = debugOk &&
                        defaultLinearInfo.addressModeU == vk::SamplerAddressMode::eRepeat &&
                        !defaultLinearInfo.anisotropyEnable &&
                        anisotropicInfo.addressModeU == vk::SamplerAddressMode::eClampToEdge &&
                        anisotropicInfo.anisotropyEnable &&
                        clampInfo.addressModeU == vk::SamplerAddressMode::eClampToBorder &&
                        clampInfo.compareEnable &&
                        clampInfo.borderColor == vk::BorderColor::eFloatOpaqueWhite &&
                        overLimitResult != luna::RHIResult::Success;

    cleanup();

    if (!passed) {
        LUNA_CORE_ERROR("phase3_sampler_vk_mapping FAIL debugOk={} default={} anisotropic={} clamp={} overLimit={}",
                        bool_text(debugOk),
                        luna::to_string(defaultLinearResult),
                        luna::to_string(anisotropicResult),
                        luna::to_string(clampResult),
                        luna::to_string(overLimitResult));
        return false;
    }

    LUNA_CORE_INFO("phase3_sampler_vk_mapping PASS");
    return true;
}

class RhiIblLabLayer final : public luna::Layer {
public:
    explicit RhiIblLabLayer(std::shared_ptr<ibl_lab::State> state)
        : luna::Layer("RhiIblLabLayer"),
          m_state(std::move(state))
    {}

    void onEvent(luna::Event& event) override
    {
        if (m_state == nullptr || m_state->page != ibl_lab::Page::Skybox) {
            m_dragging = false;
            return;
        }
        if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse) {
            return;
        }

        luna::EventDispatcher dispatcher(event);
        dispatcher.dispatch<luna::MouseButtonPressedEvent>([this](luna::MouseButtonPressedEvent& pressed) {
            if (pressed.getMouseButton() == luna::MouseCode::Left) {
                m_dragging = true;
                return true;
            }
            return false;
        });
        dispatcher.dispatch<luna::MouseButtonReleasedEvent>([this](luna::MouseButtonReleasedEvent& released) {
            if (released.getMouseButton() == luna::MouseCode::Left) {
                m_dragging = false;
                return true;
            }
            return false;
        });
        dispatcher.dispatch<luna::MouseMovedEvent>([this](luna::MouseMovedEvent& moved) {
            const float x = moved.getX();
            const float y = moved.getY();
            if (m_dragging) {
                const float deltaX = x - m_lastMouseX;
                const float deltaY = y - m_lastMouseY;
                m_state->skyYaw += deltaX * 0.0085f;
                m_state->skyPitch = std::clamp(m_state->skyPitch + deltaY * 0.0085f, -1.3f, 1.3f);
            }
            m_lastMouseX = x;
            m_lastMouseY = y;
            return m_dragging;
        });
    }

    void onImGuiRender() override
    {
        if (m_state == nullptr) {
            return;
        }

        ImGui::SetNextWindowSize(ImVec2(620.0f, 560.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("RhiIblLab")) {
            ImGui::End();
            return;
        }

        ImGui::TextUnformatted("RhiIblLab stage 6 IBL resource chain");
        ImGui::Separator();
        ImGui::Text("Page: %s", page_label(m_state->page));
        ImGui::Text("Frames: %llu", static_cast<unsigned long long>(m_state->frameCounter));
        ImGui::TextWrapped("%s", m_state->renderStatus.c_str());
        ImGui::Separator();

        draw_page_button(ibl_lab::Page::Overview);
        ImGui::SameLine();
        draw_page_button(ibl_lab::Page::SourceHdr);
        ImGui::SameLine();
        draw_page_button(ibl_lab::Page::EnvCube);
        ImGui::SameLine();
        draw_page_button(ibl_lab::Page::Irradiance);
        ImGui::SameLine();
        draw_page_button(ibl_lab::Page::Prefilter);
        ImGui::SameLine();
        draw_page_button(ibl_lab::Page::BrdfLut);
        ImGui::Separator();
        draw_page_button(ibl_lab::Page::Skybox);
        ImGui::SameLine();
        draw_page_button(ibl_lab::Page::Inspector);

        ImGui::Separator();
        switch (m_state->page) {
            case ibl_lab::Page::Overview:
                draw_overview();
                break;
            case ibl_lab::Page::SourceHdr:
                draw_source_hdr();
                break;
            case ibl_lab::Page::EnvCube:
                draw_env_cube();
                break;
            case ibl_lab::Page::Irradiance:
                draw_irradiance();
                break;
            case ibl_lab::Page::Prefilter:
                draw_prefilter();
                break;
            case ibl_lab::Page::BrdfLut:
                draw_brdf_lut();
                break;
            case ibl_lab::Page::Skybox:
                draw_skybox();
                break;
            case ibl_lab::Page::Inspector:
                draw_inspector();
                break;
        }

        ImGui::End();
    }

private:
    static const char* page_label(ibl_lab::Page page)
    {
        switch (page) {
            case ibl_lab::Page::Overview:
                return "Overview";
            case ibl_lab::Page::SourceHdr:
                return "Source HDR";
            case ibl_lab::Page::EnvCube:
                return "Env Cube";
            case ibl_lab::Page::Irradiance:
                return "Irradiance";
            case ibl_lab::Page::Prefilter:
                return "Prefilter";
            case ibl_lab::Page::BrdfLut:
                return "BRDF LUT";
            case ibl_lab::Page::Skybox:
                return "Skybox";
            case ibl_lab::Page::Inspector:
                return "Inspector";
            default:
                return "Unknown";
        }
    }

    static const char* face_label(int faceIndex)
    {
        static const char* kLabels[] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
        return kLabels[std::clamp(faceIndex, 0, 5)];
    }

    void draw_page_button(ibl_lab::Page page)
    {
        const bool selected = m_state->page == page;
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.44f, 0.24f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.52f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.36f, 0.22f, 1.0f));
        }
        if (ImGui::Button(page_label(page), ImVec2(94.0f, 0.0f))) {
            m_state->page = page;
        }
        if (selected) {
            ImGui::PopStyleColor(3);
        }
    }

    void draw_overview() const
    {
        ImGui::Text("Source HDR Ready: %s", bool_text(m_state->sourceHdrReady));
        ImGui::Text("Env Cube Ready: %s", bool_text(m_state->envCubeReady));
        ImGui::Text("Irradiance Ready: %s", bool_text(m_state->irradianceReady));
        ImGui::Text("Prefilter Cube Ready: %s", bool_text(m_state->prefilterReady));
        ImGui::Text("Prefilter Generated: %s", bool_text(m_state->prefilterGenerated));
        ImGui::Text("BRDF LUT Ready: %s", bool_text(m_state->brdfLutReady));
        ImGui::Text("Source Size: %ux%u", m_state->sourceWidth, m_state->sourceHeight);
        ImGui::Text("Env Cube: %ux%u, faces=%u, mips=%u, format=%s",
                    m_state->cubeSize,
                    m_state->cubeSize,
                    m_state->cubeFaceCount,
                    m_state->cubeMipLevels,
                    luna::to_string(luna::PixelFormat::RGBA16Float).data());
        ImGui::Text("Irradiance Cube: %ux%u, faces=%u, mips=%u, format=%s",
                    m_state->irradianceSize,
                    m_state->irradianceSize,
                    m_state->cubeFaceCount,
                    m_state->irradianceMipLevels,
                    luna::to_string(luna::PixelFormat::RGBA16Float).data());
        ImGui::Text("Prefilter Cube: %ux%u, faces=%u, mips=%u, format=%s",
                    m_state->prefilterSize,
                    m_state->prefilterSize,
                    m_state->cubeFaceCount,
                    m_state->prefilterMipLevels,
                    luna::to_string(luna::PixelFormat::RGBA16Float).data());
        ImGui::Text("BRDF LUT: %ux%u, format=%s",
                    m_state->brdfLutWidth,
                    m_state->brdfLutHeight,
                    luna::to_string(luna::PixelFormat::RG16Float).data());
        ImGui::TextWrapped("%s", m_state->sourceHdrStatus.c_str());
        ImGui::TextWrapped("%s", m_state->envCubeStatus.c_str());
        ImGui::TextWrapped("%s", m_state->irradianceStatus.c_str());
        ImGui::TextWrapped("%s", m_state->prefilterStatus.c_str());
        ImGui::TextWrapped("%s", m_state->brdfLutStatus.c_str());
        if (ImGui::Button("Generate Env Cube")) {
            m_state->regenerateEnvCubeRequested = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Generate Irradiance")) {
            m_state->regenerateIrradianceRequested = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Generate Prefilter")) {
            m_state->regeneratePrefilterRequested = true;
        }
        ImGui::Separator();

        ImGui::TextUnformatted("Public Contract");
        ImGui::Text("Image Types: %s / %s / %s / %s",
                    luna::to_string(luna::ImageType::Image2D).data(),
                    luna::to_string(luna::ImageType::Image2DArray).data(),
                    luna::to_string(luna::ImageType::Image3D).data(),
                    luna::to_string(luna::ImageType::Cube).data());
        ImGui::Text("Image Views: %s / %s / %s / %s",
                    luna::to_string(luna::ImageViewType::Image2D).data(),
                    luna::to_string(luna::ImageViewType::Image2DArray).data(),
                    luna::to_string(luna::ImageViewType::Image3D).data(),
                    luna::to_string(luna::ImageViewType::Cube).data());
        ImGui::Text("Cube Face Contract: %s, layerCount = 1, baseArrayLayer = 0..5",
                    luna::to_string(luna::ImageViewType::Image2D).data());

        ImGui::Separator();
        ImGui::TextUnformatted("Required Formats");
        for (const auto& requirement : m_state->requiredFormats) {
            draw_format_requirement(requirement);
        }
        draw_capability_summary();

        ImGui::Separator();
        ImGui::TextUnformatted("Stage Gates");
        draw_status("Source HDR", m_state->sourceHdrReady);
        draw_status("Env Cube", m_state->envCubeReady);
        draw_status("Irradiance", m_state->irradianceReady);
        draw_status("Prefilter", m_state->prefilterGenerated);
        draw_status("BRDF LUT", m_state->brdfLutReady);
        draw_status("Skybox", m_state->envCubeReady);
        draw_status("Face/Mip", m_state->envCubeReady && m_state->cubeMipLevels > 1u);
        draw_status("Stamp", m_state->envCubeReady);
    }

    void draw_source_hdr() const
    {
        ImGui::Text("Path: %s", m_state->sourcePath.empty() ? "<unresolved>" : m_state->sourcePath.c_str());
        ImGui::Text("Resolution: %u x %u", m_state->sourceWidth, m_state->sourceHeight);
        ImGui::Text("Channels: %u", m_state->sourceChannels);
        ImGui::TextWrapped("%s", m_state->sourceHdrStatus.c_str());
        ImGui::Separator();
        ImGui::TextUnformatted("The full equirect HDR preview is rendered behind this UI window.");
    }

    void draw_env_cube()
    {
        ImGui::Text("The runtime-generated HDR cubemap atlas is rendered behind this UI window.");
        int face = std::clamp(m_state->selectedFace, 0, 5);
        ImGui::SliderInt("Preview Face", &face, 0, 5);
        ImGui::SliderFloat("LOD", &m_state->envPreviewLod, 0.0f, static_cast<float>(std::max(1u, m_state->cubeMipLevels) - 1u));
        m_state->selectedFace = face;
        ImGui::Text("Current Face: %s", face_label(face));
        ImGui::Text("Mip Count: %u", m_state->cubeMipLevels);
        if (ImGui::Button("Generate Env Cube")) {
            m_state->regenerateEnvCubeRequested = true;
        }
        ImGui::TextWrapped("%s", m_state->envCubeStatus.c_str());
    }

    void draw_irradiance()
    {
        ImGui::TextUnformatted("The runtime-generated irradiance cubemap atlas is rendered behind this UI window.");
        int face = std::clamp(m_state->irradianceFace, 0, 5);
        ImGui::SliderInt("Preview Face", &face, 0, 5);
        m_state->irradianceFace = face;
        ImGui::Text("Current Face: %s", face_label(face));
        ImGui::Text("Cube Size: %u", m_state->irradianceSize);
        ImGui::Text("Mip Count: %u", m_state->irradianceMipLevels);
        if (ImGui::Button("Generate Irradiance")) {
            m_state->regenerateIrradianceRequested = true;
        }
        ImGui::TextWrapped("%s", m_state->irradianceStatus.c_str());
    }

    void draw_prefilter()
    {
        ImGui::TextUnformatted("The runtime-generated roughness prefilter cubemap atlas is rendered behind this UI window.");
        int face = std::clamp(m_state->prefilterFace, 0, 5);
        int mip = std::clamp(m_state->prefilterMip, 0, static_cast<int>(std::max(1u, m_state->prefilterMipLevels) - 1u));
        ImGui::SliderInt("Preview Face", &face, 0, 5);
        ImGui::SliderInt("Mip Level", &mip, 0, static_cast<int>(std::max(1u, m_state->prefilterMipLevels) - 1u));
        m_state->prefilterFace = face;
        m_state->prefilterMip = mip;
        const float roughness = m_state->prefilterMipLevels > 1u
                                    ? static_cast<float>(mip) / static_cast<float>(m_state->prefilterMipLevels - 1u)
                                    : 0.0f;
        ImGui::Text("Current Face: %s", face_label(face));
        ImGui::Text("Mapped Roughness: %.2f", roughness);
        ImGui::Text("Cube Size: %u", m_state->prefilterSize);
        ImGui::Text("Mip Count: %u", m_state->prefilterMipLevels);
        if (ImGui::Button("Generate Prefilter")) {
            m_state->regeneratePrefilterRequested = true;
        }
        ImGui::TextWrapped("%s", m_state->prefilterStatus.c_str());
    }

    void draw_brdf_lut() const
    {
        ImGui::Text("Path: %s", m_state->brdfLutPath.empty() ? "<unresolved>" : m_state->brdfLutPath.c_str());
        ImGui::Text("Resolution: %u x %u", m_state->brdfLutWidth, m_state->brdfLutHeight);
        ImGui::Text("Format: %s", luna::to_string(luna::PixelFormat::RG16Float).data());
        ImGui::TextWrapped("%s", m_state->brdfLutStatus.c_str());
        ImGui::Separator();
        ImGui::TextUnformatted("The offline BRDF LUT preview is rendered behind this UI window.");
    }

    void draw_skybox()
    {
        ImGui::TextUnformatted("Drag with left mouse button to rotate the skybox.");
        ImGui::SliderFloat("Yaw", &m_state->skyYaw, -3.14f, 3.14f);
        ImGui::SliderFloat("Pitch", &m_state->skyPitch, -1.3f, 1.3f);
        if (ImGui::Button("Reset Camera")) {
            m_state->skyYaw = 0.0f;
            m_state->skyPitch = 0.0f;
        }
        ImGui::TextWrapped("%s", m_state->skyboxStatus.c_str());
    }

    void draw_inspector()
    {
        int face = std::clamp(m_state->selectedFace, 0, 5);
        int mip = std::clamp(m_state->selectedMip, 0, static_cast<int>(std::max(1u, m_state->cubeMipLevels) - 1u));
        ImGui::SliderInt("Face Index", &face, 0, 5);
        ImGui::SliderInt("Mip Level", &mip, 0, static_cast<int>(std::max(1u, m_state->cubeMipLevels) - 1u));
        m_state->selectedFace = face;
        m_state->selectedMip = mip;
        ImGui::Text("Selected Face: %s", face_label(face));
        if (ImGui::Button("Stamp Selected Face")) {
            m_state->stampRequested = true;
        }
        ImGui::TextWrapped("%s", m_state->inspectorStatus.c_str());
        if (m_state->probe.ready) {
            ImGui::Separator();
            ImGui::Text("Last Probe: %s", m_state->probe.passed ? "PASS" : "FAIL");
            ImGui::TextWrapped("%s", m_state->probe.summary.c_str());
        }
    }

    static void draw_status(const char* label, bool ready)
    {
        const ImVec4 readyColor = ImVec4(0.22f, 0.72f, 0.32f, 1.0f);
        const ImVec4 blockedColor = ImVec4(0.88f, 0.24f, 0.24f, 1.0f);
        ImGui::Text("%-12s", label);
        ImGui::SameLine(110.0f);
        ImGui::TextColored(ready ? readyColor : blockedColor, "%s", ready ? "Ready" : "Pending");
    }

    void draw_format_requirement(const ibl_lab::FormatRequirementStatus& requirement) const
    {
        const ImVec4 readyColor = ImVec4(0.22f, 0.72f, 0.32f, 1.0f);
        const ImVec4 blockedColor = ImVec4(0.88f, 0.24f, 0.24f, 1.0f);
        const bool ready = requirement.ready;
        ImGui::Text("%s", luna::to_string(requirement.format).data());
        ImGui::SameLine(150.0f);
        ImGui::TextColored(ready ? readyColor : blockedColor, "%s", ready ? "Ready" : "Blocked");
        ImGui::SameLine(245.0f);
        ImGui::TextUnformatted(requirement.purpose);
        ImGui::TextDisabled("sampled=%s colorAttachment=%s backend=%s",
                            bool_text(requirement.support.sampled),
                            bool_text(requirement.support.colorAttachment),
                            requirement.support.backendFormatName.data());
    }

    void draw_capability_summary() const
    {
        const ImVec4 readyColor = ImVec4(0.22f, 0.72f, 0.32f, 1.0f);
        const ImVec4 blockedColor = ImVec4(0.88f, 0.24f, 0.24f, 1.0f);
        ImGui::TextColored(m_state->anyBlockedFormats ? blockedColor : readyColor,
                           "%s",
                           m_state->anyBlockedFormats ? "Blocked" : "Ready");
        ImGui::SameLine();
        ImGui::TextWrapped("%s", m_state->capabilityStatus.c_str());
    }

private:
    std::shared_ptr<ibl_lab::State> m_state;
    bool m_dragging = false;
    float m_lastMouseX = 0.0f;
    float m_lastMouseY = 0.0f;
};

class RhiIblLabSelfTestLayer final : public luna::Layer {
public:
    RhiIblLabSelfTestLayer(std::shared_ptr<ibl_lab::State> state,
                           std::shared_ptr<SelfTestResult> result,
                           std::string selfTestName)
        : luna::Layer("RhiIblLabSelfTestLayer"),
          m_state(std::move(state)),
          m_result(std::move(result)),
          m_selfTestName(std::move(selfTestName))
    {}

    void onAttach() override
    {
        LUNA_CORE_INFO("RhiIblLab self-test begin: {}", m_selfTestName);
        if (m_selfTestName == "phase1_boot" || is_probe_test()) {
            return;
        }

        m_result->passed = run_contract_test();
        luna::Application::get().close();
    }

    void onUpdate(luna::Timestep) override
    {
        if (m_state == nullptr || m_result == nullptr) {
            return;
        }

        if (m_selfTestName == "phase1_boot") {
            ++m_tick;
            if (m_state->pipelineReady && m_state->frameCounter >= 3) {
                m_result->passed = true;
                LUNA_CORE_INFO("RhiIblLab self-test PASS frameCounter={}", m_state->frameCounter);
                luna::Application::get().close();
                return;
            }

            if (m_tick >= 300) {
                m_result->passed = false;
                LUNA_CORE_ERROR("RhiIblLab self-test FAIL frameCounter={} status='{}'",
                                m_state->frameCounter,
                                m_state->renderStatus);
                luna::Application::get().close();
            }
            return;
        }

        if (is_probe_test()) {
            update_probe_test();
            return;
        }
    }

    bool run_contract_test()
    {
        if (m_selfTestName == "phase2_enum_contract") {
            return run_phase2_enum_contract_self_test();
        }

        if (m_selfTestName == "phase2_cube_desc_validation") {
            return run_phase2_cube_desc_validation_self_test();
        }

        if (m_selfTestName == "phase2_cube_view_validation") {
            return run_phase2_cube_view_validation_self_test();
        }

        if (m_selfTestName == "phase2_sampler_contract") {
            return run_phase2_sampler_contract_self_test();
        }

        if (m_selfTestName == "phase3_cube_image_create") {
            return run_phase3_cube_image_create_self_test();
        }

        if (m_selfTestName == "phase3_cube_full_view") {
            return run_phase3_cube_full_view_self_test();
        }

        if (m_selfTestName == "phase3_cube_face_view") {
            return run_phase3_cube_face_view_self_test();
        }

        if (m_selfTestName == "phase3_sampler_vk_mapping") {
            return run_phase3_sampler_vk_mapping_self_test();
        }

        LUNA_CORE_ERROR("Unknown contract self-test '{}'", m_selfTestName);
        return false;
    }

    bool is_probe_test() const
    {
        return m_selfTestName == "phase4_cube_faces_distinct" || m_selfTestName == "phase4_skybox_rotation" ||
               m_selfTestName == "phase4_face_mip_preview" || m_selfTestName == "phase4_face_isolation" ||
               m_selfTestName == "phase5_env_mips" || m_selfTestName == "phase6_irradiance_smooth" ||
               m_selfTestName == "phase6_prefilter_mips" || m_selfTestName == "phase6_brdf_lut_preview";
    }

    void reset_probe()
    {
        m_state->probe = {};
    }

    void finish_probe(bool passed)
    {
        m_result->passed = passed;
        if (passed) {
            LUNA_CORE_INFO("RhiIblLab {} PASS {}", m_selfTestName, m_state->probe.summary);
        } else {
            LUNA_CORE_ERROR("RhiIblLab {} FAIL {}", m_selfTestName, m_state->probe.summary);
        }
        luna::Application::get().close();
    }

    void update_probe_test()
    {
        ++m_tick;
        if (m_tick >= 240) {
            m_state->probe.summary = "timeout while waiting for probe result";
            finish_probe(false);
            return;
        }

        if (!m_state->pipelineReady) {
            return;
        }

        if (m_selfTestName == "phase6_brdf_lut_preview") {
            if (!m_state->brdfLutReady) {
                return;
            }
        } else {
            if (!m_state->envCubeReady) {
                return;
            }
            if (m_selfTestName == "phase6_irradiance_smooth") {
                if (!m_generationRequested) {
                    m_state->regenerateIrradianceRequested = true;
                    m_generationRequested = true;
                    return;
                }
                if (!m_state->irradianceReady) {
                    return;
                }
            } else if (m_selfTestName == "phase6_prefilter_mips") {
                if (!m_generationRequested) {
                    m_state->regeneratePrefilterRequested = true;
                    m_generationRequested = true;
                    return;
                }
                if (!m_state->prefilterGenerated) {
                    return;
                }
            }
        }

        if (!m_probeRequested) {
            reset_probe();
            if (m_selfTestName == "phase4_cube_faces_distinct") {
                m_state->page = ibl_lab::Page::EnvCube;
                m_state->envPreviewLod = 0.0f;
                m_state->probe.request = ibl_lab::ProbeKind::CubeFacesDistinct;
                m_expectedProbe = ibl_lab::ProbeKind::CubeFacesDistinct;
            } else if (m_selfTestName == "phase4_skybox_rotation") {
                m_state->page = ibl_lab::Page::Skybox;
                m_state->skyYaw = 0.0f;
                m_state->skyPitch = 0.0f;
                m_state->probe.request = ibl_lab::ProbeKind::SkyboxRotation;
                m_expectedProbe = ibl_lab::ProbeKind::SkyboxRotation;
            } else if (m_selfTestName == "phase4_face_mip_preview") {
                m_state->page = ibl_lab::Page::Inspector;
                m_state->selectedFace = 0;
                m_state->selectedMip = std::min(3, static_cast<int>(std::max(1u, m_state->cubeMipLevels) - 1u));
                m_state->probe.request = ibl_lab::ProbeKind::FaceMipPreview;
                m_expectedProbe = ibl_lab::ProbeKind::FaceMipPreview;
            } else if (m_selfTestName == "phase4_face_isolation") {
                m_state->page = ibl_lab::Page::Inspector;
                m_state->selectedFace = 4;
                m_state->selectedMip = std::min(2, static_cast<int>(std::max(1u, m_state->cubeMipLevels) - 1u));
                m_state->probe.request = ibl_lab::ProbeKind::FaceIsolation;
                m_expectedProbe = ibl_lab::ProbeKind::FaceIsolation;
            } else if (m_selfTestName == "phase5_env_mips") {
                m_state->page = ibl_lab::Page::EnvCube;
                m_state->selectedFace = 0;
                m_state->envPreviewLod = 0.0f;
                m_state->probe.request = ibl_lab::ProbeKind::EnvMips;
                m_expectedProbe = ibl_lab::ProbeKind::EnvMips;
            } else if (m_selfTestName == "phase6_irradiance_smooth") {
                m_state->page = ibl_lab::Page::Irradiance;
                m_state->irradianceFace = 0;
                m_state->probe.request = ibl_lab::ProbeKind::IrradianceVsEnv;
                m_expectedProbe = ibl_lab::ProbeKind::IrradianceVsEnv;
            } else if (m_selfTestName == "phase6_prefilter_mips") {
                m_state->page = ibl_lab::Page::Prefilter;
                m_state->prefilterFace = 0;
                m_state->prefilterMip = std::min(3, static_cast<int>(std::max(1u, m_state->prefilterMipLevels) - 1u));
                m_state->probe.request = ibl_lab::ProbeKind::PrefilterMips;
                m_expectedProbe = ibl_lab::ProbeKind::PrefilterMips;
            } else if (m_selfTestName == "phase6_brdf_lut_preview") {
                m_state->page = ibl_lab::Page::BrdfLut;
                m_state->probe.request = ibl_lab::ProbeKind::BrdfLutPreview;
                m_expectedProbe = ibl_lab::ProbeKind::BrdfLutPreview;
            }
            m_probeRequested = true;
            return;
        }

        if (m_state->probe.ready) {
            const bool passed = m_state->probe.completed == m_expectedProbe && m_state->probe.passed;
            finish_probe(passed);
        }
    }

private:
    std::shared_ptr<ibl_lab::State> m_state;
    std::shared_ptr<SelfTestResult> m_result;
    std::string m_selfTestName;
    int m_tick = 0;
    bool m_probeRequested = false;
    bool m_generationRequested = false;
    ibl_lab::ProbeKind m_expectedProbe = ibl_lab::ProbeKind::None;
};

} // namespace

int main(int argc, char** argv)
{
#ifndef NDEBUG
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Trace;
#else
    constexpr luna::Logger::Level kLogLevel = luna::Logger::Level::Info;
#endif

    luna::Logger::init("logs/luna.log", kLogLevel);

    CommandLineOptions options;
    if (!parse_arguments(argc, argv, &options)) {
        luna::Logger::shutdown();
        return 1;
    }

    std::shared_ptr<ibl_lab::State> state = std::make_shared<ibl_lab::State>();
    std::shared_ptr<SelfTestResult> selfTestResult = std::make_shared<SelfTestResult>();
    std::shared_ptr<luna::IRenderPipeline> renderPipeline =
        std::make_shared<ibl_lab::RhiIblLabRenderPipeline>(state);

    luna::ApplicationSpecification specification{
        .name = "RhiIblLab",
        .windowWidth = 1280,
        .windowHeight = 720,
        .maximized = false,
        .enableImGui = !options.runSelfTest,
        .enableMultiViewport = false,
        .renderService =
            {
                .applicationName = "RhiIblLab",
                .backend = luna::RHIBackend::Vulkan,
                .renderPipeline = renderPipeline,
            },
    };

    std::unique_ptr<luna::Application> app = std::make_unique<luna::Application>(specification);
    if (!app->isInitialized()) {
        app.reset();
        luna::Logger::shutdown();
        return 1;
    }

    if (options.runSelfTest) {
        app->pushLayer(std::make_unique<RhiIblLabSelfTestLayer>(state, selfTestResult, options.selfTestName));
    } else {
        app->pushLayer(std::make_unique<RhiIblLabLayer>(state));
    }

    app->run();
    app.reset();
    luna::Logger::shutdown();
    return options.runSelfTest && !selfTestResult->passed ? 1 : 0;
}
