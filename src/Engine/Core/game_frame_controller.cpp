#include "Engine/Core/game_frame_controller.hpp"

#include "Engine/Scene/scene_system.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <iostream>

namespace lve {

  void GameFrameController::updateCharacter(
    float frameTime,
    backend::InputProvider &input,
    SceneSystem &sceneSystem,
    SpriteAnimator *spriteAnimator,
    bool inputEnabled) {
    const auto characterId = sceneSystem.getCharacterId();
    auto *characterPtr = sceneSystem.findObject(characterId);
    if (!characterPtr) {
      std::cerr << "Character object missing; skipping character update\n";
      return;
    }

    auto &character = *characterPtr;
    if (inputEnabled) {
      characterController.moveInPlaneXZ(input, frameTime, character);
    } else {
      character.objState = ObjectState::IDLE;
    }
    character.transformDirty = true;
    if (spriteAnimator) {
      const char *stateName = (character.objState == ObjectState::WALKING) ? "walking" : "idle";
      if (character.spriteStateName != stateName || !character.diffuseMap) {
        spriteAnimator->applySpriteState(character, stateName);
      }
    }
    sceneSystem.updateAnimationFrame(character, 6, frameTime, 0.15f);
  }

  GameFrameState GameFrameController::updateCamera(
    SceneSystem &sceneSystem,
    const ViewportInfo &gameView,
    backend::RenderExtent windowExtent,
    float fallbackAspect,
    bool useOrthoCamera) {
    const uint32_t gameWidth = gameView.width > 0 ? gameView.width : windowExtent.width;
    const uint32_t gameHeight = gameView.height > 0 ? gameView.height : windowExtent.height;
    const float gameAspect = gameHeight > 0
      ? (static_cast<float>(gameWidth) / static_cast<float>(gameHeight))
      : fallbackAspect;

    const LveGameObject *activeCamera = sceneSystem.findActiveCamera();
    const bool useSceneCamera = activeCamera && activeCamera->camera;
    bool canRenderGameView = true;
    if (useSceneCamera) {
      gameCamera.setViewYXZ(
        activeCamera->transform.translation,
        activeCamera->transform.rotation);
    } else {
      const auto currentCharacterId = sceneSystem.getCharacterId();
      auto *currentCharacter = sceneSystem.findObject(currentCharacterId);
      if (currentCharacter) {
        const glm::vec3 gameCamOffset{-3.0f, -2.0f, 0.0f};
        const glm::vec3 gameCamPos = currentCharacter->transform.translation + gameCamOffset;
        gameCamera.setViewTarget(gameCamPos, currentCharacter->transform.translation);
      } else {
        std::cerr << "Character object missing after editor update; skipping game view render\n";
        canRenderGameView = false;
      }
    }

    if (useSceneCamera && activeCamera && activeCamera->camera) {
      const auto &camera = *activeCamera->camera;
      if (camera.projection == "ortho") {
        const float orthoHeight = camera.orthoHeight;
        const float orthoWidth = orthoHeight * gameAspect;
        gameCamera.setOrthographicProjection(
          -orthoWidth / 2.f,
          orthoWidth / 2.f,
          -orthoHeight / 2.f,
          orthoHeight / 2.f,
          camera.nearPlane,
          camera.farPlane);
      } else {
        gameCamera.setPerspectiveProjection(
          glm::radians(camera.fov),
          gameAspect,
          camera.nearPlane,
          camera.farPlane);
      }
    } else if (useOrthoCamera) {
      float orthoHeight = 10.f;
      float orthoWidth = orthoHeight * gameAspect;
      gameCamera.setOrthographicProjection(
        -orthoWidth / 2.f,
        orthoWidth / 2.f,
        -orthoHeight / 2.f,
        orthoHeight / 2.f,
        -1.f,
        100.f);
    } else {
      gameCamera.setPerspectiveProjection(glm::radians(50.f), gameAspect, 0.1f, 100.f);
    }

    return GameFrameState{gameCamera, canRenderGameView};
  }

} // namespace lve
