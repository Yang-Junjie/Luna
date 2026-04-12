add_library(
    LunaImGuiPlugin
    STATIC
    luna.plugin.toml
    src/ImGuiPlugin.cpp
)

target_include_directories(
    LunaImGuiPlugin
    PUBLIC
        ${PROJECT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
    LunaImGuiPlugin
    PUBLIC
        LunaCore
)
