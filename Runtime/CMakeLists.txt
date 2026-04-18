add_executable(
    LunaRuntime
    ${PROJECT_SOURCE_DIR}/Luna/Core/main.cpp
    LunaRuntime.h
    LunaRuntime.cpp
)

target_include_directories(
    LunaRuntime
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    LunaRuntime
    PRIVATE
        LunaCore
)

set_target_properties(
    LunaRuntime
    PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)
