#include "scene_system.hpp"

#include "Engine/Scene/scene_defaults.hpp"
#include "Engine/Scene/scene_persistence.hpp"

// std
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace lve {

  SceneSystem::SceneSystem(
    backend::RenderAssetFactory &assets,
    backend::ObjectBufferPoolPtr objectBuffers)
    : assetService{assets, "Assets"},
      gameObjectManager{std::move(objectBuffers), assets.getDefaultTexture()} {}

  void SceneSystem::setAssetDefaults(const AssetDefaults &defaults) {
    assetDefaults = defaults;
    if (assetDefaults.rootPath.empty()) {
      assetDefaults.rootPath = "Assets";
    }
    if (assetDefaults.activeMeshPath.empty()) {
      assetDefaults.activeMeshPath = "Assets/models/colored_cube.obj";
    }
    if (assetDefaults.activeSpriteMetaPath.empty()) {
      assetDefaults.activeSpriteMetaPath = "Assets/textures/characters/player.json";
    }
    assetService.setRootPath(assetDefaults.rootPath);
  }

  void SceneSystem::setAssetRootPath(const std::string &rootPath) {
    assetDefaults.rootPath = rootPath.empty() ? "Assets" : rootPath;
    assetService.setRootPath(assetDefaults.rootPath);
  }

  void SceneSystem::setActiveMeshPath(const std::string &path) {
    assetDefaults.activeMeshPath = path.empty() ? "Assets/models/colored_cube.obj" : path;
  }

  void SceneSystem::setActiveMaterialPath(const std::string &path) {
    assetDefaults.activeMaterialPath = path;
  }

  LveGameObject &SceneSystem::createEmptyObject() {
    return gameObjectManager.createGameObject();
  }

  LveGameObject *SceneSystem::findObject(LveGameObject::id_t id) {
    auto it = gameObjectManager.gameObjects.find(id);
    if (it == gameObjectManager.gameObjects.end()) return nullptr;
    return &it->second;
  }

  const LveGameObject *SceneSystem::findObject(LveGameObject::id_t id) const {
    auto it = gameObjectManager.gameObjects.find(id);
    if (it == gameObjectManager.gameObjects.end()) return nullptr;
    return &it->second;
  }

  bool SceneSystem::destroyObject(LveGameObject::id_t id) {
    return gameObjectManager.destroyGameObject(id);
  }

  void SceneSystem::collectObjects(std::vector<LveGameObject*> &out) {
    out.clear();
    out.reserve(gameObjectManager.gameObjects.size());
    for (auto &kv : gameObjectManager.gameObjects) {
      out.push_back(&kv.second);
    }
  }

  void SceneSystem::collectObjects(std::vector<const LveGameObject*> &out) const {
    out.clear();
    out.reserve(gameObjectManager.gameObjects.size());
    for (const auto &kv : gameObjectManager.gameObjects) {
      out.push_back(&kv.second);
    }
  }

  void SceneSystem::updateBuffers(int frameIndex) {
    gameObjectManager.updateBuffer(frameIndex);
  }

  void SceneSystem::updateAnimationFrame(
    LveGameObject &obj,
    int maxFrames,
    float frameTime,
    float animationSpeed) {
    gameObjectManager.updateFrame(obj, maxFrames, frameTime, animationSpeed);
  }

  ObjectState SceneSystem::objectStateFromString(const std::string &name) {
    if (name == "walking" || name == "walk") return ObjectState::WALKING;
    return ObjectState::IDLE;
  }

  std::string SceneSystem::objectStateToString(ObjectState state) {
    switch (state) {
      case ObjectState::WALKING: return "walking";
      case ObjectState::IDLE:
      default: return "idle";
    }
  }

  backend::TextureLoadOptions SceneSystem::textureLoadOptionsForAsset(const std::string &assetPath) const {
    return assetService.textureLoadOptionsForAsset(assetPath);
  }

  std::shared_ptr<backend::RenderModel> SceneSystem::loadModelCached(const std::string &path) {
    auto sharedModel = assetService.loadModelCached(path);
    if (path == "Assets/models/colored_cube.obj") {
      cubeModel = sharedModel;
    }
    return sharedModel;
  }

  std::shared_ptr<backend::RenderMaterial> SceneSystem::loadMaterialCached(const std::string &path) {
    return assetService.loadMaterialCached(path);
  }

  bool SceneSystem::updateMaterialFromData(const std::string &path, const MaterialData &data) {
    return assetService.updateMaterialFromData(path, data);
  }

  bool SceneSystem::applyMaterialToObject(LveGameObject &obj, const std::string &path) {
    return assetService.applyMaterialToObject(obj, path);
  }

  void SceneSystem::ensureNodeOverrides(LveGameObject &obj) {
    if (!obj.model) {
      obj.nodeOverrides.clear();
      return;
    }
    const auto &nodes = obj.model->getNodes();
    if (obj.nodeOverrides.size() != nodes.size()) {
      obj.nodeOverrides.clear();
      obj.nodeOverrides.resize(nodes.size());
    }
  }

  void SceneSystem::applyNodeOverrides(LveGameObject &obj, const MeshComponent &mesh) {
    ensureNodeOverrides(obj);
    for (auto &override : obj.nodeOverrides) {
      override.enabled = false;
      override.transform.translation = {};
      override.transform.rotation = {};
      override.transform.scale = {1.f, 1.f, 1.f};
    }
    for (const auto &override : mesh.nodeOverrides) {
      if (override.node < 0 || static_cast<std::size_t>(override.node) >= obj.nodeOverrides.size()) {
        continue;
      }
      auto &target = obj.nodeOverrides[static_cast<std::size_t>(override.node)];
      target.enabled = true;
      target.transform.translation = override.transform.position;
      target.transform.rotation = override.transform.rotation;
      target.transform.scale = override.transform.scale;
    }
  }

  bool SceneSystem::setActiveSpriteMetadata(const std::string &path) {
    SpriteMetadata meta{};
    const std::string assetPath = path;
    const std::string resolvedPath = assetService.assetDatabase().resolveAssetPath(assetPath);
    if (!loadSpriteMetadata(resolvedPath, meta)) {
      std::cerr << "Failed to load sprite metadata: " << path << "\n";
      return false;
    }
    playerMeta = meta;
    assetDefaults.activeSpriteMetaPath = assetPath;
    spriteAnimator = std::make_unique<SpriteAnimator>(
      assetService.assetFactory(),
      playerMeta,
      [this](const std::string &assetPath) {
        return textureLoadOptionsForAsset(assetPath);
      });

    for (auto &kv : gameObjectManager.gameObjects) {
      auto &obj = kv.second;
      if (!obj.isSprite) continue;
      obj.spriteMetaPath = assetPath;
      if (!obj.spriteStateName.empty()) {
        spriteAnimator->applySpriteState(obj, obj.spriteStateName);
      } else {
        spriteAnimator->applySpriteState(obj, obj.objState);
      }
    }
    return true;
  }

  LveGameObject &SceneSystem::createMeshObject(const glm::vec3 &position, const std::string &modelPath) {
    const std::string fallbackPath = assetDefaults.activeMeshPath.empty()
      ? "Assets/models/colored_cube.obj"
      : assetDefaults.activeMeshPath;
    const std::string pathToUse = modelPath.empty() ? fallbackPath : modelPath;
    auto model = loadModelCached(pathToUse);
    auto &obj = gameObjectManager.createGameObject();
    obj.model = model;
    obj.modelPath = pathToUse;
    obj.name = "Mesh " + std::to_string(obj.getId());
    obj.enableTextureType = model && model->hasAnyDiffuseTexture() ? 1 : 0;
    obj.isSprite = false;
    obj.billboardMode = BillboardMode::None;
    obj.transform.translation = position;
    obj.transform.scale = glm::vec3(1.f);
    obj.transformDirty = true;
    ensureNodeOverrides(obj);
    return obj;
  }

  LveGameObject &SceneSystem::createSpriteObject(const glm::vec3 &position, ObjectState state, const std::string &metaPath) {
    if (!spriteModel) {
      spriteModel = loadModelCached("Assets/models/quad.obj");
    }
    auto &obj = gameObjectManager.createGameObject();
    obj.model = spriteModel;
    obj.name = "Sprite " + std::to_string(obj.getId());
    obj.enableTextureType = 1;
    obj.isSprite = true;
    obj.billboardMode = BillboardMode::Cylindrical;
    obj.spriteMetaPath = metaPath;
    obj.transform.translation = position;
    obj.transform.rotation = {0.f, 0.f, 0.f};
    obj.objState = state;
    obj.transformDirty = true;
    if (spriteAnimator) {
      spriteAnimator->applySpriteState(obj, state);
    }
    return obj;
  }

  LveGameObject &SceneSystem::createPointLightObject(const glm::vec3 &position) {
    auto &light = gameObjectManager.makePointLight(0.2f);
    light.name = "PointLight " + std::to_string(light.getId());
    light.transform.translation = position;
    light.transformDirty = true;
    return light;
  }

  LveGameObject &SceneSystem::createCameraObject(const glm::vec3 &position) {
    auto &cameraObj = gameObjectManager.createGameObject();
    cameraObj.name = "Camera " + std::to_string(cameraObj.getId());
    cameraObj.transform.translation = position;
    cameraObj.transform.rotation = {0.f, 0.f, 0.f};
    cameraObj.transform.scale = glm::vec3(1.f);
    cameraObj.transformDirty = true;
    cameraObj.camera = CameraComponent{};
    return cameraObj;
  }

  LveGameObject &SceneSystem::createMeshObjectWithId(
    LveGameObject::id_t id,
    const glm::vec3 &position,
    const std::string &modelPath) {
    const std::string fallbackPath = assetDefaults.activeMeshPath.empty()
      ? "Assets/models/colored_cube.obj"
      : assetDefaults.activeMeshPath;
    const std::string pathToUse = modelPath.empty() ? fallbackPath : modelPath;
    auto model = loadModelCached(pathToUse);
    auto &obj = gameObjectManager.createGameObjectWithId(id);
    obj.model = model;
    obj.modelPath = pathToUse;
    obj.name = "Mesh " + std::to_string(obj.getId());
    obj.enableTextureType = model && model->hasAnyDiffuseTexture() ? 1 : 0;
    obj.isSprite = false;
    obj.billboardMode = BillboardMode::None;
    obj.transform.translation = position;
    obj.transform.scale = glm::vec3(1.f);
    obj.transformDirty = true;
    ensureNodeOverrides(obj);
    return obj;
  }

  LveGameObject &SceneSystem::createSpriteObjectWithId(
    LveGameObject::id_t id,
    const glm::vec3 &position,
    ObjectState state,
    const std::string &metaPath) {
    if (!spriteModel) {
      spriteModel = loadModelCached("Assets/models/quad.obj");
    }
    auto &obj = gameObjectManager.createGameObjectWithId(id);
    obj.model = spriteModel;
    obj.name = "Sprite " + std::to_string(obj.getId());
    obj.enableTextureType = 1;
    obj.isSprite = true;
    obj.billboardMode = BillboardMode::Cylindrical;
    obj.spriteMetaPath = metaPath;
    obj.transform.translation = position;
    obj.transform.rotation = {0.f, 0.f, 0.f};
    obj.objState = state;
    obj.transformDirty = true;
    if (spriteAnimator) {
      spriteAnimator->applySpriteState(obj, state);
    }
    return obj;
  }

  LveGameObject &SceneSystem::createPointLightObjectWithId(
    LveGameObject::id_t id,
    const glm::vec3 &position,
    float intensity,
    float radius,
    const glm::vec3 &color) {
    auto &light = gameObjectManager.makePointLightWithId(id, intensity, radius, color);
    light.name = "PointLight " + std::to_string(light.getId());
    light.transform.translation = position;
    light.transformDirty = true;
    return light;
  }

  LveGameObject &SceneSystem::createCameraObjectWithId(
    LveGameObject::id_t id,
    const glm::vec3 &position,
    const CameraComponent &camera) {
    auto &cameraObj = gameObjectManager.createGameObjectWithId(id);
    cameraObj.name = "Camera " + std::to_string(cameraObj.getId());
    cameraObj.transform.translation = position;
    cameraObj.transform.rotation = {0.f, 0.f, 0.f};
    cameraObj.transform.scale = glm::vec3(1.f);
    cameraObj.transformDirty = true;
    cameraObj.camera = camera;
    return cameraObj;
  }

  LveGameObject *SceneSystem::findActiveCamera() {
    for (auto &kv : gameObjectManager.gameObjects) {
      auto &obj = kv.second;
      if (obj.camera && obj.camera->active) {
        return &obj;
      }
    }
    return nullptr;
  }

  const LveGameObject *SceneSystem::findActiveCamera() const {
    for (const auto &kv : gameObjectManager.gameObjects) {
      const auto &obj = kv.second;
      if (obj.camera && obj.camera->active) {
        return &obj;
      }
    }
    return nullptr;
  }

  void SceneSystem::setActiveCamera(LveGameObject::id_t id, bool active) {
    for (auto &kv : gameObjectManager.gameObjects) {
      auto &obj = kv.second;
      if (!obj.camera) {
        continue;
      }
      if (obj.getId() == id) {
        obj.camera->active = active;
      } else if (active) {
        obj.camera->active = false;
      }
    }
  }

  Scene SceneSystem::exportSceneSnapshot() {
    return ScenePersistence::exportSnapshot(*this);
  }

  void SceneSystem::importSceneSnapshot(
    const Scene &scene,
    std::optional<LveGameObject::id_t> protectedId) {
    ScenePersistence::importSnapshot(*this, scene, protectedId);
  }

  void SceneSystem::saveSceneToFile(const std::string &path) {
    ScenePersistence::saveToFile(*this, path);
  }

  void SceneSystem::loadSceneFromFile(
    const std::string &path,
    std::optional<LveGameObject::id_t> protectedId) {
    ScenePersistence::loadFromFile(*this, path, protectedId);
  }

  void SceneSystem::loadGameObjects() {
    SceneDefaults::loadGameObjects(*this);
  }
} // namespace lve


