set(RHI_SYNC_LAB_SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Shaders)
set(RHI_SYNC_LAB_SHADER_OUTPUT_DIR ${PROJECT_BINARY_DIR}/Shaders/RhiSyncLab)

function(rhi_sync_lab_compile_shader output_file shader_stage shader_source)
    add_custom_command(
        OUTPUT ${output_file}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RHI_SYNC_LAB_SHADER_OUTPUT_DIR}
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} -fshader-stage=${shader_stage} ${shader_source} -o ${output_file}
        DEPENDS ${shader_source}
        VERBATIM
    )
endfunction()

set(RHI_SYNC_LAB_FULLSCREEN_VERTEX_SOURCE ${RHI_SYNC_LAB_SHADER_SOURCE_DIR}/sync_lab_fullscreen.vert)
set(RHI_SYNC_LAB_INDIRECT_DRAW_VERTEX_SOURCE ${RHI_SYNC_LAB_SHADER_SOURCE_DIR}/sync_lab_indirect_draw.vert)
set(RHI_SYNC_LAB_INDIRECT_DRAW_FRAGMENT_SOURCE ${RHI_SYNC_LAB_SHADER_SOURCE_DIR}/sync_lab_indirect_draw.frag)
set(RHI_SYNC_LAB_HISTORY_FRAGMENT_SOURCE ${RHI_SYNC_LAB_SHADER_SOURCE_DIR}/sync_lab_history_preview.frag)
set(RHI_SYNC_LAB_SINGLE_FRAGMENT_SOURCE ${RHI_SYNC_LAB_SHADER_SOURCE_DIR}/sync_lab_single_preview.frag)
set(RHI_SYNC_LAB_GENERATE_ARGS_SOURCE ${RHI_SYNC_LAB_SHADER_SOURCE_DIR}/sync_lab_generate_args.comp)
set(RHI_SYNC_LAB_PAINT_INDIRECT_SOURCE ${RHI_SYNC_LAB_SHADER_SOURCE_DIR}/sync_lab_paint_indirect.comp)

set(RHI_SYNC_LAB_FULLSCREEN_VERTEX_OUTPUT ${RHI_SYNC_LAB_SHADER_OUTPUT_DIR}/sync_lab_fullscreen.vert.spv)
set(RHI_SYNC_LAB_INDIRECT_DRAW_VERTEX_OUTPUT ${RHI_SYNC_LAB_SHADER_OUTPUT_DIR}/sync_lab_indirect_draw.vert.spv)
set(RHI_SYNC_LAB_INDIRECT_DRAW_FRAGMENT_OUTPUT ${RHI_SYNC_LAB_SHADER_OUTPUT_DIR}/sync_lab_indirect_draw.frag.spv)
set(RHI_SYNC_LAB_HISTORY_FRAGMENT_OUTPUT ${RHI_SYNC_LAB_SHADER_OUTPUT_DIR}/sync_lab_history_preview.frag.spv)
set(RHI_SYNC_LAB_SINGLE_FRAGMENT_OUTPUT ${RHI_SYNC_LAB_SHADER_OUTPUT_DIR}/sync_lab_single_preview.frag.spv)
set(RHI_SYNC_LAB_GENERATE_ARGS_OUTPUT ${RHI_SYNC_LAB_SHADER_OUTPUT_DIR}/sync_lab_generate_args.comp.spv)
set(RHI_SYNC_LAB_PAINT_INDIRECT_OUTPUT ${RHI_SYNC_LAB_SHADER_OUTPUT_DIR}/sync_lab_paint_indirect.comp.spv)

rhi_sync_lab_compile_shader(${RHI_SYNC_LAB_FULLSCREEN_VERTEX_OUTPUT} vert ${RHI_SYNC_LAB_FULLSCREEN_VERTEX_SOURCE})
rhi_sync_lab_compile_shader(${RHI_SYNC_LAB_INDIRECT_DRAW_VERTEX_OUTPUT} vert ${RHI_SYNC_LAB_INDIRECT_DRAW_VERTEX_SOURCE})
rhi_sync_lab_compile_shader(${RHI_SYNC_LAB_INDIRECT_DRAW_FRAGMENT_OUTPUT} frag ${RHI_SYNC_LAB_INDIRECT_DRAW_FRAGMENT_SOURCE})
rhi_sync_lab_compile_shader(${RHI_SYNC_LAB_HISTORY_FRAGMENT_OUTPUT} frag ${RHI_SYNC_LAB_HISTORY_FRAGMENT_SOURCE})
rhi_sync_lab_compile_shader(${RHI_SYNC_LAB_SINGLE_FRAGMENT_OUTPUT} frag ${RHI_SYNC_LAB_SINGLE_FRAGMENT_SOURCE})
rhi_sync_lab_compile_shader(${RHI_SYNC_LAB_GENERATE_ARGS_OUTPUT} comp ${RHI_SYNC_LAB_GENERATE_ARGS_SOURCE})
rhi_sync_lab_compile_shader(${RHI_SYNC_LAB_PAINT_INDIRECT_OUTPUT} comp ${RHI_SYNC_LAB_PAINT_INDIRECT_SOURCE})

add_custom_target(
    RhiSyncLabShaders
    DEPENDS
        ${RHI_SYNC_LAB_FULLSCREEN_VERTEX_OUTPUT}
        ${RHI_SYNC_LAB_INDIRECT_DRAW_VERTEX_OUTPUT}
        ${RHI_SYNC_LAB_INDIRECT_DRAW_FRAGMENT_OUTPUT}
        ${RHI_SYNC_LAB_HISTORY_FRAGMENT_OUTPUT}
        ${RHI_SYNC_LAB_SINGLE_FRAGMENT_OUTPUT}
        ${RHI_SYNC_LAB_GENERATE_ARGS_OUTPUT}
        ${RHI_SYNC_LAB_PAINT_INDIRECT_OUTPUT}
)

add_executable(
    RhiSyncLab
    main.cpp
    SyncLabState.h
    SyncLabPipeline.h
    SyncLabPipeline.cpp
)

add_dependencies(RhiSyncLab RhiSyncLabShaders)

target_include_directories(
    RhiSyncLab
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    RhiSyncLab
    PRIVATE
        LunaCore
)

target_compile_definitions(
    RhiSyncLab
    PRIVATE
        RHI_SYNC_LAB_SHADER_ROOT="${RHI_SYNC_LAB_SHADER_OUTPUT_DIR}"
)

luna_configure_runtime_output(RhiSyncLab)

if(BUILD_TESTING)
    add_test(
        NAME RhiSyncLab.selftest.phase3_upload_ring
        COMMAND $<TARGET_FILE:RhiSyncLab> --self-test=phase3_upload_ring
    )
    add_test(
        NAME RhiSyncLab.selftest.phase5_subresource_barrier
        COMMAND $<TARGET_FILE:RhiSyncLab> --self-test=phase5_subresource_barrier
    )
    set_tests_properties(
        RhiSyncLab.selftest.phase3_upload_ring
        RhiSyncLab.selftest.phase5_subresource_barrier
        PROPERTIES
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
endif()
