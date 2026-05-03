#include "Engine/render_frame_coordinator.hpp"

#include "Engine/scene_system.hpp"

#include <cstdint>

namespace lve {

  backend::CommandBufferHandle RenderFrameCoordinator::beginFrame(
    backend::RenderBackend &renderBackend,
    EditorSystem &editorSystem,
    SceneSystem &sceneSystem) {
    auto commandBuffer = renderBackend.beginFrame();
    if (renderBackend.wasSwapChainRecreated()) {
      editorSystem.onRenderPassChanged(
        renderBackend.getSwapChainRenderPass(),
        static_cast<uint32_t>(renderBackend.getSwapChainImageCount()));
      sceneSystem.resetDescriptorCaches();
    }
    return commandBuffer;
  }

  void RenderFrameCoordinator::ensureOffscreenTargets(
    backend::RenderBackend &renderBackend,
    const ViewportInfo &sceneView,
    const ViewportInfo &gameView) {
    renderBackend.ensureOffscreenTargets(
      sceneView.visible ? sceneView.width : 0,
      sceneView.visible ? sceneView.height : 0,
      gameView.visible ? gameView.width : 0,
      gameView.visible ? gameView.height : 0);
  }

  void RenderFrameCoordinator::renderFrame(
    float frameTime,
    backend::RenderBackend &renderBackend,
    EditorSystem &editorSystem,
    SceneSystem &sceneSystem,
    EditorFrameState &editorFrame,
    GameFrameState &gameFrame,
    backend::CommandBufferHandle commandBuffer) {
    const int frameIndex = renderBackend.getFrameIndex();
    sceneSystem.updateBuffers(frameIndex);

    sceneSystem.collectObjects(renderObjects);
    renderBackend.renderSceneView(
      frameTime,
      editorFrame.camera,
      renderObjects,
      commandBuffer);

    if (gameFrame.canRenderGameView) {
      sceneSystem.collectObjects(renderObjects);
      renderBackend.renderGameView(
        frameTime,
        gameFrame.camera,
        renderObjects,
        commandBuffer);
    }

    renderBackend.beginSwapChainRenderPass(commandBuffer);
    editorSystem.render(commandBuffer);
    renderBackend.endSwapChainRenderPass(commandBuffer);
    renderBackend.endFrame();
    editorSystem.renderPlatformWindows();
  }

} // namespace lve
