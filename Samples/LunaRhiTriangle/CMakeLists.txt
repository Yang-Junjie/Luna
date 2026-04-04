set(LUNA_RHI_TRIANGLE_SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Shaders)
set(LUNA_RHI_TRIANGLE_SHADER_OUTPUT_DIR ${PROJECT_BINARY_DIR}/Shaders/RhiTriangle)

set(LUNA_RHI_TRIANGLE_VERTEX_SHADER_SOURCE ${LUNA_RHI_TRIANGLE_SHADER_SOURCE_DIR}/triangle.vert)
set(LUNA_RHI_TRIANGLE_FRAGMENT_SHADER_SOURCE ${LUNA_RHI_TRIANGLE_SHADER_SOURCE_DIR}/triangle.frag)
set(LUNA_RHI_TRIANGLE_VERTEX_SHADER_OUTPUT ${LUNA_RHI_TRIANGLE_SHADER_OUTPUT_DIR}/triangle.vert.spv)
set(LUNA_RHI_TRIANGLE_FRAGMENT_SHADER_OUTPUT ${LUNA_RHI_TRIANGLE_SHADER_OUTPUT_DIR}/triangle.frag.spv)

add_custom_command(
    OUTPUT ${LUNA_RHI_TRIANGLE_VERTEX_SHADER_OUTPUT}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${LUNA_RHI_TRIANGLE_SHADER_OUTPUT_DIR}
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} -fshader-stage=vert ${LUNA_RHI_TRIANGLE_VERTEX_SHADER_SOURCE} -o ${LUNA_RHI_TRIANGLE_VERTEX_SHADER_OUTPUT}
    DEPENDS ${LUNA_RHI_TRIANGLE_VERTEX_SHADER_SOURCE}
    VERBATIM
)

add_custom_command(
    OUTPUT ${LUNA_RHI_TRIANGLE_FRAGMENT_SHADER_OUTPUT}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${LUNA_RHI_TRIANGLE_SHADER_OUTPUT_DIR}
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} -fshader-stage=frag ${LUNA_RHI_TRIANGLE_FRAGMENT_SHADER_SOURCE} -o ${LUNA_RHI_TRIANGLE_FRAGMENT_SHADER_OUTPUT}
    DEPENDS ${LUNA_RHI_TRIANGLE_FRAGMENT_SHADER_SOURCE}
    VERBATIM
)

add_custom_target(
    LunaRhiTriangleShaders
    DEPENDS
        ${LUNA_RHI_TRIANGLE_VERTEX_SHADER_OUTPUT}
        ${LUNA_RHI_TRIANGLE_FRAGMENT_SHADER_OUTPUT}
)

add_executable(
    LunaRhiTriangle
    main.cpp
)

add_dependencies(LunaRhiTriangle LunaRhiTriangleShaders)

target_include_directories(
    LunaRhiTriangle
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    LunaRhiTriangle
    PRIVATE
        LunaCore
)

target_compile_definitions(
    LunaRhiTriangle
    PRIVATE
        LUNA_RHI_TRIANGLE_SAMPLE_NAME="LunaRhiTriangle"
        LUNA_RHI_TRIANGLE_BUILD_SHADER_ROOT="${PROJECT_BINARY_DIR}/Shaders/RhiTriangle"
)

luna_configure_runtime_output(LunaRhiTriangle)

if(BUILD_TESTING)
    add_test(NAME LunaRhiTriangle.selftest.list COMMAND $<TARGET_FILE:LunaRhiTriangle> --self-test)
    add_test(NAME LunaRhiTriangle.selftest.types COMMAND $<TARGET_FILE:LunaRhiTriangle> --self-test=types)
    add_test(NAME LunaRhiTriangle.selftest.desc COMMAND $<TARGET_FILE:LunaRhiTriangle> --self-test=desc)
    add_test(NAME LunaRhiTriangle.selftest.device_init COMMAND $<TARGET_FILE:LunaRhiTriangle> --self-test=device-init)
    add_test(NAME LunaRhiTriangle.selftest.device_contract COMMAND $<TARGET_FILE:LunaRhiTriangle> --self-test=device-contract)
    add_test(NAME LunaRhiTriangle.selftest.command_contract COMMAND $<TARGET_FILE:LunaRhiTriangle> --self-test=command-contract)
    add_test(NAME LunaRhiTriangle.selftest.frame_context COMMAND $<TARGET_FILE:LunaRhiTriangle> --self-test=frame-context)
    add_test(NAME LunaRhiTriangle.selftest.buffer COMMAND $<TARGET_FILE:LunaRhiTriangle> --self-test=buffer)
    add_test(NAME LunaRhiTriangle.selftest.image COMMAND $<TARGET_FILE:LunaRhiTriangle> --self-test=image)
    add_test(NAME LunaRhiTriangle.selftest.shader COMMAND $<TARGET_FILE:LunaRhiTriangle> --self-test=shader)
    add_test(NAME LunaRhiTriangle.selftest.upload COMMAND $<TARGET_FILE:LunaRhiTriangle> --self-test=upload)
    add_test(NAME LunaRhiTriangle.selftest.layout COMMAND $<TARGET_FILE:LunaRhiTriangle> --self-test=layout)
    add_test(NAME LunaRhiTriangle.selftest.resourceset_buffer COMMAND $<TARGET_FILE:LunaRhiTriangle> --self-test=resourceset-buffer)
    add_test(NAME LunaRhiTriangle.selftest.resourceset_image COMMAND $<TARGET_FILE:LunaRhiTriangle> --self-test=resourceset-image)
    add_test(NAME LunaRhiTriangle.selftest.pipeline COMMAND $<TARGET_FILE:LunaRhiTriangle> --self-test=pipeline)
    add_test(NAME LunaRhiTriangle.selftest.layout_mismatch COMMAND $<TARGET_FILE:LunaRhiTriangle> --self-test=layout-mismatch)
    add_test(NAME LunaRhiTriangle.selftest.vertex COMMAND $<TARGET_FILE:LunaRhiTriangle> --self-test=vertex)
    add_test(NAME LunaRhiTriangle.selftest.backend_vulkan COMMAND $<TARGET_FILE:LunaRhiTriangle> --self-test=backend --backend=vulkan)
    add_test(NAME LunaRhiTriangle.selftest.backend_d3d12 COMMAND $<TARGET_FILE:LunaRhiTriangle> --self-test=backend --backend=d3d12)
    add_test(NAME LunaRhiTriangle.selftest.backend_metal COMMAND $<TARGET_FILE:LunaRhiTriangle> --self-test=backend --backend=metal)

    set_tests_properties(
        LunaRhiTriangle.selftest.list
        LunaRhiTriangle.selftest.types
        LunaRhiTriangle.selftest.desc
        LunaRhiTriangle.selftest.device_init
        LunaRhiTriangle.selftest.device_contract
        LunaRhiTriangle.selftest.command_contract
        LunaRhiTriangle.selftest.frame_context
        LunaRhiTriangle.selftest.buffer
        LunaRhiTriangle.selftest.image
        LunaRhiTriangle.selftest.shader
        LunaRhiTriangle.selftest.upload
        LunaRhiTriangle.selftest.layout
        LunaRhiTriangle.selftest.resourceset_buffer
        LunaRhiTriangle.selftest.resourceset_image
        LunaRhiTriangle.selftest.pipeline
        LunaRhiTriangle.selftest.layout_mismatch
        LunaRhiTriangle.selftest.vertex
        LunaRhiTriangle.selftest.backend_vulkan
        LunaRhiTriangle.selftest.backend_d3d12
        LunaRhiTriangle.selftest.backend_metal
        PROPERTIES
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
endif()
