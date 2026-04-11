#include "Core/Log.h"
#include "JobSystem/ResourceLoadQueue.h"
#include "JobSystem/TaskSystem.h"
#include "Renderer/ImageLoader.h"
#include "Renderer/ModelLoader.h"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#ifndef LUNA_PROJECT_ROOT
#error "LUNA_PROJECT_ROOT must be defined for TaskSystemResourcePipeline"
#endif

namespace {

int fail(const char* message)
{
    LUNA_CORE_ERROR("TaskSystem resource pipeline sample failed: {}", message);
    luna::Logger::shutdown();
    return EXIT_FAILURE;
}

struct TextureSummary {
    std::string Name;
    uint32_t Width = 0;
    uint32_t Height = 0;
    size_t ByteCount = 0;
    uint64_t Checksum = 0;
};

struct ModelSummary {
    std::string Name;
    size_t ShapeCount = 0;
    size_t MaterialCount = 0;
    size_t VertexCount = 0;
    size_t IndexCount = 0;
};

class ImportRegistry {
public:
    void addTexture(TextureSummary summary)
    {
        m_textures.push_back(std::move(summary));
    }

    void addModel(ModelSummary summary)
    {
        m_models.push_back(std::move(summary));
    }

    const std::vector<TextureSummary>& textures() const
    {
        return m_textures;
    }

    const std::vector<ModelSummary>& models() const
    {
        return m_models;
    }

private:
    std::vector<TextureSummary> m_textures;
    std::vector<ModelSummary> m_models;
};

struct RawFileData {
    std::vector<uint8_t> Bytes;
};

struct PreparedTexture {
    luna::val::ImageData Image;
    uint64_t Checksum = 0;
};

std::vector<uint8_t> readWholeFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }

    return std::vector<uint8_t>((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

uint64_t computeChecksum(const std::vector<uint8_t>& bytes)
{
    uint64_t checksum = 1469598103934665603ull;
    for (uint8_t byte : bytes) {
        checksum ^= byte;
        checksum *= 1099511628211ull;
    }
    return checksum;
}

TextureSummary makeTextureSummary(std::string name, const luna::val::ImageData& image, uint64_t checksum)
{
    return TextureSummary{std::move(name), image.Width, image.Height, image.ByteData.size(), checksum};
}

ModelSummary makeModelSummary(std::string name, const luna::val::ModelData& model)
{
    size_t vertex_count = 0;
    size_t index_count = 0;

    for (const auto& shape : model.Shapes) {
        vertex_count += shape.Vertices.size();
        index_count += shape.Indices.size();
    }

    return ModelSummary{std::move(name), model.Shapes.size(), model.Materials.size(), vertex_count, index_count};
}

void logRegistry(const ImportRegistry& registry)
{
    for (const auto& texture : registry.textures()) {
        LUNA_CORE_INFO("Texture [{}] {}x{} bytes={} checksum={}",
                       texture.Name,
                       texture.Width,
                       texture.Height,
                       texture.ByteCount,
                       texture.Checksum);
    }

    for (const auto& model : registry.models()) {
        LUNA_CORE_INFO("Model [{}] shapes={} materials={} vertices={} indices={}",
                       model.Name,
                       model.ShapeCount,
                       model.MaterialCount,
                       model.VertexCount,
                       model.IndexCount);
    }
}

} // namespace

int main()
{
    luna::Logger::init("logs/task-system-resource-pipeline.log", luna::Logger::Level::Info);

    const std::filesystem::path project_root{LUNA_PROJECT_ROOT};
    const std::filesystem::path texture_path = project_root / "assets" / "head.jpg";
    const std::filesystem::path model_path = project_root / "assets" / "basicmesh.glb";

    if (!std::filesystem::exists(texture_path)) {
        return fail("missing assets/head.jpg");
    }
    if (!std::filesystem::exists(model_path)) {
        return fail("missing assets/basicmesh.glb");
    }

    luna::TaskSystem task_system;
    if (!task_system.initialize()) {
        return fail("initialize() returned false");
    }

    ImportRegistry registry;
    luna::ResourceLoadQueue queue(task_system);

    std::atomic<uint32_t> queued_texture_commit_thread = std::numeric_limits<uint32_t>::max();
    std::atomic<uint32_t> staged_io_thread = std::numeric_limits<uint32_t>::max();
    std::atomic<uint32_t> staged_worker_thread = std::numeric_limits<uint32_t>::max();
    std::atomic<uint32_t> staged_commit_thread = std::numeric_limits<uint32_t>::max();
    std::atomic<uint32_t> final_barrier_thread = std::numeric_limits<uint32_t>::max();

    auto queued_texture_task = queue.submitLoadWithCommit(
        [texture_path_string = texture_path.string()]() {
            return luna::val::ImageLoader::LoadImageFromFile(texture_path_string);
        },
        [&registry, &task_system, &queued_texture_commit_thread, texture_name = std::string("queue/") + texture_path.filename().string()](
            luna::val::ImageData image) {
            queued_texture_commit_thread.store(task_system.getScheduler().GetThreadNum(), std::memory_order_release);
            registry.addTexture(makeTextureSummary(texture_name, image, computeChecksum(image.ByteData)));
        });

    if (!queued_texture_task.isValid()) {
        return fail("submitLoadWithCommit() did not return a valid task handle");
    }

    auto raw_file = std::make_shared<RawFileData>();
    auto prepared_texture = std::make_shared<PreparedTexture>();

    auto io_task = task_system.submit(
        [&task_system, &staged_io_thread, raw_file, texture_path_string = texture_path.string()]() {
            staged_io_thread.store(task_system.getScheduler().GetThreadNum(), std::memory_order_release);
            raw_file->Bytes = readWholeFile(texture_path_string);
        },
        {.target = luna::TaskTarget::IO});

    auto worker_task = io_task.then(
        task_system,
        [&task_system, &staged_worker_thread, raw_file, prepared_texture]() {
            staged_worker_thread.store(task_system.getScheduler().GetThreadNum(), std::memory_order_release);
            prepared_texture->Image = luna::val::ImageLoader::LoadImageFromMemory(raw_file->Bytes.data(), raw_file->Bytes.size());
            prepared_texture->Checksum = computeChecksum(prepared_texture->Image.ByteData);
        });

    auto staged_texture_task = worker_task.then(
        task_system,
        [&registry, &task_system, &staged_commit_thread, prepared_texture, texture_name = std::string("manual/") + texture_path.filename().string()]() {
            staged_commit_thread.store(task_system.getScheduler().GetThreadNum(), std::memory_order_release);
            registry.addTexture(makeTextureSummary(texture_name, prepared_texture->Image, prepared_texture->Checksum));
        },
        {.target = luna::TaskTarget::MainThread});

    if (!staged_texture_task.isValid()) {
        return fail("manual IO -> Worker -> MainThread chain did not return a valid handle");
    }

    auto model_handle = queue.submitLoad([model_path_string = model_path.string()]() {
        return luna::val::ModelLoader::Load(model_path_string);
    });

    if (!model_handle.isValid()) {
        return fail("submitLoad() for the model did not return a valid handle");
    }

    auto all_done = task_system.whenAll({queued_texture_task, staged_texture_task, model_handle.task()}).then(
        task_system,
        [&]() {
            final_barrier_thread.store(task_system.getScheduler().GetThreadNum(), std::memory_order_release);

            if (auto model = model_handle.take()) {
                registry.addModel(makeModelSummary(model_path.filename().string(), *model));
            }

            logRegistry(registry);
        },
        {.target = luna::TaskTarget::MainThread});

    all_done.wait(task_system);

    if (queued_texture_commit_thread.load(std::memory_order_acquire) != 0) {
        return fail("submitLoadWithCommit() commit did not execute on the main thread");
    }
    if (staged_commit_thread.load(std::memory_order_acquire) != 0) {
        return fail("manual pipeline commit did not execute on the main thread");
    }
    if (final_barrier_thread.load(std::memory_order_acquire) != 0) {
        return fail("whenAll() continuation did not execute on the main thread");
    }
    if (staged_io_thread.load(std::memory_order_acquire) != task_system.getFirstIOThreadNumber()) {
        return fail("IO stage did not execute on the configured IO thread");
    }
    if (staged_worker_thread.load(std::memory_order_acquire) == 0 ||
        staged_worker_thread.load(std::memory_order_acquire) == staged_io_thread.load(std::memory_order_acquire)) {
        return fail("Worker stage did not execute on a worker thread");
    }

    if (registry.textures().size() != 2) {
        return fail("expected two imported texture summaries");
    }
    if (registry.models().size() != 1) {
        return fail("expected one imported model summary");
    }

    for (const auto& texture : registry.textures()) {
        if (texture.Width == 0 || texture.Height == 0 || texture.ByteCount == 0) {
            return fail("one of the imported textures was empty");
        }
    }

    const auto& model = registry.models().front();
    if (model.ShapeCount == 0 || model.VertexCount == 0 || model.IndexCount == 0) {
        return fail("the imported model summary was empty");
    }

    LUNA_CORE_INFO("TaskSystem resource pipeline sample completed successfully");
    task_system.shutdown();
    luna::Logger::shutdown();
    return EXIT_SUCCESS;
}
