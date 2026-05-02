#include "engine_loop.hpp"

// backend
#include "camera.hpp"
#include "Engine/Backend/Factory/runtime_backend_factory.hpp"
#include "Engine/audio_system.hpp"
#include "Engine/scene_system.hpp"
#include "Game/RPGBattle/battle_overlay.hpp"
#include "Game/UI/hud_overlay.hpp"
#include "Game/Platform/player_controller.hpp"
#include "Game/VisualNovel/scenario_loader.hpp"
#include "Game/VisualNovel/visual_novel_overlay.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace lve {
  namespace {
    struct FrameConfig {
      float maxFrameTime{0.05f};
    };

    struct CameraConfig {
      glm::vec3 followOffset{0.f, 0.f, 10.f};
      float orthoHeight{10.f};
      float orthoZoomStep{0.8f};
      float orthoMinHeight{3.f};
      float orthoMaxHeight{24.f};
      float perspectiveFovDegrees{50.f};
      float orthoNear{-1.f};
      float orthoFar{100.f};
      float perspectiveNear{0.1f};
      float perspectiveFar{100.f};
    };

    struct GameplayConfig {
      const char *mapPath{"Assets/map/testMap.json"};
      float mobContactDamage{10.f};
      float bulletDamageToMob{1.f};
      glm::vec3 fallbackMobSpawnOffset{2.f, 0.f, 0.f};
      int characterAnimFrames{6};
      float characterAnimFrameDuration{0.15f};
    };

    struct AudioConfig {
      const char *runtimeAudioDirectory{"Assets/audio"};
      const char *sourceAudioDirectory{"src/Assets/audio"};
      const char *seShoot{"shoot"};
      const char *seHit{"hit"};
      const char *seMobDeath{"mobDeath"};
    };

    struct SignConfig {
      float displayDurationSeconds{5.0f};
    };

    struct ScenarioConfig {
      const char *runtimeScenarioPath{"Assets/scenario/main.json"};
      const char *sourceScenarioPath{"src/Assets/scenario/main.json"};
    };

    const FrameConfig kFrameConfig{};
    const CameraConfig kCameraConfig{};
    const GameplayConfig kGameplayConfig{};
    const AudioConfig kAudioConfig{};
    const SignConfig kSignConfig{};
    const ScenarioConfig kScenarioConfig{};

    std::unique_ptr<backend::RuntimeBackend> createRuntime() {
      backend::RuntimeBackendConfig config{};
      config.api = backend::BackendApi::Vulkan;
      config.width = EngineLoop::WIDTH;
      config.height = EngineLoop::HEIGHT;
      config.title = "2dVK";
      auto runtimeBackend = backend::createRuntimeBackend(config);
      if (!runtimeBackend) {
        throw std::runtime_error("Runtime backend initialization failed.");
      }
      return runtimeBackend;
    }

    float computeClampedFrameTime(std::chrono::high_resolution_clock::time_point &currentTime) {
      const auto newTime = std::chrono::high_resolution_clock::now();
      float frameTime =
        std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
      if (frameTime > kFrameConfig.maxFrameTime) {
        frameTime = kFrameConfig.maxFrameTime;
      }
      currentTime = newTime;
      return frameTime;
    }

    bool overlapsAabb(const LveGameObject &a, const LveGameObject &b) {
      const float ax = a.transform.scale.x * 0.5f;
      const float ay = a.transform.scale.y * 0.5f;
      const float bx = b.transform.scale.x * 0.5f;
      const float by = b.transform.scale.y * 0.5f;
      const float dx = std::abs(a.transform.translation.x - b.transform.translation.x);
      const float dy = std::abs(a.transform.translation.y - b.transform.translation.y);
      return dx <= (ax + bx) && dy <= (ay + by);
    }

    bool initializeAudio(AudioSystem &audioSystem) {
      if (!audioSystem.init()) {
        return false;
      }
      if (audioSystem.loadFromDirectory(kAudioConfig.runtimeAudioDirectory)) {
        return true;
      }
      return audioSystem.loadFromDirectory(kAudioConfig.sourceAudioDirectory);
    }
  } // namespace

  /* Engine bootstrap: initial objects */
  EngineLoop::EngineLoop()
    : runtime{createRuntime()} {
    auto &sceneSystem = runtime->sceneSystem();
    initWorld(sceneSystem);

    if (!initializeAudio(audioSystem)) {
      std::cerr << "Audio assets not found or failed to load.\n";
    } else if (!audioSystem.playFirstBgm()) {
      std::cerr << "No BGM found to auto-play.\n";
    }
  }

  EngineLoop::~EngineLoop() {
    audioSystem.shutdown();
  }

  void EngineLoop::setGameMode(GameMode nextMode) {
    if (activeGameMode == nextMode) {
      return;
    }
    activeGameMode = nextMode;
  }

  const char *EngineLoop::getGameModeName() const {
    switch (activeGameMode) {
      case GameMode::VisualNovel: return "VisualNovel";
      case GameMode::Battle: return "Battle";
      case GameMode::Platform:
      default: return "Platform";
    }
  }

  bool EngineLoop::loadVisualNovelScenario() {
    std::string error;
    if (visualNovelSystem.loadScenario(kScenarioConfig.runtimeScenarioPath, &error)) {
      return true;
    }
    std::cerr << "Failed to load runtime scenario: " << error << "\n";

    error.clear();
    if (visualNovelSystem.loadScenario(kScenarioConfig.sourceScenarioPath, &error)) {
      return true;
    }
    std::cerr << "Failed to load source scenario: " << error << "\n";
    return false;
  }

  void EngineLoop::updateModeShortcuts(backend::InputProvider &input) {
    const bool modeKeyDown = input.isKeyPressed(backend::KeyCode::V);
    if (modeKeyDown && !modeSwitchKeyHeld) {
      if (activeGameMode == GameMode::Platform) {
        if (visualNovelSystem.isLoaded()) {
          setGameMode(GameMode::VisualNovel);
        } else if (loadVisualNovelScenario()) {
          activeDialogueLines.clear();
          activeDialogueLineIndex = 0;
          setGameMode(GameMode::VisualNovel);
        }
      } else {
        setGameMode(GameMode::Platform);
      }
    }
    modeSwitchKeyHeld = modeKeyDown;
  }

  bool EngineLoop::consumeVisualNovelAdvance(backend::InputProvider &input) {
    const bool advanceDown =
      input.isKeyPressed(backend::KeyCode::Space) ||
      input.isMouseButtonPressed(backend::MouseButton::Left);
    const bool pressed = advanceDown && !visualNovelAdvanceHeld;
    visualNovelAdvanceHeld = advanceDown;
    return pressed;
  }

  const game::vn::DialogueLine *EngineLoop::getActiveDialogueLine() const {
    if (activeDialogueLineIndex >= activeDialogueLines.size()) {
      return nullptr;
    }
    return &activeDialogueLines[activeDialogueLineIndex];
  }

  game::battle::BattleDefinition EngineLoop::makeDefaultBattleDefinition(const std::string &enemyId) const {
    game::battle::BattleDefinition definition{};
    definition.id = enemyId.empty() ? "default" : enemyId;
    definition.party.push_back({"hero", "나", 100, 18, 3});
    definition.enemies.push_back({enemyId.empty() ? "slime" : enemyId, "슬라임", 45, 8, 1});
    return definition;
  }

  void EngineLoop::startBattleFromCommand(const game::vn::ScenarioCommand &command) {
    battleWinNode = command.winNode;
    battleLoseNode = command.loseNode;
    battleSystem.start(makeDefaultBattleDefinition(command.enemy));
    setGameMode(GameMode::Battle);
  }

  void EngineLoop::setVisualNovelBackground(SceneSystem &sceneSystem, const std::string &imagePath) {
    if (imagePath.empty()) {
      return;
    }

    auto texture = sceneSystem.loadTextureCached(imagePath);
    if (!texture) {
      std::cerr << "Failed to load visual novel background: " << imagePath << "\n";
      return;
    }

    LveGameObject *background = hasVisualNovelBackground
      ? sceneSystem.findObject(visualNovelBackgroundId)
      : nullptr;
    if (!background) {
      auto &created = sceneSystem.createTileSpriteObject(
        {0.f, 0.f, 0.5f},
        texture,
        1,
        1,
        0,
        0,
        {1.f, 1.f, 1.f},
        -100000);
      created.name = "VN.Background";
      created.enableTextureType = 1;
      created.transformDirty = true;
      visualNovelBackgroundId = created.getId();
      hasVisualNovelBackground = true;
      return;
    }

    background->diffuseMap = texture;
    background->enableTextureType = 1;
    background->atlasColumns = 1;
    background->atlasRows = 1;
    background->spriteState = {};
    background->spriteState.frameCount = 1;
    background->hasSpriteState = true;
    background->currentFrame = 0;
    background->renderOrder = -100000;
    background->transformDirty = true;
  }

  void EngineLoop::updateVisualNovelBackground(
    SceneSystem &sceneSystem,
    const LveCamera &gameCamera,
    float orthoWidth,
    float orthoHeight) {
    if (!hasVisualNovelBackground) {
      return;
    }
    auto *background = sceneSystem.findObject(visualNovelBackgroundId);
    if (!background) {
      hasVisualNovelBackground = false;
      return;
    }

    const glm::vec3 cameraPos = gameCamera.getPosition();
    float scaleWidth = orthoWidth;
    float scaleHeight = orthoHeight;
    if (background->diffuseMap) {
      const auto textureWidth = background->diffuseMap->getWidth();
      const auto textureHeight = background->diffuseMap->getHeight();
      if (textureWidth > 0 && textureHeight > 0 && orthoWidth > 0.f && orthoHeight > 0.f) {
        const float imageAspect = static_cast<float>(textureWidth) / static_cast<float>(textureHeight);
        const float viewAspect = orthoWidth / orthoHeight;
        if (viewAspect > imageAspect) {
          scaleWidth = orthoWidth;
          scaleHeight = orthoWidth / imageAspect;
        } else {
          scaleHeight = orthoHeight;
          scaleWidth = orthoHeight * imageAspect;
        }
      }
    }
    background->transform.translation = {cameraPos.x, cameraPos.y, 0.5f};
    background->transform.scale = {scaleWidth, scaleHeight, 1.f};
    background->renderOrder = -100000;
    background->transformDirty = true;
  }

  void EngineLoop::performBattleAction() {
    if (battleSystem.getResult() != game::battle::BattleResult::Running) {
      return;
    }

    if (battleSystem.playerAttack(0, 0) &&
        battleSystem.getResult() == game::battle::BattleResult::Running) {
      battleSystem.enemyTurn();
    }
  }

  void EngineLoop::updateBattle(backend::InputProvider &input, std::string &debugText) {
    const bool actionDown = input.isKeyPressed(backend::KeyCode::Space);
    if (actionDown && !battleActionKeyHeld) {
      performBattleAction();
    }
    battleActionKeyHeld = actionDown;

    const auto result = battleSystem.getResult();
    if (result == game::battle::BattleResult::Victory) {
      if (!battleWinNode.empty()) {
        std::string error;
        visualNovelSystem.goToNode(battleWinNode, &error);
        if (!error.empty()) {
          std::cerr << "Battle win node error: " << error << "\n";
        }
      }
      activeDialogueLines.clear();
      activeDialogueLineIndex = 0;
      setGameMode(GameMode::VisualNovel);
    } else if (result == game::battle::BattleResult::Defeat) {
      if (!battleLoseNode.empty()) {
        std::string error;
        visualNovelSystem.goToNode(battleLoseNode, &error);
        if (!error.empty()) {
          std::cerr << "Battle lose node error: " << error << "\n";
        }
      }
      activeDialogueLines.clear();
      activeDialogueLineIndex = 0;
      setGameMode(GameMode::VisualNovel);
    }

    debugText = std::string{"Mode: "} + getGameModeName() + "\n" + battleSystem.lastLog();
  }

  void EngineLoop::updateVisualNovel(
    backend::InputProvider &input,
    SceneSystem &sceneSystem,
    std::string &debugText) {
    const bool advancePressed = consumeVisualNovelAdvance(input);

    if (getActiveDialogueLine()) {
      if (advancePressed) {
        ++activeDialogueLineIndex;
        if (activeDialogueLineIndex >= activeDialogueLines.size()) {
          activeDialogueLines.clear();
          activeDialogueLineIndex = 0;
          std::string error;
          visualNovelSystem.advance(&error);
          if (!error.empty()) {
            std::cerr << "Visual novel advance error: " << error << "\n";
          }
        }
      }
      debugText = std::string{"Mode: "} + getGameModeName();
      return;
    }

    for (int guard = 0; guard < 32; ++guard) {
      const auto *command = visualNovelSystem.currentCommand();
      if (!command) {
        debugText = std::string{"Mode: "} + getGameModeName() + "\nScenario complete";
        return;
      }

      switch (command->type) {
        case game::vn::ScenarioCommandType::Say:
        case game::vn::ScenarioCommandType::Choice:
          if (command->type == game::vn::ScenarioCommandType::Say && advancePressed) {
            std::string error;
            visualNovelSystem.advance(&error);
            if (!error.empty()) {
              std::cerr << "Visual novel advance error: " << error << "\n";
            }
            continue;
          }
          debugText = std::string{"Mode: "} + getGameModeName();
          return;

        case game::vn::ScenarioCommandType::DialogueFile: {
          std::string error;
          if (!game::vn::ScenarioLoader::loadDialogueFromFile(command->file, activeDialogueLines, &error)) {
            std::cerr << "Dialogue load error: " << error << "\n";
            activeDialogueLines.clear();
            activeDialogueLineIndex = 0;
            visualNovelSystem.advance(nullptr);
            continue;
          }
          activeDialogueLineIndex = 0;
          if (activeDialogueLines.empty()) {
            visualNovelSystem.advance(nullptr);
            continue;
          }
          debugText = std::string{"Mode: "} + getGameModeName();
          return;
        }

        case game::vn::ScenarioCommandType::Battle:
          startBattleFromCommand(*command);
          debugText = std::string{"Mode: "} + getGameModeName();
          return;

        case game::vn::ScenarioCommandType::Goto: {
          std::string error;
          visualNovelSystem.advance(&error);
          if (!error.empty()) {
            std::cerr << "Visual novel goto error: " << error << "\n";
          }
          continue;
        }

        case game::vn::ScenarioCommandType::Background:
          setVisualNovelBackground(sceneSystem, command->image);
          visualNovelSystem.advance(nullptr);
          continue;

        case game::vn::ScenarioCommandType::ShowCharacter:
        case game::vn::ScenarioCommandType::HideCharacter:
        case game::vn::ScenarioCommandType::PlayBgm:
        case game::vn::ScenarioCommandType::PlaySe:
        case game::vn::ScenarioCommandType::SetFlag:
        case game::vn::ScenarioCommandType::Unknown:
        default:
          visualNovelSystem.advance(nullptr);
          continue;
      }
    }

    debugText = std::string{"Mode: "} + getGameModeName();
  }

  void EngineLoop::initWorld(SceneSystem &sceneSystem) {
    sceneSystem.loadGameObjects();

    tilemapSystem = std::make_unique<tilemap::TilemapSystem>();
    std::string mapError;
    if (!tilemapSystem->load(sceneSystem, kGameplayConfig.mapPath, &mapError)) {
      std::cerr << "Failed to load tilemap: " << mapError << "\n";
    }

    auto *character = sceneSystem.findObject(sceneSystem.getCharacterId());
    if (character) {
      glm::vec3 spawnPos{0.f, 0.f, 0.f};
      if (tilemapSystem && tilemapSystem->hasPlayerSpawnWorld()) {
        spawnPos = tilemapSystem->getPlayerSpawnWorld();
      }
      character->transform.translation = spawnPos;
      character->transformDirty = true;
    }

    std::vector<glm::vec3> mobSpawns{};
    if (tilemapSystem) {
      mobSpawns = tilemapSystem->getMobSpawnPointsWorld();
    }
    if (mobSpawns.empty()) {
      if (auto *character = sceneSystem.findObject(sceneSystem.getCharacterId())) {
        mobSpawns.push_back(character->transform.translation + kGameplayConfig.fallbackMobSpawnOffset);
      }
    }

    mobSystem.init(sceneSystem, std::move(mobSpawns));
    backgroundSystem.init(sceneSystem);
    scoreOverlay.init(sceneSystem);
    score = 0;
    activeSignMessage.clear();
    activeSignMessageTimer = 0.f;
    interactKeyHeld = false;
    statsToggleKeyHeld = false;
  }

  LveGameObject *EngineLoop::updateSimulation(
    float frameTime,
    backend::InputProvider &input,
    SceneSystem &sceneSystem,
    game::PlayerController &playerController,
    SpriteAnimator *spriteAnimator,
    std::string &tileDebugText) {
    const auto characterId = sceneSystem.getCharacterId();
    auto *characterPtr = sceneSystem.findObject(characterId);
    if (!characterPtr) {
      std::cerr << "Character object missing; cannot update\n";
      return nullptr;
    }

    auto &character = *characterPtr;

    if (activeSignMessageTimer > 0.f) {
      activeSignMessageTimer -= frameTime;
      if (activeSignMessageTimer <= 0.f) {
        activeSignMessageTimer = 0.f;
        activeSignMessage.clear();
      }
    }

    const bool interactDown = input.isKeyPressed(backend::KeyCode::E);
    if (interactDown && !interactKeyHeld && tilemapSystem) {
      std::string signMessage{};
      if (tilemapSystem->getSignMessageAtWorld(character.transform.translation, signMessage)) {
        activeSignMessage = signMessage;
        activeSignMessageTimer = kSignConfig.displayDurationSeconds;
      }
    }
    interactKeyHeld = interactDown;

    bool shouldPlayHitSe = false;
    const float hpBeforeUpdate = playerController.getStats().hp;
    playerController.update(input, frameTime, character, tilemapSystem.get(), spriteAnimator);
    if (playerController.getStats().hp < hpBeforeUpdate) {
      shouldPlayHitSe = true;
    }
    bulletSystem.update(input, frameTime, sceneSystem, character);
    if (bulletSystem.consumeShotEvent()) {
      audioSystem.playSe(kAudioConfig.seShoot);
    }

    if (spriteAnimator) {
      const char *stateName = (character.objState == ObjectState::WALKING) ? "walking" : "idle";
      if (character.spriteStateName != stateName || !character.diffuseMap) {
        spriteAnimator->applySpriteState(character, stateName);
      }
    }

    sceneSystem.updateAnimationFrame(
      character,
      kGameplayConfig.characterAnimFrames,
      frameTime,
      kGameplayConfig.characterAnimFrameDuration);
    mobSystem.update(frameTime, character.transform.translation, sceneSystem, tilemapSystem.get());

    const auto mobs = mobSystem.getMobs(sceneSystem);
    for (auto *mob : mobs) {
      if (mob && overlapsAabb(character, *mob)) {
        if (playerController.applyDamage(kGameplayConfig.mobContactDamage)) {
          shouldPlayHitSe = true;
        }
      }
    }

    const auto hitMobIds = bulletSystem.collectMobHits(sceneSystem, mobs);
    if (!hitMobIds.empty()) {
      shouldPlayHitSe = true;
    }
    for (auto mobId : hitMobIds) {
      const auto result = mobSystem.applyDamage(sceneSystem, mobId, kGameplayConfig.bulletDamageToMob);
      if (result == game::MobDamageResult::Killed) {
        score += 1;
        audioSystem.playSe(kAudioConfig.seMobDeath);
      }
    }

    if (shouldPlayHitSe) {
      audioSystem.playSe(kAudioConfig.seHit);
    }

    if (tilemapSystem) {
      tileDebugText = tilemapSystem->buildDebugString(character.transform.translation);
    } else {
      tileDebugText.clear();
    }
    std::ostringstream debugStream;
    debugStream << "Mode: " << getGameModeName() << "\nScore: " << score << "\n" << tileDebugText;
    tileDebugText = debugStream.str();

    return characterPtr;
  }

  void EngineLoop::handleGameOver(
    SceneSystem &sceneSystem,
    game::PlayerController &playerController,
    LveGameObject &character,
    std::string &tileDebugText) {
    if (playerController.getStats().hp > 0.f) {
      return;
    }

    glm::vec3 respawnPos{0.f, 0.f, 0.f};
    if (tilemapSystem && tilemapSystem->hasPlayerSpawnWorld()) {
      respawnPos = tilemapSystem->getPlayerSpawnWorld();
    }

    character.transform.translation = respawnPos;
    character.transformDirty = true;
    character.objState = ObjectState::IDLE;

    playerController.resetForNewRun(respawnPos);
    bulletSystem.reset(sceneSystem);
    mobSystem.reset(sceneSystem);
    score = 0;
    activeSignMessage.clear();
    activeSignMessageTimer = 0.f;
    interactKeyHeld = false;

    tileDebugText = "Game Over - Restarted\nScore: 0";
  }

  backend::CommandBufferHandle EngineLoop::beginFrame(
    backend::RenderBackend &renderBackend,
    backend::EditorRenderBackend &debugUi,
    SceneSystem &sceneSystem) {
    auto commandBuffer = renderBackend.beginFrame();
    if (renderBackend.wasSwapChainRecreated()) {
      debugUi.onRenderPassChanged(
        renderBackend.getSwapChainRenderPass(),
        static_cast<uint32_t>(renderBackend.getSwapChainImageCount()));
      sceneSystem.resetDescriptorCaches();
    }
    return commandBuffer;
  }

  void EngineLoop::updateCameraAndProjection(
    LveCamera &gameCamera,
    backend::RenderBackend &renderBackend,
    const LveGameObject &character,
    float &outOrthoWidth,
    float &outOrthoHeight) {
    const backend::RenderExtent windowExtent = runtime->window().getExtent();
    const uint32_t windowWidth = windowExtent.width;
    const uint32_t windowHeight = windowExtent.height;
    const float windowAspect = windowHeight > 0
      ? (static_cast<float>(windowWidth) / static_cast<float>(windowHeight))
      : renderBackend.getAspectRatio();

    const glm::vec3 gameCamPos = character.transform.translation + kCameraConfig.followOffset;
    gameCamera.setViewTarget(gameCamPos, character.transform.translation);

    outOrthoHeight = kCameraConfig.orthoHeight;
    outOrthoHeight = orthoZoomHeight;
    outOrthoWidth = outOrthoHeight * windowAspect;

    if (useOrthoCamera) {
      gameCamera.setOrthographicProjection(
        -outOrthoWidth * 0.5f,
        outOrthoWidth * 0.5f,
        -outOrthoHeight * 0.5f,
        outOrthoHeight * 0.5f,
        kCameraConfig.orthoNear,
        kCameraConfig.orthoFar);
    } else {
      gameCamera.setPerspectiveProjection(
        glm::radians(kCameraConfig.perspectiveFovDegrees),
        windowAspect,
        kCameraConfig.perspectiveNear,
        kCameraConfig.perspectiveFar);
    }
  }

  void EngineLoop::renderFrameAndUi(
    float frameTime,
    LveCamera &gameCamera,
    float orthoWidth,
    float orthoHeight,
    SceneSystem &sceneSystem,
    backend::RenderBackend &renderBackend,
    backend::EditorRenderBackend &debugUi,
    game::PlayerController &playerController,
    LveGameObject &character,
    const std::string &tileDebugText,
    std::vector<LveGameObject*> &renderObjects,
    backend::CommandBufferHandle commandBuffer) {
    if (activeGameMode == GameMode::Platform) {
      backgroundSystem.update(sceneSystem, character.transform.translation, orthoWidth, orthoHeight, frameTime);
      scoreOverlay.update(sceneSystem, gameCamera, orthoWidth, orthoHeight, score);
    }

    renderBackend.setWireframe(wireframeEnabled);
    renderBackend.setNormalView(normalViewEnabled);

    const int frameIndex = renderBackend.getFrameIndex();
    if (activeGameMode == GameMode::VisualNovel) {
      updateVisualNovelBackground(sceneSystem, gameCamera, orthoWidth, orthoHeight);
    }
    sceneSystem.updateBuffers(frameIndex);
    if (activeGameMode == GameMode::Platform) {
      sceneSystem.collectObjects(renderObjects);
      if (hasVisualNovelBackground) {
        renderObjects.erase(
          std::remove_if(
            renderObjects.begin(),
            renderObjects.end(),
            [this](const LveGameObject *obj) {
              return obj && obj->getId() == visualNovelBackgroundId;
            }),
          renderObjects.end());
      }
    } else {
      renderObjects.clear();
      if (activeGameMode == GameMode::VisualNovel) {
        if (hasVisualNovelBackground) {
          if (auto *background = sceneSystem.findObject(visualNovelBackgroundId)) {
            renderObjects.push_back(background);
          }
        }
      }
    }

    debugUi.newFrame();
    if (activeGameMode == GameMode::Platform) {
      game::ui::drawPlayerHpBar(gameCamera, character, playerController.getStats());
      game::ui::drawTimedMessage(activeSignMessage, activeSignMessageTimer);
    }
    const glm::vec3 cameraPos = gameCamera.getPosition();
    const glm::vec3 cameraRot{};
    debugUi.buildUI(
      frameTime,
      cameraPos,
      cameraRot,
      tileDebugText,
      wireframeEnabled,
      normalViewEnabled,
      useOrthoCamera,
      playerController.getTuning(),
      showEngineStats);

    if (activeGameMode == GameMode::VisualNovel) {
      game::vn::ui::VisualNovelOverlayState overlayState{};
      overlayState.command = visualNovelSystem.currentCommand();
      overlayState.dialogueLine = getActiveDialogueLine();
      overlayState.complete = visualNovelSystem.isComplete();
      overlayState.statusText = "Space / Left Click: Next    V: Platform";
      const int selectedChoice = game::vn::ui::drawVisualNovelOverlay(overlayState);
      if (selectedChoice >= 0) {
        std::string error;
        if (!visualNovelSystem.choose(static_cast<std::size_t>(selectedChoice), &error) && !error.empty()) {
          std::cerr << "Choice error: " << error << "\n";
        }
      }
    } else if (activeGameMode == GameMode::Battle) {
      const bool attackPressed = game::battle::ui::drawBattleOverlay(battleSystem);
      if (attackPressed) {
        performBattleAction();
      }
    }

    renderBackend.beginSwapChainRenderPass(commandBuffer);
    renderBackend.renderMainView(
      frameTime,
      gameCamera,
      renderObjects,
      commandBuffer);
    debugUi.render(commandBuffer);
    renderBackend.endSwapChainRenderPass(commandBuffer);
    renderBackend.endFrame();
    debugUi.renderPlatformWindows();
  }

  /* Main loop: input -> update -> render -> UI */
  void EngineLoop::run() {
    auto &sceneSystem = runtime->sceneSystem();
    auto &renderBackend = runtime->renderBackend();
    auto &window = runtime->window();
    auto &input = window.input();
    auto &debugUi = runtime->editorBackend();

    SpriteAnimator *spriteAnimator = sceneSystem.getSpriteAnimator();

    LveCamera gameCamera{};
    debugUi.init(
      renderBackend.getSwapChainRenderPass(),
      static_cast<uint32_t>(renderBackend.getSwapChainImageCount()));

    game::PlayerController playerController{};

    auto currentTime = std::chrono::high_resolution_clock::now();
    std::vector<LveGameObject*> renderObjects{};
    std::string tileDebugText;

    while (!window.shouldClose()) {
      window.pollEvents();
      updateModeShortcuts(input);

      const bool f3Down = input.isKeyPressed(backend::KeyCode::F3);
      if (f3Down && !statsToggleKeyHeld) {
        showEngineStats = !showEngineStats;
      }
      statsToggleKeyHeld = f3Down;

      const float wheelDelta = input.consumeMouseWheelDeltaY();
      if (useOrthoCamera && std::abs(wheelDelta) > 0.0001f) {
        orthoZoomHeight -= wheelDelta * kCameraConfig.orthoZoomStep;
        orthoZoomHeight = std::clamp(
          orthoZoomHeight,
          kCameraConfig.orthoMinHeight,
          kCameraConfig.orthoMaxHeight);
      }

      const float frameTime = computeClampedFrameTime(currentTime);

      LveGameObject *character = nullptr;
      switch (activeGameMode) {
        case GameMode::Platform:
          character = updateSimulation(
            frameTime,
            input,
            sceneSystem,
            playerController,
            spriteAnimator,
            tileDebugText);
          break;
        case GameMode::VisualNovel:
          updateVisualNovel(input, sceneSystem, tileDebugText);
          character = sceneSystem.findObject(sceneSystem.getCharacterId());
          break;
        case GameMode::Battle:
          updateBattle(input, tileDebugText);
          character = sceneSystem.findObject(sceneSystem.getCharacterId());
          break;
      }
      if (!character) {
        continue;
      }
      if (activeGameMode == GameMode::Platform) {
        handleGameOver(sceneSystem, playerController, *character, tileDebugText);
      }

      auto commandBuffer = beginFrame(renderBackend, debugUi, sceneSystem);
      if (!commandBuffer) {
        continue;
      }

      float orthoWidth = 0.f;
      float orthoHeight = 0.f;
      updateCameraAndProjection(
        gameCamera,
        renderBackend,
        *character,
        orthoWidth,
        orthoHeight);

      renderFrameAndUi(
        frameTime,
        gameCamera,
        orthoWidth,
        orthoHeight,
        sceneSystem,
        renderBackend,
        debugUi,
        playerController,
        *character,
        tileDebugText,
        renderObjects,
        commandBuffer);
    }

    debugUi.shutdown();
    runtime->editorBackend().waitIdle();
  }

} // namespace lve
