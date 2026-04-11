set(LUNA_PLUGIN_TARGETS "")
set(LUNA_RUNTIME_GENERATED_DIR ${PROJECT_SOURCE_DIR}/Plugins/Generated/runtime)
include(${LUNA_RUNTIME_GENERATED_DIR}/PluginList.cmake OPTIONAL)

add_library(
    LunaRuntime
    STATIC
    RuntimeApp.h
    RuntimeApp.cpp
    ${LUNA_RUNTIME_GENERATED_DIR}/ResolvedPlugins.h
    ${LUNA_RUNTIME_GENERATED_DIR}/ResolvedPlugins.cpp
)

target_include_directories(
    LunaRuntime
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(
    LunaRuntime
    PUBLIC
        LunaCore
)

if(LUNA_PLUGIN_TARGETS)
    target_link_libraries(LunaRuntime PUBLIC ${LUNA_PLUGIN_TARGETS})
endif()

add_executable(LunaRuntimeApp ${PROJECT_SOURCE_DIR}/Luna/Core/Main.cpp)
target_link_libraries(LunaRuntimeApp PRIVATE LunaRuntime)
