#pragma once

#include "Editor/Core/editor_system.hpp"
#include "Engine/Rendering/render_backend.hpp"
#include "Engine/Rendering/runtime_window.hpp"
#include "Engine/Core/camera.hpp"
#include "utils/keyboard_movement_controller.hpp"

namespace lve {
  class SceneSystem;
  class SpriteAnimator;

  struct EditorFrameState {
    EditorFrameResult result{};
    LveCamera &camera;
    const ViewportInfo &sceneView;
    const ViewportInfo &gameView;
    bool useOrthoCamera{false};
  };

  class EditorFrameController {
  public:
    void initialize(SceneSystem &sceneSystem, backend::WindowBackend &window);

    EditorFrameState update(
      float frameTime,
      backend::InputProvider &input,
      backend::WindowBackend &window,
      backend::RenderBackend &renderBackend,
      EditorSystem &editorSystem,
      SceneSystem &sceneSystem,
      LveGameObject::id_t characterId,
      SpriteAnimator *&spriteAnimator);

    LveGameObject::id_t getViewerId() const { return viewerId; }
    const ViewportInfo &getSceneView() const { return sceneViewInfo; }
    const ViewportInfo &getGameView() const { return gameViewInfo; }
    bool isUsingOrthoCamera() const { return useOrthoCamera; }

  private:
    LveCamera editorCamera{};
    KeyboardMovementController cameraController{};
    ViewportInfo sceneViewInfo{};
    ViewportInfo gameViewInfo{};
    LveGameObject::id_t viewerId{0};
    bool initialized{false};
    bool useOrthoCamera{false};
    bool wireframeEnabled{false};
    bool normalViewEnabled{false};
    editor::ResourceBrowserState resourceBrowserState{};
  };
} // namespace lve
