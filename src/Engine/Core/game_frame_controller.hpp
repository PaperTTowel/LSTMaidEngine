#pragma once

#include "Engine/Rendering/input.hpp"
#include "Engine/Rendering/render_types.hpp"
#include "Engine/Core/camera.hpp"
#include "Engine/Core/viewport_info.hpp"
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
