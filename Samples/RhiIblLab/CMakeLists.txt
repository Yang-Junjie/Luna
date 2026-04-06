set(RHI_IBL_LAB_SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Shaders)
set(RHI_IBL_LAB_SHADER_OUTPUT_DIR ${PROJECT_BINARY_DIR}/Shaders/RhiIblLab)

function(rhi_ibl_lab_compile_shader output_file shader_stage shader_source)
    add_custom_command(
        OUTPUT ${output_file}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RHI_IBL_LAB_SHADER_OUTPUT_DIR}
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} -fshader-stage=${shader_stage} ${shader_source} -o ${output_file}
        DEPENDS ${shader_source}
        VERBATIM
    )
endfunction()

set(RHI_IBL_LAB_FULLSCREEN_VERTEX_SOURCE ${RHI_IBL_LAB_SHADER_SOURCE_DIR}/ibl_lab_fullscreen.vert)
set(RHI_IBL_LAB_CUBE_ATLAS_FRAGMENT_SOURCE ${RHI_IBL_LAB_SHADER_SOURCE_DIR}/ibl_lab_cube_atlas.frag)
set(RHI_IBL_LAB_CUBE_SKYBOX_FRAGMENT_SOURCE ${RHI_IBL_LAB_SHADER_SOURCE_DIR}/ibl_lab_cube_skybox.frag)
set(RHI_IBL_LAB_FACE_PREVIEW_FRAGMENT_SOURCE ${RHI_IBL_LAB_SHADER_SOURCE_DIR}/ibl_lab_face_preview.frag)
set(RHI_IBL_LAB_CUBE_FILTER_FRAGMENT_SOURCE ${RHI_IBL_LAB_SHADER_SOURCE_DIR}/ibl_lab_cube_filter.frag)

set(RHI_IBL_LAB_FULLSCREEN_VERTEX_OUTPUT ${RHI_IBL_LAB_SHADER_OUTPUT_DIR}/ibl_lab_fullscreen.vert.spv)
set(RHI_IBL_LAB_CUBE_ATLAS_FRAGMENT_OUTPUT ${RHI_IBL_LAB_SHADER_OUTPUT_DIR}/ibl_lab_cube_atlas.frag.spv)
set(RHI_IBL_LAB_CUBE_SKYBOX_FRAGMENT_OUTPUT ${RHI_IBL_LAB_SHADER_OUTPUT_DIR}/ibl_lab_cube_skybox.frag.spv)
set(RHI_IBL_LAB_FACE_PREVIEW_FRAGMENT_OUTPUT ${RHI_IBL_LAB_SHADER_OUTPUT_DIR}/ibl_lab_face_preview.frag.spv)
set(RHI_IBL_LAB_CUBE_FILTER_FRAGMENT_OUTPUT ${RHI_IBL_LAB_SHADER_OUTPUT_DIR}/ibl_lab_cube_filter.frag.spv)

rhi_ibl_lab_compile_shader(${RHI_IBL_LAB_FULLSCREEN_VERTEX_OUTPUT} vert ${RHI_IBL_LAB_FULLSCREEN_VERTEX_SOURCE})
rhi_ibl_lab_compile_shader(${RHI_IBL_LAB_CUBE_ATLAS_FRAGMENT_OUTPUT} frag ${RHI_IBL_LAB_CUBE_ATLAS_FRAGMENT_SOURCE})
rhi_ibl_lab_compile_shader(${RHI_IBL_LAB_CUBE_SKYBOX_FRAGMENT_OUTPUT} frag ${RHI_IBL_LAB_CUBE_SKYBOX_FRAGMENT_SOURCE})
rhi_ibl_lab_compile_shader(${RHI_IBL_LAB_FACE_PREVIEW_FRAGMENT_OUTPUT} frag ${RHI_IBL_LAB_FACE_PREVIEW_FRAGMENT_SOURCE})
rhi_ibl_lab_compile_shader(${RHI_IBL_LAB_CUBE_FILTER_FRAGMENT_OUTPUT} frag ${RHI_IBL_LAB_CUBE_FILTER_FRAGMENT_SOURCE})

add_custom_target(
    RhiIblLabShaders
    DEPENDS
        ${RHI_IBL_LAB_FULLSCREEN_VERTEX_OUTPUT}
        ${RHI_IBL_LAB_CUBE_ATLAS_FRAGMENT_OUTPUT}
        ${RHI_IBL_LAB_CUBE_SKYBOX_FRAGMENT_OUTPUT}
        ${RHI_IBL_LAB_FACE_PREVIEW_FRAGMENT_OUTPUT}
        ${RHI_IBL_LAB_CUBE_FILTER_FRAGMENT_OUTPUT}
)

add_executable(
    RhiIblLab
    main.cpp
    IblLabState.h
    IblLabPipeline.h
    IblLabPipeline.cpp
)

add_dependencies(RhiIblLab RhiIblLabShaders)

target_include_directories(
    RhiIblLab
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    RhiIblLab
    PRIVATE
        LunaCore
)

target_compile_definitions(
    RhiIblLab
    PRIVATE
        RHI_IBL_LAB_SHADER_ROOT="${RHI_IBL_LAB_SHADER_OUTPUT_DIR}"
)

luna_configure_runtime_output(RhiIblLab)

if(BUILD_TESTING)
    add_test(
        NAME RhiIblLab.selftest.phase1_boot
        COMMAND $<TARGET_FILE:RhiIblLab> --self-test=phase1_boot
    )
    add_test(
        NAME RhiIblLab.selftest.phase2_enum_contract
        COMMAND $<TARGET_FILE:RhiIblLab> --self-test=phase2_enum_contract
    )
    add_test(
        NAME RhiIblLab.selftest.phase2_cube_desc_validation
        COMMAND $<TARGET_FILE:RhiIblLab> --self-test=phase2_cube_desc_validation
    )
    add_test(
        NAME RhiIblLab.selftest.phase2_cube_view_validation
        COMMAND $<TARGET_FILE:RhiIblLab> --self-test=phase2_cube_view_validation
    )
    add_test(
        NAME RhiIblLab.selftest.phase2_sampler_contract
        COMMAND $<TARGET_FILE:RhiIblLab> --self-test=phase2_sampler_contract
    )
    add_test(
        NAME RhiIblLab.selftest.phase3_cube_image_create
        COMMAND $<TARGET_FILE:RhiIblLab> --self-test=phase3_cube_image_create
    )
    add_test(
        NAME RhiIblLab.selftest.phase3_cube_full_view
        COMMAND $<TARGET_FILE:RhiIblLab> --self-test=phase3_cube_full_view
    )
    add_test(
        NAME RhiIblLab.selftest.phase3_cube_face_view
        COMMAND $<TARGET_FILE:RhiIblLab> --self-test=phase3_cube_face_view
    )
    add_test(
        NAME RhiIblLab.selftest.phase3_sampler_vk_mapping
        COMMAND $<TARGET_FILE:RhiIblLab> --self-test=phase3_sampler_vk_mapping
    )
    add_test(
        NAME RhiIblLab.selftest.phase4_cube_faces_distinct
        COMMAND $<TARGET_FILE:RhiIblLab> --self-test=phase4_cube_faces_distinct
    )
    add_test(
        NAME RhiIblLab.selftest.phase4_skybox_rotation
        COMMAND $<TARGET_FILE:RhiIblLab> --self-test=phase4_skybox_rotation
    )
    add_test(
        NAME RhiIblLab.selftest.phase4_face_mip_preview
        COMMAND $<TARGET_FILE:RhiIblLab> --self-test=phase4_face_mip_preview
    )
    add_test(
        NAME RhiIblLab.selftest.phase4_face_isolation
        COMMAND $<TARGET_FILE:RhiIblLab> --self-test=phase4_face_isolation
    )
    add_test(
        NAME RhiIblLab.selftest.phase5_env_mips
        COMMAND $<TARGET_FILE:RhiIblLab> --self-test=phase5_env_mips
    )
    add_test(
        NAME RhiIblLab.selftest.phase6_irradiance_smooth
        COMMAND $<TARGET_FILE:RhiIblLab> --self-test=phase6_irradiance_smooth
    )
    add_test(
        NAME RhiIblLab.selftest.phase6_prefilter_mips
        COMMAND $<TARGET_FILE:RhiIblLab> --self-test=phase6_prefilter_mips
    )
    add_test(
        NAME RhiIblLab.selftest.phase6_brdf_lut_preview
        COMMAND $<TARGET_FILE:RhiIblLab> --self-test=phase6_brdf_lut_preview
    )
    set_tests_properties(
        RhiIblLab.selftest.phase1_boot
        RhiIblLab.selftest.phase2_enum_contract
        RhiIblLab.selftest.phase2_cube_desc_validation
        RhiIblLab.selftest.phase2_cube_view_validation
        RhiIblLab.selftest.phase2_sampler_contract
        RhiIblLab.selftest.phase3_cube_image_create
        RhiIblLab.selftest.phase3_cube_full_view
        RhiIblLab.selftest.phase3_cube_face_view
        RhiIblLab.selftest.phase3_sampler_vk_mapping
        RhiIblLab.selftest.phase4_cube_faces_distinct
        RhiIblLab.selftest.phase4_skybox_rotation
        RhiIblLab.selftest.phase4_face_mip_preview
        RhiIblLab.selftest.phase4_face_isolation
        RhiIblLab.selftest.phase5_env_mips
        RhiIblLab.selftest.phase6_irradiance_smooth
        RhiIblLab.selftest.phase6_prefilter_mips
        RhiIblLab.selftest.phase6_brdf_lut_preview
        PROPERTIES
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
endif()
