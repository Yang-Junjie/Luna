add_library(
    LunaCore
    STATIC
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

set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../third_party)

target_include_directories(
    LunaCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/third_party
        ${PROJECT_SOURCE_DIR}/third_party/glfw/include
        ${PROJECT_SOURCE_DIR}/third_party/glm
        ${PROJECT_SOURCE_DIR}/third_party/vkbootstrap/src
)

target_link_libraries(
    LunaCore
    PUBLIC
        GPUOpen::VulkanMemoryAllocator
        Vulkan::Vulkan
        glfw
        vk-bootstrap::vk-bootstrap
)
