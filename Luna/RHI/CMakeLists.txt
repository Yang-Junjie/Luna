file(
    GLOB_RECURSE
    CACAO_PUBLIC_HEADERS
    CONFIGURE_DEPENDS
    include/*.h
    include/*.hpp
)

file(
    GLOB
    CACAO_COMMON_SOURCES
    CONFIGURE_DEPENDS
    src/*.cpp
)

file(
    GLOB
    CACAO_VULKAN_SOURCES
    CONFIGURE_DEPENDS
    src/Impls/Vulkan/*.cpp
)

add_library(
    LunaRHI
    STATIC
    ${CACAO_PUBLIC_HEADERS}
    ${CACAO_COMMON_SOURCES}
    ${CACAO_VULKAN_SOURCES}
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
        CACAO_STATIC
        CACAO_BACKEND_VULKAN
        $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
)
