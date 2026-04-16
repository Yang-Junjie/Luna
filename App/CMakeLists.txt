add_executable(
    LunaApp
    ${PROJECT_SOURCE_DIR}/Luna/Core/main.cpp
    LunaApp.h
    LunaApp.cpp
)

target_include_directories(
    LunaApp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    LunaApp
    PRIVATE
        LunaCore
)

set_target_properties(
    LunaApp
    PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
)
