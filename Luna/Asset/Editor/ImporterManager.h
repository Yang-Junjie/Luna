#pragma once
#include "Importer.h"

#include <unordered_map>
#include <vector>

namespace luna {
class TaskSystem;

class ImporterManager final {
public:
    struct ImportStats {
        size_t discoveredAssets = 0;
        size_t importedMissingAssets = 0;
        size_t loadedExistingMetadata = 0;
        size_t rebuiltMetadata = 0;
        size_t unsupportedFilesSkipped = 0;
        size_t failedAssets = 0;
        size_t missingMetadataAfterSync = 0;
        size_t generatedModelFiles = 0;
        size_t generatedModelMetadata = 0;
        size_t generatedMaterialFiles = 0;
        size_t generatedMaterialMetadata = 0;
        size_t generatedTextureMetadata = 0;
        size_t failedGeneratedModelAssets = 0;
    };

    static void init();
    static ImportStats syncProjectAssets(TaskSystem* task_system = nullptr);
    static void import();
    static Importer* getImporter(std::string extension);

private:
    static inline std::vector<std::unique_ptr<Importer>> importers;
    static inline std::unordered_map<std::string, Importer*> extensionToImporter;
};
} // namespace luna
