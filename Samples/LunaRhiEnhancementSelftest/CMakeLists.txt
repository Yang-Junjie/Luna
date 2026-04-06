add_executable(
    LunaRhiEnhancementSelftest
    main.cpp
)

add_dependencies(LunaRhiEnhancementSelftest RhiBindingLabShaders)

target_include_directories(
    LunaRhiEnhancementSelftest
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    LunaRhiEnhancementSelftest
    PRIVATE
        LunaCore
)

target_compile_definitions(
    LunaRhiEnhancementSelftest
    PRIVATE
        RHI_BINDING_LAB_SHADER_ROOT="${PROJECT_BINARY_DIR}/Shaders/RhiBindingLab"
)

luna_configure_runtime_output(LunaRhiEnhancementSelftest)

if(BUILD_TESTING)
    add_test(
        NAME LunaRhiEnhancement.selftest.rhi_enhancement_baseline
        COMMAND $<TARGET_FILE:LunaRhiEnhancementSelftest> --self-test=rhi_enhancement_baseline
    )
    add_test(
        NAME LunaRhiEnhancement.selftest.adapter_enumeration
        COMMAND $<TARGET_FILE:LunaRhiEnhancementSelftest> --self-test=adapter_enumeration
    )
    add_test(
        NAME LunaRhiEnhancement.selftest.device_creation_without_surface
        COMMAND $<TARGET_FILE:LunaRhiEnhancementSelftest> --self-test=device_creation_without_surface
    )
    add_test(
        NAME LunaRhiEnhancement.selftest.device_limits
        COMMAND $<TARGET_FILE:LunaRhiEnhancementSelftest> --self-test=device_limits
    )
    add_test(
        NAME LunaRhiEnhancement.selftest.format_support
        COMMAND $<TARGET_FILE:LunaRhiEnhancementSelftest> --self-test=format_support
    )
    add_test(
        NAME LunaRhiEnhancement.selftest.single_queue_submit
        COMMAND $<TARGET_FILE:LunaRhiEnhancementSelftest> --self-test=single_queue_submit
    )
    add_test(
        NAME LunaRhiEnhancement.selftest.swapchain_desc
        COMMAND $<TARGET_FILE:LunaRhiEnhancementSelftest> --self-test=swapchain_desc
    )
    add_test(
        NAME LunaRhiEnhancement.selftest.headless_device
        COMMAND $<TARGET_FILE:LunaRhiEnhancementSelftest> --self-test=headless_device
    )
    add_test(
        NAME LunaRhiEnhancement.selftest.headless_readback
        COMMAND $<TARGET_FILE:LunaRhiEnhancementSelftest> --self-test=headless_readback
    )
    add_test(
        NAME LunaRhiEnhancement.selftest.program_pipeline_contract
        COMMAND $<TARGET_FILE:LunaRhiEnhancementSelftest> --self-test=program_pipeline_contract
    )
    add_test(
        NAME LunaRhiEnhancement.selftest.layout_validation_negative
        COMMAND $<TARGET_FILE:LunaRhiEnhancementSelftest> --self-test=layout_validation_negative
    )
    add_test(
        NAME LunaRhiEnhancement.selftest.renderer_public_path
        COMMAND $<TARGET_FILE:LunaRhiEnhancementSelftest> --self-test=renderer_public_path
    )
    add_test(
        NAME LunaRhiEnhancement.selftest.graduation_matrix_summary
        COMMAND $<TARGET_FILE:LunaRhiEnhancementSelftest> --self-test=graduation_matrix_summary
    )
    add_test(
        NAME LunaRhiEnhancement.selftest.frame_retirement
        COMMAND $<TARGET_FILE:LunaRhiEnhancementSelftest> --self-test=frame_retirement
    )
    add_test(
        NAME LunaRhiEnhancement.selftest.resize_stress
        COMMAND $<TARGET_FILE:LunaRhiEnhancementSelftest> --self-test=resize_stress
    )

    set_tests_properties(
        LunaRhiEnhancement.selftest.rhi_enhancement_baseline
        LunaRhiEnhancement.selftest.adapter_enumeration
        LunaRhiEnhancement.selftest.device_creation_without_surface
        LunaRhiEnhancement.selftest.device_limits
        LunaRhiEnhancement.selftest.format_support
        LunaRhiEnhancement.selftest.single_queue_submit
        LunaRhiEnhancement.selftest.swapchain_desc
        LunaRhiEnhancement.selftest.headless_device
        LunaRhiEnhancement.selftest.headless_readback
        LunaRhiEnhancement.selftest.program_pipeline_contract
        LunaRhiEnhancement.selftest.layout_validation_negative
        LunaRhiEnhancement.selftest.renderer_public_path
        LunaRhiEnhancement.selftest.graduation_matrix_summary
        LunaRhiEnhancement.selftest.frame_retirement
        LunaRhiEnhancement.selftest.resize_stress
        PROPERTIES
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
endif()
