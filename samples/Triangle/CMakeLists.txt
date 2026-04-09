add_library(
    LunaTriangleSample
    STATIC
    TriangleApp.h
    TriangleApp.cpp
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
        LunaSampleCommon
)

add_executable(TriangleSample ${PROJECT_SOURCE_DIR}/Luna/Core/Main.cpp)
target_link_libraries(TriangleSample PRIVATE LunaTriangleSample)
