set(
    LUNA_GENERATED_DIR
    "${PROJECT_SOURCE_DIR}/Plugins/Generated"
    CACHE PATH
    "Directory containing generated Luna plugin build files"
)

set(LUNA_PLUGIN_LIST_FILE "${LUNA_GENERATED_DIR}/PluginList.cmake")
set(LUNA_RESOLVED_PLUGINS_HEADER "${LUNA_GENERATED_DIR}/ResolvedPlugins.h")
set(LUNA_RESOLVED_PLUGINS_SOURCE "${LUNA_GENERATED_DIR}/ResolvedPlugins.cpp")

set(LUNA_PLUGIN_TARGETS "")
include("${LUNA_PLUGIN_LIST_FILE}" OPTIONAL)

add_library(
    LunaAppHost
    STATIC
    LunaApp.h
    LunaApp.cpp
    ${LUNA_RESOLVED_PLUGINS_HEADER}
    ${LUNA_RESOLVED_PLUGINS_SOURCE}
)

target_include_directories(
    LunaAppHost
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(
    LunaAppHost
    PUBLIC
        LunaEditorFramework
)

if(LUNA_PLUGIN_TARGETS)
    target_link_libraries(LunaAppHost PUBLIC ${LUNA_PLUGIN_TARGETS})
endif()

add_executable(LunaApp ${PROJECT_SOURCE_DIR}/Luna/Core/Main.cpp)
target_link_libraries(LunaApp PRIVATE LunaAppHost)
