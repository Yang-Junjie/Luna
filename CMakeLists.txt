cmake_minimum_required(VERSION 3.16)
project(Luna LANGUAGES C CXX)

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

add_subdirectory(Luna)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Editor/CMakeLists.txt")
    add_subdirectory(Editor)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Runtime/CMakeLists.txt")
    add_subdirectory(Runtime)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/App/CMakeLists.txt")
    add_subdirectory(App)
endif()
