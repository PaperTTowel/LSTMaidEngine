#pragma once

#include "Engine/Assets/asset_database.hpp"
#include "Engine/Assets/material_data.hpp"
#include "Engine/Rendering/render_assets.hpp"
#include "Engine/Scene/game_object.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace lve {
  class SceneAssetService {
  public:
    SceneAssetService(backend::RenderAssetFactory &assets, std::string rootPath);

    backend::RenderAssetFactory &assetFactory() { return assets; }
    AssetDatabase &assetDatabase() { return database; }
    const AssetDatabase &assetDatabase() const { return database; }

    void setRootPath(const std::string &rootPath);
    void initializeDatabase();
    void clearCaches();

    backend::TextureLoadOptions textureLoadOptionsForAsset(const std::string &assetPath) const;
    std::shared_ptr<backend::RenderModel> loadModelCached(const std::string &path);
    std::shared_ptr<backend::RenderMaterial> loadMaterialCached(const std::string &path);
    bool updateMaterialFromData(const std::string &path, const MaterialData &data);
    bool applyMaterialToObject(LveGameObject &obj, const std::string &path);

  private:
    backend::RenderAssetFactory &assets;
    AssetDatabase database;
    std::unordered_map<std::string, std::shared_ptr<backend::RenderModel>> modelCache;
    std::unordered_map<std::string, std::shared_ptr<backend::RenderMaterial>> materialCache;
  };
} // namespace lve
