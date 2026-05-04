#include "Engine/Scene/scene_defaults.hpp"

#include "Engine/Scene/scene_system.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace lve {

  void SceneDefaults::loadGameObjects(SceneSystem &sceneSystem) {
    auto &defaults = sceneSystem.assetDefaults;
    if (defaults.rootPath.empty()) {
      defaults.rootPath = "Assets";
    }
    if (defaults.activeMeshPath.empty()) {
      defaults.activeMeshPath = "Assets/models/colored_cube.obj";
    }
    if (defaults.activeSpriteMetaPath.empty()) {
      defaults.activeSpriteMetaPath = "Assets/textures/characters/player.json";
    }
    sceneSystem.assetService.setRootPath(defaults.rootPath);
    sceneSystem.assetService.initializeDatabase();

    sceneSystem.cubeModel = sceneSystem.loadModelCached(defaults.activeMeshPath);
    sceneSystem.spriteModel = sceneSystem.loadModelCached("Assets/models/quad.obj");

    sceneSystem.createMeshObject({-.5f, .5f, 0.f}, defaults.activeMeshPath);

    const std::string defaultMetaPath = defaults.activeSpriteMetaPath;
    if (!loadSpriteMetadata(defaultMetaPath, sceneSystem.playerMeta)) {
      std::cerr << "Failed to load player sprite metadata; using defaults\n";
      sceneSystem.playerMeta.atlasCols = 6;
      sceneSystem.playerMeta.atlasRows = 1;
      sceneSystem.playerMeta.size = {33.f, 44.f};
      SpriteStateInfo idle{};
      idle.row = 0; idle.frameCount = 6; idle.frameDuration = 0.15f; idle.loop = true; idle.atlasCols = 6; idle.atlasRows = 1; idle.texturePath = "Assets/textures/characters/playerIDLE.png";
      SpriteStateInfo walk{};
      walk.row = 0; walk.frameCount = 8; walk.frameDuration = 0.125f; walk.loop = true; walk.atlasCols = 8; walk.atlasRows = 1; walk.texturePath = "Assets/textures/characters/playerWalking.png";
      sceneSystem.playerMeta.states["idle"] = idle;
      sceneSystem.playerMeta.states["walking"] = walk;
    }
    sceneSystem.spriteAnimator = std::make_unique<SpriteAnimator>(
      sceneSystem.assetService.assetFactory(),
      sceneSystem.playerMeta,
      [&sceneSystem](const std::string &assetPath) {
        return sceneSystem.textureLoadOptionsForAsset(assetPath);
      });

    auto &characterObj = sceneSystem.createSpriteObject({0.f, 0.f, 0.f}, ObjectState::IDLE, defaults.activeSpriteMetaPath);
    sceneSystem.characterId = characterObj.getId();

    std::vector<glm::vec3> lightColors{
        {1.f, .1f, .1f},
        {.1f, .1f, 1.f},
        {.1f, 1.f, .1f},
        {1.f, 1.f, .1f},
        {.1f, 1.f, 1.f},
        {1.f, 1.f, 1.f}};

    for (int i = 0; i < lightColors.size(); i++) {
      auto &pointLight = sceneSystem.gameObjectManager.makePointLight(0.2f);
      pointLight.color = lightColors[i];
      auto rotateLight = glm::rotate(
        glm::mat4(1.f),
        (i * glm::two_pi<float>()) / lightColors.size(),
        {0.f, -1.f, 0.f});
      pointLight.transform.translation = glm::vec3(rotateLight * glm::vec4(-1.f, -1.f, -1.f, -1.f));
    }
  }

} // namespace lve
