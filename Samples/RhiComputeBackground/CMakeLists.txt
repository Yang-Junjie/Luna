set(RHI_COMPUTE_BACKGROUND_SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Shaders)
set(RHI_COMPUTE_BACKGROUND_SHADER_OUTPUT_DIR ${PROJECT_BINARY_DIR}/Shaders/RhiComputeBackground)

set(RHI_COMPUTE_BACKGROUND_SHADER_SOURCE ${RHI_COMPUTE_BACKGROUND_SHADER_SOURCE_DIR}/background.comp)
set(RHI_COMPUTE_BACKGROUND_SHADER_OUTPUT ${RHI_COMPUTE_BACKGROUND_SHADER_OUTPUT_DIR}/background.comp.spv)

add_custom_command(
    OUTPUT ${RHI_COMPUTE_BACKGROUND_SHADER_OUTPUT}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RHI_COMPUTE_BACKGROUND_SHADER_OUTPUT_DIR}
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} -fshader-stage=comp ${RHI_COMPUTE_BACKGROUND_SHADER_SOURCE} -o ${RHI_COMPUTE_BACKGROUND_SHADER_OUTPUT}
    DEPENDS ${RHI_COMPUTE_BACKGROUND_SHADER_SOURCE}
    VERBATIM
)

add_custom_target(
    RhiComputeBackgroundShaders
    DEPENDS
        ${RHI_COMPUTE_BACKGROUND_SHADER_OUTPUT}
)

add_executable(
    RhiComputeBackground
    main.cpp
)

add_dependencies(RhiComputeBackground RhiComputeBackgroundShaders)

target_include_directories(
    RhiComputeBackground
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    RhiComputeBackground
    PRIVATE
        LunaCore
)

target_compile_definitions(
    RhiComputeBackground
    PRIVATE
        RHI_COMPUTE_BACKGROUND_SHADER_ROOT="${PROJECT_BINARY_DIR}/Shaders/RhiComputeBackground"
)

luna_configure_runtime_output(RhiComputeBackground)
