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

set(LUNA_RHI_DEFAULT_D3D OFF)
if(WIN32)
    set(LUNA_RHI_DEFAULT_D3D ON)
endif()

set(LUNA_RHI_DEFAULT_METAL OFF)
if(APPLE)
    set(LUNA_RHI_DEFAULT_METAL ON)
endif()

option(LUNA_RHI_ENABLE_VULKAN "Build the Vulkan RHI backend" ON)
option(LUNA_RHI_ENABLE_D3D11 "Build the Direct3D 11 RHI backend" ${LUNA_RHI_DEFAULT_D3D})
option(LUNA_RHI_ENABLE_D3D12 "Build the Direct3D 12 RHI backend" ${LUNA_RHI_DEFAULT_D3D})
option(LUNA_RHI_ENABLE_METAL "Build the Metal RHI backend" ${LUNA_RHI_DEFAULT_METAL})
option(LUNA_RHI_ENABLE_OPENGL "Build the desktop OpenGL RHI backend" OFF)
option(LUNA_RHI_ENABLE_WEBGPU "Build the WebGPU RHI backend" OFF)

if(LUNA_RHI_ENABLE_D3D11 AND NOT WIN32)
    message(FATAL_ERROR "LUNA_RHI_ENABLE_D3D11 is only supported on Windows")
endif()

if(LUNA_RHI_ENABLE_D3D12 AND NOT WIN32)
    message(FATAL_ERROR "LUNA_RHI_ENABLE_D3D12 is only supported on Windows")
endif()

if(LUNA_RHI_ENABLE_METAL AND NOT APPLE)
    message(FATAL_ERROR "LUNA_RHI_ENABLE_METAL is only supported on Apple platforms")
endif()

if(NOT LUNA_RHI_ENABLE_VULKAN
   AND NOT LUNA_RHI_ENABLE_D3D11
   AND NOT LUNA_RHI_ENABLE_D3D12
   AND NOT LUNA_RHI_ENABLE_METAL
   AND NOT LUNA_RHI_ENABLE_OPENGL
   AND NOT LUNA_RHI_ENABLE_WEBGPU)
    message(FATAL_ERROR "At least one Luna RHI backend must be enabled")
endif()

if(NOT slang_DIR AND DEFINED ENV{SLANG_DIR})
    set(slang_DIR "$ENV{SLANG_DIR}" CACHE PATH "Path to the Slang CMake package")
endif()

find_package(slang REQUIRED)

set(LUNA_RHI_BACKEND_SOURCES)
set(LUNA_RHI_BACKEND_DEFINITIONS)
set(LUNA_RHI_BACKEND_INCLUDE_DIRS)
set(LUNA_RHI_BACKEND_LINK_LIBRARIES)

if(LUNA_RHI_ENABLE_VULKAN)
    find_package(Vulkan REQUIRED)
    message(STATUS "LunaRHI: Vulkan backend enabled")
    message(STATUS "Found Vulkan library: ${Vulkan_LIBRARY}")
    message(STATUS "Vulkan include dirs: ${Vulkan_INCLUDE_DIRS}")

    file(
        GLOB
        LUNA_RHI_VULKAN_SOURCES
        CONFIGURE_DEPENDS
        src/Impls/Vulkan/*.cpp
    )

    list(APPEND LUNA_RHI_BACKEND_SOURCES ${LUNA_RHI_VULKAN_SOURCES})
    list(APPEND LUNA_RHI_BACKEND_DEFINITIONS LUNA_RHI_BACKEND_VULKAN)
    list(APPEND LUNA_RHI_BACKEND_LINK_LIBRARIES Vulkan::Vulkan)

    if(WIN32)
        list(APPEND LUNA_RHI_BACKEND_DEFINITIONS VK_USE_PLATFORM_WIN32_KHR)
    elseif(APPLE)
        list(APPEND LUNA_RHI_BACKEND_DEFINITIONS VK_USE_PLATFORM_METAL_EXT)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        if(NOT LUNA_PLATFORM_WINDOW_SYSTEM_DEFINITIONS OR NOT LUNA_RHI_VULKAN_PLATFORM_DEFINITIONS)
            message(FATAL_ERROR "Linux Vulkan surface support requires LUNA_LINUX_WINDOW_SYSTEM=X11 or Wayland")
        endif()
        list(APPEND LUNA_RHI_BACKEND_DEFINITIONS
            ${LUNA_PLATFORM_WINDOW_SYSTEM_DEFINITIONS}
            ${LUNA_RHI_VULKAN_PLATFORM_DEFINITIONS})
    endif()
endif()

if(LUNA_RHI_ENABLE_D3D11)
    message(STATUS "LunaRHI: Direct3D 11 backend enabled")
    file(
        GLOB
        LUNA_RHI_D3D11_SOURCES
        CONFIGURE_DEPENDS
        src/Impls/D3D11/*.cpp
    )

    list(APPEND LUNA_RHI_BACKEND_SOURCES ${LUNA_RHI_D3D11_SOURCES})
    list(APPEND LUNA_RHI_BACKEND_DEFINITIONS LUNA_RHI_BACKEND_D3D11)
    list(APPEND LUNA_RHI_BACKEND_LINK_LIBRARIES d3d11 dxgi dxguid)
endif()

if(LUNA_RHI_ENABLE_D3D12)
    message(STATUS "LunaRHI: Direct3D 12 backend enabled")
    file(
        GLOB
        LUNA_RHI_D3D12_SOURCES
        CONFIGURE_DEPENDS
        src/Impls/D3D12/*.cpp
    )

    list(APPEND LUNA_RHI_BACKEND_SOURCES
        ${LUNA_RHI_D3D12_SOURCES}
        ${PROJECT_SOURCE_DIR}/third_party/D3D12MA/D3D12MemAlloc.cpp)
    list(APPEND LUNA_RHI_BACKEND_DEFINITIONS LUNA_RHI_BACKEND_D3D12)
    list(APPEND LUNA_RHI_BACKEND_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/third_party/D3D12MA)
    list(APPEND LUNA_RHI_BACKEND_LINK_LIBRARIES d3d12 dxgi dxguid)
endif()

if(LUNA_RHI_ENABLE_METAL)
    enable_language(OBJCXX)
    message(STATUS "LunaRHI: Metal backend enabled")
    file(
        GLOB
        LUNA_RHI_METAL_SOURCES
        CONFIGURE_DEPENDS
        src/Impls/Metal/*.mm
    )

    list(APPEND LUNA_RHI_BACKEND_SOURCES ${LUNA_RHI_METAL_SOURCES})
    list(APPEND LUNA_RHI_BACKEND_DEFINITIONS LUNA_RHI_BACKEND_METAL)
    list(APPEND LUNA_RHI_BACKEND_LINK_LIBRARIES
        "-framework Metal"
        "-framework QuartzCore"
        "-framework Foundation")
endif()

if(LUNA_RHI_ENABLE_OPENGL)
    find_package(OpenGL REQUIRED)
    message(STATUS "LunaRHI: OpenGL backend enabled")
    file(
        GLOB
        LUNA_RHI_OPENGL_SOURCES
        CONFIGURE_DEPENDS
        src/Impls/OpenGL/*.cpp
    )

    list(APPEND LUNA_RHI_BACKEND_SOURCES
        ${LUNA_RHI_OPENGL_SOURCES}
        ${PROJECT_SOURCE_DIR}/third_party/glad/glad.c)
    list(APPEND LUNA_RHI_BACKEND_DEFINITIONS LUNA_RHI_BACKEND_OPENGL)
    list(APPEND LUNA_RHI_BACKEND_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/third_party/glad/include)
    list(APPEND LUNA_RHI_BACKEND_LINK_LIBRARIES OpenGL::GL)
endif()

if(LUNA_RHI_ENABLE_WEBGPU)
    set(LUNA_RHI_WEBGPU_TARGET)
    if(TARGET dawn::webgpu_dawn)
        set(LUNA_RHI_WEBGPU_TARGET dawn::webgpu_dawn)
    elseif(TARGET webgpu_dawn)
        set(LUNA_RHI_WEBGPU_TARGET webgpu_dawn)
    elseif(TARGET webgpu)
        set(LUNA_RHI_WEBGPU_TARGET webgpu)
    else()
        message(FATAL_ERROR
            "LUNA_RHI_ENABLE_WEBGPU requires a WebGPU/Dawn CMake target "
            "(dawn::webgpu_dawn, webgpu_dawn, or webgpu) to be available before Luna/RHI is configured")
    endif()

    message(STATUS "LunaRHI: WebGPU backend enabled")
    file(
        GLOB
        LUNA_RHI_WEBGPU_SOURCES
        CONFIGURE_DEPENDS
        src/Impls/WebGPU/*.cpp
    )

    list(APPEND LUNA_RHI_BACKEND_SOURCES ${LUNA_RHI_WEBGPU_SOURCES})
    list(APPEND LUNA_RHI_BACKEND_DEFINITIONS LUNA_RHI_BACKEND_WEBGPU)
    list(APPEND LUNA_RHI_BACKEND_LINK_LIBRARIES ${LUNA_RHI_WEBGPU_TARGET})
endif()

add_library(
    LunaRHI
    STATIC
    ${LUNA_RHI_PUBLIC_HEADERS}
    ${LUNA_RHI_COMMON_SOURCES}
    ${LUNA_RHI_BACKEND_SOURCES}
)

target_include_directories(
    LunaRHI
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LUNA_RHI_BACKEND_INCLUDE_DIRS}
)

target_link_libraries(
    LunaRHI
    PUBLIC
        slang::slang
        ${LUNA_RHI_BACKEND_LINK_LIBRARIES}
)

target_compile_definitions(
    LunaRHI
    PUBLIC
        LUNA_RHI_STATIC
        ${LUNA_RHI_BACKEND_DEFINITIONS}
)
