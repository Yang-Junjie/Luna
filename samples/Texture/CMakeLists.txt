add_library(
    LunaTextureSample
    STATIC
    TextureApp.h
    TextureApp.cpp
    TextureRenderPass.h
    TextureRenderPass.cpp
    Shaders/TexturedQuad.vert
    Shaders/TexturedQuad.frag
)

target_include_directories(
    LunaTextureSample
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(
    LunaTextureSample
    PUBLIC
        LunaSampleCommon
)

add_executable(TextureSample ${PROJECT_SOURCE_DIR}/Luna/Core/Main.cpp)
target_link_libraries(TextureSample PRIVATE LunaTextureSample)
