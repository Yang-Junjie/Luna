add_library(
    LunaCore
    STATIC
    Core/log.h
    Core/log.cpp
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
)

set(THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/third_party)

target_include_directories(
    LunaCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${THIRD_PARTY_DIR}/third_party
        ${THIRD_PARTY_DIR}/spdlog/include
        ${THIRD_PARTY_DIR}/glfw/include
        ${THIRD_PARTY_DIR}/glm
        ${THIRD_PARTY_DIR}/vkbootstrap/src
)

target_link_libraries(LunaCore PUBLIC
        spdlog::spdlog
        GPUOpen::VulkanMemoryAllocator
        Vulkan::Vulkan
        glfw
        vk-bootstrap::vk-bootstrap
)

target_compile_definitions(LunaCore PUBLIC SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
