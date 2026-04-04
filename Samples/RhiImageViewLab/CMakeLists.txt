set(RHI_IMAGE_VIEW_LAB_SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Shaders)
set(RHI_IMAGE_VIEW_LAB_SHADER_OUTPUT_DIR ${PROJECT_BINARY_DIR}/Shaders/RhiImageViewLab)

function(rhi_image_view_lab_compile_shader output_file shader_stage shader_source)
    add_custom_command(
        OUTPUT ${output_file}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RHI_IMAGE_VIEW_LAB_SHADER_OUTPUT_DIR}
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} -fshader-stage=${shader_stage} ${shader_source} -o ${output_file}
        DEPENDS ${shader_source}
        VERBATIM
    )
endfunction()

set(RHI_IMAGE_VIEW_LAB_FULLSCREEN_VERTEX_SOURCE ${RHI_IMAGE_VIEW_LAB_SHADER_SOURCE_DIR}/image_view_lab_fullscreen.vert)
set(RHI_IMAGE_VIEW_LAB_PRESENT_2D_FRAGMENT_SOURCE ${RHI_IMAGE_VIEW_LAB_SHADER_SOURCE_DIR}/image_view_lab_present_2d.frag)
set(RHI_IMAGE_VIEW_LAB_PRESENT_ARRAY_FRAGMENT_SOURCE ${RHI_IMAGE_VIEW_LAB_SHADER_SOURCE_DIR}/image_view_lab_present_array.frag)
set(RHI_IMAGE_VIEW_LAB_PRESENT_3D_FRAGMENT_SOURCE ${RHI_IMAGE_VIEW_LAB_SHADER_SOURCE_DIR}/image_view_lab_present_3d.frag)

set(RHI_IMAGE_VIEW_LAB_FULLSCREEN_VERTEX_OUTPUT ${RHI_IMAGE_VIEW_LAB_SHADER_OUTPUT_DIR}/image_view_lab_fullscreen.vert.spv)
set(RHI_IMAGE_VIEW_LAB_PRESENT_2D_FRAGMENT_OUTPUT ${RHI_IMAGE_VIEW_LAB_SHADER_OUTPUT_DIR}/image_view_lab_present_2d.frag.spv)
set(RHI_IMAGE_VIEW_LAB_PRESENT_ARRAY_FRAGMENT_OUTPUT ${RHI_IMAGE_VIEW_LAB_SHADER_OUTPUT_DIR}/image_view_lab_present_array.frag.spv)
set(RHI_IMAGE_VIEW_LAB_PRESENT_3D_FRAGMENT_OUTPUT ${RHI_IMAGE_VIEW_LAB_SHADER_OUTPUT_DIR}/image_view_lab_present_3d.frag.spv)

rhi_image_view_lab_compile_shader(${RHI_IMAGE_VIEW_LAB_FULLSCREEN_VERTEX_OUTPUT} vert ${RHI_IMAGE_VIEW_LAB_FULLSCREEN_VERTEX_SOURCE})
rhi_image_view_lab_compile_shader(${RHI_IMAGE_VIEW_LAB_PRESENT_2D_FRAGMENT_OUTPUT} frag ${RHI_IMAGE_VIEW_LAB_PRESENT_2D_FRAGMENT_SOURCE})
rhi_image_view_lab_compile_shader(${RHI_IMAGE_VIEW_LAB_PRESENT_ARRAY_FRAGMENT_OUTPUT} frag ${RHI_IMAGE_VIEW_LAB_PRESENT_ARRAY_FRAGMENT_SOURCE})
rhi_image_view_lab_compile_shader(${RHI_IMAGE_VIEW_LAB_PRESENT_3D_FRAGMENT_OUTPUT} frag ${RHI_IMAGE_VIEW_LAB_PRESENT_3D_FRAGMENT_SOURCE})

add_custom_target(
    RhiImageViewLabShaders
    DEPENDS
        ${RHI_IMAGE_VIEW_LAB_FULLSCREEN_VERTEX_OUTPUT}
        ${RHI_IMAGE_VIEW_LAB_PRESENT_2D_FRAGMENT_OUTPUT}
        ${RHI_IMAGE_VIEW_LAB_PRESENT_ARRAY_FRAGMENT_OUTPUT}
        ${RHI_IMAGE_VIEW_LAB_PRESENT_3D_FRAGMENT_OUTPUT}
)

add_executable(
    RhiImageViewLab
    main.cpp
    ImageViewLabState.h
    ImageViewLabPipeline.h
    ImageViewLabPipeline.cpp
)

add_dependencies(RhiImageViewLab RhiImageViewLabShaders)

target_include_directories(
    RhiImageViewLab
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    RhiImageViewLab
    PRIVATE
        LunaCore
)

target_compile_definitions(
    RhiImageViewLab
    PRIVATE
        RHI_IMAGE_VIEW_LAB_SHADER_ROOT="${RHI_IMAGE_VIEW_LAB_SHADER_OUTPUT_DIR}"
)

luna_configure_runtime_output(RhiImageViewLab)
