#pragma once

#include "Engine/Rendering/render_backend.hpp"
#include "Engine/Rendering/runtime_window.hpp"

namespace lve {
  class SceneSystem;
}

namespace lve::backend {
  class RuntimeBackend {
  public:
    virtual ~RuntimeBackend() = default;

    virtual WindowBackend &window() = 0;
    virtual RenderBackend &renderBackend() = 0;
    virtual SceneSystem &sceneSystem() = 0;
  };
} // namespace lve::backend
