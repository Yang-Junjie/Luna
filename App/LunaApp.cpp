#include "Core/Layer.h"
#include "Core/Log.h"
#include "LunaApp.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/ModelLoader.h"
#include "Scene/Components.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <glm/common.hpp>
#include <imgui.h>
#include <limits>
#include <numbers>
#include <optional>
#include <string_view>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

constexpr luna::RHI::PresentMode kRequestedPresentMode = luna::RHI::PresentMode::Immediate;

bool backendSupportsImGui(luna::RHI::BackendType backend)
{
    return backend == luna::RHI::BackendType::Vulkan || backend == luna::RHI::BackendType::DirectX11 ||
           backend == luna::RHI::BackendType::DirectX12;
}

std::filesystem::path projectRoot()
{
    return std::filesystem::path(LUNA_PROJECT_ROOT);
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

std::shared_ptr<luna::Mesh> createNormalizedMeshFromShape(const luna::rhi::ModelData::Shape& shape)
{
    if (shape.Vertices.empty() || shape.Indices.empty()) {
        return {};
    }

    std::vector<luna::StaticMeshVertex> vertices;
    vertices.reserve(shape.Vertices.size());

    glm::vec3 bounds_min(std::numeric_limits<float>::max());
    glm::vec3 bounds_max(std::numeric_limits<float>::lowest());

    for (const auto& vertex : shape.Vertices) {
        bounds_min = glm::min(bounds_min, vertex.Position);
        bounds_max = glm::max(bounds_max, vertex.Position);
        vertices.push_back(luna::StaticMeshVertex{
            .position = vertex.Position,
            .uv = vertex.TexCoord,
            .normal = vertex.Normal,
        });
    }

    const glm::vec3 center = (bounds_min + bounds_max) * 0.5f;
    const glm::vec3 extent = bounds_max - bounds_min;
    const float max_extent = (std::max) ((std::max) (extent.x, extent.y), (std::max) (extent.z, 0.0001f));
    const float scale = 2.0f / max_extent;

    for (auto& vertex : vertices) {
        vertex.position = (vertex.position - center) * scale;
    }

    const std::string mesh_name = shape.Name.empty() ? "DemoAssetMesh" : shape.Name;
    return luna::Mesh::create(
        mesh_name, std::move(vertices), std::vector<uint32_t>{shape.Indices.begin(), shape.Indices.end()});
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

    return luna::Mesh::create("ProceduralCube", vertices, indices);
}

std::shared_ptr<luna::Material> createFallbackMaterial()
{
    return luna::Material::create("FallbackMaterial", {}, glm::vec4(0.96f, 0.52f, 0.18f, 1.0f));
}

class RuntimeHudLayer final : public luna::Layer {
public:
    explicit RuntimeHudLayer(luna::LunaRuntimeApplication& application)
        : Layer("RuntimeHud"),
          m_application(&application)
    {}

    void onImGuiRender() override
    {
        if (m_application == nullptr) {
            return;
        }

        auto& application = *m_application;
        auto& renderer = luna::Application::get().getRenderer();
        const float delta_seconds = luna::Application::get().getTimestep().getSeconds();
        const float fps = 1.0f / (std::max) (delta_seconds, 0.0001f);

        bool auto_rotate = application.isAutoRotateEnabled();
        float spin_speed = application.getSpinSpeed();

        ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Luna ImGui Test");
        ImGui::Text("Backend: Luna RHI / %s", backendTypeToString(application.getBackend()));
        ImGui::Text("Frame: %.2f ms  |  %.1f FPS", delta_seconds * 1000.0f, fps);
        ImGui::Separator();
        ImGui::Text("Scene Source: %s", application.getAssetLabel().c_str());
        if (ImGui::Checkbox("Auto Rotate", &auto_rotate)) {
            application.setAutoRotateEnabled(auto_rotate);
        }
        if (ImGui::SliderFloat("Spin Speed", &spin_speed, 0.0f, 3.5f, "%.2f rad/s")) {
            application.setSpinSpeed(spin_speed);
        }
        ImGui::ColorEdit4("Clear Color", &renderer.getClearColor().x);
        if (ImGui::Button("Reset Camera")) {
            application.resetCamera();
        }
        ImGui::SameLine();
        if (ImGui::Button("Close")) {
            luna::Application::get().close();
        }
        ImGui::Separator();
        ImGui::Checkbox("Show Demo Window", &m_show_demo_window);
        ImGui::TextUnformatted("This program is the minimal ImGui validation app for the new renderer path.");
        ImGui::End();

        if (m_show_demo_window) {
            ImGui::ShowDemoWindow(&m_show_demo_window);
        }
    }

private:
    luna::LunaRuntimeApplication* m_application{nullptr};
    bool m_show_demo_window{true};
};

} // namespace

namespace luna {

LunaRuntimeApplication::LunaRuntimeApplication(luna::RHI::BackendType backend)
    : Application(ApplicationSpecification{
          .m_name = "Luna ImGui Test",
          .m_window_width = 1'600,
          .m_window_height = 900,
          .m_maximized = false,
          .m_enable_imgui = backendSupportsImGui(backend),
          .m_enable_multi_viewport = false,
      }),
      m_backend(backend)
{}

const std::string& LunaRuntimeApplication::getAssetLabel() const
{
    return m_asset_label;
}

luna::RHI::BackendType LunaRuntimeApplication::getBackend() const
{
    return m_backend;
}

bool LunaRuntimeApplication::isAutoRotateEnabled() const
{
    return m_auto_rotate;
}

void LunaRuntimeApplication::setAutoRotateEnabled(bool enabled)
{
    m_auto_rotate = enabled;
}

float LunaRuntimeApplication::getSpinSpeed() const
{
    return m_spin_speed;
}

Renderer::InitializationOptions LunaRuntimeApplication::getRendererInitializationOptions()
{
    LUNA_RUNTIME_INFO("LunaApp requested backend '{}' and present mode '{}' via code",
                      backendTypeToString(m_backend),
                      presentModeToString(kRequestedPresentMode));
    return Renderer::InitializationOptions{m_backend, kRequestedPresentMode};
}

void LunaRuntimeApplication::setSpinSpeed(float speed)
{
    m_spin_speed = speed;
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
    getRenderer().getClearColor() = glm::vec4(0.08f, 0.09f, 0.11f, 1.0f);
    resetCamera();
    buildScene();
    if (getImGuiLayer() != nullptr) {
        pushOverlay(std::make_unique<RuntimeHudLayer>(*this));
    } else {
        LUNA_RUNTIME_INFO("ImGui overlay disabled for backend '{}'", backendTypeToString(m_backend));
    }
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

    m_demo_entity = m_scene.createEntity("Demo Mesh");
    m_demo_entity.addComponent<StaticMeshComponent>(m_demo_mesh, m_demo_material);

    auto& transform = m_demo_entity.getComponent<TransformComponent>();
    transform.translation = glm::vec3(0.0f);
    transform.rotation = glm::vec3(-0.35f, 0.0f, 0.0f);
    transform.scale = glm::vec3(1.0f);
}

bool LunaRuntimeApplication::tryLoadDefaultAsset()
{
    const std::array<std::filesystem::path, 2> candidates = {
        projectRoot() / "Assets" / "basicmesh.glb",
        projectRoot() / "Assets" / "material_sphere" / "material_sphere.obj",
    };

    for (const auto& candidate : candidates) {
        if (!std::filesystem::exists(candidate)) {
            continue;
        }

        try {
            const auto model_data = rhi::ModelLoader::Load(candidate.string());
            if (model_data.Shapes.empty()) {
                continue;
            }

            const auto& shape = model_data.Shapes.front();
            m_demo_mesh = createNormalizedMeshFromShape(shape);
            if (!m_demo_mesh || !m_demo_mesh->isValid()) {
                continue;
            }

            if (shape.MaterialIndex < model_data.Materials.size()) {
                m_demo_material = Material::createFromModelMaterial(model_data.Materials[shape.MaterialIndex]);
            }
            if (!m_demo_material) {
                m_demo_material = createFallbackMaterial();
            }

            m_asset_label = candidate.filename().string();
            LUNA_RUNTIME_INFO("Loaded demo asset '{}'", candidate.string());
            return true;
        } catch (const std::exception& error) {
            LUNA_RUNTIME_WARN("Failed to load demo asset '{}': {}", candidate.string(), error.what());
        }
    }

    return false;
}

void LunaRuntimeApplication::createFallbackAsset()
{
    m_demo_mesh = createProceduralCubeMesh();
    m_demo_material = createFallbackMaterial();
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
    LUNA_RUNTIME_INFO("Starting LunaApp with backend '{}'", backendTypeToString(backend));
    return new LunaRuntimeApplication(backend);
}

} // namespace luna
