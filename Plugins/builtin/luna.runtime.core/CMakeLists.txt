add_library(
    LunaRuntimeCorePlugin
    STATIC
    luna.plugin.toml
    src/RuntimeCorePlugin.cpp
    src/RuntimeClearColorLayer.h
)

target_include_directories(
    LunaRuntimeCorePlugin
    PUBLIC
        ${PROJECT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
    LunaRuntimeCorePlugin
    PUBLIC
        LunaCore
)
