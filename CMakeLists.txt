cmake_minimum_required(VERSION 3.16)
project(Luna LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

function(luna_configure_runtime_output target_name)
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/Debug"
    )

    foreach(config Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER "${config}" config_upper)
        set_target_properties(${target_name} PROPERTIES
            "RUNTIME_OUTPUT_DIRECTORY_${config_upper}" "${PROJECT_BINARY_DIR}/${config}"
        )
    endforeach()
endfunction()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wno-nullability-completeness)
endif()

if(MSVC)
    add_compile_options(/utf-8 /FS)
endif()

find_package(Vulkan REQUIRED)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/glfw)
add_subdirectory(third_party/spdlog)
add_subdirectory(third_party/vkbootstrap)

add_subdirectory(Luna)
add_subdirectory(Editor)

add_executable(LunaApp Luna/Core/main.cpp)
target_link_libraries(LunaApp PRIVATE LunaEditor)
luna_configure_runtime_output(LunaApp)

if(BUILD_TESTING)
    add_test(
        NAME LunaApp.selftest.phase3_editor_resize
        COMMAND $<TARGET_FILE:LunaApp> --self-test=phase3_editor_resize
    )
    set_tests_properties(
        LunaApp.selftest.phase3_editor_resize
        PROPERTIES
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
endif()
