#pragma once

#include "Editor/Core/editor_backend.hpp"
#include "Editor/Core/editor_frame_controller.hpp"
#include "Editor/Core/editor_render_frame_coordinator.hpp"
#include "Editor/Core/editor_system.hpp"
#include "Engine/Core/game_frame_controller.hpp"

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
    EditorBackend backend;
    std::unique_ptr<EditorSystem> editorSystem;
    EditorFrameController editorFrameController;
    GameFrameController gameFrameController;
    EditorRenderFrameCoordinator renderFrameCoordinator;
  };

} // namespace lve
