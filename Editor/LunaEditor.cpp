#include "Core/Layer.h"
#include "Core/Log.h"
#include "Imgui/ImGuiContext.h"
#include "LunaEditor.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/ModelLoader.h"
#include "Scene/Components.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <glm/common.hpp>
#include <glm/trigonometric.hpp>
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
            .tangent = vertex.Tangent,
            .bitangent = vertex.Bitangent,
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
    luna::Material::SurfaceProperties surface;
    surface.BaseColorFactor = glm::vec4(0.96f, 0.52f, 0.18f, 1.0f);
    return luna::Material::create("FallbackMaterial", {}, surface);
}

bool drawVec3Control(
    const std::string& label, glm::vec3& values, float reset_value = 0.0f, float column_width = 100.0f, float drag_speed = 0.1f)
{
    bool changed = false;
    ImGui::PushID(label.c_str());
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2{0.0f, 1.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{4.0f, 1.0f});

    if (ImGui::BeginTable("##Vec3Table", 2, ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthFixed, column_width);
        ImGui::TableSetupColumn("##controls", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetFontSize() + 2.0f);

        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label.c_str());

        ImGui::TableNextColumn();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{4.0f, 0.0f});

        const float line_height = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        const ImVec2 button_size{line_height + 3.0f, line_height};
        const float item_spacing = ImGui::GetStyle().ItemSpacing.x;
        const float item_width =
            (std::max) ((ImGui::GetContentRegionAvail().x - button_size.x * 3.0f - item_spacing * 2.0f) / 3.0f, 1.0f);

        auto draw_axis_control =
            [&](const char* axis_label, float& value, const ImVec4& color, const ImVec4& hovered_color, bool last) {
                bool axis_changed = false;

                ImGui::PushStyleColor(ImGuiCol_Button, color);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered_color);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
                if (ImGui::Button(axis_label, button_size)) {
                    value = reset_value;
                    axis_changed = true;
                }
                ImGui::PopStyleColor(3);

                ImGui::SameLine();
                ImGui::SetNextItemWidth(item_width);
                if (ImGui::DragFloat((std::string("##") + axis_label).c_str(), &value, drag_speed, 0.0f, 0.0f, "%.2f")) {
                    axis_changed = true;
                }

                if (!last) {
                    ImGui::SameLine();
                }

                return axis_changed;
            };

        changed |= draw_axis_control("X", values.x, ImVec4{0.80f, 0.10f, 0.15f, 1.0f}, ImVec4{0.90f, 0.20f, 0.20f, 1.0f}, false);
        changed |= draw_axis_control("Y", values.y, ImVec4{0.20f, 0.70f, 0.20f, 1.0f}, ImVec4{0.30f, 0.80f, 0.30f, 1.0f}, false);
        changed |= draw_axis_control("Z", values.z, ImVec4{0.10f, 0.25f, 0.80f, 1.0f}, ImVec4{0.20f, 0.35f, 0.90f, 1.0f}, true);

        ImGui::PopStyleVar();
        ImGui::EndTable();
    }

    ImGui::PopStyleVar(2);
    ImGui::PopID();
    return changed;
}

void drawTransformMatrix(const luna::TransformComponent& transform)
{
    const glm::mat4 matrix = transform.getTransform();
    ImGui::TextUnformatted("Transform Matrix");
    ImGui::Separator();

    for (int row = 0; row < 4; ++row) {
        ImGui::Text(
            "[%7.3f %7.3f %7.3f %7.3f]",
            matrix[0][row],
            matrix[1][row],
            matrix[2][row],
            matrix[3][row]);
    }
}

class EditorHudLayer final : public luna::Layer {
public:
    explicit EditorHudLayer(luna::LunaEditorApplication& application)
        : Layer("EditorHud"),
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
        ImGui::Begin("Scene");
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
        const auto viewport_extent = renderer.getSceneOutputSize();
        ImGui::Text("Viewport: %u x %u", viewport_extent.width, viewport_extent.height);
        ImGui::Checkbox("Show Demo Window", &m_show_demo_window);
        ImGui::TextUnformatted("Scene rendering now targets a persistent offscreen texture and is presented in the Viewport panel.");
        ImGui::End();

        drawEntityPanel(application);
        drawInspectorPanel(application);

        ImGui::SetNextWindowSize(ImVec2(960.0f, 640.0f), ImGuiCond_FirstUseEver);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport");

        const ImVec2 available = ImGui::GetContentRegionAvail();
        const float dpi_scale = ImGui::GetWindowViewport() != nullptr ? ImGui::GetWindowViewport()->DpiScale : 1.0f;
        const uint32_t viewport_width = static_cast<uint32_t>((std::max) (available.x * dpi_scale, 0.0f));
        const uint32_t viewport_height = static_cast<uint32_t>((std::max) (available.y * dpi_scale, 0.0f));
        renderer.setSceneOutputSize(viewport_width, viewport_height);

        const auto& scene_texture = renderer.getSceneOutputTexture();
        const ImTextureID texture_id = luna::rhi::ImGuiRhiContext::GetTextureId(scene_texture);
        if (texture_id != 0 && available.x > 0.0f && available.y > 0.0f) {
            ImGui::Image(texture_id, available);
        } else if (available.x > 0.0f && available.y > 0.0f) {
            ImGui::SetCursorPos(ImVec2(16.0f, 16.0f));
            ImGui::TextUnformatted("Viewport texture will appear after the first rendered frame.");
        }

        ImGui::End();
        ImGui::PopStyleVar();

        if (m_show_demo_window) {
            ImGui::ShowDemoWindow(&m_show_demo_window);
        }
    }

private:
    void drawEntityPanel(luna::LunaEditorApplication& application)
    {
        ImGui::SetNextWindowSize(ImVec2(280.0f, 320.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Entities");

        auto& scene = application.getScene();
        auto view = scene.registry().view<luna::TagComponent>();
        const luna::Entity selected_entity = application.getSelectedEntity();

        if (view.begin() == view.end()) {
            ImGui::TextUnformatted("No entities in scene.");
        } else {
            for (const auto entity_handle : view) {
                luna::Entity entity(entity_handle, &scene);
                const auto& tag = view.get<luna::TagComponent>(entity_handle);

                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                                           ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if (entity == selected_entity) {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }

                ImGui::TreeNodeEx(
                    reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(entity))), flags, "%s", tag.tag.c_str());
                if (ImGui::IsItemClicked()) {
                    application.setSelectedEntity(entity);
                }
            }
        }

        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()) {
            application.setSelectedEntity({});
        }

        ImGui::End();
    }

    void drawInspectorPanel(luna::LunaEditorApplication& application)
    {
        ImGui::SetNextWindowSize(ImVec2(360.0f, 420.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Inspector");

        luna::Entity selected_entity = application.getSelectedEntity();
        if (!selected_entity) {
            ImGui::TextUnformatted("Select an entity to inspect.");
            ImGui::End();
            return;
        }

        ImGui::Text("Entity: %s", selected_entity.getName().c_str());
        ImGui::Separator();

        if (selected_entity.hasComponent<luna::TransformComponent>()) {
            auto& transform = selected_entity.getComponent<luna::TransformComponent>();
            drawVec3Control("Translation", transform.translation, 0.0f);

            glm::vec3 rotation_degrees = glm::degrees(transform.rotation);
            if (drawVec3Control("Rotation", rotation_degrees, 0.0f)) {
                transform.rotation = glm::radians(rotation_degrees);
            }

            drawVec3Control("Scale", transform.scale, 1.0f);

            ImGui::Spacing();
            drawTransformMatrix(transform);
        } else {
            ImGui::TextUnformatted("Selected entity does not have a TransformComponent.");
        }

        ImGui::End();
    }

    luna::LunaEditorApplication* m_application{nullptr};
    bool m_show_demo_window{true};
};

} // namespace

namespace luna {

LunaEditorApplication::LunaEditorApplication(luna::RHI::BackendType backend)
    : Application(ApplicationSpecification{
          .m_name = "Luna Editor",
          .m_window_width = 1'600,
          .m_window_height = 900,
          .m_maximized = false,
          .m_enable_imgui = backendSupportsImGui(backend),
          .m_enable_multi_viewport = false,
      }),
      m_backend(backend)
{}

const std::string& LunaEditorApplication::getAssetLabel() const
{
    return m_asset_label;
}

luna::RHI::BackendType LunaEditorApplication::getBackend() const
{
    return m_backend;
}

bool LunaEditorApplication::isAutoRotateEnabled() const
{
    return m_auto_rotate;
}

void LunaEditorApplication::setAutoRotateEnabled(bool enabled)
{
    m_auto_rotate = enabled;
}

float LunaEditorApplication::getSpinSpeed() const
{
    return m_spin_speed;
}

Renderer::InitializationOptions LunaEditorApplication::getRendererInitializationOptions()
{
    LUNA_RUNTIME_INFO("LunaEditor requested backend '{}' and present mode '{}' via code",
                      backendTypeToString(m_backend),
                      presentModeToString(kRequestedPresentMode));
    return Renderer::InitializationOptions{m_backend, kRequestedPresentMode};
}

void LunaEditorApplication::setSpinSpeed(float speed)
{
    m_spin_speed = speed;
}

void LunaEditorApplication::resetCamera()
{
    auto& camera = getRenderer().getMainCamera();
    camera.m_position = glm::vec3(0.0f, 0.45f, 4.75f);
    camera.m_pitch = -0.12f;
    camera.m_yaw = 0.0f;
}

Scene& LunaEditorApplication::getScene()
{
    return m_scene;
}

const Scene& LunaEditorApplication::getScene() const
{
    return m_scene;
}

Entity LunaEditorApplication::getSelectedEntity() const
{
    return m_selected_entity;
}

void LunaEditorApplication::setSelectedEntity(Entity entity)
{
    m_selected_entity = entity;
}

void LunaEditorApplication::onInit()
{
    getRenderer().getClearColor() = glm::vec4(0.08f, 0.09f, 0.11f, 1.0f);
    resetCamera();
    buildScene();
    if (getImGuiLayer() != nullptr) {
        getRenderer().setSceneOutputMode(Renderer::SceneOutputMode::OffscreenTexture);
        pushOverlay(std::make_unique<EditorHudLayer>(*this));
    } else {
        getRenderer().setSceneOutputMode(Renderer::SceneOutputMode::Swapchain);
        LUNA_RUNTIME_INFO("ImGui overlay disabled for backend '{}'", backendTypeToString(m_backend));
    }
}

void LunaEditorApplication::onUpdate(Timestep timestep)
{
    updateDemoTransform(timestep.getSeconds());
    m_scene.onUpdateRuntime();
}

void LunaEditorApplication::buildScene()
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

    m_selected_entity = m_demo_entity;
}

bool LunaEditorApplication::tryLoadDefaultAsset()
{
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

void LunaEditorApplication::createFallbackAsset()
{
    m_demo_mesh = createProceduralCubeMesh();
    m_demo_material = createFallbackMaterial();
    m_asset_label = "Procedural cube";
    LUNA_RUNTIME_INFO("Using fallback procedural cube for editor demo");
}

void LunaEditorApplication::updateDemoTransform(float delta_time)
{
    if (!m_demo_entity || !m_demo_entity.hasComponent<TransformComponent>()) {
        return;
    }

    auto& transform = m_demo_entity.getComponent<TransformComponent>();
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
    LUNA_RUNTIME_INFO("Starting LunaEditor with backend '{}'", backendTypeToString(backend));
    return new LunaEditorApplication(backend);
}

} // namespace luna
