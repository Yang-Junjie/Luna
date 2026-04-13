add_library(
    LunaRuntimeCorePlugin
    STATIC
    luna.plugin.toml
    src/RuntimeCorePlugin.cpp
    src/RuntimeStaticMeshLayer.h
    Shaders/RuntimeSceneGeometry.vert
    Shaders/RuntimeSceneGeometry.frag
    Shaders/RuntimeSceneLighting.vert
    Shaders/RuntimeSceneLighting.frag
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
