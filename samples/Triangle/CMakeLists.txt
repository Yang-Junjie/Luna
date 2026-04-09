add_library(
    LunaTriangleSample
    STATIC
    TriangleApp.h
    TriangleApp.cpp
    TriangleLayer.h
    TriangleRenderPass.h
    TriangleRenderPass.cpp
    Shaders/Triangle.vert
    Shaders/Triangle.frag
)

target_include_directories(
    LunaTriangleSample
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(
    LunaTriangleSample
    PUBLIC
        LunaCore
)

target_compile_definitions(
    LunaTriangleSample
    PUBLIC
        LUNA_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)

add_executable(TriangleSample ${PROJECT_SOURCE_DIR}/Luna/Core/Main.cpp)
target_link_libraries(TriangleSample PRIVATE LunaTriangleSample)
