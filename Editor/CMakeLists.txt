add_executable(
    LunaEditor
    ${PROJECT_SOURCE_DIR}/Luna/Core/main.cpp
    LunaEditor.h
    LunaEditor.cpp
)

target_include_directories(
    LunaEditor
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    LunaEditor
    PRIVATE
        LunaCore
)

set_target_properties(
    LunaEditor
    PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)
