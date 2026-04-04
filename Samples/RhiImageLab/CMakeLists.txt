set(RHI_IMAGE_LAB_SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Shaders)
set(RHI_IMAGE_LAB_SHADER_OUTPUT_DIR ${PROJECT_BINARY_DIR}/Shaders/RhiImageLab)

function(rhi_image_lab_compile_shader output_file shader_stage shader_source)
    add_custom_command(
        OUTPUT ${output_file}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RHI_IMAGE_LAB_SHADER_OUTPUT_DIR}
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} -fshader-stage=${shader_stage} ${ARGN} ${shader_source} -o ${output_file}
        DEPENDS ${shader_source}
        VERBATIM
    )
endfunction()

set(RHI_IMAGE_LAB_FULLSCREEN_VERTEX_SOURCE ${RHI_IMAGE_LAB_SHADER_SOURCE_DIR}/image_lab_fullscreen.vert)
set(RHI_IMAGE_LAB_PRESENT_2D_FRAGMENT_SOURCE ${RHI_IMAGE_LAB_SHADER_SOURCE_DIR}/image_lab_present_2d.frag)
set(RHI_IMAGE_LAB_PRESENT_ARRAY_FRAGMENT_SOURCE ${RHI_IMAGE_LAB_SHADER_SOURCE_DIR}/image_lab_present_array.frag)
set(RHI_IMAGE_LAB_PRESENT_3D_FRAGMENT_SOURCE ${RHI_IMAGE_LAB_SHADER_SOURCE_DIR}/image_lab_present_3d.frag)
set(RHI_IMAGE_LAB_MRT_FRAGMENT_SOURCE ${RHI_IMAGE_LAB_SHADER_SOURCE_DIR}/image_lab_mrt.frag)

set(RHI_IMAGE_LAB_FULLSCREEN_VERTEX_OUTPUT ${RHI_IMAGE_LAB_SHADER_OUTPUT_DIR}/image_lab_fullscreen.vert.spv)
set(RHI_IMAGE_LAB_PRESENT_2D_FRAGMENT_OUTPUT ${RHI_IMAGE_LAB_SHADER_OUTPUT_DIR}/image_lab_present_2d.frag.spv)
set(RHI_IMAGE_LAB_PRESENT_ARRAY_FRAGMENT_OUTPUT ${RHI_IMAGE_LAB_SHADER_OUTPUT_DIR}/image_lab_present_array.frag.spv)
set(RHI_IMAGE_LAB_PRESENT_3D_FRAGMENT_OUTPUT ${RHI_IMAGE_LAB_SHADER_OUTPUT_DIR}/image_lab_present_3d.frag.spv)
set(RHI_IMAGE_LAB_MRT1_FRAGMENT_OUTPUT ${RHI_IMAGE_LAB_SHADER_OUTPUT_DIR}/image_lab_mrt_1.frag.spv)
set(RHI_IMAGE_LAB_MRT2_FRAGMENT_OUTPUT ${RHI_IMAGE_LAB_SHADER_OUTPUT_DIR}/image_lab_mrt_2.frag.spv)
set(RHI_IMAGE_LAB_MRT3_FRAGMENT_OUTPUT ${RHI_IMAGE_LAB_SHADER_OUTPUT_DIR}/image_lab_mrt_3.frag.spv)
set(RHI_IMAGE_LAB_MRT4_FRAGMENT_OUTPUT ${RHI_IMAGE_LAB_SHADER_OUTPUT_DIR}/image_lab_mrt_4.frag.spv)

rhi_image_lab_compile_shader(
    ${RHI_IMAGE_LAB_FULLSCREEN_VERTEX_OUTPUT}
    vert
    ${RHI_IMAGE_LAB_FULLSCREEN_VERTEX_SOURCE}
)

rhi_image_lab_compile_shader(
    ${RHI_IMAGE_LAB_PRESENT_2D_FRAGMENT_OUTPUT}
    frag
    ${RHI_IMAGE_LAB_PRESENT_2D_FRAGMENT_SOURCE}
)

rhi_image_lab_compile_shader(
    ${RHI_IMAGE_LAB_PRESENT_ARRAY_FRAGMENT_OUTPUT}
    frag
    ${RHI_IMAGE_LAB_PRESENT_ARRAY_FRAGMENT_SOURCE}
)

rhi_image_lab_compile_shader(
    ${RHI_IMAGE_LAB_PRESENT_3D_FRAGMENT_OUTPUT}
    frag
    ${RHI_IMAGE_LAB_PRESENT_3D_FRAGMENT_SOURCE}
)

rhi_image_lab_compile_shader(
    ${RHI_IMAGE_LAB_MRT1_FRAGMENT_OUTPUT}
    frag
    ${RHI_IMAGE_LAB_MRT_FRAGMENT_SOURCE}
    -DMRT_OUTPUT_COUNT=1
)

rhi_image_lab_compile_shader(
    ${RHI_IMAGE_LAB_MRT2_FRAGMENT_OUTPUT}
    frag
    ${RHI_IMAGE_LAB_MRT_FRAGMENT_SOURCE}
    -DMRT_OUTPUT_COUNT=2
)

rhi_image_lab_compile_shader(
    ${RHI_IMAGE_LAB_MRT3_FRAGMENT_OUTPUT}
    frag
    ${RHI_IMAGE_LAB_MRT_FRAGMENT_SOURCE}
    -DMRT_OUTPUT_COUNT=3
)

rhi_image_lab_compile_shader(
    ${RHI_IMAGE_LAB_MRT4_FRAGMENT_OUTPUT}
    frag
    ${RHI_IMAGE_LAB_MRT_FRAGMENT_SOURCE}
    -DMRT_OUTPUT_COUNT=4
)

add_custom_target(
    RhiImageLabShaders
    DEPENDS
        ${RHI_IMAGE_LAB_FULLSCREEN_VERTEX_OUTPUT}
        ${RHI_IMAGE_LAB_PRESENT_2D_FRAGMENT_OUTPUT}
        ${RHI_IMAGE_LAB_PRESENT_ARRAY_FRAGMENT_OUTPUT}
        ${RHI_IMAGE_LAB_PRESENT_3D_FRAGMENT_OUTPUT}
        ${RHI_IMAGE_LAB_MRT1_FRAGMENT_OUTPUT}
        ${RHI_IMAGE_LAB_MRT2_FRAGMENT_OUTPUT}
        ${RHI_IMAGE_LAB_MRT3_FRAGMENT_OUTPUT}
        ${RHI_IMAGE_LAB_MRT4_FRAGMENT_OUTPUT}
)

add_executable(
    RhiImageLab
    main.cpp
    ImageLabState.h
    ImageLabPipeline.h
    ImageLabPipeline.cpp
)

add_dependencies(RhiImageLab RhiImageLabShaders)

target_include_directories(
    RhiImageLab
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    RhiImageLab
    PRIVATE
        LunaCore
)

target_compile_definitions(
    RhiImageLab
    PRIVATE
        RHI_IMAGE_LAB_SHADER_ROOT="${RHI_IMAGE_LAB_SHADER_OUTPUT_DIR}"
)

luna_configure_runtime_output(RhiImageLab)
