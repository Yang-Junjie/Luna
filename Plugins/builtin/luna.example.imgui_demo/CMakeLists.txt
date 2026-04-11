add_library(
    LunaExampleImGuiDemoPlugin
    STATIC
    luna.plugin.toml
    src/ImGuiDemoPlugin.cpp
    src/ImGuiDemoPanel.h
    src/ImGuiDemoPanel.cpp
)

target_include_directories(
    LunaExampleImGuiDemoPlugin
    PUBLIC
        ${PROJECT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
    LunaExampleImGuiDemoPlugin
    PUBLIC
        LunaEditorFramework
)
