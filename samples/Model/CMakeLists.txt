add_library(
    LunaModelSample
    STATIC
    ModelApp.h
    ModelApp.cpp
    ModelLayer.h
    ModelRenderPass.h
    ModelRenderPass.cpp
    Shaders/Model.vert
    Shaders/Model.frag
)

target_include_directories(
    LunaModelSample
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(
    LunaModelSample
    PUBLIC
        LunaCore
)

target_compile_definitions(
    LunaModelSample
    PUBLIC
        LUNA_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)

add_executable(ModelSample ${PROJECT_SOURCE_DIR}/Luna/Core/Main.cpp)
target_link_libraries(ModelSample PRIVATE LunaModelSample)
