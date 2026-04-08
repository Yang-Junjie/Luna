set(THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/third_party)

set(IMGUI_SOURCE ${THIRD_PARTY_DIR}/imgui/imgui.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_demo.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_draw.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_tables.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_widgets.cpp
    ${THIRD_PARTY_DIR}/imgui/backends/imgui_impl_glfw.cpp
    ${THIRD_PARTY_DIR}/imgui/backends/imgui_impl_vulkan.cpp)

set(RHI_SOURCE
    Renderer/Types.h
    Renderer/Shader.h
    Renderer/ResourceLayout.h)

set(VULKAN_BACKEND_SOURCE 
    Vulkan/VkTypes.h
    Vulkan/VkInitializers.cpp
    Vulkan/VkInitializers.h
    Vulkan/VkImages.h
    Vulkan/VkImages.cpp 
    Vulkan/VkDescriptors.h
    Vulkan/VkDescriptors.cpp
    Vulkan/VkPipelines.h
    Vulkan/VkPipelines.cpp
    Vulkan/VkEngine.h
    Vulkan/VkEngine.cpp
    Vulkan/VkLoader.h
    Vulkan/VkLoader.cpp
    Vulkan/VkShader.cpp
    Vulkan/VkShader.h
    Vulkan/Camera.cpp
    Vulkan/Camera.h)

add_library(
    LunaCore
    STATIC
    Core/Application.h
    Core/Application.cpp
    Core/Input.h
    Imgui/ImGuiLayer.hpp
    Imgui/ImGuiLayer.cpp
    Core/KeyCodes.h
    Core/Layer.h
    Core/LayerStack.h
    Core/LayerStack.cpp
    Core/Log.h
    Core/Log.cpp
    Core/MouseCodes.h
    Core/Timestep.h
    Core/Window.h
    Core/Window.cpp
    Events/ApplicationEvent.h
    Events/Event.h
    Events/KeyEvent.h
    Events/MouseEvent.h
    Platform/GLFWKeyCodes.hpp
    Platform/GLFWMouseCodes.hpp
    Platform/GLFWWindow.hpp
    Platform/GLFWWindow.cpp
    Platform/GLFWWindowInput.cpp
    ${RHI_SOURCE}
    ${VULKAN_BACKEND_SOURCE}
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
        spirv-cross-core
)

target_compile_definitions(
    LunaCore
    PUBLIC
        SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
        VULKAN_HPP_DISABLE_ENHANCED_MODE
        VULKAN_HPP_NO_CONSTRUCTORS
        VULKAN_HPP_NO_EXCEPTIONS
)

