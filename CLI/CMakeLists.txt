add_executable(
    LunaCLI
    LunaCLI.cpp
)

target_link_libraries(
    LunaCLI
    PRIVATE
        LunaCore
)

set_target_properties(
    LunaCLI
    PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

if(BUILD_TESTING)
    add_test(
        NAME CliSmokeTest
        COMMAND
            LunaCLI
            new
            primitive CubeBox Cube
            transform CubeBox 1 2 3 0 45 0 1 1 1
            point-light KeyLight
            transform KeyLight 0 4 2 -35 0 0 1 1 1
            save "${CMAKE_CURRENT_BINARY_DIR}/Smoke/CliSmoke"
    )

    add_test(
        NAME CliJsonSmokeTest
        COMMAND
            LunaCLI
            --json
            new
            primitive JsonCube Cube
            transform JsonCube 0 0 0 0 0 0 1 1 1
            save "${CMAKE_CURRENT_BINARY_DIR}/Smoke/CliJsonSmoke"
    )

    add_test(
        NAME CliPlanSmokeTest
        COMMAND
            LunaCLI
            plan "${CMAKE_CURRENT_SOURCE_DIR}/client/examples/smoke.plan.json"
    )

    add_test(
        NAME CliInspectSmokeTest
        COMMAND
            LunaCLI
            --json
            new
            primitive InspectCube Cube
            transform InspectCube 1 2 3 0 30 0 1 1 1
            inspect entity InspectCube
            inspect hierarchy
    )

    add_test(
        NAME CliVerifySmokeTest
        COMMAND
            LunaCLI
            --json
            new
            primitive VerifyCube Cube
            verify entity VerifyCube
            verify component VerifyCube Mesh
            verify entity-count-at-least 3
            save "${CMAKE_CURRENT_BINARY_DIR}/Smoke/CliVerifySmoke"
            verify saved
    )

    find_program(LUNA_NODE_EXECUTABLE node)
    if(LUNA_NODE_EXECUTABLE)
        add_test(
            NAME CliTsClientSmokeTest
            COMMAND
                "${LUNA_NODE_EXECUTABLE}"
                --disable-warning=ExperimentalWarning
                --experimental-strip-types
                "${CMAKE_CURRENT_SOURCE_DIR}/client/luna.ts"
                --host "$<TARGET_FILE:LunaCLI>"
                run
                new
                primitive TsCube Cube
                transform TsCube 2 1 0 0 30 0 1 1 1
                save "${CMAKE_CURRENT_BINARY_DIR}/Smoke/CliTsClientSmoke"
        )
    endif()
endif()
