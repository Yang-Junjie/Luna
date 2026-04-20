#include "Core/Log.h"
#include "Asset/AssetManager.h"
#include "Asset/AssetDatabase.h"
#include "Asset/Editor/ImporterManager.h"
#include "Asset/Editor/MaterialFactory.h"
#include "Asset/Editor/MeshLoader.h"
#include "Asset/Editor/TextureImporter.h"
#include "LunaRuntime.h"
#include "Project/ProjectManager.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Scene/Components.h"

#include <cctype>

#include <algorithm>
#include <array>
#include <filesystem>
#include <glm/common.hpp>
#include <limits>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr luna::RHI::PresentMode kRequestedPresentMode = luna::RHI::PresentMode::Immediate;

std::filesystem::path projectRoot()
{
    return std::filesystem::path(LUNA_PROJECT_ROOT);
}

std::filesystem::path sampleProjectRoot()
{
    return projectRoot() / "SampleProject";
}

std::filesystem::path sampleProjectFile()
{
    return sampleProjectRoot() / "Sample Project.lunaproj";
}

luna::AssetHandle registerMemoryAsset(const std::shared_ptr<luna::Asset>& asset)
{
    if (!asset) {
        return luna::AssetHandle(0);
    }

    luna::AssetManager::get().registerMemoryAsset(asset->handle, asset);
    return asset->handle;
}

std::shared_ptr<luna::Mesh> createNormalizedMesh(const luna::Mesh& mesh);

bool loadSampleProjectAssets()
{
    const auto project_file = sampleProjectFile();
    if (!std::filesystem::exists(project_file)) {
        return false;
    }

    if (!luna::ProjectManager::instance().loadProject(project_file)) {
        LUNA_RUNTIME_WARN("Failed to load sample project '{}'", project_file.string());
        return false;
    }

    luna::AssetManager::get().clear();
    luna::AssetDatabase::clear();
    luna::ImporterManager::init();
    luna::ImporterManager::import();
    luna::AssetManager::get().init();
    return true;
}

luna::AssetHandle ensureImportedTextureHandle(const std::filesystem::path& asset_path)
{
    if (!std::filesystem::exists(asset_path)) {
        return luna::AssetHandle(0);
    }

    if (const luna::AssetHandle existing_handle = luna::AssetDatabase::findHandleByFilePath(asset_path);
        existing_handle.isValid()) {
        return existing_handle;
    }

    luna::TextureImporter importer;
    const std::filesystem::path meta_path = luna::importer_detail::getMetadataPath(asset_path);
    luna::AssetMetadata metadata =
        std::filesystem::exists(meta_path) ? importer.deserializeMetadata(meta_path) : importer.import(asset_path);
    if (!std::filesystem::exists(meta_path)) {
        importer.serializeMetadata(metadata);
    }

    luna::AssetDatabase::set(metadata.Handle, metadata);
    return metadata.Handle;
}

luna::AssetHandle ensureDamagedHelmetMaterialAsset()
{
    const auto helmet_dir = sampleProjectRoot() / "Assets" / "Model" / "DamagedHelmet";
    const auto material_path = helmet_dir / "DamagedHelmet.lunamat";

    if (const luna::AssetHandle existing_handle = luna::AssetDatabase::findHandleByFilePath(material_path);
        existing_handle.isValid()) {
        return existing_handle;
    }

    luna::MaterialAssetDescriptor descriptor = luna::MaterialFactory::makeDefaultDescriptor("DamagedHelmet");
    descriptor.Textures.BaseColor = ensureImportedTextureHandle(helmet_dir / "Default_albedo.jpg");
    descriptor.Textures.Normal = ensureImportedTextureHandle(helmet_dir / "Default_normal.jpg");
    descriptor.Textures.MetallicRoughness = ensureImportedTextureHandle(helmet_dir / "Default_metalRoughness.jpg");
    descriptor.Textures.Emissive = ensureImportedTextureHandle(helmet_dir / "Default_emissive.jpg");
    descriptor.Textures.Occlusion = ensureImportedTextureHandle(helmet_dir / "Default_AO.jpg");
    descriptor.Surface.EmissiveFactor = glm::vec3(1.0f);
    descriptor.Surface.MetallicFactor = 1.0f;
    descriptor.Surface.RoughnessFactor = 1.0f;

    if (!descriptor.Textures.BaseColor.isValid() || !descriptor.Textures.Normal.isValid() ||
        !descriptor.Textures.MetallicRoughness.isValid() || !descriptor.Textures.Emissive.isValid() ||
        !descriptor.Textures.Occlusion.isValid()) {
        LUNA_RUNTIME_WARN("DamagedHelmet textures were not imported correctly; cannot create material asset");
        return luna::AssetHandle(0);
    }

    if (!luna::MaterialFactory::createMaterialFile(material_path, descriptor, true)) {
        LUNA_RUNTIME_WARN("Failed to create DamagedHelmet material asset '{}'", material_path.string());
        return luna::AssetHandle(0);
    }

    return luna::AssetDatabase::findHandleByFilePath(material_path);
}

bool loadSampleProjectDamagedHelmet(std::shared_ptr<luna::Mesh>& mesh,
                                    luna::AssetHandle& material_handle,
                                    std::string& asset_label)
{
    if (!loadSampleProjectAssets()) {
        return false;
    }

    const auto mesh_path = sampleProjectRoot() / "Assets" / "Model" / "DamagedHelmet" / "DamagedHelmet.gltf";
    if (!std::filesystem::exists(mesh_path)) {
        return false;
    }

    const auto loaded_mesh = luna::MeshLoader::loadFromFile(mesh_path);
    if (!loaded_mesh || !loaded_mesh->isValid()) {
        return false;
    }

    mesh = createNormalizedMesh(*loaded_mesh);
    if (!mesh || !mesh->isValid()) {
        return false;
    }

    material_handle = ensureDamagedHelmetMaterialAsset();
    if (!material_handle.isValid()) {
        return false;
    }

    asset_label = "SampleProject DamagedHelmet";
    return true;
}

const char* presentModeToString(luna::RHI::PresentMode mode)
{
    switch (mode) {
        case luna::RHI::PresentMode::Immediate:
            return "Immediate";
        case luna::RHI::PresentMode::Mailbox:
            return "Mailbox";
        case luna::RHI::PresentMode::Fifo:
            return "Fifo";
        case luna::RHI::PresentMode::FifoRelaxed:
            return "FifoRelaxed";
        default:
            return "Unknown";
    }
}

const char* backendTypeToString(luna::RHI::BackendType type)
{
    switch (type) {
        case luna::RHI::BackendType::Auto:
            return "Auto";
        case luna::RHI::BackendType::Vulkan:
            return "Vulkan";
        case luna::RHI::BackendType::DirectX12:
            return "DirectX12";
        case luna::RHI::BackendType::DirectX11:
            return "DirectX11";
        case luna::RHI::BackendType::Metal:
            return "Metal";
        case luna::RHI::BackendType::OpenGL:
            return "OpenGL";
        case luna::RHI::BackendType::OpenGLES:
            return "OpenGLES";
        case luna::RHI::BackendType::WebGPU:
            return "WebGPU";
        default:
            return "Unknown";
    }
}

std::optional<luna::RHI::BackendType> parseBackendValue(std::string_view value)
{
    std::string normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (normalized == "vulkan" || normalized == "vk") {
        return luna::RHI::BackendType::Vulkan;
    }
    if (normalized == "d3d12" || normalized == "dx12" || normalized == "directx12") {
        return luna::RHI::BackendType::DirectX12;
    }
    if (normalized == "d3d11" || normalized == "dx11" || normalized == "directx11") {
        return luna::RHI::BackendType::DirectX11;
    }
    return std::nullopt;
}

luna::RHI::BackendType parseBackendFromArgs(int argc, char** argv)
{
    luna::RHI::BackendType selected_backend = luna::RHI::BackendType::Vulkan;

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i] != nullptr ? std::string_view(argv[i]) : std::string_view{};
        std::string_view backend_value;

        if (argument == "--backend") {
            if (i + 1 >= argc || argv[i + 1] == nullptr) {
                LUNA_RUNTIME_WARN("Missing value after '--backend'; defaulting to '{}'",
                                  backendTypeToString(selected_backend));
                continue;
            }

            backend_value = std::string_view(argv[++i]);
        } else if (argument.starts_with("--backend=")) {
            backend_value = argument.substr(std::string_view("--backend=").size());
        } else {
            continue;
        }

        if (const auto parsed = parseBackendValue(backend_value)) {
            selected_backend = *parsed;
            continue;
        }

        LUNA_RUNTIME_WARN("Unsupported backend '{}' requested via command line; defaulting to '{}'",
                          std::string(backend_value),
                          backendTypeToString(selected_backend));
    }

    return selected_backend;
}

std::shared_ptr<luna::Mesh> createNormalizedMesh(const luna::Mesh& mesh)
{
    if (!mesh.isValid()) {
        return {};
    }

    std::vector<luna::SubMesh> sub_meshes = mesh.getSubMeshes();

    glm::vec3 bounds_min(std::numeric_limits<float>::max());
    glm::vec3 bounds_max(std::numeric_limits<float>::lowest());
    bool has_vertex = false;

    for (const auto& sub_mesh : sub_meshes) {
        for (const auto& vertex : sub_mesh.Vertices) {
            bounds_min = glm::min(bounds_min, vertex.Position);
            bounds_max = glm::max(bounds_max, vertex.Position);
            has_vertex = true;
        }
    }

    if (!has_vertex) {
        return {};
    }

    const glm::vec3 center = (bounds_min + bounds_max) * 0.5f;
    const glm::vec3 extent = bounds_max - bounds_min;
    const float max_extent = (std::max) ((std::max) (extent.x, extent.y), (std::max) (extent.z, 0.0001f));
    const float scale = 2.0f / max_extent;

    for (auto& sub_mesh : sub_meshes) {
        for (auto& vertex : sub_mesh.Vertices) {
            vertex.Position = (vertex.Position - center) * scale;
        }
    }

    return luna::Mesh::create(mesh.getName().empty() ? "RuntimeAssetMesh" : mesh.getName(), std::move(sub_meshes));
}

std::shared_ptr<luna::Mesh> createProceduralCubeMesh()
{
    const std::vector<luna::StaticMeshVertex> vertices = {
        {{-1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},

        {{1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
        {{-1.0f, 1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        {{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},

        {{-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
        {{-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
        {{-1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
        {{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},

        {{1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        {{1.0f, -1.0f, -1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        {{1.0f, 1.0f, -1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},

        {{-1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        {{1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        {{1.0f, 1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},

        {{-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
        {{1.0f, -1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
        {{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
        {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
    };

    const std::vector<uint32_t> indices = {
        0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,  8,  9,  10, 8,  10, 11,
        12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
    };

    luna::SubMesh sub_mesh;
    sub_mesh.Name = "ProceduralCube";
    sub_mesh.Vertices = vertices;
    sub_mesh.Indices = indices;
    return luna::Mesh::create("ProceduralCube", {std::move(sub_mesh)});
}

std::shared_ptr<luna::Material> createFallbackMaterial()
{
    luna::Material::SurfaceProperties surface;
    surface.BaseColorFactor = glm::vec4(0.96f, 0.52f, 0.18f, 1.0f);
    return luna::Material::create("FallbackMaterial", {}, surface);
}

} // namespace

namespace luna {

LunaRuntimeApplication::LunaRuntimeApplication(luna::RHI::BackendType backend)
    : Application(ApplicationSpecification{
          .m_name = "Luna Runtime",
          .m_window_width = 1'600,
          .m_window_height = 900,
          .m_maximized = false,
          .m_enable_imgui = false,
          .m_enable_multi_viewport = false,
      }),
      m_backend(backend)
{}

Renderer::InitializationOptions LunaRuntimeApplication::getRendererInitializationOptions()
{
    LUNA_RUNTIME_INFO("LunaRuntime requested backend '{}' and present mode '{}' via code",
                      backendTypeToString(m_backend),
                      presentModeToString(kRequestedPresentMode));
    return Renderer::InitializationOptions{m_backend, kRequestedPresentMode};
}

void LunaRuntimeApplication::resetCamera()
{
    auto& camera = getRenderer().getMainCamera();
    camera.m_position = glm::vec3(0.0f, 0.45f, 4.75f);
    camera.m_pitch = -0.12f;
    camera.m_yaw = 0.0f;
}

void LunaRuntimeApplication::onInit()
{
    auto& renderer = getRenderer();
    renderer.setSceneOutputMode(Renderer::SceneOutputMode::Swapchain);
    renderer.getClearColor() = glm::vec4(0.08f, 0.09f, 0.11f, 1.0f);
    resetCamera();
    buildScene();
}

void LunaRuntimeApplication::onUpdate(Timestep timestep)
{
    updateDemoTransform(timestep.getSeconds());
    m_scene.onUpdateRuntime();
}

void LunaRuntimeApplication::buildScene()
{
    if (!tryLoadDefaultAsset()) {
        createFallbackAsset();
    }

    m_demo_entity = m_scene.createEntity("Runtime Mesh");
    auto& mesh_component = m_demo_entity.addComponent<MeshComponent>();
    if (!m_demo_mesh_handle.isValid()) {
        m_demo_mesh_handle = registerMemoryAsset(m_demo_mesh);
    }
    mesh_component.meshHandle = m_demo_mesh_handle;
    mesh_component.memoryOnly = m_demo_mesh_memory_only;
    mesh_component.memoryMeshType = m_demo_memory_mesh_type;

    if (m_demo_mesh) {
        mesh_component.resizeSubmeshMaterials(m_demo_mesh->getSubMeshes().size());
    }

    if (!m_demo_material_handle.isValid()) {
        m_demo_material_handle = registerMemoryAsset(m_demo_material);
    }

    const AssetHandle material_handle = m_demo_material_handle;
    for (uint32_t submesh_index = 0; submesh_index < mesh_component.getSubmeshMaterialCount(); ++submesh_index) {
        mesh_component.setSubmeshMaterial(submesh_index, material_handle);
    }

    auto& transform = m_demo_entity.getComponent<TransformComponent>();
    transform.translation = glm::vec3(0.0f);
    transform.rotation = glm::vec3(-0.35f, 0.0f, 0.0f);
    transform.scale = glm::vec3(1.0f);
}

bool LunaRuntimeApplication::tryLoadDefaultAsset()
{
    if (loadSampleProjectDamagedHelmet(m_demo_mesh, m_demo_material_handle, m_asset_label)) {
        m_demo_material.reset();
        m_demo_mesh_handle = AssetHandle(0);
        m_demo_mesh_memory_only = true;
        m_demo_memory_mesh_type = MemoryMeshType::None;
        LUNA_RUNTIME_INFO("Loaded SampleProject DamagedHelmet with generated .lunamat material");
        return true;
    }

    const std::array<std::filesystem::path, 3> candidates = {
        projectRoot() / "Assets" / "DamagedHelmet" / "DamagedHelmet.gltf",
        projectRoot() / "Assets" / "basicmesh.glb",
        projectRoot() / "Assets" / "material_sphere" / "material_sphere.obj",
    };

    for (const auto& candidate : candidates) {
        if (!std::filesystem::exists(candidate)) {
            continue;
        }

        try {
            const auto loaded_mesh = MeshLoader::loadFromFile(candidate);
            if (!loaded_mesh || !loaded_mesh->isValid()) {
                continue;
            }

            m_demo_mesh = createNormalizedMesh(*loaded_mesh);
            if (!m_demo_mesh || !m_demo_mesh->isValid()) {
                continue;
            }

            m_demo_material = createFallbackMaterial();
            m_demo_mesh_handle = AssetHandle(0);
            m_demo_material_handle = AssetHandle(0);
            m_demo_mesh_memory_only = true;
            m_demo_memory_mesh_type = MemoryMeshType::None;

            m_asset_label = candidate.filename().string();
            LUNA_RUNTIME_INFO("Loaded runtime asset '{}'", candidate.string());
            return true;
        } catch (const std::exception& error) {
            LUNA_RUNTIME_WARN("Failed to load runtime asset '{}': {}", candidate.string(), error.what());
        }
    }

    return false;
}

void LunaRuntimeApplication::createFallbackAsset()
{
    m_demo_mesh = createProceduralCubeMesh();
    m_demo_material = createFallbackMaterial();
    m_demo_mesh_handle = AssetHandle(0);
    m_demo_material_handle = AssetHandle(0);
    m_demo_mesh_memory_only = true;
    m_demo_memory_mesh_type = MemoryMeshType::ProceduralCube;
    m_asset_label = "Procedural cube";
    LUNA_RUNTIME_INFO("Using fallback procedural cube for runtime demo");
}

void LunaRuntimeApplication::updateDemoTransform(float delta_time)
{
    if (!m_demo_entity || !m_demo_entity.hasComponent<TransformComponent>()) {
        return;
    }

    auto& transform = m_demo_entity.getComponent<TransformComponent>();
    transform.rotation.x = -0.35f;
    if (m_auto_rotate) {
        transform.rotation.y += delta_time * m_spin_speed;
        const float turn = std::numbers::pi_v<float> * 2.0f;
        if (transform.rotation.y > turn) {
            transform.rotation.y -= turn;
        }
    }
}

Application* createApplication(int argc, char** argv)
{
    const auto backend = parseBackendFromArgs(argc, argv);
    LUNA_RUNTIME_INFO("Starting LunaRuntime with backend '{}'", backendTypeToString(backend));
    return new LunaRuntimeApplication(backend);
}

} // namespace luna
