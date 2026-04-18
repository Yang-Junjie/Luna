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

file(
    GLOB
    LUNA_RHI_D3D11_SOURCES
    CONFIGURE_DEPENDS
    src/Impls/D3D11/*.cpp
)

file(
    GLOB
    LUNA_RHI_D3D12_SOURCES
    CONFIGURE_DEPENDS
    src/Impls/D3D12/*.cpp
)

set(slang_DIR "C:/slang/cmake" CACHE PATH "Path to the Slang CMake package")
find_package(slang CONFIG REQUIRED)

set(
    LUNA_RHI_PLATFORM_SOURCES
    ${LUNA_RHI_VULKAN_SOURCES}
)

if(WIN32)
    list(
        APPEND
        LUNA_RHI_PLATFORM_SOURCES
        ${LUNA_RHI_D3D11_SOURCES}
        ${LUNA_RHI_D3D12_SOURCES}
        ${PROJECT_SOURCE_DIR}/third_party/D3D12MA/D3D12MemAlloc.cpp
    )
endif()

add_library(
    LunaRHI
    STATIC
    ${LUNA_RHI_PUBLIC_HEADERS}
    ${LUNA_RHI_COMMON_SOURCES}
    ${LUNA_RHI_PLATFORM_SOURCES}
)

target_include_directories(
    LunaRHI
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        $<$<PLATFORM_ID:Windows>:${PROJECT_SOURCE_DIR}/third_party/D3D12MA>
)

target_link_libraries(
    LunaRHI
    PUBLIC
        slang::slang
        Vulkan::Vulkan
        $<$<PLATFORM_ID:Windows>:d3d11>
        $<$<PLATFORM_ID:Windows>:d3d12>
        $<$<PLATFORM_ID:Windows>:dxgi>
        $<$<PLATFORM_ID:Windows>:dxguid>
)

target_compile_definitions(
    LunaRHI
    PUBLIC
        LUNA_RHI_STATIC
        LUNA_RHI_BACKEND_VULKAN
        $<$<PLATFORM_ID:Windows>:LUNA_RHI_BACKEND_D3D11>
        $<$<PLATFORM_ID:Windows>:LUNA_RHI_BACKEND_D3D12>
        $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR>
)
