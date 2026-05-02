cmake_minimum_required(VERSION 3.16)
project(Luna LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wno-nullability-completeness)
endif()

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Vulkan REQUIRED)

message(STATUS "Found Vulkan library: ${Vulkan_LIBRARY}")
message(STATUS "Vulkan include dirs: ${Vulkan_INCLUDE_DIRS}")

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

set(LUNA_PLATFORM_WINDOW_SYSTEM_DEFINITIONS)
set(LUNA_RHI_VULKAN_PLATFORM_DEFINITIONS)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LUNA_LINUX_WINDOW_SYSTEM "X11" CACHE STRING "Linux window system used by Luna native surface creation")
    set_property(CACHE LUNA_LINUX_WINDOW_SYSTEM PROPERTY STRINGS X11 Wayland)
    string(TOUPPER "${LUNA_LINUX_WINDOW_SYSTEM}" LUNA_LINUX_WINDOW_SYSTEM_UPPER)

    if(LUNA_LINUX_WINDOW_SYSTEM_UPPER STREQUAL "X11")
        set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
        set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
        set(LUNA_PLATFORM_WINDOW_SYSTEM_DEFINITIONS USE_XLIB)
        set(LUNA_RHI_VULKAN_PLATFORM_DEFINITIONS VK_USE_PLATFORM_XLIB_KHR)
    elseif(LUNA_LINUX_WINDOW_SYSTEM_UPPER STREQUAL "WAYLAND")
        set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
        set(LUNA_PLATFORM_WINDOW_SYSTEM_DEFINITIONS USE_WAYLAND)
        set(LUNA_RHI_VULKAN_PLATFORM_DEFINITIONS VK_USE_PLATFORM_WAYLAND_KHR)
    else()
        message(FATAL_ERROR "Unsupported LUNA_LINUX_WINDOW_SYSTEM='${LUNA_LINUX_WINDOW_SYSTEM}'. Use X11 or Wayland.")
    endif()
endif()

set(SIMDJSON_DEVELOPER_MODE OFF CACHE BOOL "" FORCE)
set(SIMDJSON_SINGLEHEADER OFF CACHE BOOL "" FORCE)

set(FASTGLTF_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_DOCS OFF CACHE BOOL "" FORCE)
set(FASTGLTF_COMPILE_AS_CPP20 ON CACHE BOOL "" FORCE)

set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)

set(ENKITS_BUILD_C_INTERFACE OFF CACHE BOOL "" FORCE)
set(ENKITS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ENKITS_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ENKITS_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/glfw)
add_subdirectory(third_party/enkiTS)
add_subdirectory(third_party/spdlog)
add_subdirectory(third_party/stb)
add_subdirectory(third_party/simdjson)
add_subdirectory(third_party/fastgltf)
add_subdirectory(third_party/yaml-cpp)

add_library(ufbx STATIC third_party/ufbx/ufbx.c)
target_include_directories(ufbx PUBLIC third_party/ufbx)
target_compile_definitions(ufbx PUBLIC UFBX_NO_ASSERT)

add_library(lua STATIC
    third_party/lua/lapi.c
    third_party/lua/lauxlib.c
    third_party/lua/lbaselib.c
    third_party/lua/lcode.c
    third_party/lua/lcorolib.c
    third_party/lua/lctype.c
    third_party/lua/ldblib.c
    third_party/lua/ldebug.c
    third_party/lua/ldo.c
    third_party/lua/ldump.c
    third_party/lua/lfunc.c
    third_party/lua/lgc.c
    third_party/lua/linit.c
    third_party/lua/liolib.c
    third_party/lua/llex.c
    third_party/lua/lmathlib.c
    third_party/lua/lmem.c
    third_party/lua/loadlib.c
    third_party/lua/lobject.c
    third_party/lua/lopcodes.c
    third_party/lua/loslib.c
    third_party/lua/lparser.c
    third_party/lua/lstate.c
    third_party/lua/lstring.c
    third_party/lua/lstrlib.c
    third_party/lua/ltable.c
    third_party/lua/ltablib.c
    third_party/lua/ltm.c
    third_party/lua/lundump.c
    third_party/lua/lutf8lib.c
    third_party/lua/lvm.c
    third_party/lua/lzio.c
)
target_include_directories(lua PUBLIC third_party/lua)

add_subdirectory(Luna)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Editor/CMakeLists.txt")
    add_subdirectory(Editor)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Runtime/CMakeLists.txt")
    add_subdirectory(Runtime)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Plugins/CMakeLists.txt")
    add_subdirectory(Plugins)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/App/CMakeLists.txt")
    add_subdirectory(App)
endif()
