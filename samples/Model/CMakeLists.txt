add_library(
    LunaModelSample
    STATIC
    ModelApp.h
    ModelApp.cpp
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
        LunaSampleCommon
)

add_executable(ModelSample ${PROJECT_SOURCE_DIR}/Luna/Core/Main.cpp)
target_link_libraries(ModelSample PRIVATE LunaModelSample)
