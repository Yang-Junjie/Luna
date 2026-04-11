add_library(
    LunaEditor
    STATIC
    EditorApp.h
    EditorApp.cpp
)

target_include_directories(
    LunaEditor
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(
    LunaEditor
    PUBLIC
        LunaCore
)

target_compile_definitions(
    LunaEditor
    PUBLIC
        LUNA_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)

add_executable(LunaEditorApp ${PROJECT_SOURCE_DIR}/Luna/Core/Main.cpp)
target_link_libraries(LunaEditorApp PRIVATE LunaEditor)
