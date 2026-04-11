add_library(
    LunaExampleHelloPlugin
    STATIC
    luna.plugin.toml
    src/HelloPlugin.cpp
    src/HelloPanel.h
    src/HelloPanel.cpp
)

target_include_directories(
    LunaExampleHelloPlugin
    PUBLIC
        ${PROJECT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
    LunaExampleHelloPlugin
    PUBLIC
        LunaEditorFramework
)
