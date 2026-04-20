#pragma once
#include "Importer.h"

#include <unordered_map>
#include <vector>

namespace luna {
class ImporterManager final {
public:
    static void init();
    static void import();
    static Importer* getImporter(std::string extension);

private:
    static inline std::vector<std::unique_ptr<Importer>> importers;
    static inline std::unordered_map<std::string, Importer*> extensionToImporter;
};
} // namespace luna
