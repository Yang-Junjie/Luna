add_executable(
    ScriptPluginContractTests
    ScriptPluginContractTests.cpp
)

add_executable(
    SceneSerializerTests
    SceneSerializerTests.cpp
)

set(LUNA_TEST_SCRIPT_PLUGIN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/ScriptPluginTestPlugins")
set(LUNA_TEST_SCRIPT_PLUGIN_TARGETS)

function(add_luna_test_script_plugin target_name)
    add_library(
        ${target_name}
        SHARED
        FakeScriptPlugin.cpp
    )

    target_include_directories(
        ${target_name}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/Luna
    )

    target_compile_features(
        ${target_name}
        PRIVATE
            cxx_std_20
    )

    if(ARGN)
        target_compile_definitions(
            ${target_name}
            PRIVATE
                ${ARGN}
        )
    endif()

    set_target_properties(
        ${target_name}
        PROPERTIES
            PREFIX ""
            RUNTIME_OUTPUT_DIRECTORY "${LUNA_TEST_SCRIPT_PLUGIN_OUTPUT_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${LUNA_TEST_SCRIPT_PLUGIN_OUTPUT_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${LUNA_TEST_SCRIPT_PLUGIN_OUTPUT_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${LUNA_TEST_SCRIPT_PLUGIN_OUTPUT_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${LUNA_TEST_SCRIPT_PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY "${LUNA_TEST_SCRIPT_PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_DEBUG "${LUNA_TEST_SCRIPT_PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_RELEASE "${LUNA_TEST_SCRIPT_PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${LUNA_TEST_SCRIPT_PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${LUNA_TEST_SCRIPT_PLUGIN_OUTPUT_DIR}"
    )

    list(APPEND LUNA_TEST_SCRIPT_PLUGIN_TARGETS ${target_name})
    set(LUNA_TEST_SCRIPT_PLUGIN_TARGETS "${LUNA_TEST_SCRIPT_PLUGIN_TARGETS}" PARENT_SCOPE)
endfunction()

add_luna_test_script_plugin(LunaTestScriptPluginGood)
add_luna_test_script_plugin(LunaTestScriptPluginBackendMismatch LUNA_TEST_SCRIPT_PLUGIN_BACKEND_MISMATCH)
add_luna_test_script_plugin(LunaTestScriptPluginLanguageMismatch LUNA_TEST_SCRIPT_PLUGIN_LANGUAGE_MISMATCH)
add_luna_test_script_plugin(LunaTestScriptPluginExtensionMismatch LUNA_TEST_SCRIPT_PLUGIN_EXTENSION_MISMATCH)
add_luna_test_script_plugin(LunaTestScriptPluginEmptyExtensions LUNA_TEST_SCRIPT_PLUGIN_EMPTY_EXTENSIONS)

target_link_libraries(
    ScriptPluginContractTests
    PRIVATE
        LunaCore
)

target_link_libraries(
    SceneSerializerTests
    PRIVATE
        LunaCore
)

target_compile_features(
    ScriptPluginContractTests
    PRIVATE
        cxx_std_20
)

target_compile_features(
    SceneSerializerTests
    PRIVATE
        cxx_std_20
)

target_compile_definitions(
    ScriptPluginContractTests
    PRIVATE
        LUNA_TEST_SCRIPT_PLUGIN_DIR="${LUNA_TEST_SCRIPT_PLUGIN_OUTPUT_DIR}"
)

if(TARGET LunaScriptLuaPlugin)
    add_dependencies(ScriptPluginContractTests LunaScriptLuaPlugin)
endif()

add_dependencies(ScriptPluginContractTests ${LUNA_TEST_SCRIPT_PLUGIN_TARGETS})

add_test(
    NAME ScriptPluginContractTests
    COMMAND ScriptPluginContractTests
)

add_test(
    NAME SceneSerializerTests
    COMMAND SceneSerializerTests
)
