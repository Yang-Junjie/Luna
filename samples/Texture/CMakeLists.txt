add_library(
    LunaTextureSample
    STATIC
    TextureApp.h
    TextureApp.cpp
    TextureLayer.h
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
        LunaCore
)

target_compile_definitions(
    LunaTextureSample
    PUBLIC
        LUNA_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)

add_executable(TextureSample ${PROJECT_SOURCE_DIR}/Luna/Core/Main.cpp)
target_link_libraries(TextureSample PRIVATE LunaTextureSample)
