#include "Editor/Core/editor.hpp"

#include "Engine/Scene/scene_system.hpp"

#include <chrono>

namespace lve {
  namespace {
    backend::RuntimeBackendConfig createEditorBackendConfig() {
      backend::RuntimeBackendConfig config{};
      config.api = backend::BackendApi::Vulkan;
      config.width = Editor::WIDTH;
      config.height = Editor::HEIGHT;
      config.title = "PaperTTowelEngine";
      return config;
    }
  } // namespace

  Editor::Editor()
    : backend{createEditorBackend(createEditorBackendConfig())}
    , editorSystem{std::make_unique<EditorSystem>(backend.editorRender())} {
    auto &sceneSystem = backend.runtime().sceneSystem();
    sceneSystem.loadGameObjects();
  }

  Editor::~Editor() {}

  void Editor::run() {
    auto &runtime = backend.runtime();
    auto &sceneSystem = runtime.sceneSystem();
    auto &renderBackend = runtime.renderBackend();
    auto &window = runtime.window();
    auto &input = window.input();

    SpriteAnimator *spriteAnimator = sceneSystem.getSpriteAnimator();

    editorSystem->init(
      renderBackend.getSwapChainRenderPass(),
      static_cast<uint32_t>(renderBackend.getSwapChainImageCount()));

    editorFrameController.initialize(sceneSystem, window);

    auto currentTime = std::chrono::high_resolution_clock::now();
    while (!window.shouldClose()) {
      window.pollEvents();

      auto newTime = std::chrono::high_resolution_clock::now();
      float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
      currentTime = newTime;

      gameFrameController.updateCharacter(frameTime, input, sceneSystem, spriteAnimator);

      auto commandBuffer = renderFrameCoordinator.beginFrame(
        renderBackend,
        *editorSystem,
        sceneSystem);
      if (commandBuffer) {
        renderFrameCoordinator.ensureOffscreenTargets(
          renderBackend,
          editorFrameController.getSceneView(),
          editorFrameController.getGameView());

        const backend::RenderExtent windowExtent = window.getExtent();
        EditorFrameState editorFrame = editorFrameController.update(
          frameTime,
          input,
          window,
          renderBackend,
          *editorSystem,
          sceneSystem,
          sceneSystem.getCharacterId(),
          spriteAnimator);

        GameFrameState gameFrame = gameFrameController.updateCamera(
          sceneSystem,
          editorFrame.gameView,
          windowExtent,
          renderBackend.getAspectRatio(),
          editorFrame.useOrthoCamera);

        renderFrameCoordinator.renderFrame(
          frameTime,
          renderBackend,
          *editorSystem,
          sceneSystem,
          editorFrame,
          gameFrame,
          commandBuffer);
      }
    }

    renderBackend.shutdown();
    editorSystem->shutdown();
    backend.editorRender().waitIdle();
  }

} // namespace lve
