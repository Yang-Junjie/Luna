set(RHI_BINDING_LAB_SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Shaders)
set(RHI_BINDING_LAB_SHADER_OUTPUT_DIR ${PROJECT_BINARY_DIR}/Shaders/RhiBindingLab)

function(rhi_binding_lab_compile_shader output_file shader_stage shader_source)
    add_custom_command(
        OUTPUT ${output_file}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RHI_BINDING_LAB_SHADER_OUTPUT_DIR}
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} -fshader-stage=${shader_stage} ${shader_source} -o ${output_file}
        DEPENDS ${shader_source}
        VERBATIM
    )
endfunction()

set(RHI_BINDING_LAB_FULLSCREEN_VERTEX_SOURCE ${RHI_BINDING_LAB_SHADER_SOURCE_DIR}/binding_lab_fullscreen.vert)
set(RHI_BINDING_LAB_PROBE_FRAGMENT_SOURCE ${RHI_BINDING_LAB_SHADER_SOURCE_DIR}/binding_lab_probe.frag)
set(RHI_BINDING_LAB_MULTISET_FRAGMENT_SOURCE ${RHI_BINDING_LAB_SHADER_SOURCE_DIR}/binding_lab_multiset.frag)
set(RHI_BINDING_LAB_DESCRIPTOR_ARRAY_FRAGMENT_SOURCE ${RHI_BINDING_LAB_SHADER_SOURCE_DIR}/binding_lab_descriptor_array.frag)
set(RHI_BINDING_LAB_DYNAMIC_UNIFORM_FRAGMENT_SOURCE ${RHI_BINDING_LAB_SHADER_SOURCE_DIR}/binding_lab_dynamic_uniform.frag)

set(RHI_BINDING_LAB_FULLSCREEN_VERTEX_OUTPUT ${RHI_BINDING_LAB_SHADER_OUTPUT_DIR}/binding_lab_fullscreen.vert.spv)
set(RHI_BINDING_LAB_PROBE_FRAGMENT_OUTPUT ${RHI_BINDING_LAB_SHADER_OUTPUT_DIR}/binding_lab_probe.frag.spv)
set(RHI_BINDING_LAB_MULTISET_FRAGMENT_OUTPUT ${RHI_BINDING_LAB_SHADER_OUTPUT_DIR}/binding_lab_multiset.frag.spv)
set(RHI_BINDING_LAB_DESCRIPTOR_ARRAY_FRAGMENT_OUTPUT ${RHI_BINDING_LAB_SHADER_OUTPUT_DIR}/binding_lab_descriptor_array.frag.spv)
set(RHI_BINDING_LAB_DYNAMIC_UNIFORM_FRAGMENT_OUTPUT ${RHI_BINDING_LAB_SHADER_OUTPUT_DIR}/binding_lab_dynamic_uniform.frag.spv)

rhi_binding_lab_compile_shader(${RHI_BINDING_LAB_FULLSCREEN_VERTEX_OUTPUT} vert ${RHI_BINDING_LAB_FULLSCREEN_VERTEX_SOURCE})
rhi_binding_lab_compile_shader(${RHI_BINDING_LAB_PROBE_FRAGMENT_OUTPUT} frag ${RHI_BINDING_LAB_PROBE_FRAGMENT_SOURCE})
rhi_binding_lab_compile_shader(${RHI_BINDING_LAB_MULTISET_FRAGMENT_OUTPUT} frag ${RHI_BINDING_LAB_MULTISET_FRAGMENT_SOURCE})
rhi_binding_lab_compile_shader(${RHI_BINDING_LAB_DESCRIPTOR_ARRAY_FRAGMENT_OUTPUT} frag ${RHI_BINDING_LAB_DESCRIPTOR_ARRAY_FRAGMENT_SOURCE})
rhi_binding_lab_compile_shader(${RHI_BINDING_LAB_DYNAMIC_UNIFORM_FRAGMENT_OUTPUT} frag ${RHI_BINDING_LAB_DYNAMIC_UNIFORM_FRAGMENT_SOURCE})

add_custom_target(
    RhiBindingLabShaders
    DEPENDS
        ${RHI_BINDING_LAB_FULLSCREEN_VERTEX_OUTPUT}
        ${RHI_BINDING_LAB_PROBE_FRAGMENT_OUTPUT}
        ${RHI_BINDING_LAB_MULTISET_FRAGMENT_OUTPUT}
        ${RHI_BINDING_LAB_DESCRIPTOR_ARRAY_FRAGMENT_OUTPUT}
        ${RHI_BINDING_LAB_DYNAMIC_UNIFORM_FRAGMENT_OUTPUT}
)

add_executable(
    RhiBindingLab
    main.cpp
    BindingLabState.h
    BindingLabPipeline.h
    BindingLabPipeline.cpp
)

add_dependencies(RhiBindingLab RhiBindingLabShaders)

target_include_directories(
    RhiBindingLab
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    RhiBindingLab
    PRIVATE
        LunaCore
)

target_compile_definitions(
    RhiBindingLab
    PRIVATE
        RHI_BINDING_LAB_SHADER_ROOT="${RHI_BINDING_LAB_SHADER_OUTPUT_DIR}"
)

luna_configure_runtime_output(RhiBindingLab)

if(BUILD_TESTING)
    add_test(
        NAME RhiBindingLab.selftest.phase3_descriptor_recycle
        COMMAND $<TARGET_FILE:RhiBindingLab> --self-test=phase3_descriptor_recycle
    )
    add_test(
        NAME RhiBindingLab.selftest.phase5_layout_validation
        COMMAND $<TARGET_FILE:RhiBindingLab> --self-test=phase5_layout_validation
    )
    set_tests_properties(
        RhiBindingLab.selftest.phase3_descriptor_recycle
        RhiBindingLab.selftest.phase5_layout_validation
        PROPERTIES
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
endif()
