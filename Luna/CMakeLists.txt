set(THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/third_party)

set(IMGUI_SOURCE ${THIRD_PARTY_DIR}/imgui/imgui.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_demo.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_draw.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_tables.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_widgets.cpp
    ${THIRD_PARTY_DIR}/imgui/backends/imgui_impl_glfw.cpp
    ${THIRD_PARTY_DIR}/imgui/backends/imgui_impl_vulkan.cpp)

set(RHI_SOURCE
    RHI/Types.h
    RHI/Shader.h
    RHI/DescriptorEnums.h
    RHI/ResourceDescriptors.h
    RHI/PipelineDescriptors.h
    RHI/ImageContracts.h
    RHI/Descriptors.h
    RHI/CommandTypes.h
    RHI/CommandBarriers.h
    RHI/CommandRendering.h
    RHI/CommandContext.h
    RHI/RHIDeviceTypes.h
    RHI/RHIQueue.h
    RHI/RHISwapchain.h
    RHI/RHIAdapter.h
    RHI/RHIDevice.h
    RHI/RHIDevice.cpp
    RHI/ResourceLayout.h)

set(VULKAN_BACKEND_SOURCE 
    Vulkan/vk_types.h
    Vulkan/vk_initializers.cpp
    Vulkan/vk_initializers.h
    Vulkan/vk_images.h
    Vulkan/vk_images.cpp 
    Vulkan/vk_descriptor_layouts.h
    Vulkan/vk_descriptor_allocators.h
    Vulkan/vk_resource_bindings.h
    Vulkan/vk_descriptors.h
    Vulkan/vk_descriptors.cpp
    Vulkan/detail/vk_descriptors_layouts.inl
    Vulkan/detail/vk_descriptors_bindings.inl
    Vulkan/detail/vk_descriptors_allocators.inl
    Vulkan/detail/vk_descriptors_writer.inl
    Vulkan/vk_pipelines.h
    Vulkan/vk_pipelines.cpp
    Vulkan/vk_engine_types.h
    Vulkan/vk_device_context.h
    Vulkan/vk_device_context.cpp
    Vulkan/vk_rhi_device.h
    Vulkan/vk_rhi_device.cpp
    Vulkan/detail/vk_rhi_device_classes.inl
    Vulkan/detail/vk_rhi_device_device.inl
    Vulkan/detail/vk_rhi_device_frame.inl
    Vulkan/detail/vk_rhi_device_command_context.inl
    Vulkan/detail/vk_rhi_device_tail.inl
    Vulkan/vk_shader.cpp
    Vulkan/vk_shader.h
    Vulkan/camera.cpp
    Vulkan/camera.h)

add_library(
    LunaCore
    STATIC
    Core/application.h
    Core/application.cpp
    Core/input.h
    Imgui/ImGuiLayer.hpp
    Imgui/ImGuiLayer.cpp
    Core/key_codes.h
    Core/layer.h
    Core/layer_stack.h
    Core/layer_stack.cpp
    Core/log.h
    Core/log.cpp
    Core/mouse_codes.h
    Core/Paths.h
    Renderer/SceneRenderPipeline.h
    Renderer/SceneRenderPipeline.cpp
    Renderer/RenderPipeline.h
    Renderer/RenderService.h
    Renderer/RenderService.cpp
    Core/timestep.h
    Core/window.h
    Core/window.cpp
    Events/application_event.h
    Events/event.h
    Events/key_event.h
    Events/mouse_event.h
    Platform/GLFWKeyCodes.hpp
    Platform/GLFWMouseCodes.hpp
    Platform/GLFWWindow.hpp
    Platform/GLFWWindow.cpp
    Platform/GLFWWindowInput.cpp
    ${RHI_SOURCE}
    ${VULKAN_BACKEND_SOURCE}
    ${IMGUI_SOURCE}
)

set(LUNA_RHI_SCENE_SHADER_SOURCE_DIR ${PROJECT_SOURCE_DIR}/Shaders/Internal)
set(LUNA_RHI_SCENE_SHADER_OUTPUT_DIR ${PROJECT_BINARY_DIR}/Shaders/RhiScene)

set(LUNA_RHI_SCENE_MESH_VERTEX_SHADER_SOURCE ${LUNA_RHI_SCENE_SHADER_SOURCE_DIR}/rhi_scene_mesh.vert)
set(LUNA_RHI_SCENE_MESH_FRAGMENT_SHADER_SOURCE ${LUNA_RHI_SCENE_SHADER_SOURCE_DIR}/rhi_scene_mesh.frag)
set(LUNA_RHI_SCENE_FULLSCREEN_VERTEX_SHADER_SOURCE ${LUNA_RHI_SCENE_SHADER_SOURCE_DIR}/rhi_scene_fullscreen.vert)
set(LUNA_RHI_SCENE_FULLSCREEN_FRAGMENT_SHADER_SOURCE ${LUNA_RHI_SCENE_SHADER_SOURCE_DIR}/rhi_scene_fullscreen.frag)

set(LUNA_RHI_SCENE_MESH_VERTEX_SHADER_OUTPUT ${LUNA_RHI_SCENE_SHADER_OUTPUT_DIR}/rhi_scene_mesh.vert.spv)
set(LUNA_RHI_SCENE_MESH_FRAGMENT_SHADER_OUTPUT ${LUNA_RHI_SCENE_SHADER_OUTPUT_DIR}/rhi_scene_mesh.frag.spv)
set(LUNA_RHI_SCENE_FULLSCREEN_VERTEX_SHADER_OUTPUT ${LUNA_RHI_SCENE_SHADER_OUTPUT_DIR}/rhi_scene_fullscreen.vert.spv)
set(LUNA_RHI_SCENE_FULLSCREEN_FRAGMENT_SHADER_OUTPUT ${LUNA_RHI_SCENE_SHADER_OUTPUT_DIR}/rhi_scene_fullscreen.frag.spv)

add_custom_command(
    OUTPUT ${LUNA_RHI_SCENE_MESH_VERTEX_SHADER_OUTPUT}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${LUNA_RHI_SCENE_SHADER_OUTPUT_DIR}
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} -fshader-stage=vert ${LUNA_RHI_SCENE_MESH_VERTEX_SHADER_SOURCE} -o ${LUNA_RHI_SCENE_MESH_VERTEX_SHADER_OUTPUT}
    DEPENDS ${LUNA_RHI_SCENE_MESH_VERTEX_SHADER_SOURCE}
    VERBATIM
)

add_custom_command(
    OUTPUT ${LUNA_RHI_SCENE_MESH_FRAGMENT_SHADER_OUTPUT}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${LUNA_RHI_SCENE_SHADER_OUTPUT_DIR}
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} -fshader-stage=frag ${LUNA_RHI_SCENE_MESH_FRAGMENT_SHADER_SOURCE} -o ${LUNA_RHI_SCENE_MESH_FRAGMENT_SHADER_OUTPUT}
    DEPENDS ${LUNA_RHI_SCENE_MESH_FRAGMENT_SHADER_SOURCE}
    VERBATIM
)

add_custom_command(
    OUTPUT ${LUNA_RHI_SCENE_FULLSCREEN_VERTEX_SHADER_OUTPUT}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${LUNA_RHI_SCENE_SHADER_OUTPUT_DIR}
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} -fshader-stage=vert ${LUNA_RHI_SCENE_FULLSCREEN_VERTEX_SHADER_SOURCE} -o ${LUNA_RHI_SCENE_FULLSCREEN_VERTEX_SHADER_OUTPUT}
    DEPENDS ${LUNA_RHI_SCENE_FULLSCREEN_VERTEX_SHADER_SOURCE}
    VERBATIM
)

add_custom_command(
    OUTPUT ${LUNA_RHI_SCENE_FULLSCREEN_FRAGMENT_SHADER_OUTPUT}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${LUNA_RHI_SCENE_SHADER_OUTPUT_DIR}
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} -fshader-stage=frag ${LUNA_RHI_SCENE_FULLSCREEN_FRAGMENT_SHADER_SOURCE} -o ${LUNA_RHI_SCENE_FULLSCREEN_FRAGMENT_SHADER_OUTPUT}
    DEPENDS ${LUNA_RHI_SCENE_FULLSCREEN_FRAGMENT_SHADER_SOURCE}
    VERBATIM
)

add_custom_target(
    LunaRhiSceneShaders
    DEPENDS
        ${LUNA_RHI_SCENE_MESH_VERTEX_SHADER_OUTPUT}
        ${LUNA_RHI_SCENE_MESH_FRAGMENT_SHADER_OUTPUT}
        ${LUNA_RHI_SCENE_FULLSCREEN_VERTEX_SHADER_OUTPUT}
        ${LUNA_RHI_SCENE_FULLSCREEN_FRAGMENT_SHADER_OUTPUT}
)

add_dependencies(LunaCore LunaRhiSceneShaders)

target_include_directories(
    LunaCore
    SYSTEM PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${THIRD_PARTY_DIR}/imgui
        ${THIRD_PARTY_DIR}/spdlog/include
        ${THIRD_PARTY_DIR}/glfw/include
        ${THIRD_PARTY_DIR}/glm
        ${THIRD_PARTY_DIR}/vkbootstrap/src
)

target_link_libraries(LunaCore 
        PUBLIC
        spdlog::spdlog
        GPUOpen::VulkanMemoryAllocator
        Vulkan::Vulkan
        glfw
        vk-bootstrap::vk-bootstrap
        stb::stb
        fastgltf::fastgltf
        simdjson::simdjson
        spirv-cross-core
)

target_compile_definitions(
    LunaCore
    PUBLIC
        SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
        LUNA_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
        LUNA_SHADER_ROOT="${PROJECT_SOURCE_DIR}/Shaders"
        LUNA_ASSET_ROOT="${PROJECT_SOURCE_DIR}/assets"
        LUNA_RHI_SCENE_SHADER_ROOT="${PROJECT_BINARY_DIR}/Shaders/RhiScene"
        VULKAN_HPP_DISABLE_ENHANCED_MODE
        VULKAN_HPP_NO_CONSTRUCTORS
        VULKAN_HPP_NO_EXCEPTIONS
)
