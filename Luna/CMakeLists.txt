set(THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/third_party)

set(IMGUI_SOURCE ${THIRD_PARTY_DIR}/imgui/imgui.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_demo.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_draw.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_tables.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_widgets.cpp
    ${THIRD_PARTY_DIR}/imgui/backends/imgui_impl_glfw.cpp
    ${THIRD_PARTY_DIR}/imgui/backends/imgui_impl_vulkan.cpp)
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
    Vulkan/vk_types.h
    Vulkan/vk_initializers.cpp
    Vulkan/vk_initializers.h
    Vulkan/vk_images.h
    Vulkan/vk_images.cpp 
    Vulkan/vk_descriptors.h
    Vulkan/vk_descriptors.cpp
    Vulkan/vk_pipelines.h
    Vulkan/vk_pipelines.cpp
    Vulkan/vk_engine.h
    Vulkan/vk_engine.cpp
    Vulkan/vk_loader.h
    Vulkan/vk_loader.cpp
    Vulkan/camera.cpp
    Vulkan/camera.h
    ${IMGUI_SOURCE}
)

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
)

target_compile_definitions(LunaCore PUBLIC SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
