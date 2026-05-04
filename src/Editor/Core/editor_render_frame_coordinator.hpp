#pragma once

#include "Editor/Core/editor_frame_controller.hpp"
#include "Editor/Core/editor_system.hpp"
#include "Engine/Rendering/render_backend.hpp"
#include "Engine/Core/game_frame_controller.hpp"

#include <vector>

namespace lve {
  class LveGameObject;
  class SceneSystem;

  class EditorRenderFrameCoordinator {
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
