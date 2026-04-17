file(
    GLOB_RECURSE
    LUNA_RHI_PUBLIC_HEADERS
    CONFIGURE_DEPENDS
    include/*.h
    include/*.hpp
)

file(
    GLOB
    LUNA_RHI_COMMON_SOURCES
    CONFIGURE_DEPENDS
    src/*.cpp
)

file(
    GLOB
    LUNA_RHI_VULKAN_SOURCES
    CONFIGURE_DEPENDS
    src/Impls/Vulkan/*.cpp
)

add_library(
    LunaRHI
    STATIC
    ${LUNA_RHI_PUBLIC_HEADERS}
    ${LUNA_RHI_COMMON_SOURCES}
    ${LUNA_RHI_VULKAN_SOURCES}
)

target_include_directories(
    LunaRHI
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
    LunaRHI
    PUBLIC
        Vulkan::Vulkan
)

target_compile_definitions(
    LunaRHI
    PUBLIC
        LUNA_RHI_STATIC
        LUNA_RHI_BACKEND_VULKAN
        $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
)
