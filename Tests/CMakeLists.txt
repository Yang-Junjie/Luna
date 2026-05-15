add_executable(
    ScriptPluginContractTests
    ScriptPluginContractTests.cpp
)

add_executable(
    NativeEditorPluginContractTests
    NativeEditorPluginContractTests.cpp
    ${PROJECT_SOURCE_DIR}/Editor/Source/EditorEnginePaths.cpp
    ${PROJECT_SOURCE_DIR}/Editor/Source/Shell/EditorPluginDiscovery.cpp
    ${PROJECT_SOURCE_DIR}/Editor/Source/Shell/EditorBuiltinPluginRegistry.cpp
    ${PROJECT_SOURCE_DIR}/Editor/Source/Shell/EditorPluginManifest.cpp
    ${PROJECT_SOURCE_DIR}/Editor/Source/Shell/EditorPluginDependencyResolver.cpp
)

add_executable(
    SceneSerializerTests
    SceneSerializerTests.cpp
)

add_executable(
    AuthoringSessionTests
    AuthoringSessionTests.cpp
)

add_executable(
    AuthoringExecutorTests
    AuthoringExecutorTests.cpp
)

add_executable(
    AuthoringHostTests
    AuthoringHostTests.cpp
)

add_executable(
    AuthoringProtocolTests
    AuthoringProtocolTests.cpp
)

add_executable(
    ViewportInstanceTests
    ViewportInstanceTests.cpp
    ${PROJECT_SOURCE_DIR}/Editor/Source/Viewport/TextureViewportInstance.cpp
    ${PROJECT_SOURCE_DIR}/Editor/Source/Viewport/ViewportSurface.cpp
)

function(luna_set_test_properties test_name timeout labels)
    set_tests_properties(
        ${test_name}
        PROPERTIES
            TIMEOUT ${timeout}
            LABELS "${labels}"
    )
endfunction()

set(LUNA_TEST_SCRIPT_PLUGIN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/ScriptPluginTestPlugins")
set(LUNA_TEST_SCRIPT_PLUGIN_TARGETS)
set(LUNA_TEST_EDITOR_PLUGIN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/EditorPluginTestPlugins")
set(LUNA_TEST_EDITOR_PLUGIN_TARGETS)

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

function(add_luna_test_editor_plugin target_name)
    add_library(
        ${target_name}
        SHARED
        FakeNativeEditorPlugin.cpp
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
            RUNTIME_OUTPUT_DIRECTORY "${LUNA_TEST_EDITOR_PLUGIN_OUTPUT_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${LUNA_TEST_EDITOR_PLUGIN_OUTPUT_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${LUNA_TEST_EDITOR_PLUGIN_OUTPUT_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${LUNA_TEST_EDITOR_PLUGIN_OUTPUT_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${LUNA_TEST_EDITOR_PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY "${LUNA_TEST_EDITOR_PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_DEBUG "${LUNA_TEST_EDITOR_PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_RELEASE "${LUNA_TEST_EDITOR_PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${LUNA_TEST_EDITOR_PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${LUNA_TEST_EDITOR_PLUGIN_OUTPUT_DIR}"
    )

    list(APPEND LUNA_TEST_EDITOR_PLUGIN_TARGETS ${target_name})
    set(LUNA_TEST_EDITOR_PLUGIN_TARGETS "${LUNA_TEST_EDITOR_PLUGIN_TARGETS}" PARENT_SCOPE)
endfunction()

add_luna_test_editor_plugin(LunaTestEditorPluginGood)
add_luna_test_editor_plugin(LunaTestEditorPluginMissingSymbol LUNA_TEST_EDITOR_PLUGIN_MISSING_SYMBOL)
add_luna_test_editor_plugin(LunaTestEditorPluginApiMismatch LUNA_TEST_EDITOR_PLUGIN_API_MISMATCH)
add_luna_test_editor_plugin(LunaTestEditorPluginIdMismatch LUNA_TEST_EDITOR_PLUGIN_ID_MISMATCH)
add_luna_test_editor_plugin(LunaTestEditorPluginMissingCallbacks LUNA_TEST_EDITOR_PLUGIN_MISSING_CALLBACKS)
add_luna_test_editor_plugin(LunaTestEditorPluginLoadFailure LUNA_TEST_EDITOR_PLUGIN_LOAD_FAILURE)
add_luna_test_editor_plugin(LunaTestEditorPluginLeakViewport LUNA_TEST_EDITOR_PLUGIN_LEAK_VIEWPORT)

target_link_libraries(
    ScriptPluginContractTests
    PRIVATE
        LunaCore
)

target_link_libraries(
    NativeEditorPluginContractTests
    PRIVATE
        LunaCore
)

target_link_libraries(
    SceneSerializerTests
    PRIVATE
        LunaCore
)

target_link_libraries(
    AuthoringSessionTests
    PRIVATE
        LunaCore
)

target_link_libraries(
    AuthoringExecutorTests
    PRIVATE
        LunaCore
)

target_link_libraries(
    AuthoringHostTests
    PRIVATE
        LunaCore
)

target_link_libraries(
    AuthoringProtocolTests
    PRIVATE
        LunaCore
)

target_link_libraries(
    ViewportInstanceTests
    PRIVATE
        LunaCore
)

target_compile_features(
    ScriptPluginContractTests
    PRIVATE
        cxx_std_20
)

target_compile_features(
    NativeEditorPluginContractTests
    PRIVATE
        cxx_std_20
)

target_compile_features(
    SceneSerializerTests
    PRIVATE
        cxx_std_20
)

target_compile_features(
    AuthoringSessionTests
    PRIVATE
        cxx_std_20
)

target_compile_features(
    AuthoringExecutorTests
    PRIVATE
        cxx_std_20
)

target_compile_features(
    AuthoringHostTests
    PRIVATE
        cxx_std_20
)

target_compile_features(
    AuthoringProtocolTests
    PRIVATE
        cxx_std_20
)

target_compile_features(
    ViewportInstanceTests
    PRIVATE
        cxx_std_20
)

target_compile_definitions(
    ScriptPluginContractTests
    PRIVATE
        LUNA_TEST_SCRIPT_PLUGIN_DIR="${LUNA_TEST_SCRIPT_PLUGIN_OUTPUT_DIR}"
)

target_compile_definitions(
    NativeEditorPluginContractTests
    PRIVATE
        LUNA_TEST_EDITOR_PLUGIN_DIR="${LUNA_TEST_EDITOR_PLUGIN_OUTPUT_DIR}"
)

target_include_directories(
    NativeEditorPluginContractTests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/Editor/Source
)

target_include_directories(
    ViewportInstanceTests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/Editor/Source
)

target_compile_definitions(
    AuthoringProtocolTests
    PRIVATE
        LUNA_AUTHORING_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Fixtures/Authoring"
        LUNA_AUTHORING_SCHEMA_DIR="${PROJECT_SOURCE_DIR}/Docs/Schemas"
)

if(TARGET LunaScriptLuaPlugin)
    add_dependencies(ScriptPluginContractTests LunaScriptLuaPlugin)
endif()

add_dependencies(ScriptPluginContractTests ${LUNA_TEST_SCRIPT_PLUGIN_TARGETS})
add_dependencies(NativeEditorPluginContractTests ${LUNA_TEST_EDITOR_PLUGIN_TARGETS})

add_test(
    NAME ScriptPluginContractTests
    COMMAND ScriptPluginContractTests
)
luna_set_test_properties(ScriptPluginContractTests 20 "quick;unit;core;plugin")

add_test(
    NAME NativeEditorPluginContractTests
    COMMAND NativeEditorPluginContractTests
)
luna_set_test_properties(NativeEditorPluginContractTests 20 "quick;unit;editor;plugin")

add_test(
    NAME SceneSerializerTests
    COMMAND SceneSerializerTests
)
luna_set_test_properties(SceneSerializerTests 20 "quick;unit;core;scene")

add_test(
    NAME AuthoringSessionTests
    COMMAND AuthoringSessionTests
)
luna_set_test_properties(AuthoringSessionTests 20 "quick;unit;authoring")

add_test(
    NAME AuthoringExecutorTests
    COMMAND AuthoringExecutorTests
)
luna_set_test_properties(AuthoringExecutorTests 20 "quick;unit;authoring")

add_test(
    NAME AuthoringHostTests
    COMMAND AuthoringHostTests
)
luna_set_test_properties(AuthoringHostTests 20 "quick;unit;authoring")

add_test(
    NAME AuthoringProtocolTests
    COMMAND AuthoringProtocolTests
)
luna_set_test_properties(AuthoringProtocolTests 20 "quick;unit;authoring;protocol")

add_test(
    NAME ViewportInstanceTests
    COMMAND ViewportInstanceTests
)
luna_set_test_properties(ViewportInstanceTests 20 "quick;unit;editor;viewport")

add_test(
    NAME EditorSdkNativeTemplateContract
    COMMAND
        ${CMAKE_COMMAND}
        "-DLUNA_BINARY_DIR=${PROJECT_BINARY_DIR}"
        "-DLUNA_SDK_CONTRACT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/EditorSdkNativeTemplateContract"
        -DLUNA_TEST_CONFIGURATION=$<CONFIG>
        "-DLUNA_CMAKE_GENERATOR=${CMAKE_GENERATOR}"
        "-DLUNA_CMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DLUNA_CMAKE_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/EditorSdkNativeTemplateContract.cmake"
)
luna_set_test_properties(EditorSdkNativeTemplateContract 120 "sdk;contract;editor;plugin")

add_test(
    NAME EditorPluginBoundaryContract
    COMMAND
        ${CMAKE_COMMAND}
        "-DLUNA_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/EditorPluginBoundaryContract.cmake"
)
luna_set_test_properties(EditorPluginBoundaryContract 20 "quick;sdk;contract;editor;plugin")
