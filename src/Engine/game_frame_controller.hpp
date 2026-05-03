#pragma once

#include "Engine/Backend/input.hpp"
#include "Engine/Backend/render_types.hpp"
#include "Engine/camera.hpp"
#include "Engine/viewport_info.hpp"
#include "utils/keyboard_movement_controller.hpp"

#include <cstdint>

namespace lve {
  class SceneSystem;
  class SpriteAnimator;

  struct GameFrameState {
    LveCamera &camera;
    bool canRenderGameView{false};
  };

  class GameFrameController {
  public:
    void updateCharacter(
      float frameTime,
      backend::InputProvider &input,
      SceneSystem &sceneSystem,
      SpriteAnimator *spriteAnimator);

    GameFrameState updateCamera(
      SceneSystem &sceneSystem,
      const ViewportInfo &gameView,
      backend::RenderExtent windowExtent,
      float fallbackAspect,
      bool useOrthoCamera);

  private:
    CharacterMovementController characterController{};
    LveCamera gameCamera{};
  };
} // namespace lve
