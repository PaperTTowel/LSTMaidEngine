#pragma once

#include "Engine/Backend/render_backend.hpp"
#include "Engine/game_frame_controller.hpp"
#include "Editor/editor_frame_controller.hpp"
#include "Editor/editor_system.hpp"

#include <vector>

namespace lve {
  class LveGameObject;
  class SceneSystem;

  class RenderFrameCoordinator {
  public:
    backend::CommandBufferHandle beginFrame(
      backend::RenderBackend &renderBackend,
      EditorSystem &editorSystem,
      SceneSystem &sceneSystem);

    void ensureOffscreenTargets(
      backend::RenderBackend &renderBackend,
      const ViewportInfo &sceneView,
      const ViewportInfo &gameView);

    void renderFrame(
      float frameTime,
      backend::RenderBackend &renderBackend,
      EditorSystem &editorSystem,
      SceneSystem &sceneSystem,
      EditorFrameState &editorFrame,
      GameFrameState &gameFrame,
      backend::CommandBufferHandle commandBuffer);

  private:
    std::vector<LveGameObject*> renderObjects{};
  };
} // namespace lve
