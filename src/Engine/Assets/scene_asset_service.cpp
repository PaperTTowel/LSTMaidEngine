#include "Engine/Assets/scene_asset_service.hpp"

#include <iostream>
#include <utility>

namespace lve {

  SceneAssetService::SceneAssetService(
    backend::RenderAssetFactory &assets,
    std::string rootPath)
    : assets{assets}
    , database{std::move(rootPath)} {}

  void SceneAssetService::setRootPath(const std::string &rootPath) {
    database.setRootPath(rootPath.empty() ? "Assets" : rootPath);
  }

  void SceneAssetService::initializeDatabase() {
    database.initialize();
  }

  void SceneAssetService::clearCaches() {
    modelCache.clear();
    materialCache.clear();
  }

  backend::TextureLoadOptions SceneAssetService::textureLoadOptionsForAsset(
    const std::string &assetPath) const {
    backend::TextureLoadOptions options{};
    if (const auto *meta = database.getMetaForPath(assetPath)) {
      if (meta->type == AssetType::Texture) {
        options.sRGB = meta->textureSettings.sRGB;
        options.generateMipmaps = meta->textureSettings.generateMipmaps;
      }
    }
    return options;
  }

  std::shared_ptr<backend::RenderModel> SceneAssetService::loadModelCached(
    const std::string &path) {
    if (path.empty()) return {};
    const std::string assetPath = path;
    auto it = modelCache.find(assetPath);
    if (it != modelCache.end()) {
      return it->second;
    }

    const std::string resolvedPath = database.resolveAssetPath(assetPath);
    backend::ModelLoadOptions loadOptions{};
    if (const auto *meta = database.getMetaForPath(assetPath)) {
      if (meta->type == AssetType::Model) {
        loadOptions.scale = meta->modelSettings.scale;
        loadOptions.generateNormals = meta->modelSettings.generateNormals;
        loadOptions.generateTangents = meta->modelSettings.generateTangents;
        loadOptions.flipUV = meta->modelSettings.flipUV;
      }
    }

    auto sharedModel = assets.loadModel(resolvedPath, loadOptions);
    if (!sharedModel) {
      std::cerr << "Failed to load model " << resolvedPath << "\n";
      return {};
    }
    modelCache[assetPath] = sharedModel;
    return sharedModel;
  }

  std::shared_ptr<backend::RenderMaterial> SceneAssetService::loadMaterialCached(
    const std::string &path) {
    if (path.empty()) return {};
    auto it = materialCache.find(path);
    if (it != materialCache.end()) {
      return it->second;
    }

    std::string error;
    auto material = assets.loadMaterial(
      path,
      &error,
      [this](const std::string &assetPath) {
        return database.resolveAssetPath(assetPath);
      });
    if (!material) {
      std::cerr << "Failed to load material " << path;
      if (!error.empty()) {
        std::cerr << ": " << error;
      }
      std::cerr << "\n";
      return {};
    }
    materialCache[path] = material;
    return material;
  }

  bool SceneAssetService::updateMaterialFromData(
    const std::string &path,
    const MaterialData &data) {
    if (path.empty()) return false;
    auto it = materialCache.find(path);
    std::shared_ptr<backend::RenderMaterial> target;
    if (it != materialCache.end()) {
      target = it->second;
    } else {
      target = assets.createMaterial();
      if (target) {
        materialCache[path] = target;
      }
    }

    if (!target) {
      return false;
    }

    target->setPath(path);
    std::string error;
    const bool ok = target->applyData(
      data,
      &error,
      [this](const std::string &assetPath) {
        return database.resolveAssetPath(assetPath);
      });
    if (!ok && !error.empty()) {
      std::cerr << "Failed to update material " << path << ": " << error << "\n";
    }
    return ok;
  }

  bool SceneAssetService::applyMaterialToObject(
    LveGameObject &obj,
    const std::string &path) {
    if (path.empty()) {
      obj.materialPath.clear();
      obj.material.reset();
    } else {
      auto material = loadMaterialCached(path);
      if (!material) {
        return false;
      }
      obj.materialPath = path;
      obj.material = material;
    }

    if (obj.material && obj.material->hasBaseColorTexture()) {
      obj.enableTextureType = 1;
    } else if (obj.model && obj.model->hasAnyDiffuseTexture()) {
      obj.enableTextureType = 1;
    } else {
      obj.enableTextureType = 0;
    }
    return true;
  }

} // namespace lve
