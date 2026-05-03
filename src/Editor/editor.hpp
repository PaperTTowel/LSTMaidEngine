#pragma once

#include "Editor/editor_frame_controller.hpp"
#include "Editor/editor_render_frame_coordinator.hpp"
#include "Editor/editor_system.hpp"
#include "Engine/Backend/runtime_backend.hpp"
#include "Engine/game_frame_controller.hpp"

#include <memory>

namespace lve {

  class Editor {
  public:
    static constexpr int WIDTH = 800;
    static constexpr int HEIGHT = 600;

    Editor();
    ~Editor();

    Editor(const Editor &) = delete;
    Editor &operator=(const Editor &) = delete;

    void run();

  private:
    std::unique_ptr<backend::RuntimeBackend> runtime;
    std::unique_ptr<EditorSystem> editorSystem;
    EditorFrameController editorFrameController;
    GameFrameController gameFrameController;
    EditorRenderFrameCoordinator renderFrameCoordinator;
  };

} // namespace lve
