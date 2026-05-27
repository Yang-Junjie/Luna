#include "LoaderManager.h"
#include "MaterialLoader.h"
#include "MeshLoader.h"
#include "ModelLoader.h"
#include "ScriptLoader.h"
#include "TextureLoader.h"

namespace luna {

void LoaderManager::init()
{
    if (!loaders.empty()) {
        return;
    }

    auto register_loader = [](AssetType type, std::unique_ptr<Loader> loader) {
        type_to_loader[type] = loader.get();
        loaders.push_back(std::move(loader));
    };

    register_loader(AssetType::Mesh, std::make_unique<MeshLoader>());
    register_loader(AssetType::Material, std::make_unique<MaterialLoader>());
    register_loader(AssetType::Model, std::make_unique<ModelLoader>());
    register_loader(AssetType::Script, std::make_unique<ScriptLoader>());
    register_loader(AssetType::Texture, std::make_unique<TextureLoader>());
}

Loader* LoaderManager::getLoader(AssetType type)
{
    init();

    if (const auto it = type_to_loader.find(type); it != type_to_loader.end()) {
        return it->second;
    }

    return nullptr;
}

} // namespace luna
