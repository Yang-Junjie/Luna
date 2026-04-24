set(THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/third_party)

add_subdirectory(RHI)

set(IMGUI_SOURCE ${THIRD_PARTY_DIR}/imgui/imgui.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_demo.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_draw.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_tables.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_widgets.cpp
    ${THIRD_PARTY_DIR}/imgui/backends/imgui_impl_glfw.cpp)

set(RENDERER_SOURCE
    Renderer/Camera.h
    Renderer/Camera.cpp
    Renderer/Material.h
    Renderer/Material.cpp
    Renderer/Mesh.h
    Renderer/Mesh.cpp
    Renderer/PrimitiveMeshFactory.h
    Renderer/PrimitiveMeshFactory.cpp
    Renderer/RenderGraph.h
    Renderer/RenderGraph.cpp
    Renderer/RenderGraphBuilder.h
    Renderer/RenderGraphBuilder.cpp
    Renderer/RendererUtilities.h
    Renderer/RendererUtilities.cpp
    Renderer/SceneRenderer/SceneRenderer.h
    Renderer/SceneRenderer/SceneRendererSupport.h
    Renderer/SceneRenderer/SceneRendererSupport.cpp
    Renderer/SceneRenderer/SceneRendererDrawQueue.h
    Renderer/SceneRenderer/SceneRendererDrawQueue.cpp
    Renderer/SceneRenderer/SceneRendererAssetCache.h
    Renderer/SceneRenderer/SceneRendererAssetCache.cpp
    Renderer/SceneRenderer/SceneRendererEnvironment.h
    Renderer/SceneRenderer/SceneRendererEnvironment.cpp
    Renderer/SceneRenderer/SceneRendererPipelineLibrary.h
    Renderer/SceneRenderer/SceneRendererPipelineLibrary.cpp
    Renderer/SceneRenderer/SceneRendererResourceManager.h
    Renderer/SceneRenderer/SceneRenderer.cpp
    Renderer/SceneRenderer/SceneRendererResourceManager.cpp
    Renderer/Shaders/SceneGeometry.slang
    Renderer/Renderer.h
    Renderer/Renderer.cpp
    
    Renderer/Texture.h
    )

set(SCENE_SOURCE
    Scene/Components.h
    Scene/Entity.h
    Scene/Entity.cpp
    Scene/EntityManager.h
    Scene/EntityManager.cpp
    Scene/Scene.h
    Scene/Scene.cpp
    Scene/SceneSerializer.h
    Scene/SceneSerializer.cpp)

set(RHI_RUNTIME_SOURCE
    Imgui/ImGuiContext.cpp
    Imgui/ImGuiRenderPass.h
    Imgui/Shaders/ImGui.slang)

set(PROJECT_SOURCE
    Project/BuiltinMaterialOverrides.h
    Project/BuiltinMaterialOverrides.cpp
    Project/ProjectInfo.h
    Project/ProjectManager.h
    Project/ProjectManager.cpp)

set(PLATFORM_SOURCE
    Platform/Common/FileDialogs.h)

set(ASSET_SOURCE 
    Asset/BuiltinAssets.h
    Asset/BuiltinAssets.cpp
    Asset/AssetManager.h
    Asset/AssetManager.cpp
    Asset/Asset.h
    Asset/AssetDatabase.h
    Asset/AssetDatabase.cpp
    Asset/AssetMetadata.h
    Asset/AssetTypes.h
    Asset/Editor/Importer.h
    Asset/Editor/ImporterManager.h
    Asset/Editor/ImporterManager.cpp
    Asset/Editor/ImageLoader.h
    Asset/Editor/ImageLoader.cpp
    Asset/Editor/Loader.h
    Asset/Editor/LoaderManager.h
    Asset/Editor/LoaderManager.cpp
    Asset/Editor/MaterialFactory.h
    Asset/Editor/MaterialFactory.cpp
    Asset/Editor/MaterialImporter.h
    Asset/Editor/MaterialLoader.h
    Asset/Editor/MeshImporter.h
    Asset/Editor/MeshLoader.h
    Asset/Editor/MeshLoader.cpp
    Asset/Editor/TextureImporter.h
    Asset/Editor/TextureLoader.h
    Asset/Editor/TextureLoader.cpp
)

if(WIN32)
    list(APPEND PLATFORM_SOURCE
        Platform/Windows/WinFileDialogs.cpp)
endif()

add_library(
    LunaCore
    STATIC
    Core/Application.h
    Core/Application.cpp
    Core/Input.h
    Imgui/ImGuiLayer.hpp
    Imgui/ImGuiLayer.cpp
    Core/KeyCodes.h
    Core/Layer.h
    Core/LayerStack.h
    Core/LayerStack.cpp
    Core/Log.h
    Core/Log.cpp
    Core/MouseCodes.h
    Core/Timestep.h
    Core/Window.h
    Core/Window.cpp
    Core/UUID.h 
    Core/UUID.cpp

    Events/ApplicationEvent.h
    Events/Event.h
    Events/KeyEvent.h
    Events/MouseEvent.h

    Platform/GLFWKeyCodes.hpp
    Platform/GLFWMouseCodes.hpp
    Platform/GLFWWindow.hpp
    Platform/GLFWWindow.cpp
    Platform/GLFWWindowInput.cpp
    ${PLATFORM_SOURCE}

    JobSystem/ResourceLoadQueue.h
    JobSystem/TaskHandle.h
    JobSystem/TaskHandle.cpp
    JobSystem/TaskHandleState.h
    JobSystem/TaskSystem.h
    JobSystem/TaskSystem.cpp

    ${PROJECT_SOURCE}

    ${ASSET_SOURCE}

    ${SCENE_SOURCE}
    ${RENDERER_SOURCE}
    ${RHI_RUNTIME_SOURCE}
    ${IMGUI_SOURCE}
)

target_include_directories(
    LunaCore
    SYSTEM PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${THIRD_PARTY_DIR}/imgui
        ${THIRD_PARTY_DIR}/spdlog/include
        ${THIRD_PARTY_DIR}/glfw/include
        ${THIRD_PARTY_DIR}/glm
        ${THIRD_PARTY_DIR}/entt/src
        ${EXTERNAL_DIR}/yaml-cpp/include
        ${PROJECT_SOURCE_DIR}
)

target_link_libraries(LunaCore 
        PUBLIC
        LunaRHI
        spdlog::spdlog
        fastgltf::fastgltf
        # add_subdirectory(third_party/VMA)
        Vulkan::Vulkan
        glfw
        enkiTS
        # spirv-cross-core
        stb::stb
        yaml-cpp::yaml-cpp
)

target_compile_definitions(
    LunaCore
    PUBLIC
        LUNA_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
        SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
        GLM_ENABLE_EXPERIMENTAL
)
