add_executable(
    LunaCLI
    LunaCLI.cpp
)

add_executable(
    LunaAuthoringHost
    LunaAuthoringHost.cpp
)

target_link_libraries(
    LunaCLI
    PRIVATE
        LunaCore
)

target_link_libraries(
    LunaAuthoringHost
    PRIVATE
        LunaCore
        nlohmann_json::nlohmann_json
)

set_target_properties(
    LunaCLI
    PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

set_target_properties(
    LunaAuthoringHost
    PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

function(luna_set_test_properties test_name timeout labels)
    set_tests_properties(
        ${test_name}
        PROPERTIES
            TIMEOUT ${timeout}
            LABELS "${labels}"
    )
endfunction()

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
    luna_set_test_properties(CliSmokeTest 30 "quick;smoke;cli;authoring")

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
    luna_set_test_properties(CliJsonSmokeTest 30 "quick;smoke;cli;authoring")

    add_test(
        NAME CliDryRunSmokeTest
        COMMAND
            LunaCLI
            --dry-run
            --json
            new
            primitive DryRunCube Cube
            save "${CMAKE_CURRENT_BINARY_DIR}/Smoke/CliDryRunSmoke"
    )
    luna_set_test_properties(CliDryRunSmokeTest 30 "quick;smoke;cli;authoring")

    add_test(
        NAME CliPlanSmokeTest
        COMMAND
            LunaCLI
            plan "${CMAKE_CURRENT_SOURCE_DIR}/client/examples/smoke.plan.json"
    )
    luna_set_test_properties(CliPlanSmokeTest 30 "quick;smoke;cli;authoring")

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
            snapshot
    )
    luna_set_test_properties(CliInspectSmokeTest 30 "quick;smoke;cli;authoring")

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
    luna_set_test_properties(CliVerifySmokeTest 30 "quick;smoke;cli;authoring")

    add_test(
        NAME CliCapabilitiesSmokeTest
        COMMAND
            LunaCLI
            capabilities
            --json
    )
    luna_set_test_properties(CliCapabilitiesSmokeTest 30 "quick;smoke;cli;authoring")

    find_program(LUNA_NODE_EXECUTABLE node)
    if(LUNA_NODE_EXECUTABLE)
        add_test(
            NAME CliTsAuthoringProtocolSmokeTest
            COMMAND
                "${LUNA_NODE_EXECUTABLE}"
                --disable-warning=ExperimentalWarning
                --experimental-strip-types
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/authoringProtocol.smoke.ts"
                "${PROJECT_SOURCE_DIR}/Tests/Fixtures/Authoring/create_cube_scene.plan.json"
                "${PROJECT_SOURCE_DIR}/Tests/Fixtures/Authoring/invalid_unknown_command.plan.json"
        )
        luna_set_test_properties(CliTsAuthoringProtocolSmokeTest 30 "quick;smoke;cli;ts;authoring;protocol")

        add_test(
            NAME CliTsEditorShellSmokeTest
            COMMAND
                "${LUNA_NODE_EXECUTABLE}"
                --disable-warning=ExperimentalWarning
                --experimental-strip-types
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/editorShell.smoke.ts"
        )
        luna_set_test_properties(CliTsEditorShellSmokeTest 30 "quick;smoke;cli;ts;editor;protocol")

        add_test(
            NAME CliTsEditorViewBridgeSmokeTest
            COMMAND
                "${LUNA_NODE_EXECUTABLE}"
                --disable-warning=ExperimentalWarning
                --experimental-strip-types
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/editorViewBridge.smoke.ts"
                "$<TARGET_FILE:LunaEditorViewHost>"
        )
        luna_set_test_properties(CliTsEditorViewBridgeSmokeTest 45 "quick;smoke;cli;ts;editor;view;protocol")

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
        luna_set_test_properties(CliTsClientSmokeTest 45 "quick;smoke;cli;ts;authoring")

        add_test(
            NAME CliTsClientPlanFixtureSmokeTest
            COMMAND
                "${LUNA_NODE_EXECUTABLE}"
                --disable-warning=ExperimentalWarning
                --experimental-strip-types
                "${CMAKE_CURRENT_SOURCE_DIR}/client/luna.ts"
                --host "$<TARGET_FILE:LunaCLI>"
                --json
                plan
                "${PROJECT_SOURCE_DIR}/Tests/Fixtures/Authoring/create_cube_scene.plan.json"
        )
        luna_set_test_properties(CliTsClientPlanFixtureSmokeTest 45 "quick;smoke;cli;ts;authoring")

        add_test(
            NAME CliTsInteractivePlanSmokeTest
            COMMAND
                "${LUNA_NODE_EXECUTABLE}"
                --disable-warning=ExperimentalWarning
                --experimental-strip-types
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/interactive.smoke.ts"
                "${CMAKE_CURRENT_SOURCE_DIR}/client/luna.ts"
                "$<TARGET_FILE:LunaCLI>"
                "$<TARGET_FILE:LunaAuthoringHost>"
        )
        luna_set_test_properties(CliTsInteractivePlanSmokeTest 60 "quick;smoke;cli;ts;authoring")

        add_test(
            NAME CliTsComposeSmokeTest
            COMMAND
                "${LUNA_NODE_EXECUTABLE}"
                --disable-warning=ExperimentalWarning
                --experimental-strip-types
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/compose.smoke.ts"
                "${CMAKE_CURRENT_SOURCE_DIR}/client/luna.ts"
                "$<TARGET_FILE:LunaCLI>"
                "$<TARGET_FILE:LunaAuthoringHost>"
        )
        luna_set_test_properties(CliTsComposeSmokeTest 60 "quick;smoke;cli;ts;authoring")

        add_test(
            NAME CliTsAuthoringControllerSmokeTest
            COMMAND
                "${LUNA_NODE_EXECUTABLE}"
                --disable-warning=ExperimentalWarning
                --experimental-strip-types
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/authoringController.smoke.ts"
                "$<TARGET_FILE:LunaAuthoringHost>"
        )
        luna_set_test_properties(CliTsAuthoringControllerSmokeTest 45 "quick;smoke;cli;ts;authoring")

        add_test(
            NAME CliTsAuthoringTurnSmokeTest
            COMMAND
                "${LUNA_NODE_EXECUTABLE}"
                --disable-warning=ExperimentalWarning
                --experimental-strip-types
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/authoringTurn.smoke.ts"
                "$<TARGET_FILE:LunaAuthoringHost>"
                "${CMAKE_CURRENT_SOURCE_DIR}/client/luna.ts"
        )
        luna_set_test_properties(CliTsAuthoringTurnSmokeTest 60 "quick;smoke;cli;ts;authoring")

        add_test(
            NAME CliTsAuthoringAiPlannerSmokeTest
            COMMAND
                "${LUNA_NODE_EXECUTABLE}"
                --disable-warning=ExperimentalWarning
                --experimental-strip-types
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/authoringAiPlanner.smoke.ts"
                "$<TARGET_FILE:LunaAuthoringHost>"
                "${CMAKE_CURRENT_SOURCE_DIR}/client/luna.ts"
        )
        luna_set_test_properties(CliTsAuthoringAiPlannerSmokeTest 180 "ai;cli;ts;authoring")

        add_test(
            NAME CliTsChatSmokeTest
            COMMAND
                "${LUNA_NODE_EXECUTABLE}"
                --disable-warning=ExperimentalWarning
                --experimental-strip-types
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/chat.smoke.ts"
                "$<TARGET_FILE:LunaAuthoringHost>"
                "${CMAKE_CURRENT_SOURCE_DIR}/client/luna.ts"
        )
        luna_set_test_properties(CliTsChatSmokeTest 180 "ai;cli;ts;authoring")

        add_test(
            NAME CliTsChatDotEnvSmokeTest
            COMMAND
                "${LUNA_NODE_EXECUTABLE}"
                --disable-warning=ExperimentalWarning
                --experimental-strip-types
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/chat.env.smoke.ts"
                "$<TARGET_FILE:LunaAuthoringHost>"
                "${CMAKE_CURRENT_SOURCE_DIR}/client/luna.ts"
        )
        luna_set_test_properties(CliTsChatDotEnvSmokeTest 180 "ai;cli;ts;authoring")

        add_test(
            NAME CliTsAiEvalSmokeTest
            COMMAND
                "${LUNA_NODE_EXECUTABLE}"
                --disable-warning=ExperimentalWarning
                --experimental-strip-types
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/aiEval.smoke.ts"
                "$<TARGET_FILE:LunaAuthoringHost>"
                "${CMAKE_CURRENT_SOURCE_DIR}/client/luna.ts"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/ai-evals/basic-scene.json"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/ai-evals/multi-turn-scene.json"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/ai-evals/repair-invalid-plan.json"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/ai-evals/save-open-persistence.json"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/ai-evals/chinese-intent-basic.json"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/ai-evals/undo-redo-session.json"
        )
        luna_set_test_properties(CliTsAiEvalSmokeTest 600 "ai;long;cli;ts;authoring")

        add_test(
            NAME AuthoringHostStdioSmokeTest
            COMMAND
                "${LUNA_NODE_EXECUTABLE}"
                --disable-warning=ExperimentalWarning
                --experimental-strip-types
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/authoringHost.smoke.ts"
                "$<TARGET_FILE:LunaAuthoringHost>"
        )
        luna_set_test_properties(AuthoringHostStdioSmokeTest 45 "quick;smoke;cli;ts;authoring")
    endif()
endif()
