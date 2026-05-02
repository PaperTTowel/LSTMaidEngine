#pragma once

#include "Engine/Backend/runtime_backend.hpp"
#include "Engine/audio_system.hpp"
#include "Game/Platform/Tilemap/tilemap_system.hpp"
#include "Game/Platform/background_system.hpp"
#include "Game/Platform/bullet_system.hpp"
#include "Game/Platform/mob_system.hpp"
#include "Game/RPGBattle/battle_system.hpp"
#include "Game/UI/score_overlay.hpp"
#include "Game/VisualNovel/visual_novel_system.hpp"

// std
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace lve {
  class LveCamera;
  class LveGameObject;
  class SceneSystem;
  class SpriteAnimator;

  namespace game {
    class PlayerController;
  }

  namespace backend {
    class EditorRenderBackend;
    class InputProvider;
    class RenderBackend;
  }

  class EngineLoop {
  public:
    static constexpr int WIDTH = 800;
    static constexpr int HEIGHT = 600;

    enum class GameMode {
      Platform,
      VisualNovel,
      Battle
    };

    EngineLoop();
    ~EngineLoop();

    EngineLoop(const EngineLoop &) = delete;
    EngineLoop &operator=(const EngineLoop &) = delete;

    void run();

  private:
    void initWorld(SceneSystem &sceneSystem);
    LveGameObject *updateSimulation(
      float frameTime,
      backend::InputProvider &input,
      SceneSystem &sceneSystem,
      game::PlayerController &playerController,
      SpriteAnimator *spriteAnimator,
      std::string &tileDebugText);
    void handleGameOver(
      SceneSystem &sceneSystem,
      game::PlayerController &playerController,
      LveGameObject &character,
      std::string &tileDebugText);
    backend::CommandBufferHandle beginFrame(
      backend::RenderBackend &renderBackend,
      backend::EditorRenderBackend &debugUi,
      SceneSystem &sceneSystem);
    void updateCameraAndProjection(
      LveCamera &gameCamera,
      backend::RenderBackend &renderBackend,
      const LveGameObject &character,
      float &outOrthoWidth,
      float &outOrthoHeight);
    void renderFrameAndUi(
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
      backend::CommandBufferHandle commandBuffer);
    void setGameMode(GameMode nextMode);
    const char *getGameModeName() const;
    bool loadVisualNovelScenario();
    void updateModeShortcuts(backend::InputProvider &input);
    bool consumeVisualNovelAdvance(backend::InputProvider &input);
    void updateVisualNovel(
      backend::InputProvider &input,
      SceneSystem &sceneSystem,
      std::string &debugText);
    void updateBattle(backend::InputProvider &input, std::string &debugText);
    void performBattleAction();
    void startBattleFromCommand(const game::vn::ScenarioCommand &command);
    game::battle::BattleDefinition makeDefaultBattleDefinition(const std::string &enemyId) const;
    const game::vn::DialogueLine *getActiveDialogueLine() const;
    void setVisualNovelBackground(SceneSystem &sceneSystem, const std::string &imagePath);
    void updateVisualNovelBackground(
      SceneSystem &sceneSystem,
      const LveCamera &gameCamera,
      float orthoWidth,
      float orthoHeight);

    std::unique_ptr<backend::RuntimeBackend> runtime;
    std::unique_ptr<tilemap::TilemapSystem> tilemapSystem;
    game::BackgroundSystem backgroundSystem{};
    game::BulletSystem bulletSystem{};
    game::MobSystem mobSystem{};
    game::vn::VisualNovelSystem visualNovelSystem{};
    game::battle::BattleSystem battleSystem{};
    std::vector<game::vn::DialogueLine> activeDialogueLines{};
    std::size_t activeDialogueLineIndex{0};
    LveGameObject::id_t visualNovelBackgroundId{0};
    bool hasVisualNovelBackground{false};
    std::string battleWinNode{};
    std::string battleLoseNode{};
    AudioSystem audioSystem{};
    game::ui::ScoreOverlay scoreOverlay{};
    GameMode activeGameMode{GameMode::Platform};
    bool useOrthoCamera{true};
    bool wireframeEnabled{false};
    bool normalViewEnabled{false};
    bool showEngineStats{false};
    float orthoZoomHeight{10.f};
    int score{0};
    std::string activeSignMessage{};
    float activeSignMessageTimer{0.f};
    bool interactKeyHeld{false};
    bool statsToggleKeyHeld{false};
    bool modeSwitchKeyHeld{false};
    bool visualNovelAdvanceHeld{false};
    bool battleActionKeyHeld{false};
  };
} // namespace lve



